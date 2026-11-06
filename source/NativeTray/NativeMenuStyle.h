#pragma once
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>

namespace TrayMenuStyle
{
inline winrt::Windows::UI::Xaml::ResourceDictionary Colors()
{
    namespace x=winrt::Windows::UI::Xaml;
    auto brush=[](uint32_t argb){return x::Media::SolidColorBrush(winrt::Windows::UI::Color{
        static_cast<uint8_t>(argb>>24),static_cast<uint8_t>(argb>>16),static_cast<uint8_t>(argb>>8),static_cast<uint8_t>(argb)});};
    x::ResourceDictionary resources;
    for(int theme=0;theme<3;++theme){
        x::ResourceDictionary values;
        const bool light=theme==0,contrast=theme==2;
        auto system=[](int index){const auto c=GetSysColor(index);return 0xFF000000u|(static_cast<uint32_t>(GetRValue(c))<<16)|
            (static_cast<uint32_t>(GetGValue(c))<<8)|GetBValue(c);};
        const auto normalText=brush(contrast ? system(COLOR_WINDOWTEXT) : light ? 0xE4000000 : 0xFFFFFFFF);
        const auto selectedText=brush(contrast ? system(COLOR_HIGHLIGHTTEXT) : light ? 0xE4000000 : 0xFFFFFFFF);
        // Installed WinUI theme: SubtleFillColorSecondary / Tertiary.
        const auto hover=brush(contrast ? system(COLOR_HIGHLIGHT) : light ? 0x09000000 : 0x0FFFFFFF);
        const auto pressed=brush(contrast ? system(COLOR_HIGHLIGHT) : light ? 0x06000000 : 0x0AFFFFFF);
        for(auto key:{L"SystemControlHighlightListLowBrush",L"SystemControlHighlightListAccentLowBrush",L"SystemControlHighlightListAccentHighBrush"})
            values.Insert(winrt::box_value(key),hover);
        for(auto key:{L"SystemControlHighlightListMediumBrush",L"SystemControlHighlightListAccentMediumBrush"})
            values.Insert(winrt::box_value(key),pressed);
        for(auto prefix:{L"MenuFlyoutItem",L"ToggleMenuFlyoutItem",L"MenuFlyoutSubItem"}){
            auto set=[&](wchar_t const* suffix,auto const& value){values.Insert(winrt::box_value(std::wstring(prefix)+suffix),value);};
            set(L"BackgroundPointerOver",hover);set(L"BackgroundPressed",pressed);set(L"BackgroundSubMenuOpened",hover);
            set(L"Foreground",normalText);set(L"ForegroundPointerOver",selectedText);set(L"ForegroundPressed",selectedText);set(L"ForegroundSubMenuOpened",selectedText);
        }
        resources.ThemeDictionaries().Insert(winrt::box_value(theme==0 ? L"Light" : theme==1 ? L"Default" : L"HighContrast"),values);
    }
    return resources;
}
inline winrt::Windows::UI::Xaml::Style Presenter(bool light=true)
{
    namespace x=winrt::Windows::UI::Xaml;
    auto base=x::Markup::XamlReader::Load(LR"XAML(
<Style xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
       xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" TargetType="MenuFlyoutPresenter">
 <Setter Property="CornerRadius" Value="8"/>
 <Setter Property="Padding" Value="4,2,4,2"/>
 <Setter Property="MinWidth" Value="160"/>
 <Setter Property="MaxWidth" Value="480"/>
 <Setter Property="FontSize" Value="14"/>
 <Setter Property="BorderThickness" Value="1"/>
 <Setter Property="BorderBrush" Value="{ThemeResource FlyoutBorderThemeBrush}"/>
 <Setter Property="IsDefaultShadowEnabled" Value="True"/>
 <Setter Property="ScrollViewer.HorizontalScrollMode" Value="Disabled"/>
 <Setter Property="ScrollViewer.HorizontalScrollBarVisibility" Value="Disabled"/>
 <Setter Property="ScrollViewer.VerticalScrollMode" Value="Auto"/>
 <Setter Property="ScrollViewer.VerticalScrollBarVisibility" Value="Auto"/>
 <Setter Property="Template">
  <Setter.Value>
   <ControlTemplate TargetType="MenuFlyoutPresenter">
    <Border Background="{TemplateBinding Background}" BorderBrush="{TemplateBinding BorderBrush}"
            BorderThickness="{TemplateBinding BorderThickness}" CornerRadius="{TemplateBinding CornerRadius}">
     <ScrollViewer x:Name="MenuFlyoutPresenterScrollViewer" Margin="{TemplateBinding Padding}"
                   MinWidth="{Binding RelativeSource={RelativeSource TemplatedParent},Path=TemplateSettings.FlyoutContentMinWidth}"
                   HorizontalScrollMode="Disabled" HorizontalScrollBarVisibility="Disabled"
                   VerticalScrollMode="Auto" VerticalScrollBarVisibility="Auto" ZoomMode="Disabled"
                   AutomationProperties.AccessibilityView="Raw">
      <ItemsPresenter/>
     </ScrollViewer>
    </Border>
   </ControlTemplate>
  </Setter.Value>
 </Setter>
</Style>)XAML").as<x::Style>();
    // The loaded style can be sealed; keep material overrides in a derived style.
    x::Style style;style.TargetType(base.TargetType());style.BasedOn(base);
    const bool contrast=winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
    x::Media::Brush background{nullptr};
    if(contrast){
        const auto color=GetSysColor(COLOR_WINDOW);
        background=x::Media::SolidColorBrush(winrt::Windows::UI::Color{255,GetRValue(color),GetGValue(color),GetBValue(color)});
        const auto stroke=GetSysColor(COLOR_WINDOWTEXT);
        x::Setter border;border.Property(x::Controls::Control::BorderBrushProperty());
        border.Value(x::Media::SolidColorBrush(winrt::Windows::UI::Color{255,GetRValue(stroke),GetGValue(stroke),GetBValue(stroke)}));style.Setters().Append(border);
    }else{
        x::Media::AcrylicBrush acrylic;acrylic.BackgroundSource(x::Media::AcrylicBackgroundSource::HostBackdrop);
        acrylic.TintColor(light?winrt::Windows::UI::Color{255,252,252,252}:winrt::Windows::UI::Color{255,44,44,44});
        acrylic.TintOpacity(light?0.0:0.15);
        acrylic.TintLuminosityOpacity(winrt::box_value(light?0.85:0.96).as<winrt::Windows::Foundation::IReference<double>>());
        acrylic.FallbackColor(light?winrt::Windows::UI::Color{255,249,249,249}:winrt::Windows::UI::Color{255,44,44,44});
        background=acrylic;
    }
    x::Setter fill;fill.Property(x::Controls::Control::BackgroundProperty());fill.Value(background);style.Setters().Append(fill);
    return style;
}

inline void TemplateColors(winrt::Windows::UI::Xaml::DependencyObject const& node,
    winrt::Windows::UI::Xaml::ResourceDictionary const& colors,wchar_t const* prefix,int depth=0)
{
    namespace x=winrt::Windows::UI::Xaml;
    if(depth>8)return;
    if(auto element=node.try_as<x::FrameworkElement>())for(auto const& group:x::VisualStateManager::GetVisualStateGroups(element))for(auto const& state:group.States()){
        const auto name=std::wstring(state.Name());
        if(name!=L"PointerOver" && name!=L"Pressed" && name!=L"SubMenuOpened")continue;
        const auto suffix=name==L"Pressed" ? L"Pressed" : name==L"SubMenuOpened" ? L"SubMenuOpened" : L"PointerOver";
        auto replace=[&](std::wstring const& property,auto const& set){
            if(property.find(L"Background")!=std::wstring::npos)set(colors.Lookup(winrt::box_value(std::wstring(prefix)+L"Background"+suffix)));
            else if(property.find(L"Foreground")!=std::wstring::npos)set(colors.Lookup(winrt::box_value(std::wstring(prefix)+L"Foreground"+suffix)));
        };
        for(auto const& value:state.Setters())if(auto setter=value.try_as<x::Setter>())
            replace(std::wstring(setter.Target().Path().Path()),[&](auto const& brush){setter.Value(brush);});
        if(auto story=state.Storyboard())for(auto const& timeline:story.Children())
            if(auto animation=timeline.try_as<x::Media::Animation::ObjectAnimationUsingKeyFrames>())
                for(auto const& frame:animation.KeyFrames())replace(std::wstring(x::Media::Animation::Storyboard::GetTargetProperty(animation)),[&](auto const& brush){frame.Value(brush);});
    }
    for(int i=0;i<x::Media::VisualTreeHelper::GetChildrenCount(node);++i)TemplateColors(x::Media::VisualTreeHelper::GetChild(node,i),colors,prefix,depth+1);
}

inline bool TemplatePadding(winrt::Windows::UI::Xaml::DependencyObject const& node,
    winrt::Windows::UI::Xaml::Thickness const& padding,int depth=0)
{
    namespace x=winrt::Windows::UI::Xaml;
    if(depth>8)return false;
    bool active=false;
    if(auto element=node.try_as<x::FrameworkElement>())for(auto const& group:x::VisualStateManager::GetVisualStateGroups(element))for(auto const& state:group.States()){
        if(state.Name()!=L"NarrowPadding")continue;
        if(auto current=group.CurrentState();current&&current.Name()==state.Name())active=true;
        for(auto const& value:state.Setters())if(auto setter=value.try_as<x::Setter>())
            if(setter.Target().Path().Path()==L"Padding")setter.Value(winrt::box_value(padding));
        if(auto story=state.Storyboard())for(auto const& timeline:story.Children())
            if(auto animation=timeline.try_as<x::Media::Animation::ObjectAnimationUsingKeyFrames>())
                if(x::Media::Animation::Storyboard::GetTargetProperty(animation)==L"Padding")
                    for(auto const& frame:animation.KeyFrames())frame.Value(winrt::box_value(padding));
    }
    for(int i=0;i<x::Media::VisualTreeHelper::GetChildrenCount(node);++i)
        if(TemplatePadding(x::Media::VisualTreeHelper::GetChild(node,i),padding,depth+1))active=true;
    return active;
}

inline void Item(winrt::Windows::UI::Xaml::Controls::Control const& item)
{
    // Keep the mouse menu's text inset and inter-item spacing separate, as in
    // the Windows 11 tray menus. Measure each row from its text, without a fixed minimum.
    item.FontSize(14);item.MinHeight(0);item.Padding({10,3,10,3});item.CornerRadius({4,4,4,4});
    item.Margin({0,2,0,2});
    item.Resources().Insert(winrt::box_value(L"MenuFlyoutItemThemePaddingNarrow"),winrt::box_value(item.Padding()));
    item.Resources().MergedDictionaries().Append(Colors());
    item.Loaded([](auto const& sender,auto const&) noexcept {try{
        namespace x=winrt::Windows::UI::Xaml;
        auto item=sender.template as<x::Controls::Control>();item.ApplyTemplate();
        // Mouse/keyboard NarrowPadding animates the template's LayoutRoot.Padding,
        // independently of Control.Padding. Refresh an already active animation.
        if(TemplatePadding(item,item.Padding())){
            x::VisualStateManager::GoToState(item,L"DefaultPadding",false);
            x::VisualStateManager::GoToState(item,L"NarrowPadding",false);
        }
        const bool contrast=winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
        if(contrast)return;
        const bool light=item.ActualTheme()!=x::ElementTheme::Dark;
        auto colors=Colors().ThemeDictionaries().Lookup(winrt::box_value(light ? L"Light" : L"Default")).as<x::ResourceDictionary>();
        const auto prefix=item.try_as<x::Controls::MenuFlyoutSubItem>() ? L"MenuFlyoutSubItem" : item.try_as<x::Controls::ToggleMenuFlyoutItem>() ? L"ToggleMenuFlyoutItem" : L"MenuFlyoutItem";
        TemplateColors(item,colors,prefix);
    }catch(...){}});
}

inline winrt::Windows::UI::Xaml::Controls::MenuFlyoutSeparator Separator(bool light)
{
    namespace x=winrt::Windows::UI::Xaml;
    x::Controls::MenuFlyoutSeparator item;
    item.Height(1);item.Margin({8,1,8,1});
    const bool contrast=winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
    const auto system=GetSysColor(COLOR_WINDOWTEXT);
    item.Background(x::Media::SolidColorBrush(contrast ? winrt::Windows::UI::Color{255,GetRValue(system),GetGValue(system),GetBValue(system)} :
        light ? winrt::Windows::UI::Color{15,0,0,0} : winrt::Windows::UI::Color{21,255,255,255}));
    item.Template(x::Markup::XamlReader::Load(LR"(<ControlTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" TargetType="MenuFlyoutSeparator"><Border Height="1" Background="{TemplateBinding Background}"/></ControlTemplate>)").as<x::Controls::ControlTemplate>());
    return item;
}
}
