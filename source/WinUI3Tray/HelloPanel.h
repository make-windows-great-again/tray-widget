#pragma once
#include <winrt/Microsoft.UI.Interop.h>
#include <shellscalingapi.h>
#include "../NativeTray/FlyoutPlacement.h"

namespace ui=winrt::Microsoft::UI::Xaml;
namespace controls=ui::Controls;
namespace composition=winrt::Microsoft::UI::Composition;

// A tray dismissal keeps the material alive until the exit actually completes.
// System policy (transparency, contrast, energy saver) remains controller-owned.
struct PanelAcrylic : ui::Media::SystemBackdropT<PanelAcrylic> {
    composition::SystemBackdrops::DesktopAcrylicController controller{nullptr};
    composition::SystemBackdrops::SystemBackdropConfiguration configuration{nullptr};
    void OnTargetConnected(composition::ICompositionSupportsSystemBackdrop const& target,ui::XamlRoot const& root){
        controller=composition::SystemBackdrops::DesktopAcrylicController();
        configuration=composition::SystemBackdrops::SystemBackdropConfiguration();
        configuration.IsInputActive(true);
        Theme(root.Content().as<ui::FrameworkElement>().ActualTheme());
        controller.SetSystemBackdropConfiguration(configuration);
        controller.AddSystemBackdropTarget(target);
    }
    void OnTargetDisconnected(composition::ICompositionSupportsSystemBackdrop const& target){
        if(controller){controller.RemoveSystemBackdropTarget(target);controller.Close();controller=nullptr;}
        configuration=nullptr;
    }
    void Theme(ui::ElementTheme theme){
        if(configuration)configuration.Theme(theme==ui::ElementTheme::Dark?composition::SystemBackdrops::SystemBackdropTheme::Dark:composition::SystemBackdrops::SystemBackdropTheme::Light);
    }
};

class HelloPanel {
    HWND window=nullptr;
    ui::Hosting::DesktopWindowXamlSource island{nullptr};
    controls::Grid root{nullptr},card{nullptr};
    controls::Slider slider{nullptr};
    controls::TextBlock widthText{nullptr};
    controls::MenuFlyout menu{nullptr};
    composition::Visual visual{nullptr};
    winrt::com_ptr<PanelAcrylic> acrylic;
    std::shared_ptr<bool> alive=std::make_shared<bool>(true);
    std::function<void(int)> phaseChanged;
    std::function<void(UINT)> widthChanged;
    std::function<void()> quit;
    uint64_t transition=0;
    int phase=0,theme=0;
    bool updating=false,contextOnly=false;
    UINT requestedWidth=88;
    RECT anchor{};
public:
    ~HelloPanel(){Close();}
    void Initialize(HWND hwnd,std::function<void(int)> onPhase,std::function<void(UINT)> onWidth,std::function<void()> onQuit){
        window=hwnd;phaseChanged=std::move(onPhase);widthChanged=std::move(onWidth);quit=std::move(onQuit);
        island=ui::Hosting::DesktopWindowXamlSource();island.Initialize(winrt::Microsoft::UI::GetWindowIdFromWindow(hwnd));
        root=ui::Markup::XamlReader::Load(LR"(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
 <Grid x:Name="HelloCard" Width="336" Height="292" HorizontalAlignment="Left" VerticalAlignment="Top">
  <SystemBackdropElement x:Name="Material" CornerRadius="8"/>
  <Border CornerRadius="8" BorderThickness="1" BorderBrush="{ThemeResource SurfaceStrokeColorDefaultBrush}" Padding="24">
   <StackPanel Spacing="12">
    <TextBlock Text="Hello World" Style="{ThemeResource SubtitleTextBlockStyle}" AutomationProperties.AutomationId="HelloTitle"/>
    <TextBlock Text="VPN · WinUI 3" Foreground="{ThemeResource TextFillColorSecondaryBrush}"/>
    <Grid Margin="0,12,0,0"><TextBlock Text="Ширина виджета"/><TextBlock x:Name="WidthText" Text="88 DIP" HorizontalAlignment="Right"/></Grid>
    <Slider x:Name="WidthSlider" Minimum="32" Maximum="320" Value="88" StepFrequency="1" IsThumbToolTipEnabled="True" AutomationProperties.Name="Ширина виджета" AutomationProperties.AutomationId="TrayWidth"/>
    <StackPanel Orientation="Horizontal" Spacing="8"><Button x:Name="AutoWidth" Content="Авто"/><Button x:Name="Width88" Content="88"/><Button x:Name="Width144" Content="144"/></StackPanel>
    <Grid><Button x:Name="ClosePanel" Content="Закрыть"/><Button x:Name="More" Content="&#xE712;" FontFamily="Segoe Fluent Icons" HorizontalAlignment="Right" AutomationProperties.Name="Меню"/></Grid>
   </StackPanel>
  </Border>
 </Grid>
</Grid>)").as<controls::Grid>();
        card=root.FindName(L"HelloCard").as<controls::Grid>();
        slider=root.FindName(L"WidthSlider").as<controls::Slider>();
        widthText=root.FindName(L"WidthText").as<controls::TextBlock>();
        acrylic=winrt::make_self<PanelAcrylic>();
        root.FindName(L"Material").as<controls::SystemBackdropElement>().SystemBackdrop(acrylic.as<ui::Media::SystemBackdrop>());
        island.Content(root);Theme(0);
        root.ActualThemeChanged([this](auto const&,auto const&){if(acrylic)acrylic->Theme(root.ActualTheme());});
        slider.ValueChanged([this](auto const&,auto const& args){if(!updating)SetWidth(static_cast<UINT>(std::lround(args.NewValue())),true);});
        root.FindName(L"AutoWidth").as<controls::Button>().Click([this](auto const&,auto const&){SetWidth(0,true);});
        root.FindName(L"Width88").as<controls::Button>().Click([this](auto const&,auto const&){SetWidth(88,true);});
        root.FindName(L"Width144").as<controls::Button>().Click([this](auto const&,auto const&){SetWidth(144,true);});
        root.FindName(L"ClosePanel").as<controls::Button>().Click([this](auto const&,auto const&){Hide();});
        root.KeyDown([this](auto const&,ui::Input::KeyRoutedEventArgs const& args){if(args.Key()==winrt::Windows::System::VirtualKey::Escape){args.Handled(true);Hide();}});
        menu=controls::MenuFlyout();
        AddMenu(L"Автоматическая ширина",[this]{SetWidth(0,true);});
        for(UINT width:{64u,88u,144u,240u})AddMenu(std::to_wstring(width)+L" DIP",[this,width]{SetWidth(width,true);});
        menu.Items().Append(controls::MenuFlyoutSeparator());
        AddMenu(L"Системная тема",[this]{Theme(0);});AddMenu(L"Светлая тема",[this]{Theme(1);});AddMenu(L"Тёмная тема",[this]{Theme(2);});
        menu.Items().Append(controls::MenuFlyoutSeparator());AddMenu(L"Выход",[this]{quit();});
        menu.Closed([this](auto const&,auto const&){if(contextOnly)Hide();});
        root.FindName(L"More").as<controls::Button>().Flyout(menu);
        visual=ui::Hosting::ElementCompositionPreview::GetElementVisual(card);
        root.TabFocusNavigation(ui::Input::KeyboardNavigationMode::Cycle);
    }
    void AddMenu(std::wstring const& text,std::function<void()> action){
        controls::MenuFlyoutItem item;item.Text(text);item.Click([action=std::move(action)](auto const&,auto const&){action();});menu.Items().Append(item);
    }
    void SetWidth(UINT value,bool notify=false){
        if(value!=0&&(value<32||value>320))winrt::throw_hresult(E_INVALIDARG);
        requestedWidth=value;updating=true;slider.Value(value?value:44);updating=false;
        widthText.Text(value?std::to_wstring(value)+L" DIP":L"Авто");
        if(notify&&widthChanged)widthChanged(value);
    }
    void Theme(int value){
        theme=value;DWORD light=1,size=sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);
        root.RequestedTheme(value==2||(value==0&&!light)?ui::ElementTheme::Dark:ui::ElementTheme::Light);
        if(acrylic)acrylic->Theme(root.ActualTheme());
    }
    void RefreshTheme(){Theme(theme);}
    void Place(RECT bounds){
        anchor=bounds;POINT point{(bounds.left+bounds.right)/2,bounds.top};
        auto monitor=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};winrt::check_bool(GetMonitorInfoW(monitor,&info));
        UINT dx=96,dy=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dx,&dy);const double scale=dx/96.0;
        const auto fit=TrayFlyoutLayout::Fit({double(info.rcWork.left),double(info.rcWork.top),double(info.rcWork.right-info.rcWork.left),double(info.rcWork.bottom-info.rcWork.top)},point.x,point.y,336*scale,292*scale,scale);
        SetWindowPos(window,HWND_TOPMOST,static_cast<int>(std::lround(fit.host.x)),static_cast<int>(std::lround(fit.host.y)),static_cast<int>(std::lround(fit.host.width)),static_cast<int>(std::lround(fit.host.height)),SWP_NOACTIVATE);
        card.Width(fit.surface.width/scale);card.Height(fit.surface.height/scale);card.Margin({fit.insetX/scale,fit.insetY/scale,0,0});
        ResizeIsland();
    }
    void ResizeIsland(){if(island){RECT r{};GetClientRect(window,&r);island.SiteBridge().MoveAndResize({0,0,r.right,r.bottom});}}
    void Phase(int value){phase=value;if(phaseChanged)phaseChanged(value);}
    void Animate(bool opening){
        const auto generation=++transition;
        if(!winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled()){visual.Offset({0,0,0});Finish(opening,generation);return;}
        auto compositor=visual.Compositor();auto batch=compositor.CreateScopedBatch(composition::CompositionBatchTypes::Animation);
        auto motion=compositor.CreateVector3KeyFrameAnimation();
        motion.Duration(std::chrono::milliseconds(opening?250:167));
        motion.InsertKeyFrame(1,{0,opening?0.0f:static_cast<float>(card.ActualHeight()+24),0},compositor.CreateCubicBezierEasingFunction({0.1f,0.9f},{0.2f,1}));
        visual.StartAnimation(L"Offset",motion);
        batch.Completed([this,life=std::weak_ptr<bool>(alive),opening,generation](auto const&,auto const&){if(auto guard=life.lock();guard&&*guard)Finish(opening,generation);});batch.End();
    }
    void Finish(bool opening,uint64_t generation){
        if(generation!=transition)return;
        if(opening){Phase(2);if(GetForegroundWindow()==window)slider.Focus(ui::FocusState::Programmatic);}
        else{ShowWindow(window,SW_HIDE);Phase(0);contextOnly=false;}
    }
    void Show(RECT bounds){
        if(phase==3)return;
        if(phase){SetForegroundWindow(window);return;}
        contextOnly=false;card.Visibility(ui::Visibility::Visible);Place(bounds);root.UpdateLayout();
        visual.Offset({0,static_cast<float>(card.ActualHeight()+24),0});Phase(1);
        ShowWindow(window,SW_SHOWNORMAL);SetForegroundWindow(window);
        Animate(true);
    }
    void Toggle(RECT bounds){if(phase)Hide();else Show(bounds);}
    void Hide(){if(!phase||phase==3)return;Phase(3);menu.Hide();if(contextOnly)Finish(false,++transition);else Animate(false);}
    void Context(RECT bounds){
        if(phase==3)return;
        if(!phase){contextOnly=true;card.Visibility(ui::Visibility::Collapsed);Place(bounds);Phase(2);ShowWindow(window,SW_SHOWNORMAL);SetForegroundWindow(window);}
        controls::Primitives::FlyoutShowOptions options;options.Position(winrt::Windows::Foundation::Point{static_cast<float>(card.Margin().Left),static_cast<float>(card.Margin().Top+card.Height())});
        options.Placement(controls::Primitives::FlyoutPlacementMode::TopEdgeAlignedRight);menu.ShowAt(root,options);
    }
    void Deactivated(HWND next,bool ownTrayPress){
        if(ownTrayPress||phase==3)return;
        DWORD pid=0;GetWindowThreadProcessId(next,&pid);if(pid==GetCurrentProcessId())return;
        Hide();
    }
    int Phase() const noexcept{return phase;}
    bool MenuOpen() const{return menu&&menu.IsOpen();}
    void Close() noexcept {
        if(!alive||!*alive)return;*alive=false;++transition;
        try{if(menu)menu.Hide();if(island){island.Content(nullptr);island.Close();island=nullptr;}}catch(...){}
        root=nullptr;card=nullptr;slider=nullptr;widthText=nullptr;menu=nullptr;visual=nullptr;acrylic=nullptr;
    }
};
