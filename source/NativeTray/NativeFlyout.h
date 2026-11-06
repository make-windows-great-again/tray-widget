#pragma once
#include <windows.h>
#include <windowsx.h>
#include <inspectable.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <functional>
#include <memory>
#include <string>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "FlyoutPlacement.h"
#include "NativeMenuStyle.h"

// Documented Windows.UI.Xaml interface; this SDK omits its C++ declaration.
struct __declspec(uuid("06636c29-5a17-458d-8ea2-2422d997a922")) IXamlSourceTransparencyYata : ::IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE get_IsBackgroundTransparent(boolean* value)=0;
    virtual HRESULT STDMETHODCALLTYPE put_IsBackgroundTransparent(boolean value)=0;
};

// A fixed transparent XAML island clips composition motion to the work area.
// Context menus use the platform MenuFlyout and its native popup behavior.
class NativeFlyout
{
public:
    enum class Phase { Closed=0, Opening=1, Open=2, Closing=3 };
    NativeFlyout()=default;
    NativeFlyout(NativeFlyout const&)=delete;
    NativeFlyout& operator=(NativeFlyout const&)=delete;
    NativeFlyout(NativeFlyout&&)=delete;
    NativeFlyout& operator=(NativeFlyout&&)=delete;
    ~NativeFlyout() { Close(); }
    void Initialize(HWND owner, std::function<void(bool)> visibility, std::function<void(UINT)> command,
        std::function<void(Phase)> phaseChanged={})
    {
        if(m_source)winrt::throw_hresult(E_ILLEGAL_METHOD_CALL);
        m_lifetime=std::make_shared<bool>(true);m_destroying=false;
        namespace x = winrt::Windows::UI::Xaml;
        namespace c = x::Controls;
        m_owner = owner;
        m_visibility = std::move(visibility);
        m_command = std::move(command);
        m_phaseChanged = std::move(phaseChanged);
        m_manager = x::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        m_source = x::Hosting::DesktopWindowXamlSource();
        winrt::check_hresult(x::Window::Current().as<IXamlSourceTransparencyYata>()->put_IsBackgroundTransparent(true));
        auto interop = m_source.as<IDesktopWindowXamlSourceNative>();
        winrt::check_hresult(interop->AttachToWindow(owner));
        winrt::check_hresult(interop->get_WindowHandle(&m_child));
        SetWindowPos(m_child, nullptr, 0, 0, 1, 1, SWP_NOZORDER | SWP_SHOWWINDOW);
        m_root = c::Grid();
        m_root.Width(1);m_root.Height(1);
        m_root.Loaded([this,life=m_lifetime](auto const&,auto const&) noexcept {if(!*life)return;EventGuard([&]{OpenPending();});});
        m_root.Background(x::Media::SolidColorBrush(winrt::Windows::UI::Color{0,0,0,0}));
        m_root.KeyDown([this,life=m_lifetime](auto const&, x::Input::KeyRoutedEventArgs const& args) noexcept {if(!*life)return;EventGuard([&]{
            if (args.Key() == winrt::Windows::System::VirtualKey::Escape) { Hide(); args.Handled(true); }
        });});
        m_root.PointerPressed([this,life=m_lifetime](auto const&, x::Input::PointerRoutedEventArgs const& args) noexcept {if(!*life)return;EventGuard([&]{
            const auto point=args.GetCurrentPoint(m_root).Position();
            if(m_visible && (point.X<m_insetX || point.X>m_insetX+m_panelWidth ||
                point.Y<m_insetY || point.Y>m_insetY+m_panelHeight))Hide();
        });});
        m_source.Content(m_root);
        m_panel = x::Markup::XamlReader::Load(LR"XAML(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" Padding="24,16,24,20" Background="Transparent">
 <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/></Grid.RowDefinitions>
 <Grid Margin="0,0,0,16" ColumnSpacing="12">
  <Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="32"/></Grid.ColumnDefinitions>
  <TextBlock x:Name="PanelTitle" Text="Native controls" FontSize="14" FontWeight="SemiBold" VerticalAlignment="Center"/>
  <Button x:Name="CloseButton" Grid.Column="1" Width="32" Height="32" Padding="0" Background="Transparent" BorderThickness="0" CornerRadius="4" UseSystemFocusVisuals="True" AutomationProperties.Name="Закрыть" AutomationProperties.AutomationId="ClosePanel" ToolTipService.ToolTip="Закрыть (Esc)">
   <FontIcon FontFamily="Segoe Fluent Icons" Glyph="&#xE8BB;" FontSize="12" AutomationProperties.AccessibilityView="Raw"/>
  </Button>
 </Grid>
 <ContentPresenter x:Name="PanelContent" Grid.Row="1" HorizontalContentAlignment="Stretch"/>
</Grid>)XAML").as<c::Grid>();
        const auto close = m_panel.FindName(L"CloseButton").as<c::Button>();
        close.Click([this,life=m_lifetime](auto const&, auto const&) noexcept {if(!*life)return;EventGuard([&]{Hide();});});
        m_presenter=x::Markup::XamlReader::Load(LR"(<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" CornerRadius="8" BorderThickness="1" BorderBrush="{ThemeResource FlyoutBorderThemeBrush}" Background="{ThemeResource SystemControlAcrylicWindowBrush}"/>)").as<c::Border>();
        x::Automation::AutomationProperties::SetAutomationId(m_presenter,L"VpnPanelSurface");
        m_panelScroll=c::ScrollViewer();
        m_panelScroll.HorizontalScrollMode(c::ScrollMode::Disabled);
        m_panelScroll.HorizontalScrollBarVisibility(c::ScrollBarVisibility::Disabled);
        m_panelScroll.VerticalScrollMode(c::ScrollMode::Auto);
        m_panelScroll.VerticalScrollBarVisibility(c::ScrollBarVisibility::Auto);
        m_panelScroll.ZoomMode(c::ZoomMode::Disabled);m_panelScroll.IsTabStop(false);
        m_panelScroll.Content(m_panel);
        m_presenter.Child(m_panelScroll);
        m_shadowHost=c::Grid();
        m_surfaceRoot=c::Grid();
        m_surfaceRoot.HorizontalAlignment(x::HorizontalAlignment::Left);
        m_surfaceRoot.VerticalAlignment(x::VerticalAlignment::Top);
        m_surfaceRoot.Margin({24,24,0,0});
        m_surfaceRoot.Children().Append(m_shadowHost);
        m_surfaceRoot.Children().Append(m_presenter);
        m_surfaceRoot.Visibility(x::Visibility::Collapsed);
        m_root.Children().Append(m_surfaceRoot);
        m_root.TabFocusNavigation(x::Input::KeyboardNavigationMode::Cycle);
    }

    void Theme(bool light)
    {
        if(m_menuOpen)m_menu.Hide();
        const auto theme = light ? winrt::Windows::UI::Xaml::ElementTheme::Light : winrt::Windows::UI::Xaml::ElementTheme::Dark;
        if(m_root) m_root.RequestedTheme(theme);
        if(m_panel) m_panel.RequestedTheme(theme);
        m_light=light;
        if(m_shadow)m_shadow.Opacity(light ? 0.18f : 0.32f);
    }

    // All standard Windows.UI.Xaml controls are accepted by this content slot.
    // Call on the owning UI thread, after Initialize; no window-specific demo code.
    void SetContent(winrt::Windows::UI::Xaml::UIElement const& content,
        winrt::hstring const& title, float preferredWidth=520)
    {
        if(!m_panel || !content)winrt::throw_hresult(E_INVALIDARG);
        m_panel.FindName(L"PanelTitle").as<winrt::Windows::UI::Xaml::Controls::TextBlock>().Text(title);
        m_panel.FindName(L"PanelContent").as<winrt::Windows::UI::Xaml::Controls::ContentPresenter>().Content(content);
        PreferredWidth(preferredWidth);
    }

    auto ContentRoot() const {return m_root;}

    // Borderless client surfaces are supplied by the statically linked shell
    // panel library. Their composition material lives through the exit batch.
    void ShellSurface()
    {
        namespace x=winrt::Windows::UI::Xaml;
        m_shellSurface=true;
        m_panel.Padding({0,0,0,0});
        m_panel.Children().GetAt(0).Visibility(x::Visibility::Collapsed);
        m_panel.RowDefinitions().GetAt(0).Height({0,x::GridUnitType::Pixel});
        m_presenter.Background(x::Media::SolidColorBrush(winrt::Windows::UI::Color{0,0,0,0}));
        m_presenter.BorderThickness({0,0,0,0});
        m_panelScroll.VerticalScrollBarVisibility(x::Controls::ScrollBarVisibility::Disabled);
        if(m_materialGrid)m_materialGrid.Visibility(x::Visibility::Collapsed);
        BOOL enable=TRUE;
        winrt::check_hresult(DwmSetWindowAttribute(m_owner,DWMWA_USE_HOSTBACKDROPBRUSH,&enable,sizeof(enable)));
    }
    auto GenericSurface()
    {
        namespace x=winrt::Windows::UI::Xaml;
        m_shellSurface=false;
        m_panel.Padding({24,16,24,20});
        m_panel.Children().GetAt(0).Visibility(x::Visibility::Visible);
        m_panel.RowDefinitions().GetAt(0).Height({1,x::GridUnitType::Auto});
        m_presenter.Background(x::Media::SolidColorBrush({0,0,0,0}));
        m_presenter.BorderThickness({1,1,1,1});
        m_panelScroll.VerticalScrollBarVisibility(x::Controls::ScrollBarVisibility::Auto);m_topInset=12;
        if(!m_materialGrid){m_materialGrid=x::Controls::Grid();m_surfaceRoot.Children().InsertAt(1,m_materialGrid);}
        m_materialGrid.Visibility(x::Visibility::Visible);
        BOOL enable=TRUE;winrt::check_hresult(DwmSetWindowAttribute(m_owner,DWMWA_USE_HOSTBACKDROPBRUSH,&enable,sizeof(enable)));
        return m_materialGrid;
    }
    void ShadowRenderer(std::function<void(winrt::Windows::UI::Xaml::Controls::Grid)> render){m_customShadow=std::move(render);}
    void TopInset(float dip){m_topInset=dip;}
    void Motion(bool horizontal,unsigned enterMs=250,unsigned exitMs=167){m_horizontal=horizontal;m_enterMs=enterMs;m_exitMs=exitMs;}
    void MenuBackground(winrt::Windows::UI::Xaml::Media::Brush const& brush){m_menuBackground=brush;}

    void Show(POINT anchor)
    {
        namespace x=winrt::Windows::UI::Xaml;
        if(m_menu)m_menu.Hide();
        if(m_phase!=Phase::Closed)return;
        SetPhase(Phase::Opening);
        m_surfaceRoot.Visibility(x::Visibility::Visible);
        m_surfaceRoot.Opacity(0);
        m_anchor=anchor;
        const auto placement=LayoutPanel(anchor);
        const int left=static_cast<int>(std::floor(placement.host.x));
        const int top=static_cast<int>(std::floor(placement.host.y));
        const int w=static_cast<int>(std::ceil(placement.host.x+placement.host.width))-left;
        const int h=static_cast<int>(std::ceil(placement.host.y+placement.host.height))-top;
        m_pending=1;
        SetWindowPos(m_owner,HWND_TOPMOST,left,top,w,h,SWP_SHOWWINDOW);
        SetWindowPos(m_child,nullptr,0,0,w,h,SWP_NOZORDER|SWP_SHOWWINDOW);
        SetForegroundWindow(m_owner);SetFocus(m_child);
        m_root.UpdateLayout();
        if(m_root.IsLoaded())OpenPending();
    }

    void PreferredWidth(float width)
    {
        m_requestedWidth=std::max(1.0f,width);
        if(m_visible)Reposition(m_anchor);
    }
    void Reposition(POINT anchor)
    {
        m_anchor=anchor;
        if(!m_visible)return;
        if(Transitioning()){m_relayoutPending=true;return;}
        const auto placement=LayoutPanel(anchor);
        const int left=static_cast<int>(std::floor(placement.host.x));
        const int top=static_cast<int>(std::floor(placement.host.y));
        const int w=static_cast<int>(std::ceil(placement.host.x+placement.host.width))-left;
        const int h=static_cast<int>(std::ceil(placement.host.y+placement.host.height))-top;
        SetWindowPos(m_owner,nullptr,left,top,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowPos(m_child,nullptr,0,0,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
        m_root.UpdateLayout();UpdateShadow();
    }

    void OwnerDeactivated(HWND next,bool ownTrayPress=false)
    {
        if(ownTrayPress)return;
        DWORD pid=0;
        if(next)GetWindowThreadProcessId(next,&pid);
        // Material policy is owned by ShellPanels, independently of focus.
        if(m_phase!=Phase::Closed && pid!=GetCurrentProcessId())Hide();
    }

    void Menu(POINT anchor, LONG mode, UINT width)
    {
        ++m_menuGeneration;
        if(m_menu){m_menu.Closed(m_menuClosed);m_menu.Hide();}
        namespace c = winrt::Windows::UI::Xaml::Controls;
        MONITORINFO monitor{sizeof(monitor)};
        const auto handle=MonitorFromPoint(anchor,MONITOR_DEFAULTTONEAREST);
        if(GetMonitorInfoW(handle,&monitor)){
            UINT dpiX=96,dpiY=96;
            if(FAILED(GetDpiForMonitor(handle,MDT_EFFECTIVE_DPI,&dpiX,&dpiY)))dpiX=GetDpiForWindow(m_owner);
            // The menu's native placement adds its own 4 DIP separation.
            anchor.y=std::min(anchor.y,monitor.rcWork.bottom-MulDiv(8,static_cast<int>(dpiX),96));
        }
        FinishPanelClose();
        m_root.Width(1);m_root.Height(1);
        winrt::Windows::UI::Xaml::Media::RectangleGeometry clip;
        clip.Rect({0,0,1,1});m_root.Clip(clip);
        m_menu = c::MenuFlyout();
        m_menu.MenuFlyoutPresenterStyle(MenuStyle());
        PopupBackdrop(m_menu);
        m_menu.AreOpenCloseAnimationsEnabled(winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled());
        m_menu.ShouldConstrainToRootBounds(false);
        auto add = [this,life=m_lifetime](auto const& collection, wchar_t const* label, UINT command) {
            c::MenuFlyoutItem item;
            item.Text(label);
            TrayMenuStyle::Item(item);
            item.Click([this,command,life=m_lifetime](auto const&,auto const&) noexcept {if(!*life)return;EventGuard([&]{m_command(command);});});
            collection.Append(item);
        };
        add(m_menu.Items(),L"Открыть панель",1);
        add(m_menu.Items(),L"Элементы управления",9);
        for(auto const& row : {std::pair{L"Подпись",3u},std::pair{L"Две строки (пример)",4u},std::pair{L"Обычный значок",5u}})
        {
            c::ToggleMenuFlyoutItem item;
            TrayMenuStyle::Item(item);
            item.Text(row.first);
            item.IsChecked(mode == (row.second == 3 ? 1 : row.second == 4 ? 2 : 0));
            item.Click([this,id=row.second,life=m_lifetime](auto const&,auto const&) noexcept {if(!*life)return;EventGuard([&]{m_command(id);});});
            m_menu.Items().Append(item);
        }
        c::MenuFlyoutSubItem widths;
        TrayMenuStyle::Item(widths);
        widths.Text(width ? L"Ширина: " + std::to_wstring(width) + L" DIP" : L"Ширина: по содержимому");
        add(widths.Items(),L"По содержимому",6);
        add(widths.Items(),L"Уже на 8 DIP",7);
        add(widths.Items(),L"Шире на 8 DIP",8);
        m_menu.Items().Append(widths);
        m_menu.Items().Append(TrayMenuStyle::Separator(m_light));
        add(m_menu.Items(),L"Выход",2);
        m_menuOpen = true;
        const auto menuGeneration=m_menuGeneration;
        m_menuClosed=m_menu.Closed([this,menuGeneration,life=m_lifetime](auto const&,auto const&) noexcept {if(!*life)return;EventGuard([&]{if(menuGeneration==m_menuGeneration){m_menuOpen=false;HideAnchor();}});});
        m_pending=2;
        ShowAnchor(anchor);
        if(m_root.IsLoaded())OpenPending();
    }

    void Hide()
    {
        if(m_phase==Phase::Opening){if(m_surface)Animate(true);else m_dismissAfterOpen=true;return;}
        m_pending=0;
        if(m_phase==Phase::Open)Animate(true);
        if(m_menu)m_menu.Hide();
    }
    void ShowMenu(winrt::Windows::UI::Xaml::Controls::MenuFlyout const& menu,POINT anchor)
    {
        ++m_menuGeneration;
        if(m_menu){m_menu.Closed(m_menuClosed);m_menu.Hide();}
        FinishPanelClose();
        m_root.Width(1);m_root.Height(1);
        winrt::Windows::UI::Xaml::Media::RectangleGeometry clip;clip.Rect({0,0,1,1});m_root.Clip(clip);
        m_menu=menu;m_menuOpen=true;
        m_menu.MenuFlyoutPresenterStyle(MenuStyle());
        PopupBackdrop(m_menu);
        m_menu.AreOpenCloseAnimationsEnabled(winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled());
        m_menu.ShouldConstrainToRootBounds(false);
        const auto generation=m_menuGeneration;
        m_menuClosed=m_menu.Closed([this,generation,life=m_lifetime](auto const&,auto const&) noexcept {
            if(!*life)return;EventGuard([&]{if(generation==m_menuGeneration){m_menuOpen=false;HideAnchor();}});
        });
        m_pending=2;ShowAnchor(anchor);if(m_root.IsLoaded())OpenPending();
    }
    void Toggle(POINT anchor)
    {
        if(m_phase==Phase::Opening){Hide();return;}
        if(m_phase==Phase::Closing)return;
        if(m_visible)Hide();else Show(anchor);
    }
    bool Transitioning() const {return m_phase==Phase::Opening || m_phase==Phase::Closing;}
    Phase CurrentPhase() const {return m_phase;}
    bool Visible() const { return m_visible; }
    bool MenuOpen() const { return m_menuOpen; }
#ifdef NATIVE_FLYOUT_SELFTEST
    void AnimationsForTest(bool enabled){m_testAnimations=enabled?1:0;}
    auto MaterialForTest() const {return m_presenter.Background();}
    void MaterialForTest(winrt::Windows::UI::Xaml::Media::Brush const& brush){m_presenter.Background(brush);}
    RECT SurfaceForTest() const
    {
        const auto point=m_presenter.TransformToVisual(m_root).TransformPoint({0,0});
        RECT host{};GetWindowRect(m_owner,&host);
        return {host.left+static_cast<LONG>(std::lround(point.X*m_scale)),host.top+static_cast<LONG>(std::lround(point.Y*m_scale)),
            host.left+static_cast<LONG>(std::lround((point.X+m_panelWidth)*m_scale)),host.top+static_cast<LONG>(std::lround((point.Y+m_panelHeight)*m_scale))};
    }
#endif
    bool Process(MSG const& message)
    {
        if(!m_source) return false;
        BOOL handled=FALSE;
        winrt::check_hresult(m_source.as<IDesktopWindowXamlSourceNative2>()->PreTranslateMessage(&message,&handled));
        // Child controls and their popups get Escape first. Unhandled XAML Escape
        // bubbles to m_root.KeyDown; only a key addressed to the Win32 host falls back here.
        if(!handled && message.hwnd==m_owner && message.message==WM_KEYDOWN &&
            message.wParam==VK_ESCAPE && (m_visible || m_menuOpen)){Hide();return true;}
        return handled != FALSE;
    }
    void Close() noexcept
    {
        m_destroying=true;if(m_lifetime)*m_lifetime=false;
        try{CancelCompletion();}catch(...){Trace(L"Cancel composition callback failed");}
        try{if(m_surface)m_surface.StopAnimation(L"Translation");}catch(...){}
        try{FinishPanelClose();}catch(...){}
        try{if(m_menu)m_menu.Hide();}catch(...){}
        try{if(m_source)m_source.Content(nullptr);}catch(...){}
        try{if(m_source)m_source.Close();}catch(...){}
        m_source=nullptr;m_menu=nullptr;m_panel=nullptr;m_root=nullptr;m_surfaceRoot=nullptr;m_presenter=nullptr;m_shadowHost=nullptr;m_panelScroll=nullptr;m_materialGrid=nullptr;
        m_surface=nullptr;m_shadow=nullptr;
        try{if(m_manager)m_manager.Close();}catch(...){}
        m_manager=nullptr;
    }
private:
    auto MenuStyle(){
        namespace x=winrt::Windows::UI::Xaml;
        auto style=TrayMenuStyle::Presenter(m_light);
        if(m_menuBackground){x::Setter setter;setter.Property(x::Controls::Control::BackgroundProperty());setter.Value(m_menuBackground);style.Setters().Append(setter);}
        return style;
    }
    static void PopupBackdrop(winrt::Windows::UI::Xaml::Controls::MenuFlyout const& menu){
        menu.Opened([](auto const&,auto const&){EnumWindows([](HWND w,LPARAM)->BOOL{DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid==GetCurrentProcessId()){wchar_t name[64]{};GetClassNameW(w,name,64);if(!wcscmp(name,L"Xaml_WindowedPopupClass")){BOOL enabled=TRUE;DwmSetWindowAttribute(w,DWMWA_USE_HOSTBACKDROPBRUSH,&enabled,sizeof(enabled));}}return TRUE;},0);});
    }
    template<class Function>void EventGuard(Function&& action) noexcept {
        if(m_destroying)return;
        try{action();}catch(...){Trace(L"Flyout callback failed");try{FinishPanelClose();}catch(...){}}
    }
    TrayFlyoutLayout::Layout LayoutPanel(POINT anchor)
    {
        namespace x=winrt::Windows::UI::Xaml;
        const auto monitor=MonitorFromPoint(anchor,MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};winrt::check_bool(GetMonitorInfoW(monitor,&info));
        UINT dpiX=96,dpiY=96;
        if(FAILED(GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dpiX,&dpiY)))dpiX=GetDpiForWindow(m_owner);
        m_scale=static_cast<double>(dpiX)/96;
        const TrayFlyoutLayout::Box work{static_cast<double>(info.rcWork.left),static_cast<double>(info.rcWork.top),
            static_cast<double>(info.rcWork.right-info.rcWork.left),static_cast<double>(info.rcWork.bottom-info.rcWork.top)};
        auto placement=TrayFlyoutLayout::Fit(work,anchor.x,anchor.y,m_requestedWidth*m_scale,1,m_scale,m_topInset);
        m_panelWidth=static_cast<float>(placement.surface.width/m_scale);
        m_panel.Width(std::max(1.0f,m_panelWidth-(m_shellSurface?0:2)));
        m_presenter.Width(m_panelWidth);
        m_presenter.Height(NAN);m_panelScroll.MaxHeight(INFINITY);
        m_presenter.Measure({m_panelWidth,100000});
        placement=TrayFlyoutLayout::Fit(work,anchor.x,anchor.y,m_panelWidth*m_scale,m_presenter.DesiredSize().Height*m_scale,m_scale,m_topInset);
        m_panelHeight=static_cast<float>(placement.surface.height/m_scale);
        m_presenter.Height(m_panelHeight);m_panelScroll.MaxHeight(std::max(1.0f,m_panelHeight-(m_shellSurface?0:2)));
        m_surfaceRoot.Width(m_panelWidth);m_surfaceRoot.Height(m_panelHeight);
        const double left=std::floor(placement.host.x),top=std::floor(placement.host.y);
        m_insetX=static_cast<float>((std::round(placement.surface.x)-left)/m_scale);
        m_insetY=static_cast<float>((std::round(placement.surface.y)-top)/m_scale);
        m_surfaceRoot.Margin({m_insetX,m_insetY,0,0});
        const auto width=static_cast<float>((std::ceil(placement.host.x+placement.host.width)-left)/m_scale);
        const auto height=static_cast<float>((std::ceil(placement.host.y+placement.host.height)-top)/m_scale);
        m_root.Width(width);m_root.Height(height);
        x::Media::RectangleGeometry clip;clip.Rect({0,0,width,height});m_root.Clip(clip);
        m_travel=m_horizontal?width:height;
        return placement;
    }
    void UpdateShadow()
    {
        if(m_shellSurface){if(m_customShadow)m_customShadow(m_shadowHost);return;}
        namespace x=winrt::Windows::UI::Xaml;
        auto compositor=x::Hosting::ElementCompositionPreview::GetElementVisual(m_surfaceRoot).Compositor();
        auto sprite=compositor.CreateSpriteVisual();sprite.Size({m_panelWidth,m_panelHeight});
        m_shadow=compositor.CreateDropShadow();m_shadow.BlurRadius(24);m_shadow.Offset({0,4,0});
        m_shadow.Opacity(m_light ? 0.18f : 0.32f);m_shadow.Color({255,0,0,0});
        auto geometry=compositor.CreateRoundedRectangleGeometry();
        geometry.Size({m_panelWidth,m_panelHeight});geometry.CornerRadius({8,8});
        auto shape=compositor.CreateSpriteShape(geometry);shape.FillBrush(compositor.CreateColorBrush({255,255,255,255}));
        auto maskVisual=compositor.CreateShapeVisual();maskVisual.Size({m_panelWidth,m_panelHeight});maskVisual.Shapes().Append(shape);
        auto maskSurface=compositor.CreateVisualSurface();maskSurface.SourceVisual(maskVisual);maskSurface.SourceSize({m_panelWidth,m_panelHeight});
        m_shadow.Mask(compositor.CreateSurfaceBrush(maskSurface));sprite.Shadow(m_shadow);
        x::Hosting::ElementCompositionPreview::SetElementChildVisual(m_shadowHost,sprite);
    }
    static void Trace(wchar_t const* message) noexcept
    {
        try{
        wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
        const auto path=std::wstring(temp)+L"YataNativeFrameworkFlyout.log";
        HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE){auto line=std::wstring(message)+L"\r\n";DWORD written=0;WriteFile(file,line.data(),static_cast<DWORD>(line.size()*sizeof(wchar_t)),&written,nullptr);CloseHandle(file);}
        }catch(...){}
    }
    bool AnimationsEnabled() const {
#ifdef NATIVE_FLYOUT_SELFTEST
        if(m_testAnimations>=0)return m_testAnimations!=0;
#endif
        return winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
    }
    void CancelCompletion()
    {
        ++m_generation;
        if(m_batch){m_batch.Completed(m_completed);m_batch=nullptr;}
    }
    void Animate(bool closing)
    {
        namespace x=winrt::Windows::UI::Xaml;
        namespace comp=winrt::Windows::UI::Composition;
        CancelCompletion();
        SetPhase(closing ? Phase::Closing : Phase::Opening);

        if(!m_surface) {
            x::Hosting::ElementCompositionPreview::SetIsTranslationEnabled(m_surfaceRoot,true);
            m_surface=x::Hosting::ElementCompositionPreview::GetElementVisual(m_surfaceRoot);
            m_surface.Properties().InsertVector3(L"Translation",MotionOffset(m_travel));
            UpdateShadow();
        }
        m_surfaceRoot.Opacity(1);
        if(!AnimationsEnabled()) {
            m_surface.Properties().InsertVector3(L"Translation",{0,0,0});
            if(closing)FinishPanelClose();
            else FinishPanelOpen();
            return;
        }
        auto compositor=m_surface.Compositor();
        m_batch=compositor.CreateScopedBatch(comp::CompositionBatchTypes::Animation);
        auto movement=compositor.CreateVector3KeyFrameAnimation();
        movement.InsertExpressionKeyFrame(0,L"this.StartingValue");
        // Quick Settings: decelerating entrance and accelerating gentle exit.
        // Compared against the installed panel's captured trajectory.
        auto easing=compositor.CreateCubicBezierEasingFunction(closing ? winrt::Windows::Foundation::Numerics::float2{1.0f,0.0f} : winrt::Windows::Foundation::Numerics::float2{0.0f,0.0f},
            closing ? winrt::Windows::Foundation::Numerics::float2{1.0f,1.0f} : winrt::Windows::Foundation::Numerics::float2{0.0f,1.0f});
        movement.InsertKeyFrame(1,MotionOffset(closing ? m_travel : 0),easing);
        movement.Duration(std::chrono::milliseconds(closing ? m_exitMs : m_enterMs));
        m_surface.StartAnimation(L"Translation",movement);
        const auto generation=m_generation;
        m_completed=m_batch.Completed([this,generation,closing,life=m_lifetime](auto const&,auto const&) noexcept {if(!*life)return;EventGuard([&]{
            if(generation!=m_generation || m_destroying)return;
            if(closing)FinishPanelClose();
            else FinishPanelOpen();
        });});
        m_batch.End();
    }
    winrt::Windows::Foundation::Numerics::float3 MotionOffset(float distance)const{return m_horizontal?winrt::Windows::Foundation::Numerics::float3{distance,0,0}:winrt::Windows::Foundation::Numerics::float3{0,distance,0};}
    void OpenPending()
    {
        namespace c=winrt::Windows::UI::Xaml::Controls;
        if(!m_root.IsLoaded() || !m_pending)return;
        const int pending=std::exchange(m_pending,0);
        c::Primitives::FlyoutShowOptions options;
        options.Placement(c::Primitives::FlyoutPlacementMode::Top);
        if(pending==1){
            SetVisible(true);Animate(false);
            m_source.NavigateFocus(winrt::Windows::UI::Xaml::Hosting::XamlSourceFocusNavigationRequest(winrt::Windows::UI::Xaml::Hosting::XamlSourceFocusNavigationReason::First));
            if(m_shellSurface){
                if(auto focused=winrt::Windows::UI::Xaml::Input::FocusManager::GetFocusedElement(m_root.XamlRoot()).try_as<c::Control>())
                    focused.Focus(winrt::Windows::UI::Xaml::FocusState::Pointer);
            }
            if(!m_shellSurface)m_panel.FindName(L"CloseButton").as<c::Button>().Focus(winrt::Windows::UI::Xaml::FocusState::Pointer);
        }else{
            m_menu.XamlRoot(m_root.XamlRoot());
            m_menu.ShowAt(m_root,options);
        }
    }
    void FinishPanelOpen()
    {
        SetPhase(Phase::Open);
        if(std::exchange(m_dismissAfterOpen,false))Hide();
        else if(std::exchange(m_relayoutPending,false))Reposition(m_anchor);
    }
    void FinishPanelClose()
    {
        CancelCompletion();
        m_pending=0;
        m_dismissAfterOpen=false;
        m_relayoutPending=false;
        if(m_surface)m_surface.StopAnimation(L"Translation");
        if(m_surfaceRoot){m_surfaceRoot.Opacity(0);m_surfaceRoot.Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);}
        m_surface=nullptr;
        if(m_visible)SetVisible(false);
        SetPhase(Phase::Closed);
        HideAnchor();
    }
    void SetPhase(Phase phase)
    {
        if(m_phase==phase)return;
        m_phase=phase;
        if(!m_destroying && m_phaseChanged)m_phaseChanged(phase);
    }
    void SetVisible(bool value) {m_visible=value;if(!m_destroying && m_visibility)m_visibility(value);}
    void ShowAnchor(POINT point)
    {
        SetWindowPos(m_owner,HWND_TOPMOST,point.x,point.y,1,1,SWP_SHOWWINDOW);
        SetWindowPos(m_child,nullptr,0,0,1,1,SWP_NOZORDER|SWP_SHOWWINDOW);
        SetForegroundWindow(m_owner);
        SetFocus(m_child);
        m_root.UpdateLayout();
    }
    void HideAnchor() {if(!m_destroying && m_phase==Phase::Closed && !m_menuOpen)ShowWindow(m_owner,SW_HIDE);}
    std::shared_ptr<bool> m_lifetime=std::make_shared<bool>(false);
    HWND m_owner=nullptr,m_child=nullptr;
#ifdef NATIVE_FLYOUT_SELFTEST
    int m_testAnimations=-1;
#endif
    bool m_visible=false,m_menuOpen=false,m_destroying=false;
    int m_pending=0;
    bool m_dismissAfterOpen=false,m_light=true;
    bool m_relayoutPending=false;
    bool m_shellSurface=false;
    POINT m_anchor{};
    double m_scale=1;
    float m_requestedWidth=362,m_panelWidth=362,m_insetX=24,m_insetY=24;
    Phase m_phase=Phase::Closed;
    float m_travel=0;
    bool m_horizontal=false;
    unsigned m_enterMs=250,m_exitMs=167;
    float m_topInset=12;
    unsigned m_generation=0;
    winrt::event_token m_completed{};
    winrt::Windows::UI::Composition::Visual m_surface{nullptr};
    winrt::Windows::UI::Composition::CompositionScopedBatch m_batch{nullptr};
    std::function<void(bool)> m_visibility;
    std::function<void(UINT)> m_command;
    std::function<void(Phase)> m_phaseChanged;
    std::function<void(winrt::Windows::UI::Xaml::Controls::Grid)> m_customShadow;
    winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager m_manager{nullptr};
    winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource m_source{nullptr};
    winrt::Windows::UI::Xaml::Controls::Grid m_root{nullptr},m_panel{nullptr};
    winrt::Windows::UI::Xaml::Controls::Grid m_surfaceRoot{nullptr},m_shadowHost{nullptr};
    winrt::Windows::UI::Xaml::Controls::Grid m_materialGrid{nullptr};
    winrt::Windows::UI::Xaml::Controls::Border m_presenter{nullptr};
    uint64_t m_menuGeneration=0;winrt::event_token m_menuClosed{};
    winrt::Windows::UI::Xaml::Controls::ScrollViewer m_panelScroll{nullptr};
    winrt::Windows::UI::Composition::DropShadow m_shadow{nullptr};
    float m_panelHeight=0;
    winrt::Windows::UI::Xaml::Controls::MenuFlyout m_menu{nullptr};
    winrt::Windows::UI::Xaml::Media::Brush m_menuBackground{nullptr};
};
