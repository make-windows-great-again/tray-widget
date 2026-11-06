#pragma once
#include "../NativeTray/NativeFlyout.h"
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
namespace ShellPanels::detail
{
namespace x = winrt::Windows::UI::Xaml;
namespace c = x::Controls;
template <class F> void Visit(x::DependencyObject const &element, F const &action)
{
    action(element);
    const int count = x::Media::VisualTreeHelper::GetChildrenCount(element);
    for (int i = 0; i < count; ++i)
        Visit(x::Media::VisualTreeHelper::GetChild(element, i), action);
}
inline x::Controls::ControlTemplate ButtonTemplate()
{
    return x::Markup::XamlReader::
        Load(LR"(<ControlTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" TargetType="Button">
 <Grid>
  <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
   <VisualState x:Name="Normal"/>
   <VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="Overlay.Background" Value="{ThemeResource SystemControlHighlightListLowBrush}"/></VisualState.Setters></VisualState>
   <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="Overlay.Background" Value="{ThemeResource SystemControlHighlightListMediumBrush}"/><Setter Target="Content.Opacity" Value="0.65"/></VisualState.Setters></VisualState>
   <VisualState x:Name="Disabled"><VisualState.Setters><Setter Target="Content.Opacity" Value="0.36"/></VisualState.Setters></VisualState>
  </VisualStateGroup></VisualStateManager.VisualStateGroups>
  <Border Background="{TemplateBinding Background}" BorderBrush="{TemplateBinding BorderBrush}" BorderThickness="{TemplateBinding BorderThickness}" CornerRadius="{TemplateBinding CornerRadius}"/>
  <Border x:Name="Overlay" CornerRadius="{TemplateBinding CornerRadius}"/>
  <ContentPresenter x:Name="Content" Content="{TemplateBinding Content}" Padding="{TemplateBinding Padding}" HorizontalContentAlignment="{TemplateBinding HorizontalContentAlignment}" VerticalContentAlignment="{TemplateBinding VerticalContentAlignment}"/>
 </Grid></ControlTemplate>)")
            .as<c::ControlTemplate>();
}
inline void ButtonStyle(c::Button const &b)
{
    b.Template(ButtonTemplate());
}
inline c::ControlTemplate SliderTemplate(bool light)
{
    std::wstring markup =
        LR"(<ControlTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" TargetType="Slider">
 <Grid>
  <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates"><VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"/><VisualState x:Name="Pressed"/><VisualState x:Name="Disabled"><VisualState.Setters><Setter Target="SliderContainer.Opacity" Value="0.4"/></VisualState.Setters></VisualState></VisualStateGroup></VisualStateManager.VisualStateGroups>
  <Grid x:Name="SliderContainer" Height="44" Background="Transparent" Control.IsTemplateFocusTarget="True">
   <Grid x:Name="HorizontalTemplate">
    <Grid.ColumnDefinitions><ColumnDefinition Width="Auto"/><ColumnDefinition Width="Auto"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
    <Rectangle x:Name="HorizontalTrackRect" Grid.ColumnSpan="3" Fill="{TemplateBinding Background}" Height="4" RadiusX="2" RadiusY="2" VerticalAlignment="Center"/>
    <Rectangle x:Name="HorizontalDecreaseRect" Fill="{TemplateBinding Foreground}" Height="4" RadiusX="2" RadiusY="2" VerticalAlignment="Center"/>
    <Thumb x:Name="HorizontalThumb" Grid.Column="1" Width="20" Height="20" Background="{TemplateBinding Foreground}" VerticalAlignment="Center" AutomationProperties.AccessibilityView="Raw">
     <Thumb.Template><ControlTemplate TargetType="Thumb"><Grid>
      <VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
       <VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="Dot.Width" Value="12"/><Setter Target="Dot.Height" Value="12"/></VisualState.Setters></VisualState>
       <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="Dot.Width" Value="8"/><Setter Target="Dot.Height" Value="8"/></VisualState.Setters></VisualState>
       <VisualState x:Name="Disabled"/>
       <VisualStateGroup.Transitions><VisualTransition GeneratedDuration="0:0:0.1"/></VisualStateGroup.Transitions>
      </VisualStateGroup></VisualStateManager.VisualStateGroups>
      <Ellipse Fill="#FFFFFFFF" Stroke="#20000000" StrokeThickness="1"/>
      <Ellipse x:Name="Dot" Width="10" Height="10" Fill="{TemplateBinding Background}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
     </Grid></ControlTemplate></Thumb.Template>
    </Thumb>
   </Grid>
  </Grid>
 </Grid></ControlTemplate>)";
    if (!light)
    {
        auto at = markup.find(L"#FFFFFFFF");
        markup.replace(at, 9, L"#FF454545");
    }
    return x::Markup::XamlReader::Load(markup).as<c::ControlTemplate>();
}
inline void CalendarStyle(c::CalendarView const &view, bool light, x::Media::Brush const &accent)
{
    auto transparent = x::Media::SolidColorBrush({0, 0, 0, 0});
    auto text = x::Media::SolidColorBrush(light ? winrt::Windows::UI::Color{228, 0, 0, 0}
                                                : winrt::Windows::UI::Color{255, 255, 255, 255});
    view.HorizontalAlignment(x::HorizontalAlignment::Stretch);
    view.DayItemFontSize(14);
    view.DayItemFontWeight({400});
    view.MonthYearItemFontSize(14);
    view.CalendarItemCornerRadius({22, 22, 22, 22});
    view.CalendarItemBorderThickness({0, 0, 0, 0});
    view.CalendarItemBackground(transparent);
    view.Background(transparent);
    view.OutOfScopeBackground(transparent);
    view.CalendarItemBorderBrush(transparent);
    view.CalendarItemForeground(text);
    view.Foreground(text);
    view.TodayBackground(accent);
    view.TodayForeground(x::Media::SolidColorBrush({255, 255, 255, 255}));
    view.Loaded([](auto const &sender, auto const &) noexcept {
        try
        {
            auto calendar = sender.template as<c::CalendarView>();
            calendar.ApplyTemplate();
            calendar.BorderBrush(x::Media::SolidColorBrush({0, 0, 0, 0}));
            Visit(calendar, [](auto const &node) {
                if (auto b = node.template try_as<c::Button>())
                {
                    if (b.Name() == L"HeaderButton")
                    {
                        b.FontSize(14);
                        b.FontWeight({600});

                        b.Height(36);
                        b.VerticalAlignment(x::VerticalAlignment::Top);
                        b.Margin({6, 5, 6, 0});
                        b.Padding({8, 0, 0, 0});
                        auto header = x::Media::VisualTreeHelper::GetParent(b).as<c::Grid>();
                        header.ColumnDefinitions().GetAt(0).Width({1, x::GridUnitType::Star});
                        header.ColumnDefinitions().GetAt(1).Width({40, x::GridUnitType::Pixel});
                        header.ColumnDefinitions().GetAt(2).Width({40, x::GridUnitType::Pixel});
                        auto layout = x::Media::VisualTreeHelper::GetParent(header).as<c::Grid>();
                        layout.RowDefinitions().GetAt(0).Height({60, x::GridUnitType::Pixel});
                    }
                    if (b.Name() == L"PreviousButton" || b.Name() == L"NextButton")
                    {
                        b.FontSize(10);
                        b.Height(36);
                        b.Width(34.6667);
                        b.VerticalAlignment(x::VerticalAlignment::Top);
                        b.Margin({0, 5, 0, 0});
                        x::Shapes::Polygon icon;
                        icon.Width(8);
                        icon.Height(5);
                        const bool next = b.Name() == L"NextButton";
                        icon.Points().Append({0, next ? 0.f : 5.f});
                        icon.Points().Append({4, next ? 5.f : 0.f});
                        icon.Points().Append({8, next ? 0.f : 5.f});
                        icon.Fill(x::Media::SolidColorBrush({255, 128, 128, 128}));
                        b.Content(icon);
                    }
                }
                else if (auto grid = node.template try_as<c::Grid>())
                {
                    if (grid.Name() == L"MonthView")
                        grid.RowDefinitions().GetAt(0).Height({36, x::GridUnitType::Pixel});
                    if (grid.Name() == L"Views")
                    {
                        grid.Margin({1, 0, 1, 0});
                    }
                }
                else if (auto t = node.template try_as<c::TextBlock>())
                    if (std::wstring_view(t.Name()).starts_with(L"WeekDay"))
                    {
                        t.FontSize(12);
                        t.FontWeight({600});
                    }
            });
        }
        catch (...)
        {
            OutputDebugStringW(L"Calendar template does not match SDK profile\n");
        }
    });
}
} // namespace ShellPanels::detail
