#include "ShellPanels.h"
#include "../NativeTray/NativeFlyout.h"
#include "Calendar.h"
#include "CardStack.h"
#include "ControlStyles.h"
#include "Material.h"
#ifdef SHELLPANELS_LAYOUT_TRACE
#include "LayoutTrace.h"
#endif
#include <atomic>
#include <commctrl.h>
#include <ctime>
#include <winrt/Windows.Globalization.DateTimeFormatting.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Documents.h>
#pragma comment(lib, "comctl32.lib")

namespace ShellPanels
{
namespace x = winrt::Windows::UI::Xaml;
namespace c = x::Controls;
namespace a = x::Automation;
namespace f = winrt::Windows::Foundation;
namespace
{
auto Brush(BYTE alpha, BYTE r, BYTE g, BYTE b)
{
    return x::Media::SolidColorBrush({alpha, r, g, b});
}
auto Transparent()
{
    return Brush(0, 0, 0, 0);
}
c::TextBlock Text(winrt::param::hstring const &value, double size = 14)
{
    c::TextBlock t;
    t.Text(value);
    t.FontSize(size);
    t.FontFamily(x::Media::FontFamily(L"Segoe UI Variable"));
    t.VerticalAlignment(x::VerticalAlignment::Center);
    return t;
}
c::FontIcon Glyph(wchar_t value, double size = 20)
{
    c::FontIcon i;
    i.Glyph(winrt::hstring(std::wstring(1, value)));
    i.FontFamily(x::Media::FontFamily(L"Segoe Fluent Icons"));
    i.FontSize(size);
    return i;
}
void Row(c::Grid const &g, double value = 0)
{
    c::RowDefinition r;
    r.Height({value, value == 0 ? x::GridUnitType::Auto : x::GridUnitType::Pixel});
    g.RowDefinitions().Append(r);
}
void Column(c::Grid const &g, double value = 0)
{
    c::ColumnDefinition d;
    d.Width({value == 0 ? 1 : value, value == 0 ? x::GridUnitType::Star : x::GridUnitType::Pixel});
    g.ColumnDefinitions().Append(d);
}
void Place(c::Grid const &g, x::UIElement const &item, int row = 0, int col = 0)
{
    auto element = item.as<x::FrameworkElement>();
    c::Grid::SetRow(element, row);
    c::Grid::SetColumn(element, col);
    g.Children().Append(item);
}
c::Button Button(x::UIElement const &content, winrt::param::hstring const &name)
{
    c::Button b;
    b.Content(content);
    b.Background(Transparent());
    b.BorderThickness({0, 0, 0, 0});
    b.CornerRadius({4, 4, 4, 4});
    detail::ButtonStyle(b);
    b.HorizontalContentAlignment(x::HorizontalAlignment::Center);
    b.VerticalContentAlignment(x::VerticalAlignment::Center);
    a::AutomationProperties::SetName(b, name);
    return b;
}
} // namespace
struct Host::Impl
{
    HWND owner;
    Handler handler;
    NativeFlyout flyout;
    Model model;
    Kind current = Kind::QuickSettings;
    std::vector<std::unique_ptr<detail::Material>> materials;
    std::unique_ptr<detail::Material> genericMaterial;
    std::unique_ptr<detail::Material> menuMaterial;
    winrt::com_ptr<detail::StableBrush> menuBrush;
    std::vector<c::Grid> cardSurfaces;
    std::vector<std::pair<winrt::weak_ref<c::Grid>, winrt::event_token>> shadowSubscriptions;
    x::UIElement contentRoot{nullptr};
    std::shared_ptr<detail::Calendar> dateCalendar;
    c::ScrollViewer quickTiles{nullptr};
    c::StackPanel quickPager{nullptr};
    bool quickCompact = false;
    void ResizeQuick(POINT anchor)
    {
        if (!quickTiles)
            return;
        const bool compact = StackHeight(anchor) < 720;
        quickTiles.Height(compact ? 120 : NAN);
        quickTiles.VerticalScrollMode(compact ? c::ScrollMode::Enabled : c::ScrollMode::Disabled);
        quickPager.Visibility(compact ? x::Visibility::Visible : x::Visibility::Collapsed);
        if (quickCompact != compact)
            quickTiles.ChangeView(nullptr, 0., nullptr, true);
        quickCompact = compact;
    }
    std::shared_ptr<detail::CardStack> cardStack;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    bool built = false, custom = false;
    bool updatingModel = false;
    std::vector<std::function<void()>> bindings;
    bool pendingOpen = false;
    UINT reopenMessage = RegisterWindowMessageW(L"Yata.ShellPanels.DeferredOpen.v1");
    UINT layoutMessage = RegisterWindowMessageW(L"Yata.ShellPanels.DisplayLayout.v1");
    bool layoutQueued = false;
    x::XamlRoot observedRoot{nullptr};
    winrt::event_token rootLoaded{}, rootChanged{};
    double observedScale = 0;
    void ObserveRoot()
    {
        auto root = flyout.ContentRoot().XamlRoot();
        if (!root || root == observedRoot)
            return;
        if (observedRoot)
            observedRoot.Changed(rootChanged);
        observedRoot = root;
        observedScale = root.RasterizationScale();
        rootChanged = root.Changed([this, life = alive](auto const &sender, auto const &) noexcept {
            if (*life && sender.RasterizationScale() != observedScale)
            {
                observedScale = sender.RasterizationScale();
                PostMessageW(owner, layoutMessage, 0, 0);
            }
        });
    }
    winrt::Windows::UI::ViewManagement::UISettings uiSettings;
    winrt::Windows::UI::ViewManagement::AccessibilitySettings accessibility;
    winrt::event_token effectsToken{}, contrastToken{}, colorsToken{}, textScaleToken{};
    std::shared_ptr<std::atomic_bool> listening = std::make_shared<std::atomic_bool>(true);
    UINT policyMessage = RegisterWindowMessageW(L"Yata.ShellPanels.PolicyChanged.v1");
    static LRESULT CALLBACK WindowMetrics(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR,
                                          DWORD_PTR reference) noexcept
    {
        auto self = reinterpret_cast<Impl *>(reference);
        if (message == WM_DPICHANGED || message == WM_DISPLAYCHANGE ||
            (message == WM_SETTINGCHANGE && wp == SPI_SETWORKAREA))
        {
            // These are sent window messages; a normal MSG pump never sees them.
            // Defer until Windows and the Island finish updating their DPI.
            if (!self->layoutQueued)
            {
                self->layoutQueued = true;
                if (!PostMessageW(window, self->layoutMessage, 0, 0))
                    self->layoutQueued = false;
            }
        }
        return DefSubclassProc(window, message, wp, lp);
    }
    x::Media::SolidColorBrush Accent()
    {
        if (accessibility.HighContrast())
        {
            auto color = GetSysColor(COLOR_HIGHLIGHT);
            return Brush(255, GetRValue(color), GetGValue(color), GetBValue(color));
        }
        if (model.accentArgb)
        {
            const auto color = *model.accentArgb;
            return Brush(255, static_cast<BYTE>(color >> 16), static_cast<BYTE>(color >> 8), static_cast<BYTE>(color));
        }
        return x::Media::SolidColorBrush(
            uiSettings.GetColorValue(model.light ? winrt::Windows::UI::ViewManagement::UIColorType::AccentDark1
                                                 : winrt::Windows::UI::ViewManagement::UIColorType::AccentLight2));
    }
    Impl(HWND w, Handler h, std::function<void(bool)> visibility = {}, std::function<void(UINT)> command = {},
         std::function<void(Host::Phase)> phase = {})
        : owner(w), handler(std::move(h))
    {
        flyout.Initialize(w, std::move(visibility), std::move(command), [this, phase](NativeFlyout::Phase value) {
            if (phase)
                phase(static_cast<Host::Phase>(value));
            if (value == NativeFlyout::Phase::Closed && pendingOpen)
                PostMessageW(owner, reopenMessage, 0, 0);
        });
        flyout.ShellSurface();
        flyout.ShadowRenderer([this](c::Grid const &target) { Shadows(target); });
        auto notify = [w, message = policyMessage, life = listening](auto const &, auto const &) noexcept {
            if (life->load())
                PostMessageW(w, message, 0, 0);
        };
        effectsToken = uiSettings.AdvancedEffectsEnabledChanged(notify);
        colorsToken = uiSettings.ColorValuesChanged(notify);
        contrastToken = accessibility.HighContrastChanged(notify);
        textScaleToken = uiSettings.TextScaleFactorChanged(
            [w, message = layoutMessage, life = listening](auto const &, auto const &) noexcept {
                if (life->load())
                    PostMessageW(w, message, 0, 0);
            });
        winrt::check_bool(SetWindowSubclass(owner, WindowMetrics, reinterpret_cast<UINT_PTR>(this),
                                            reinterpret_cast<DWORD_PTR>(this)));
        rootLoaded = flyout.ContentRoot().Loaded([this, life = alive](auto const &, auto const &) {
            if (*life)
                ObserveRoot();
        });
        ObserveRoot();
    }
    ~Impl()
    {
        *alive = false;
        listening->store(false);
        RemoveWindowSubclass(owner, WindowMetrics, reinterpret_cast<UINT_PTR>(this));
        flyout.ContentRoot().Loaded(rootLoaded);
        if (observedRoot)
            observedRoot.Changed(rootChanged);
        uiSettings.AdvancedEffectsEnabledChanged(effectsToken);
        uiSettings.ColorValuesChanged(colorsToken);
        accessibility.HighContrastChanged(contrastToken);
        uiSettings.TextScaleFactorChanged(textScaleToken);
        ClearShadows();
        materials.clear();
        cardStack.reset();
        genericMaterial.reset();
        menuMaterial.reset();
        cardSurfaces.clear();
        dateCalendar.reset();
        flyout.Close();
        menuBrush = nullptr;
    }
    void RefreshMaterial()
    {
        if (genericMaterial)
            genericMaterial->Theme(model.light, model.tintArgb, model.luminosity);
        for (auto const &material : materials)
            material->Theme(model.light, model.tintArgb, model.luminosity);
        if (menuMaterial)
        {
            menuMaterial->Theme(model.light, model.tintArgb, model.luminosity.value_or(model.light ? .85 : .96));
            menuBrush->Set(menuMaterial->Brush());
        }
    }
    void PrepareMenu()
    {
        if (!menuMaterial)
        {
            menuMaterial = std::make_unique<detail::Material>();
            menuMaterial->Attach(c::Grid());
            menuMaterial->Theme(model.light, model.tintArgb, model.luminosity.value_or(model.light ? .85 : .96));
            menuBrush = winrt::make_self<detail::StableBrush>(menuMaterial->Brush());
            flyout.MenuBackground(menuBrush.as<x::Media::Brush>());
        }
        else
            RefreshMaterial();
    }
    void ClearShadows() noexcept
    {
        for (auto const &[weak, token] : shadowSubscriptions)
            try
            {
                if (auto card = weak.get())
                    card.SizeChanged(token);
            }
            catch (...)
            {
            }
        shadowSubscriptions.clear();
    }
    void Shadows(c::Grid const &target)
    {
        ClearShadows();
        auto compositor = x::Hosting::ElementCompositionPreview::GetElementVisual(target).Compositor();
        auto container = compositor.CreateContainerVisual();
        for (auto const &card : cardSurfaces)
        {
            const f::Numerics::float2 size{static_cast<float>(card.ActualWidth()),
                                           static_cast<float>(card.ActualHeight())};
            auto offset = card.TransformToVisual(contentRoot).TransformPoint({0, 0});
            auto sprite = compositor.CreateSpriteVisual();
            sprite.Size(size);
            sprite.Offset({offset.X, offset.Y, 0});
            auto geometry = compositor.CreateRoundedRectangleGeometry();
            geometry.Size(size);
            geometry.CornerRadius({8, 8});
            auto shape = compositor.CreateSpriteShape(geometry);
            shape.FillBrush(compositor.CreateColorBrush({255, 255, 255, 255}));
            auto mask = compositor.CreateShapeVisual();
            mask.Size(size);
            mask.Shapes().Append(shape);
            auto surface = compositor.CreateVisualSurface();
            surface.SourceVisual(mask);
            surface.SourceSize(size);
            auto shadow = compositor.CreateDropShadow();
            shadow.BlurRadius(24);
            shadow.Offset({0, 4, 0});
            shadow.Color({255, 0, 0, 0});
            shadow.Opacity(model.light ? .18f : .32f);
            shadow.Mask(compositor.CreateSurfaceBrush(surface));
            sprite.Shadow(shadow);
            // Reuse the shadow resources when XAML lays out a changing card.
            // Do not bind to XAML's hand-off Visual.Size: its facade does not
            // consistently expose the arranged size in OS XAML Islands.
            auto resize = [sprite, geometry, mask, surface,
                           root = winrt::make_weak(contentRoot)](auto const &sender, auto const &) noexcept {
                try
                {
                    auto view = sender.template as<c::Grid>();
                    if (auto parent = root.get())
                    {
                        const f::Numerics::float2 dimensions{static_cast<float>(view.ActualWidth()),
                                                             static_cast<float>(view.ActualHeight())};
                        auto point = view.TransformToVisual(parent).TransformPoint({0, 0});
                        sprite.Size(dimensions);
                        sprite.Offset({point.X, point.Y, 0});
                        geometry.Size(dimensions);
                        mask.Size(dimensions);
                        surface.SourceSize(dimensions);
                    }
                }
                catch (...)
                {
                    OutputDebugStringW(L"ShellPanels: shadow relayout failed\n");
                }
            };
            shadowSubscriptions.emplace_back(winrt::make_weak(card), card.SizeChanged(resize));
            container.Children().InsertAtTop(sprite);
        }
        x::Hosting::ElementCompositionPreview::SetElementChildVisual(target, container);
    }
    void Invoke(Action action, double value = 0)
    {
        if (updatingModel)
            return;
        if (action == Action::NetworkToggle)
            value = model.wifiEnabled ? 0 : 1;
        else if (action == Action::BluetoothToggle)
            value = model.bluetoothEnabled ? 0 : 1;
        else if (action == Action::Airplane)
            value = model.airplaneEnabled ? 0 : 1;
        else if (action == Action::EnergySaver)
            value = model.energySaverEnabled ? 0 : 1;
        if (handler)
            handler(action, value);
    }
    void Bind(c::Button const &button, Action action, double value = 0)
    {
        button.Click([this, life = alive, action, value](auto const &, auto const &) noexcept {
            if (!*life)
                return;
            try
            {
                Invoke(action, value);
            }
            catch (...)
            {
                OutputDebugStringW(L"ShellPanels: application action failed\n");
            }
        });
    }
    c::Grid Card(x::UIElement const &content)
    {
        c::Grid card, background;
        card.CornerRadius({8, 8, 8, 8});
        // The Shell stroke surrounds the material. Painting backdrop beneath
        // it makes a light edge opaque and a dark edge excessively bright.
        background.Margin({1, 1, 1, 1});
        card.Children().Append(background);
        auto material = std::make_unique<detail::Material>();
        material->Attach(background, 7);
        material->Theme(model.light, model.tintArgb, model.luminosity);
        materials.push_back(std::move(material));
        c::Border frame;
        frame.BorderThickness({1, 1, 1, 1});
        auto frameTheme = [this, weak = winrt::make_weak(frame)] {
            if (auto view = weak.get())
            {
                const auto color = GetSysColor(COLOR_WINDOWTEXT);
                view.BorderBrush(accessibility.HighContrast()
                                     ? Brush(255, GetRValue(color), GetGValue(color), GetBValue(color))
                                     : Brush(model.light ? 16 : 24, 255, 255, 255));
            }
        };
        frameTheme();
        bindings.push_back(frameTheme);
        frame.CornerRadius({8, 8, 8, 8});
        frame.Child(content);
        card.Children().Append(frame);
        cardSurfaces.push_back(card);
        return card;
    }
    c::Border Divider()
    {
        c::Border b;
        b.Height(1);
        b.Background(model.light ? Brush(14, 0, 0, 0) : Brush(21, 255, 255, 255));
        return b;
    }
    c::Button IconButton(wchar_t icon, winrt::hstring const &name, double width = 32)
    {
        auto b = Button(Glyph(icon, 16), name);
        b.Width(width);
        b.Height(32);
        b.Padding({0, 0, 0, 0});
        c::ToolTipService::SetToolTip(b, winrt::box_value(name));
        return b;
    }
    void RestingButton(c::Button const &button)
    {
        auto update = [this, weak = winrt::make_weak(button)] {
            if (auto view = weak.get())
            {
                view.Background(model.light ? Brush(180, 255, 255, 255) : Brush(18, 255, 255, 255));
                view.BorderBrush(model.light ? Brush(30, 0, 0, 0) : Brush(18, 255, 255, 255));
            }
        };
        update();
        bindings.push_back(update);
        button.BorderThickness({1, 1, 1, 1});
    }
    double StackHeight(POINT point)
    {
        const auto display = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor{sizeof(monitor)};
        winrt::check_bool(GetMonitorInfoW(display, &monitor));
        UINT dpi = 96, dpiY = 96;
        if (FAILED(GetDpiForMonitor(display, MDT_EFFECTIVE_DPI, &dpi, &dpiY)))
            dpi = GetDpiForWindow(owner);
        return std::max(1.0, (monitor.rcWork.bottom - monitor.rcWork.top) * 96.0 / dpi - 20);
    }
    UINT DpiAt(POINT point) const
    {
        UINT dpi = 96, dpiY = 96;
        if (FAILED(GetDpiForMonitor(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &dpi, &dpiY)))
            dpi = GetDpiForWindow(owner);
        return dpi;
    }
    x::UIElement Sections(std::vector<CardSection> sections)
    {
        auto stack = std::make_shared<detail::CardStack>();
        stack->Build(std::move(sections), StackHeight(lastAnchor), [this](auto const &body) { return Card(body); });
        cardStack = stack;
        return stack->Root();
    }
    x::UIElement LanguagePanel()
    {
        c::StackPanel body;
        c::Grid header;
        header.Height(48);
        header.Margin({16, 0, 16, 0});
        Column(header, 110);
        Column(header);
        Place(header, Text(L"Keyboard layout"));
        c::StackPanel hint;
        hint.Orientation(c::Orientation::Horizontal);
        hint.Spacing(2);
        hint.Opacity(.6);
        hint.VerticalAlignment(x::VerticalAlignment::Center);
        hint.HorizontalAlignment(x::HorizontalAlignment::Left);
        for (int key = 0; key < 3; ++key)
        {
            if (key == 1)
            {
                hint.Children().Append(Text(L"+", 11));
                continue;
            }
            c::Border cap;
            cap.Height(16);
            cap.Padding({2, 0, 2, 0});
            cap.BorderThickness({1, 1, 1, 1});
            cap.BorderBrush(model.light ? Brush(96, 0, 0, 0) : Brush(96, 255, 255, 255));
            cap.CornerRadius({2, 2, 2, 2});
            if (key == 0)
            {
                cap.Width(16);
                cap.Child(Glyph(L'\xE782', 10));
            }
            else
                cap.Child(Text(L"Spacebar", 12));
            hint.Children().Append(cap);
        }
        Place(header, hint, 0, 1);
        body.Children().Append(header);
        c::ListView languages;
        languages.SelectionMode(c::ListViewSelectionMode::Single);
        languages.IsItemClickEnabled(true);
        languages.IsTabStop(true);
        languages.Padding({4, 0, 4, 0});
        languages.Margin({0, 0, 0, 0});
        for (size_t index = 0; index < model.languages.size(); ++index)
        {
            const auto &language = model.languages[index];
            c::Grid row;
            row.Height(56);
            Column(row, 48);
            Column(row);
            auto code = Text(language.code, 12);
            code.Margin({12, 0, 0, 0});
            Place(row, code);
            c::StackPanel labels;
            labels.VerticalAlignment(x::VerticalAlignment::Center);
            labels.Margin({-4, 0, 0, 0});
            labels.Children().Append(Text(language.name));
            auto sub = Text(language.layout, 12);
            sub.Opacity(.65);
            labels.Children().Append(sub);
            Place(row, labels, 0, 1);
            c::ListViewItem item;
            item.Content(row);
            item.Padding({0, 0, 0, 0});
            item.MinHeight(56);
            item.HorizontalContentAlignment(x::HorizontalAlignment::Stretch);
            item.CornerRadius({4, 4, 4, 4});
            a::AutomationProperties::SetName(item, language.name + L", " + language.layout);
            item.Template(
                x::Markup::XamlReader::Load(
                    LR"(<ControlTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" TargetType="ListViewItem"><Grid>
    <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
     <VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="Fill.Background" Value="{ThemeResource SystemControlHighlightListLowBrush}"/></VisualState.Setters></VisualState>
     <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="Fill.Background" Value="{ThemeResource SystemControlHighlightListMediumBrush}"/></VisualState.Setters></VisualState>
     <VisualState x:Name="Selected"><VisualState.Setters><Setter Target="Fill.Background" Value="{ThemeResource SystemControlHighlightListLowBrush}"/><Setter Target="Indicator.Opacity" Value="1"/></VisualState.Setters></VisualState>
     <VisualState x:Name="PointerOverSelected"><VisualState.Setters><Setter Target="Fill.Background" Value="{ThemeResource SystemControlHighlightListMediumBrush}"/><Setter Target="Indicator.Opacity" Value="1"/></VisualState.Setters></VisualState>
     <VisualState x:Name="PressedSelected"><VisualState.Setters><Setter Target="Fill.Background" Value="{ThemeResource SystemControlHighlightListMediumBrush}"/><Setter Target="Indicator.Opacity" Value="1"/></VisualState.Setters></VisualState>
    </VisualStateGroup></VisualStateManager.VisualStateGroups>
    <Border x:Name="Fill" CornerRadius="4"/><Border x:Name="Indicator" Width="3" Height="20" CornerRadius="2" HorizontalAlignment="Left" VerticalAlignment="Center" Background="{ThemeResource SystemControlHighlightAccentBrush}" Opacity="0"/>
    <ContentPresenter Content="{TemplateBinding Content}" HorizontalContentAlignment="Stretch"/></Grid></ControlTemplate>)")
                    .as<c::ControlTemplate>());
            languages.Items().Append(item);
        }
        if (!model.languages.empty())
            languages.SelectedIndex(
                std::clamp(model.selectedLanguage, 0, static_cast<int>(model.languages.size()) - 1));
        languages.SelectionChanged(
            [this, life = alive, weak = winrt::make_weak(languages)](auto const &, auto const &) noexcept {
                if (!*life)
                    return;
                try
                {
                    if (auto control = weak.get())
                    {
                        model.selectedLanguage = control.SelectedIndex();
                        Invoke(Action::LanguageSelected, model.selectedLanguage);
                        flyout.Hide();
                    }
                }
                catch (...)
                {
                }
            });
        body.Children().Append(languages);
        body.Children().Append(Divider());
        auto settings = Button(Text(L"More keyboard settings", 12), L"More keyboard settings");
        settings.Foreground(model.light ? Brush(160, 0, 0, 0) : Brush(200, 255, 255, 255));
        settings.Height(40);
        settings.Margin({4, 4, 4, 4});
        settings.Padding({12, 0, 16, 0});
        settings.HorizontalAlignment(x::HorizontalAlignment::Stretch);
        settings.HorizontalContentAlignment(x::HorizontalAlignment::Left);
        Bind(settings, Action::Settings);
        body.Children().Append(settings);
        return Card(body);
    }
    c::StackPanel Tile(wchar_t icon, winrt::hstring const &title, bool selected, bool split, Action action)
    {
        c::StackPanel tile;
        tile.Spacing(8);
        c::Grid button;
        button.Height(48);
        button.CornerRadius({4, 4, 4, 4});
        button.Background(selected ? Accent() : model.light ? Brush(185, 255, 255, 255) : Brush(18, 255, 255, 255));
        Column(button);
        if (split)
            Column(button);
        auto primary = Button(Glyph(icon, 16), title);
        primary.Height(48);
        primary.HorizontalAlignment(x::HorizontalAlignment::Stretch);
        primary.Padding({0, 0, 0, 0});
        Bind(primary, action == Action::NetworkDetails     ? Action::NetworkToggle
                      : action == Action::BluetoothDetails ? Action::BluetoothToggle
                                                           : action);
        if (selected)
            primary.Foreground(model.light ? Brush(255, 255, 255, 255) : Brush(255, 0, 0, 0));
        Place(button, primary);
        if (split)
        {
            auto detail = Button(Glyph(L'\xE76C', 12), title + L" details");
            detail.HorizontalAlignment(x::HorizontalAlignment::Stretch);
            detail.Height(48);
            detail.Padding({0, 0, 0, 0});
            if (selected)
                detail.Foreground(model.light ? Brush(255, 255, 255, 255) : Brush(255, 0, 0, 0));
            Bind(detail, action);
            Place(button, detail, 0, 1);
            c::Border divider;
            divider.Width(1);
            divider.HorizontalAlignment(x::HorizontalAlignment::Left);
            divider.Background(selected ? Brush(24, 255, 255, 255) : Brush(14, 0, 0, 0));
            Place(button, divider, 0, 1);
        }
        c::Border outline;
        outline.IsHitTestVisible(false);
        outline.CornerRadius({4, 4, 4, 4});
        outline.BorderThickness({1, 1, 1, 1});
        outline.BorderBrush(model.light ? Brush(12, 0, 0, 0) : Brush(20, 255, 255, 255));
        c::Grid::SetColumnSpan(outline, split ? 2 : 1);
        button.Children().Append(outline);
        tile.Children().Append(button);
        auto label = Text(title, 12);
        label.HorizontalAlignment(x::HorizontalAlignment::Center);
        label.TextTrimming(x::TextTrimming::CharacterEllipsis);
        tile.Children().Append(label);
        bindings.push_back([this, weak = winrt::make_weak(button), caption = winrt::make_weak(label), action] {
            auto grid = weak.get();
            if (!grid)
                return;
            bool enabled = false;
            if (action == Action::NetworkDetails)
            {
                enabled = model.wifiEnabled;
                if (auto text = caption.get())
                    text.Text(model.network);
            }
            else if (action == Action::BluetoothDetails)
            {
                enabled = model.bluetoothEnabled;
                if (auto text = caption.get())
                    text.Text(model.bluetooth);
            }
            else if (action == Action::Airplane)
                enabled = model.airplaneEnabled;
            else if (action == Action::EnergySaver)
                enabled = model.energySaverEnabled;
            grid.Background(enabled ? Accent() : model.light ? Brush(185, 255, 255, 255) : Brush(18, 255, 255, 255));
            for (auto const &child : grid.Children())
                if (auto control = child.try_as<c::Button>())
                    control.Foreground(enabled ? (model.light ? Brush(255, 255, 255, 255) : Brush(255, 0, 0, 0))
                                               : (model.light ? Brush(228, 0, 0, 0) : Brush(255, 255, 255, 255)));
        });
        return tile;
    }
    c::Grid SliderRow(wchar_t icon, winrt::hstring const &label, int value, Action action, bool audio)
    {
        c::Grid row;
        row.Height(48);
        Column(row, 36);
        Column(row);
        Column(row, 32);
        Place(row, Glyph(icon));
        c::Slider slider;
        slider.Minimum(0);
        slider.Maximum(100);
        slider.Value(value);
        slider.VerticalAlignment(x::VerticalAlignment::Center);
        a::AutomationProperties::SetName(slider, label);
        a::AutomationProperties::SetAutomationId(slider, label);
        slider.Template(detail::SliderTemplate(model.light));
        slider.Foreground(Accent());
        slider.Background(model.light ? Brush(110, 0, 0, 0) : Brush(140, 255, 255, 255));
        slider.ValueChanged([this, life = alive,
                             action](auto const &, c::Primitives::RangeBaseValueChangedEventArgs const &e) noexcept {
            if (!*life || updatingModel)
                return;
            try
            {
                if (action == Action::Volume)
                    model.volume = static_cast<int>(std::lround(e.NewValue()));
                else if (action == Action::Brightness)
                    model.brightness = static_cast<int>(std::lround(e.NewValue()));
                Invoke(action, e.NewValue());
            }
            catch (...)
            {
            }
        });
        Place(row, slider, 0, 1);
        bindings.push_back([this, weak = winrt::make_weak(slider), action] {
            if (auto control = weak.get())
            {
                control.Value(action == Action::Volume ? model.volume : model.brightness);
                control.Foreground(Accent());
            }
        });
        if (audio)
        {
            auto settings = IconButton(L'\xE713', L"Select a sound output");
            Bind(settings, Action::AudioDetails);
            Place(row, settings, 0, 2);
        }
        return row;
    }
    x::UIElement QuickPanel()
    {
        c::StackPanel cards;
        cards.Spacing(12);
        if (model.mediaVisible)
        {
            c::Grid media;
            media.Height(176);
            media.Padding({16, 12, 16, 12});
            Row(media, 32);
            Row(media, 64);
            Row(media, 48);
            Place(media, Text(model.mediaSubtitle, 12));
            auto title = Text(model.mediaTitle, 14);
            title.FontWeight({600});
            Place(media, title, 1);
            c::StackPanel transport;
            transport.Orientation(c::Orientation::Horizontal);
            transport.HorizontalAlignment(x::HorizontalAlignment::Center);
            transport.Spacing(22);
            int index = 0;
            for (auto icon : {L'\xE892', L'\xE768', L'\xE893'})
            {
                auto button = IconButton(icon, index == 0 ? L"Previous" : index == 1 ? L"Play or pause" : L"Next");
                Bind(button, index == 0 ? Action::MediaPrevious : index == 1 ? Action::MediaToggle : Action::MediaNext);
                transport.Children().Append(button);
                ++index;
            }
            Place(media, transport, 2);
            cards.Children().Append(Card(media));
        }
        c::StackPanel body;
        c::Grid tiles;
        tiles.Margin({22, 24, 24, 24});
        tiles.ColumnSpacing(12);
        tiles.RowSpacing(26);
        for (int i = 0; i < 3; ++i)
            Column(tiles);
        Row(tiles);
        Row(tiles);
        Place(tiles, Tile(L'\xE701', winrt::hstring(model.network), model.wifiEnabled, true, Action::NetworkDetails));
        Place(tiles,
              Tile(L'\xE702', winrt::hstring(model.bluetooth), model.bluetoothEnabled, true, Action::BluetoothDetails),
              0, 1);
        Place(tiles, Tile(L'\xE709', L"Airplane mode", model.airplaneEnabled, false, Action::Airplane), 0, 2);
        Place(tiles, Tile(L'\xE776', L"Accessibility", false, true, Action::Accessibility), 1);
        Place(tiles, Tile(L'\xE705', L"VPN", false, true, Action::VpnDetails), 1, 1);
        Place(tiles, Tile(L'\xEBA8', L"Energy saver", model.energySaverEnabled, false, Action::EnergySaver), 1, 2);
        quickTiles = c::ScrollViewer();
        quickTiles.Content(tiles);
        quickTiles.HorizontalScrollMode(c::ScrollMode::Disabled);
        quickTiles.HorizontalScrollBarVisibility(c::ScrollBarVisibility::Disabled);
        quickTiles.VerticalScrollBarVisibility(c::ScrollBarVisibility::Hidden);
        quickTiles.ZoomMode(c::ZoomMode::Disabled);
        a::AutomationProperties::SetName(quickTiles, L"Quick settings pages");
        c::Grid tileArea;
        tileArea.Children().Append(quickTiles);
        quickPager = c::StackPanel();
        quickPager.HorizontalAlignment(x::HorizontalAlignment::Right);
        quickPager.VerticalAlignment(x::VerticalAlignment::Center);
        quickPager.Margin({0, 0, 6, 0});
        for (int page = 0; page < 2; ++page)
        {
            x::Shapes::Ellipse dot;
            dot.Width(3);
            dot.Height(3);
            dot.Fill(model.light ? Brush(128, 0, 0, 0) : Brush(160, 255, 255, 255));
            auto button = Button(dot, page ? L"More quick settings" : L"First quick settings page");
            button.MinWidth(0);
            button.MinHeight(0);
            button.Width(12);
            button.Height(12);
            button.Padding({0, 0, 0, 0});
            button.Click([weak = winrt::make_weak(quickTiles), page](auto const &, auto const &) {
                if (auto scroll = weak.get())
                    scroll.ChangeView(nullptr, page ? scroll.ScrollableHeight() : 0., nullptr);
            });
            quickPager.Children().Append(button);
        }
        tileArea.Children().Append(quickPager);
        body.Children().Append(tileArea);
        ResizeQuick(lastAnchor);
        body.Children().Append(Divider());
        c::StackPanel sliders;
        sliders.Margin({24, 12, 24, 12});
        sliders.Children().Append(SliderRow(L'\xE706', L"Brightness", model.brightness, Action::Brightness, false));
        sliders.Children().Append(SliderRow(L'\xE993', L"Volume", model.volume, Action::Volume, true));
        body.Children().Append(sliders);
        body.Children().Append(Divider());
        c::Grid footer;
        footer.Height(48);
        footer.Padding({16, 0, 16, 0});
        Column(footer);
        Column(footer, 32);
        c::StackPanel battery;
        battery.Orientation(c::Orientation::Horizontal);
        battery.Spacing(8);
        battery.Children().Append(Glyph(L'\xE83F', 16));
        battery.Children().Append(Text(std::to_wstring(model.battery) + L"%", 12));
        Place(footer, battery);
        auto settings = IconButton(L'\xE713', L"All settings");
        Bind(settings, Action::Settings);
        Place(footer, settings, 0, 1);
        body.Children().Append(footer);
        cards.Children().Append(Card(body));
        return cards;
    }
    x::UIElement DatePanel()
    {
        c::Grid header;
        header.Height(48);
        header.Padding({16, 0, 16, 0});
        Column(header);
        Column(header, 24);
        Column(header, 72);
        Place(header, Text(L"Notifications", 14));
        auto dnd = IconButton(L'\xE7ED', L"Do not disturb", 24);
        dnd.Height(24);
        RestingButton(dnd);
        Bind(dnd, Action::Settings);
        Place(header, dnd, 0, 1);
        auto clear = Button(Text(L"Clear all", 12), L"Clear all notifications");
        clear.Height(24);
        clear.Margin({8, 0, 0, 0});
        RestingButton(clear);
        Bind(clear, Action::ClearNotifications);
        Place(header, clear, 0, 2);
        c::StackPanel entries;
        if (model.notifications.empty())
        {
            auto empty = Text(L"No new notifications");
            empty.Opacity(.65);
            empty.HorizontalAlignment(x::HorizontalAlignment::Center);
            empty.Margin({0, 72, 0, 0});
            entries.Children().Append(empty);
            clear.Visibility(x::Visibility::Collapsed);
        }
        for (size_t index = 0; index < model.notifications.size(); ++index)
        {
            const auto &data = model.notifications[index];
            c::StackPanel group;
            auto app = Text(data.app, 12);
            app.Height(36);
            app.Margin({16, 0, 16, 0});
            group.Children().Append(app);
            c::StackPanel text;
            auto time = Text(data.time, 12);
            time.Opacity(.65);
            time.Margin({8, 8, 8, 12});
            text.Children().Append(time);
            auto title = Text(data.title);
            title.Margin({12, 0, 12, 0});
            text.Children().Append(title);
            auto body = Text(data.body);
            body.TextWrapping(x::TextWrapping::Wrap);
            body.Opacity(.65);
            body.Margin({12, 0, 12, 16});
            text.Children().Append(body);
            if (!data.action.empty())
            {
                auto action = Button(Text(data.action), data.action);
                action.Margin({12, 4, 12, 12});
                action.HorizontalAlignment(x::HorizontalAlignment::Stretch);
                action.Height(32);
                RestingButton(action);
                Bind(action, Action::NotificationAction, static_cast<double>(index));
                text.Children().Append(action);
            }
            if (data.grouped)
            {
                auto more = Button(Text(L"+" + std::to_wstring(data.grouped) + L" notifications", 12),
                                   L"Expand grouped notifications");
                more.Margin({12, 4, 12, 12});
                more.Height(24);
                RestingButton(more);
                Bind(more, Action::NotificationAction, static_cast<double>(index));
                text.Children().Append(more);
            }
            c::Border tile;
            tile.Margin({4, 0, 4, 4});
            tile.CornerRadius({4, 4, 4, 4});
            tile.BorderThickness({1, 1, 1, 1});
            tile.BorderBrush(model.light ? Brush(16, 0, 0, 0) : Brush(20, 255, 255, 255));
            tile.Background(model.light ? Brush(18, 255, 255, 255) : Brush(8, 255, 255, 255));
            tile.Child(text);
            group.Children().Append(tile);
            entries.Children().Append(group);
        }
        c::ScrollViewer scroller;
        scroller.VerticalScrollBarVisibility(c::ScrollBarVisibility::Auto);
        scroller.HorizontalScrollBarVisibility(c::ScrollBarVisibility::Disabled);
        scroller.Content(entries);
        c::StackPanel calendarHeader;
        c::Grid dayHeader;
        dayHeader.Name(L"DateHeader");
        dayHeader.Height(48);
        dayHeader.Padding({16, 0, 16, 0});
        Column(dayHeader);
        Column(dayHeader, 24);
        winrt::Windows::Globalization::DateTimeFormatting::DateTimeFormatter formatter(L"dayofweek month day");
        Place(dayHeader, Text(formatter.Format(winrt::clock::now())));
        auto collapse = IconButton(L'\xE70D', L"Collapse calendar", 24);
        collapse.Content(Glyph(L'\xE70D', 12));
        collapse.Height(24);
        RestingButton(collapse);
        Place(dayHeader, collapse, 0, 1);
        calendarHeader.Children().Append(dayHeader);
        calendarHeader.Children().Append(Divider());
        dateCalendar = std::make_shared<detail::Calendar>(model.light, Accent(), [this, life = alive](double value) {
            if (*life)
                Invoke(Action::DateSelected, value);
        });
        auto calendar = dateCalendar->Create([w = owner, message = layoutMessage] { PostMessageW(w, message, 0, 0); });
        calendar.Name(L"DateBody");
        collapse.Click([this, button = winrt::make_weak(collapse), life = alive](auto const &, auto const &) noexcept {
            if (!*life)
                return;
            try
            {
                auto control = button.get();
                if (!cardStack || !control)
                    return;
                const bool open = cardStack->Expanded(1);
                cardStack->Expand(1, !open, true);
                control.Content(Glyph(open ? L'\xE70E' : L'\xE70D', 12));
                a::AutomationProperties::SetName(control, open ? L"Expand calendar" : L"Collapse calendar");
            }
            catch (...)
            {
            }
        });
        c::StackPanel calendarFooter;
        calendarFooter.Children().Append(Divider());
        c::Grid focus;
        focus.Name(L"DateFooter");
        focus.Height(48);
        focus.Padding({16, 0, 16, 0});
        Column(focus, 24);
        Column(focus, 82);
        Column(focus, 24);
        Column(focus);
        Column(focus, 72);
        auto minus = IconButton(L'\xE738', L"Decrease duration", 24);
        auto plus = IconButton(L'\xE710', L"Increase duration", 24);
        minus.Content(Glyph(L'\xE738', 12));
        plus.Content(Glyph(L'\xE710', 12));
        minus.Height(24);
        plus.Height(24);
        RestingButton(minus);
        RestingButton(plus);
        Place(focus, minus);
        auto duration = Text(L"30 mins", 14);
        x::Documents::Run count, unit;
        count.Text(L"30");
        count.FontWeight({600});
        unit.Text(L" mins");
        unit.Foreground(model.light ? Brush(156, 0, 0, 0) : Brush(180, 255, 255, 255));
        duration.Inlines().Clear();
        duration.Inlines().Append(count);
        duration.Inlines().Append(unit);
        duration.HorizontalAlignment(x::HorizontalAlignment::Center);
        Place(focus, duration, 0, 1);
        Place(focus, plus, 0, 2);
        auto minutes = std::make_shared<int>(30);
        auto adjust = [minutes, count](int delta) {
            *minutes = std::clamp(*minutes + delta, 5, 240);
            count.Text(std::to_wstring(*minutes));
        };
        minus.Click([adjust](auto const &, auto const &) { adjust(-5); });
        plus.Click([adjust](auto const &, auto const &) { adjust(5); });
        c::StackPanel startContent;
        startContent.Orientation(c::Orientation::Horizontal);
        startContent.Spacing(4);
        startContent.Children().Append(Glyph(L'\xF5B0', 12));
        startContent.Children().Append(Text(L"Focus", 12));
        auto start = Button(startContent, L"Start focus session");
        start.HorizontalAlignment(x::HorizontalAlignment::Stretch);
        start.Height(24);
        start.Padding({4, 0, 4, 0});
        RestingButton(start);
        start.Click([this, life = alive, minutes](auto const &, auto const &) noexcept {
            if (!*life)
                return;
            try
            {
                Invoke(Action::Focus, *minutes);
            }
            catch (...)
            {
            }
        });
        Place(focus, start, 0, 4);
        calendarFooter.Children().Append(focus);
        return Sections({{header, scroller, nullptr, 0, true}, {calendarHeader, calendar, calendarFooter, 356}});
    }
    void Build(Kind kind)
    {
        if (flyout.Visible())
            winrt::throw_hresult(E_ILLEGAL_METHOD_CALL);
        bindings.clear();
        ClearShadows();
        cardStack.reset();
        cardSurfaces.clear();
        materials.clear();
        dateCalendar.reset();
        current = kind;
        quickTiles = nullptr;
        quickPager = nullptr;
        quickCompact = false;
        custom = false;
        flyout.ShellSurface();
        flyout.Motion(kind == Kind::Date,
                      kind == Kind::Language ? 83u
                      : kind == Kind::Date   ? 167u
                                             : 250u,
                      kind == Kind::Language ? 83u : 167u);
        flyout.TopInset(kind == Kind::Date ? 8.f : 12.f);
        flyout.Theme(model.light);
        auto resources = flyout.ContentRoot().Resources();
        resources.Insert(winrt::box_value(L"SystemControlHighlightListLowBrush"),
                         model.light ? Brush(9, 0, 0, 0) : Brush(15, 255, 255, 255));
        resources.Insert(winrt::box_value(L"SystemControlHighlightListMediumBrush"),
                         model.light ? Brush(6, 0, 0, 0) : Brush(10, 255, 255, 255));
        resources.Insert(winrt::box_value(L"SystemControlHighlightAccentBrush"), Accent());
        auto content = kind == Kind::Language        ? LanguagePanel()
                       : kind == Kind::QuickSettings ? QuickPanel()
                                                     : DatePanel();
        contentRoot = content;
        flyout.SetContent(content, L"", kind == Kind::Date ? 336.f : 360.f);
        built = true;
    }
    POINT Anchor(Kind kind, POINT point)
    {
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor);
        if (kind != Kind::Language)
            point.x = monitor.rcWork.right;
        else
            point.x -= MulDiv(52, DpiAt(point), 96);
        point.y = monitor.rcWork.bottom;
        return point;
    }
    POINT lastAnchor{};
};
Host::Host() = default;
Host::Host(HWND w, Handler h) : impl(std::make_unique<Impl>(w, std::move(h)))
{
}
Host::~Host() = default;
void Host::Initialize(HWND w, std::function<void(bool)> v, std::function<void(UINT)> c, std::function<void(Phase)> p)
{
    if (impl)
        winrt::throw_hresult(E_ILLEGAL_METHOD_CALL);
    impl = std::make_unique<Impl>(w, Handler{}, std::move(v), std::move(c), std::move(p));
}
void Host::SetContent(x::UIElement const &content, winrt::hstring const &title, float width)
{
    impl->cardStack.reset();
    impl->flyout.Motion(false);
    impl->bindings.clear();
    impl->flyout.GenericSurface();
    if (!impl->genericMaterial)
    {
        impl->genericMaterial = std::make_unique<detail::Material>();
        impl->genericMaterial->Attach(impl->flyout.GenericSurface());
    }
    impl->genericMaterial->Theme(impl->model.light, impl->model.tintArgb, impl->model.luminosity);
    impl->flyout.SetContent(content, title, width);
    impl->custom = true;
    impl->built = true;
    impl->cardSurfaces.clear();
    impl->materials.clear();
    impl->dateCalendar.reset();
    impl->quickTiles = nullptr;
    impl->quickPager = nullptr;
    impl->contentRoot = nullptr;
}
void Host::SetSections(std::vector<CardSection> sections, float width)
{
    if (!impl || impl->flyout.CurrentPhase() != NativeFlyout::Phase::Closed)
        winrt::throw_hresult(E_ILLEGAL_METHOD_CALL);
    detail::CardStack::Validate(sections);
    if (!std::isfinite(width) || width <= 0)
        winrt::throw_hresult(E_INVALIDARG);
    impl->cardStack.reset();
    impl->bindings.clear();
    impl->cardSurfaces.clear();
    impl->materials.clear();
    impl->dateCalendar.reset();
    impl->quickTiles = nullptr;
    impl->quickPager = nullptr;
    impl->flyout.ShellSurface();
    impl->flyout.Motion(true, 167, 167);
    impl->flyout.TopInset(8);
    impl->flyout.Theme(impl->model.light);
    impl->contentRoot = impl->Sections(std::move(sections));
    impl->flyout.SetContent(impl->contentRoot, L"", width);
    impl->custom = true;
    impl->built = true;
}
void Host::ExpandSection(size_t index, bool expanded, bool animate)
{
    if (!impl || !impl->cardStack)
        winrt::throw_hresult(E_ILLEGAL_METHOD_CALL);
    impl->cardStack->Expand(index, expanded, animate && impl->flyout.Visible());
}
bool Host::SectionExpanded(size_t index) const
{
    if (!impl || !impl->cardStack)
        winrt::throw_hresult(E_ILLEGAL_METHOD_CALL);
    return impl->cardStack->Expanded(index);
}
void Host::Select(Kind kind)
{
    impl->custom = false;
    impl->current = kind;
    impl->built = false;
    if (impl->flyout.Visible())
        impl->flyout.Hide();
}
void Host::Show(POINT anchor)
{
    if (impl->custom)
    {
        impl->lastAnchor = anchor;
        if (impl->cardStack)
        {
            impl->cardStack->Height(impl->StackHeight(anchor));
            anchor = impl->Anchor(Kind::Date, anchor);
        }
        impl->flyout.Show(anchor);
    }
    else
        Show(impl->current, anchor);
}
void Host::Toggle(POINT anchor)
{
    if (impl->custom)
    {
        if (impl->flyout.Visible())
            impl->flyout.Hide();
        else
            Show(anchor);
    }
    else
        Toggle(impl->current, anchor);
}
void Host::Theme(bool light)
{
    auto model = impl->model;
    model.light = light;
    SetModel(std::move(model));
}
void Host::Menu(POINT p, LONG mode, UINT width)
{
    impl->PrepareMenu();
    MONITORINFO monitor{sizeof(monitor)};
    winrt::check_bool(GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &monitor));
    p.y = monitor.rcWork.bottom - MulDiv(8, impl->DpiAt(p), 96);
    impl->flyout.Menu(p, mode, width);
}
void Host::Reposition(POINT p)
{
    impl->lastAnchor = p;
    impl->ResizeQuick(p);
    if (impl->cardStack)
    {
        if (impl->dateCalendar)
            impl->cardStack->BodyHeight(1, impl->dateCalendar->Resize(impl->StackHeight(p)));
        impl->cardStack->Height(impl->StackHeight(p));
        p = impl->Anchor(Kind::Date, p);
    }
    impl->flyout.Reposition(impl->custom ? p : impl->Anchor(impl->current, p));
}
void Host::PreferredWidth(float w)
{
    impl->flyout.PreferredWidth(w);
}
bool Host::Transitioning() const
{
    return impl && impl->flyout.Transitioning();
}
Host::Phase Host::CurrentPhase() const
{
    return impl ? static_cast<Phase>(impl->flyout.CurrentPhase()) : Phase::Closed;
}
bool Host::MenuOpen() const
{
    return impl && impl->flyout.MenuOpen();
}
void Host::Close() noexcept
{
    impl.reset();
}
void Host::SetModel(Model m)
{
    m.volume = std::clamp(m.volume, 0, 100);
    m.brightness = std::clamp(m.brightness, 0, 100);
    m.battery = std::clamp(m.battery, 0, 100);
    const bool themeChanged = impl->model.light != m.light;
    const bool materialChanged =
        themeChanged || impl->model.tintArgb != m.tintArgb || impl->model.luminosity != m.luminosity;
    impl->model = std::move(m);
    impl->built = false;
    if (themeChanged)
        impl->flyout.Theme(impl->model.light);
    if (materialChanged)
        impl->RefreshMaterial();
    impl->updatingModel = true;
    try
    {
        for (auto const &binding : impl->bindings)
            binding();
    }
    catch (...)
    {
        impl->updatingModel = false;
        throw;
    }
    impl->updatingModel = false;
}
void Host::SetHandler(Handler handler)
{
    impl->handler = std::move(handler);
}
void Host::Show(Kind kind, POINT point)
{
    if (impl->flyout.CurrentPhase() != NativeFlyout::Phase::Closed)
    {
        if (impl->custom || !impl->built || kind != impl->current)
        {
            impl->current = kind;
            impl->lastAnchor = point;
            impl->pendingOpen = true;
            impl->flyout.Hide();
        }
        return;
    }
    impl->lastAnchor = point;
    if (impl->custom || !impl->built || kind != impl->current)
        impl->Build(kind);
    impl->ResizeQuick(point);
    if (impl->cardStack)
    {
        if (impl->dateCalendar)
            impl->cardStack->BodyHeight(1, impl->dateCalendar->Resize(impl->StackHeight(point)));
        impl->cardStack->Height(impl->StackHeight(point));
    }
    impl->flyout.Show(impl->Anchor(kind, point));
}
void Host::Toggle(Kind kind, POINT point)
{
    if (impl->flyout.Visible())
    {
        impl->flyout.Hide();
        return;
    }
    Show(kind, point);
}
void Host::Hide()
{
    if (impl)
        impl->flyout.Hide();
}
void Host::Deactivated(HWND w, bool own)
{
    if (!impl || own)
        return;
    DWORD process = 0;
    if (w)
        GetWindowThreadProcessId(w, &process);
    if (process == GetCurrentProcessId() && w != impl->owner && !IsChild(impl->owner, w) &&
        GetAncestor(w, GA_ROOTOWNER) != impl->owner)
    {
        wchar_t type[96]{};
        GetClassNameW(w, type, 96);
        if (wcscmp(type, L"Xaml_WindowedPopupClass") && wcscmp(type, L"Xaml_PopupClass"))
        {
            impl->flyout.Hide();
            return;
        }
    }
    impl->flyout.OwnerDeactivated(w);
}
bool Host::Process(MSG const &m)
{
    if (!impl)
        return false;
    if (m.hwnd == impl->owner && m.message == impl->layoutMessage)
    {
        impl->layoutQueued = false;
        if (impl->flyout.MenuOpen())
            impl->flyout.Hide();
        if (impl->flyout.Visible())
            Reposition(impl->lastAnchor);
        return true;
    }
#ifdef SHELLPANELS_LAYOUT_TRACE
    static const UINT trace = RegisterWindowMessageW(L"Yata.ShellPanels.TraceLayout.v1");
    if (m.hwnd == impl->owner && m.message == trace)
    {
        detail::WriteLayoutTrace(impl->contentRoot, impl->owner);
        return true;
    }
#endif
    if (m.hwnd == impl->owner && m.message == impl->reopenMessage)
    {
        if (std::exchange(impl->pendingOpen, false))
            Show(impl->current, impl->lastAnchor);
        return true;
    }
    if (m.hwnd == impl->owner &&
        (m.message == impl->policyMessage || m.message == WM_SETTINGCHANGE || m.message == WM_POWERBROADCAST))
    {
        impl->RefreshMaterial();
        if (m.message == impl->policyMessage)
            return true;
    }
    return impl->flyout.Process(m);
}
bool Host::Visible() const
{
    return impl && impl->flyout.Visible();
}
int Host::PhaseValue() const
{
    return static_cast<int>(CurrentPhase());
}
void Host::ContextMenu(Kind kind, POINT anchor)
{
    ContextMenu(kind == Kind::Language        ? TrayPart::Language
                : kind == Kind::QuickSettings ? TrayPart::Volume
                                              : TrayPart::Clock,
                anchor);
}
void Host::ContextMenu(TrayPart part, POINT anchor)
{
    if (part == TrayPart::Language)
        return;
    impl->PrepareMenu();
    c::MenuFlyout menu;
    auto add = [&](wchar_t const *label, wchar_t icon) {
        c::MenuFlyoutItem item;
        item.Text(label);
        TrayMenuStyle::Item(item);
        if (icon)
            item.Icon(Glyph(icon, 16));
        item.Click([state = impl.get(), life = impl->alive](auto const &, auto const &) noexcept {
            if (!*life)
                return;
            try
            {
                state->Invoke(Action::Settings);
            }
            catch (...)
            {
            }
        });
        menu.Items().Append(item);
    };
    if (part == TrayPart::Volume)
    {
        add(L"Troubleshoot sound problems", 0);
        menu.Items().Append(TrayMenuStyle::Separator(impl->model.light));
        add(L"Open volume mixer", L'\xE713');
        add(L"Sound settings", L'\xE713');
    }
    else if (part == TrayPart::Network)
    {
        add(L"Diagnose network problems", L'\xE90F');
        add(L"Perform speed test", L'\xEC4A');
        menu.Items().Append(TrayMenuStyle::Separator(impl->model.light));
        add(L"Network and Internet settings", L'\xE713');
    }
    else if (part == TrayPart::Battery)
        add(L"Power and sleep settings", L'\xE713');
    else
    {
        add(L"Adjust date and time", L'\xE713');
        add(L"Notifications settings", L'\xE713');
    }
    impl->flyout.Theme(impl->model.light);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &monitor);
    const auto dpi = impl->DpiAt(anchor);
    // All tray-widget menus use the same gap, including the battery menu.
    // The pointer-dependent battery placement in Windows is intentionally not copied.
    anchor.y = monitor.rcWork.bottom - MulDiv(8, dpi, 96);
    const int width = part == TrayPart::Clock ? 190 : part == TrayPart::Battery ? 212 : 244;
    anchor.x = std::min(anchor.x, monitor.rcWork.right - MulDiv(width / 2 + 12, dpi, 96));
    impl->flyout.ShowMenu(menu, anchor);
}
} // namespace ShellPanels
