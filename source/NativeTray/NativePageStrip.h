#pragma once
#include <windows.h>
#undef GetCurrentTime
#include <algorithm>
#include <cmath>
#include <memory>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

namespace NativeControls {
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
// Pivot supplies the native header visuals and keyboard/Automation selection.
// Content lives in a finite native ScrollViewer: every page has an arranged slot,
// including pages crossed by a long direct manipulation before the finger lifts.
class PageStrip : public std::enable_shared_from_this<PageStrip> {
    // Resolve elements from the named root: weak refs to unnamed projected
    // wrappers may expire even while their native XAML objects stay in the tree.
    winrt::weak_ref<c::Grid> root;
    int pageCount=0;
    double pageWidth=0;
    int headerTarget=0;
    bool reflowing=false,syncingHeader=false,headerNavigation=false,layoutQueued=false;
    bool keyboardSelection=false,focusPageOnCompletion=false;
    struct Flag {
        bool& target;bool previous;
        explicit Flag(bool& value):target(value),previous(value){target=true;}
        ~Flag(){target=previous;}
    };
    int Clamp(int index) const {return std::clamp(index,0,pageCount-1);}
    int Index(double offset) const {return Clamp(static_cast<int>(std::floor(offset/pageWidth+0.5)));}
    int PageOf(c::Grid const& owner,x::DependencyObject node) const {
        auto panel=owner.FindName(L"ControlPageStrip").as<c::StackPanel>();
        for(;node;node=x::Media::VisualTreeHelper::GetParent(node))
            for(unsigned index=0;index<panel.Children().Size();++index)if(node==panel.Children().GetAt(index))return static_cast<int>(index);
        return -1;
    }
    void FocusChanging(x::Input::GettingFocusEventArgs const& args) noexcept {
        if(pageWidth<=0)return;
        try{if(auto owner=root.get()){
            const int index=PageOf(owner,args.NewFocusedElement());auto tabs=owner.FindName(L"ControlPages").as<c::Pivot>();
            if(index>=0&&tabs.SelectedIndex()!=index){
                // Tab can enter another page. A delayed focus restoration must
                // not activate or edit a page that is now outside the viewport.
                const bool tab=args.Direction()==x::Input::FocusNavigationDirection::Next||args.Direction()==x::Input::FocusNavigationDirection::Previous;
                if(reflowing||headerNavigation||!tab)args.TryCancel();
                else tabs.SelectedIndex(index);
            }
        }}catch(...){OutputDebugStringW(L"NativeControls: page focus synchronization failed\n");}
    }
    void BringRequested(x::BringIntoViewRequestedEventArgs const& args) noexcept {
        try{if(auto owner=root.get()){
            auto target=args.TargetElement();
            const int index=PageOf(owner,args.OriginalSource().try_as<x::DependencyObject>());
            auto tabs=owner.FindName(L"ControlPages").as<c::Pivot>();
            if(index>=0&&index!=tabs.SelectedIndex()){args.Handled(true);return;}
            auto view=owner.FindName(L"ControlPagesViewport").as<c::ScrollViewer>();
            // Resolve the horizontal part before ScrollViewer's class handler.
            // The remaining request targets the viewport itself, allowing outer
            // vertical hosts to bring the original vertical rectangle into view.
            auto rect=target.TransformToVisual(view).TransformBounds(args.TargetRect());
            rect.X=0;rect.Width=static_cast<float>(view.ViewportWidth());
            args.TargetElement(view);args.TargetRect(rect);args.HorizontalOffset(0);
        }}catch(...){args.Handled(true);OutputDebugStringW(L"NativeControls: bring-into-view adjustment failed\n");}
    }
    void Layout() noexcept {
        if(reflowing)return;
        try{
            auto owner=root.get();if(!owner||!pageCount)return;
            auto view=owner.FindName(L"ControlPagesViewport").as<c::ScrollViewer>();
            auto panel=owner.FindName(L"ControlPageStrip").as<c::StackPanel>();
            auto tabs=owner.FindName(L"ControlPages").as<c::Pivot>();
            const double width=view.ActualWidth(),height=view.ActualHeight();
            if(width<=0||height<=0)return;
            const int selected=Clamp(tabs.SelectedIndex());
            Flag guard(reflowing);
            pageWidth=width;
            for(auto const& child:panel.Children()){
                auto page=child.as<c::Border>();
                page.Width(width);
                auto clip=page.Clip().try_as<x::Media::RectangleGeometry>();
                if(!clip){clip=x::Media::RectangleGeometry();page.Clip(clip);}
                clip.Rect({0,0,static_cast<float>(width),static_cast<float>(height)});
            }
            panel.Height(height);
            view.UpdateLayout();
            headerNavigation=false;
            view.ChangeView(selected*pageWidth,nullptr,nullptr,true);
        }catch(...){OutputDebugStringW(L"NativeControls: page strip layout failed\n");}
    }
    void ScheduleLayout() noexcept {
        if(layoutQueued)return;
        try{if(auto owner=root.get()){
            layoutQueued=true;
            owner.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[weak=weak_from_this()](){
                if(auto self=weak.lock()){self->layoutQueued=false;self->Layout();}
            });
        }}catch(...){layoutQueued=false;OutputDebugStringW(L"NativeControls: page strip layout scheduling failed\n");}
    }
    void HeaderChanged() noexcept {
        if(syncingHeader||reflowing||pageWidth<=0)return;
        try{
            auto owner=root.get();if(!owner)return;
            auto tabs=owner.FindName(L"ControlPages").as<c::Pivot>();auto view=owner.FindName(L"ControlPagesViewport").as<c::ScrollViewer>();
            headerTarget=Clamp(tabs.SelectedIndex());
            const double destination=headerTarget*pageWidth;
            focusPageOnCompletion=keyboardSelection;
            headerNavigation=true;
            if(!view.ChangeView(destination,nullptr,nullptr,false))headerNavigation=false;
        }catch(...){headerNavigation=false;OutputDebugStringW(L"NativeControls: page selection failed\n");}
    }
    void Scrolled(bool intermediate) noexcept {
        if(reflowing||pageWidth<=0||(headerNavigation&&intermediate))return;
        try{
            auto owner=root.get();if(!owner)return;
            auto tabs=owner.FindName(L"ControlPages").as<c::Pivot>();auto view=owner.FindName(L"ControlPagesViewport").as<c::ScrollViewer>();
            // A replaced animation can still deliver its final ViewChanged.
            // It must not complete or reverse a newer header request.
            if(headerNavigation&&std::abs(view.HorizontalOffset()-headerTarget*pageWidth)>0.5)return;
            const bool completedHeader=headerNavigation&&!intermediate;
            const bool keyboardFocus=completedHeader&&focusPageOnCompletion;
            if(!intermediate)headerNavigation=false;
            const int selected=completedHeader?headerTarget:Index(view.HorizontalOffset());
            if(tabs.SelectedIndex()!=selected){Flag guard(syncingHeader);tabs.SelectedIndex(selected);}
            // Animated ChangeView can finish a fraction of a DIP away from its
            // target (over one physical pixel at 250%). Complete header changes
            // on the exact edge; direct manipulation retains native snap physics.
            if(completedHeader&&std::abs(view.HorizontalOffset()-selected*pageWidth)>0.01)
                view.ChangeView(selected*pageWidth,nullptr,nullptr,true);
            const int focused=intermediate?-1:PageOf(owner,x::Input::FocusManager::GetFocusedElement(owner.XamlRoot()).try_as<x::DependencyObject>());
            if(keyboardFocus||(focused>=0&&focused!=selected)){
                focusPageOnCompletion=false;
                auto panel=owner.FindName(L"ControlPageStrip").as<c::StackPanel>();
                auto next=x::Input::FocusManager::FindFirstFocusableElement(panel.Children().GetAt(selected)).try_as<c::Control>();
                if(next)next.Focus(keyboardFocus?x::FocusState::Keyboard:x::FocusState::Pointer);
            }
        }catch(...){OutputDebugStringW(L"NativeControls: page header synchronization failed\n");}
    }
    void Key(x::Input::KeyRoutedEventArgs const& args) noexcept {
        using winrt::Windows::System::VirtualKey;
        if(args.Handled()||GetKeyState(VK_CONTROL)>=0)return;
        const auto key=args.Key();
        if(key!=VirtualKey::Tab&&key!=VirtualKey::PageUp&&key!=VirtualKey::PageDown)return;
        try{
            if(auto owner=root.get()){
                auto tabs=owner.FindName(L"ControlPages").as<c::Pivot>();
                const bool back=key==VirtualKey::PageUp||(key==VirtualKey::Tab&&GetKeyState(VK_SHIFT)<0);
                Flag keyboard(keyboardSelection);
                tabs.SelectedIndex(Clamp(tabs.SelectedIndex()+(back?-1:1)));args.Handled(true);
            }
        }catch(...){OutputDebugStringW(L"NativeControls: keyboard page selection failed\n");}
    }
  public:
    static void Attach(c::Grid const& root,c::Pivot const& tabs,c::ScrollViewer const& view,c::StackPanel const& panel){
        auto state=std::make_shared<PageStrip>();state->root=root;state->pageCount=static_cast<int>(tabs.Items().Size());
        for(auto const& value:tabs.Items()){
            auto item=value.as<c::PivotItem>();auto content=item.Content().as<x::UIElement>();item.Content(nullptr);
            c::Border page;page.Padding({12,0,12,0});page.Child(content);panel.Children().Append(page);
        }
        std::weak_ptr<PageStrip> weak=state;
        // The root owns the coordinator; all references back into its tree are weak.
        root.Loaded([state](auto const&,auto const&) noexcept {state->ScheduleLayout();});
        root.Unloaded([state](auto const&,auto const&) noexcept {state->headerNavigation=false;});
        root.PreviewKeyDown([weak](auto const&,auto const& args) noexcept {if(auto self=weak.lock())self->Key(args);});
        root.GettingFocus([weak](auto const&,auto const& args) noexcept {if(auto self=weak.lock())self->FocusChanging(args);});
        view.SizeChanged([weak](auto const&,auto const&) noexcept {if(auto self=weak.lock())self->ScheduleLayout();});
        view.DirectManipulationStarted([weak](auto const&,auto const&) noexcept {if(auto self=weak.lock())self->headerNavigation=false;});
        // This non-scrolling container receives the bubbling request before the
        // horizontal ScrollViewer, after any inner vertical scroller.
        panel.BringIntoViewRequested([weak](auto const&,auto const& args) noexcept {if(auto self=weak.lock())self->BringRequested(args);});
        view.ViewChanged([weak](auto const&,auto const& args) noexcept {if(auto self=weak.lock())self->Scrolled(args.IsIntermediate());});
        tabs.SelectionChanged([weak](auto const&,auto const&) noexcept {if(auto self=weak.lock())self->HeaderChanged();});
    }
};
}
