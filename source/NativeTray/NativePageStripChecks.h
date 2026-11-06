#pragma once
#include "NativeControls.h"
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <stdexcept>
#include <cstdio>

// Shared by the isolated framework test and the recorded DPI gallery bench.
// These inspect real arranged XAML elements, not a second model of the pager.
template<class Pump,class Check>
void CheckPageStrip(winrt::Windows::UI::Xaml::Controls::Grid const& root,Pump pump,Check check){
    namespace x=winrt::Windows::UI::Xaml;
    namespace c=x::Controls;
    auto tabs=root.FindName(L"ControlPages").as<c::Pivot>();
    auto view=root.FindName(L"ControlPagesViewport").as<c::ScrollViewer>();
    auto panel=root.FindName(L"ControlPageStrip").as<c::StackPanel>();
    const auto originalIndex=tabs.SelectedIndex();
    const auto originalWidth=root.Width();
    const double epsilon=1.0/root.XamlRoot().RasterizationScale()+0.02;
    auto closeEnough=[epsilon](double a,double b){return std::abs(a-b)<=epsilon;};
    auto settled=[&](int index){
        auto aligned=[&]{return tabs.SelectedIndex()==index&&std::abs(view.HorizontalOffset()-index*view.ViewportWidth())<=0.01;};
        const auto until=GetTickCount64()+2500;
        do{pump(20);}while(!aligned()&&GetTickCount64()<until);
        if(!aligned()){
            char detail[200]{};std::snprintf(detail,sizeof(detail),"expected page %d; actual page %d offset %.4f width %.4f",index,tabs.SelectedIndex(),view.HorizontalOffset(),view.ViewportWidth());check(false,detail);
        }
        check(aligned(),"page selection and viewport settle on the exact page edge");
    };
    auto geometry=[&]{
        check(view.ViewportWidth()>0&&panel.Children().Size()==6,"finite viewport contains all six page slots");
        check(closeEnough(view.ScrollableWidth(),5*view.ViewportWidth()),"scroll extent stops at the first and last page");
        double end=0;
        for(auto const& entry:panel.Children()){
            auto page=entry.as<c::Border>();auto body=page.Child().as<x::FrameworkElement>();
            auto origin=page.TransformToVisual(panel).TransformPoint({0,0});
            check(page.IsLoaded()&&body.IsLoaded()&&body.ActualWidth()>0&&body.ActualHeight()>0&&body.Visibility()==x::Visibility::Visible,"page content stays realized and arranged");
            check(closeEnough(origin.X,end)&&closeEnough(page.ActualWidth(),view.ViewportWidth()),"page slots tile continuously without gaps or overlaps");
            check(page.Clip()!=nullptr,"each page clips content to its own slot");
            end=origin.X+page.ActualWidth();
        }
    };
    pump(120);geometry();
    check(!tabs.IsHeaderItemsCarouselEnabled(),"header carousel is disabled");
    check(view.HorizontalSnapPointsType()==c::SnapPointsType::Mandatory,"native snapping allows multiple pages per gesture");
    auto input=root.FindName(L"TextInput").as<c::TextBox>();const auto originalText=input.Text();input.Text(L"Persistent swipe state");
    // Cross multiple boundaries in both directions, sampling fractional offsets
    // before the release snap. A virtualized +/-1 implementation fails geometry.
    for(int direction:{1,-1}){
        for(int step=0;step<=20;++step){
            const double position=(direction>0?step:20-step)*0.25;
            view.ChangeView(position*view.ViewportWidth(),nullptr,nullptr,true);pump(24);
            geometry();
        }
    }
    settled(0);
    view.ChangeView(100*view.ViewportWidth(),nullptr,nullptr,true);settled(5);
    view.ChangeView(-100*view.ViewportWidth(),nullptr,nullptr,true);settled(0);
    for(int index:{4,1,5,0,2}){tabs.SelectedIndex(index);settled(index);}
    tabs.SelectedIndex(5);pump(30);tabs.SelectedIndex(1);pump(30);tabs.SelectedIndex(3);settled(3);
    check(input.Text()==L"Persistent swipe state","input state survives long and interrupted page changes");
    const auto arrangedWidth=root.ActualWidth();root.Width(arrangedWidth-37.3);pump(150);settled(3);geometry();
    root.Width(originalWidth);pump(150);settled(3);geometry();
    tabs.SelectedIndex(2);settled(2);
    const auto originalHeight=view.Height();view.Height(160);pump(150);
    auto vertical=panel.Children().GetAt(2).as<c::Border>().Child().as<c::ScrollViewer>();
    root.FindName(L"ListInput").as<c::ListBox>().StartBringIntoView();pump(200);
    check(vertical.VerticalOffset()>0,"inner vertical scroller still brings its controls into view");
    view.Height(originalHeight);pump(150);tabs.SelectedIndex(3);settled(3);
    auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(input);
    check(peer!=nullptr&&peer.IsOffscreen(),"offscreen input is reported offscreen to accessibility");
    input.Focus(x::FocusState::Programmatic);pump(50);
    check(x::Input::FocusManager::GetFocusedElement(root.XamlRoot())!=input,"inactive page cannot steal keyboard focus");
    input.StartBringIntoView();pump(150);
    settled(3);
    tabs.SelectedIndex(1);settled(1);
    check(!peer.IsOffscreen(),"selected page input is visible to accessibility");
    input.Text(originalText);tabs.SelectedIndex(originalIndex);settled(originalIndex);
}
