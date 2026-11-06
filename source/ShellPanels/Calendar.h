#pragma once
#include "ControlStyles.h"
#include <chrono>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
namespace ShellPanels::detail
{
// OS CalendarView supplies localization, keyboard navigation and UIA. Its Island
// day chrome ignores CornerRadius on this profile; composition rounds the chrome.
class Calendar : public std::enable_shared_from_this<Calendar>
{
    c::Grid root;
    c::CalendarView view;
    bool light;
    std::function<void(double)> changed;
    x::Media::SolidColorBrush accent;

    static void RoundDay(c::CalendarViewDayItem const &item)
    {
        auto visual = x::Hosting::ElementCompositionPreview::GetElementVisual(item);
        auto compositor = visual.Compositor();
        auto geometry = compositor.CreateRoundedRectangleGeometry();
        const float width = static_cast<float>(item.ActualWidth());
        const float height = static_cast<float>(item.ActualHeight());
        const float diameter = std::min(width, height);
        geometry.Size({diameter, diameter});
        geometry.Offset({(width - diameter) / 2, (height - diameter) / 2});
        geometry.CornerRadius({diameter / 2, diameter / 2});
        visual.Clip(compositor.CreateGeometricClip(geometry));
    }

  public:
    Calendar(bool isLight, x::Media::SolidColorBrush const &color, std::function<void(double)> action)
        : light(isLight), changed(std::move(action)), accent(color)
    {
    }
    double Resize(double availableHeight)
    {
        if (!root.IsLoaded())
            return 356;
        // Measured desktop profile: compact two-week calendar on short work areas.
        const int weeks = availableHeight < 720 ? 2 : 6;
        if (view.NumberOfWeeksInView() != weeks)
            view.NumberOfWeeksInView(weeks);
        root.Height(NAN);
        view.InvalidateMeasure();
        root.Measure({334, 10000});
        return root.DesiredSize().Height;
    }
    c::Grid Create(std::function<void()> ready)
    {
        root.Height(356);
        view.Name(L"CalendarControl");
        view.Margin({0, 5, 0, 0});
        view.FontFamily(x::Media::FontFamily(L"Segoe UI Variable"));
        view.DayItemFontFamily(view.FontFamily());
        view.MonthYearItemFontFamily(view.FontFamily());
        view.FirstOfMonthLabelFontFamily(view.FontFamily());
        view.FirstOfYearDecadeLabelFontFamily(view.FontFamily());
        view.IsGroupLabelVisible(false);
        view.SelectionMode(c::CalendarViewSelectionMode::Single);
        CalendarStyle(view, light, accent);
        view.Loaded([ready](auto const &, auto const &) { ready(); });
        view.VerticalAlignment(x::VerticalAlignment::Top);
        view.TodayFontWeight({400});

        view.OutOfScopeForeground(x::Media::SolidColorBrush(light ? winrt::Windows::UI::Color{110, 0, 0, 0}
                                                                  : winrt::Windows::UI::Color{110, 255, 255, 255}));
        view.TodayForeground(x::Media::SolidColorBrush(light ? winrt::Windows::UI::Color{255, 255, 255, 255}
                                                             : winrt::Windows::UI::Color{255, 0, 0, 0}));
        view.CalendarViewDayItemChanging(
            [](auto const &, c::CalendarViewDayItemChangingEventArgs const &args) noexcept {
                try
                {
                    if (!args.InRecycleQueue())
                    {
                        auto item = args.Item();

                        if (!item.Tag())
                        {
                            item.Tag(winrt::box_value(true));
                            item.SizeChanged([](auto const &sender, auto const &) noexcept {
                                try
                                {
                                    RoundDay(sender.template as<c::CalendarViewDayItem>());
                                }
                                catch (...)
                                {
                                    OutputDebugStringW(L"ShellPanels: day size clip failed\n");
                                }
                            });
                        }
                        RoundDay(item);
                    }
                }
                catch (...)
                {
                    OutputDebugStringW(L"ShellPanels: calendar day clip failed\n");
                }
            });
        view.SelectedDatesChanged([weak = weak_from_this()](auto const &sender, auto const &) {
            if (auto self = weak.lock(); self && self->changed && sender.SelectedDates().Size())
            {
                winrt::Windows::Globalization::Calendar calendar;
                calendar.SetDateTime(sender.SelectedDates().GetAt(0));
                auto day =
                    std::chrono::sys_days(std::chrono::year(calendar.Year()) / std::chrono::month(calendar.Month()) /
                                          std::chrono::day(calendar.Day()));
                self->changed(static_cast<double>(day.time_since_epoch().count()));
            }
        });
        root.Children().Append(view);
        return root;
    }
};
} // namespace ShellPanels::detail
