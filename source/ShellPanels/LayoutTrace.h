#pragma once
// Built into the diagnostic bench only. No telemetry/file writes in runtime.
#include <cstdio>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
namespace ShellPanels::detail
{
inline void WriteLayoutTrace(winrt::Windows::UI::Xaml::UIElement const &root, HWND owner)
{
    namespace x = winrt::Windows::UI::Xaml;
    namespace c = x::Controls;
    FILE *file = nullptr;
    if (fopen_s(&file, "layout-trace.csv", "wb") || !file)
        return;
    std::fprintf(file, "depth,type,name,id,x,y,w,h,desired_w,desired_h,height,margin_top,margin_bottom,rounding,scale,"
                       "screen_x,screen_y\n");
    POINT origin{};
    ClientToScreen(owner, &origin);
    const auto offset = root.TransformToVisual(root.XamlRoot().Content()).TransformPoint({0, 0});
    const double scale = root.XamlRoot().RasterizationScale();
    auto visit = [&](auto &&self, x::DependencyObject const &node, int depth) -> void {
        if (auto element = node.try_as<x::FrameworkElement>())
        {
            auto position = element.TransformToVisual(root).TransformPoint({0, 0});
            auto name = winrt::to_string(element.Name());
            auto id = winrt::to_string(x::Automation::AutomationProperties::GetAutomationId(element));
            auto type = winrt::to_string(winrt::get_class_name(element));
            auto margin = element.Margin();
            std::fprintf(file, "%d,%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%.4f,%.4f,%.4f\n", depth,
                         type.c_str(), name.c_str(), id.c_str(), position.X, position.Y, element.ActualWidth(),
                         element.ActualHeight(), element.DesiredSize().Width, element.DesiredSize().Height,
                         element.Height(), margin.Top, margin.Bottom, element.UseLayoutRounding(),
                         element.XamlRoot() ? element.XamlRoot().RasterizationScale() : 0,
                         origin.x + (offset.X + position.X) * scale, origin.y + (offset.Y + position.Y) * scale);
        }
        for (int i = 0; i < x::Media::VisualTreeHelper::GetChildrenCount(node); ++i)
            self(self, x::Media::VisualTreeHelper::GetChild(node, i), depth + 1);
    };
    try
    {
        visit(visit, root, 0);
    }
    catch (...)
    {
        std::fputs("trace_failed\n", file);
    }
    std::fclose(file);
}
} // namespace ShellPanels::detail
