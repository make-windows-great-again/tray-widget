#pragma once
#include "ShellPanels.h"
#include <cmath>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>

namespace ShellPanels::detail
{
namespace x = winrt::Windows::UI::Xaml;
namespace c = x::Controls;
namespace anim = x::Media::Animation;
// Layout remains real XAML so clipping, focus, UIA and hit testing agree with
// the visible cards. Height animation is intentionally dependent; no timers or
// per-frame reconstruction of controls/materials/shadows are used.
class CardStack : public std::enable_shared_from_this<CardStack>
{
    struct Section
    {
        CardSection spec;
        c::Grid viewport{nullptr};
        x::Media::RectangleGeometry clip{nullptr};
        anim::Storyboard motion{nullptr};
        winrt::event_token completed{}, size{};
    };
    c::Grid root;
    std::vector<Section> sections;

    static bool FocusHeader(x::DependencyObject const &node)
    {
        if (!node)
            return false;
        if (auto control = node.try_as<c::Control>(); control && control.IsTabStop() && control.IsEnabled() &&
                                                      control.Visibility() == x::Visibility::Visible &&
                                                      control.Focus(x::FocusState::Programmatic))
            return true;
        for (int i = 0; i < x::Media::VisualTreeHelper::GetChildrenCount(node); ++i)
            if (FocusHeader(x::Media::VisualTreeHelper::GetChild(node, i)))
                return true;
        return false;
    }

    void Stop(Section &section)
    {
        if (!section.motion)
            return;
        const double height = section.viewport.Height();
        section.motion.Completed(section.completed);
        section.motion.Stop();
        section.motion = nullptr;
        section.viewport.Height(height);
    }
    void Finish(size_t index)
    {
        auto &section = sections.at(index);
        Stop(section);
        section.viewport.Height(section.spec.expanded ? section.spec.bodyHeight : 0);
        section.viewport.Visibility(section.spec.expanded ? x::Visibility::Visible : x::Visibility::Collapsed);
        section.viewport.IsHitTestVisible(section.spec.expanded);
    }

  public:
    using Frame = std::function<c::Grid(x::UIElement const &)>;
    ~CardStack()
    {
        for (auto &section : sections)
        {
            try
            {
                Stop(section);
                section.viewport.SizeChanged(section.size);
            }
            catch (...)
            {
            }
        }
    }
    static void Validate(std::vector<CardSection> const &cards)
    {
        if (cards.empty())
            winrt::throw_hresult(E_INVALIDARG);
        for (auto const &card : cards)
            if (!std::isfinite(card.bodyHeight) || card.bodyHeight < 0 || (card.flexible && !card.expanded))
                winrt::throw_hresult(E_INVALIDARG);
    }
    void Build(std::vector<CardSection> cards, double height, Frame const &frame)
    {
        Validate(cards);
        root.Height(height);
        sections.reserve(cards.size());
        for (size_t i = 0; i < cards.size(); ++i)
        {
            auto &spec = cards[i];
            c::RowDefinition row;
            row.Height({1, spec.flexible ? x::GridUnitType::Star : x::GridUnitType::Auto});
            root.RowDefinitions().Append(row);
            c::Grid content;
            for (int j = 0; j < 3; ++j)
            {
                c::RowDefinition part;
                part.Height({1, j == 1 && spec.flexible ? x::GridUnitType::Star : x::GridUnitType::Auto});
                content.RowDefinitions().Append(part);
            }
            if (spec.header)
                content.Children().Append(spec.header);
            c::Grid viewport;
            x::Media::RectangleGeometry clip;
            viewport.Clip(clip);
            if (!spec.flexible)
                viewport.Height(spec.expanded ? spec.bodyHeight : 0);
            viewport.Visibility(spec.expanded ? x::Visibility::Visible : x::Visibility::Collapsed);
            if (spec.body)
            {
                if (!spec.flexible)
                {
                    auto body = spec.body.as<x::FrameworkElement>();
                    body.Height(spec.bodyHeight);
                    body.VerticalAlignment(x::VerticalAlignment::Top);
                }
                viewport.Children().Append(spec.body);
            }
            auto token = viewport.SizeChanged([clip](auto const &sender, auto const &) {
                auto element = sender.template as<x::FrameworkElement>();
                clip.Rect(
                    {0, 0, static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
            });
            c::Grid::SetRow(viewport, 1);
            content.Children().Append(viewport);
            if (spec.footer)
            {
                c::Grid::SetRow(spec.footer.as<x::FrameworkElement>(), 2);
                content.Children().Append(spec.footer);
            }
            auto card = frame(content);
            card.Margin({0, i ? 12.0 : 0.0, 0, 0});
            c::Grid::SetRow(card, static_cast<int>(i));
            root.Children().Append(card);
            Section section;
            section.spec = std::move(spec);
            section.viewport = viewport;
            section.clip = clip;
            section.size = token;
            sections.push_back(std::move(section));
        }
    }
    c::Grid Root() const
    {
        return root;
    }
    void Height(double height)
    {
        // Settle before work-area/DPI relayout, preserving the requested state.
        for (size_t i = 0; i < sections.size(); ++i)
            if (sections[i].motion)
                Finish(i);
        root.Height(std::max(1.0, height));
    }
    bool Expanded(size_t index) const
    {
        return sections.at(index).spec.expanded;
    }
    void BodyHeight(size_t index, double height)
    {
        auto &section = sections.at(index);
        if (section.spec.flexible || !std::isfinite(height) || height < 0)
            winrt::throw_hresult(E_INVALIDARG);
        Stop(section);
        section.spec.bodyHeight = height;
        if (section.spec.body)
            section.spec.body.as<x::FrameworkElement>().Height(height);
        Finish(index);
    }
    void Expand(size_t index, bool expanded, bool animate)
    {
        auto &section = sections.at(index);
        if (section.spec.flexible)
            winrt::throw_hresult(E_INVALIDARG);
        if (section.spec.expanded == expanded)
            return;
        const double from = section.viewport.Height();
        Stop(section);
        section.spec.expanded = expanded;
        if (!expanded)
        {
            // XAML Islands do not use the CoreWindow focus scope.
            auto focused =
                root.XamlRoot()
                    ? x::Input::FocusManager::GetFocusedElement(root.XamlRoot()).try_as<x::DependencyObject>()
                    : nullptr;
            for (auto node = focused; node; node = x::Media::VisualTreeHelper::GetParent(node))
                if (node == section.viewport)
                {
                    if (!FocusHeader(section.spec.header))
                        FocusHeader(section.spec.footer);
                    break;
                }
        }
        if (!animate || !root.IsLoaded() || !winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled() ||
            !anim::Timeline::AllowDependentAnimations())
        {
            Finish(index);
            return;
        }
        section.viewport.Visibility(x::Visibility::Visible);
        // A closing body must not retain focus or receive a click through the
        // clipping edge. Header/footer remain interactive, including reversal.
        section.viewport.IsHitTestVisible(false);
        anim::DoubleAnimationUsingKeyFrames change;
        change.EnableDependentAnimation(true);
        change.Duration(x::DurationHelper::FromTimeSpan(std::chrono::milliseconds(300)));
        anim::DiscreteDoubleKeyFrame start;
        start.KeyTime(anim::KeyTimeHelper::FromTimeSpan(std::chrono::milliseconds(0)));
        start.Value(from);
        change.KeyFrames().Append(start);
        anim::SplineDoubleKeyFrame end;
        end.KeyTime(anim::KeyTimeHelper::FromTimeSpan(std::chrono::milliseconds(300)));
        end.Value(expanded ? section.spec.bodyHeight : 0);
        anim::KeySpline spline;
        spline.ControlPoint1({0.1f, 0.9f});
        spline.ControlPoint2({0.2f, 1.0f});
        end.KeySpline(spline);
        change.KeyFrames().Append(end);
        anim::Storyboard::SetTarget(change, section.viewport);
        anim::Storyboard::SetTargetProperty(change, L"Height");
        section.motion = anim::Storyboard();
        section.motion.Children().Append(change);
        section.completed =
            section.motion.Completed([weak = weak_from_this(), index](auto const &, auto const &) noexcept {
                try
                {
                    if (auto self = weak.lock())
                        self->Finish(index);
                }
                catch (...)
                {
                    OutputDebugStringW(L"ShellPanels: section completion failed\n");
                }
            });
        section.motion.Begin();
    }
};
} // namespace ShellPanels::detail
