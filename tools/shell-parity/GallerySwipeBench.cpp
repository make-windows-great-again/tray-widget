#include <windows.h>
#include <shellapi.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "NativeControls.h"
#ifndef BASELINE_GALLERY
#include "NativePageStripChecks.h"
#endif

namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
x::Hosting::DesktopWindowXamlSource island{nullptr};
c::Grid gallery{nullptr};
c::Border contentRoot{nullptr};
c::Pivot headers{nullptr};
c::ScrollViewer viewport{nullptr};
HWND window=nullptr,child=nullptr;
FILE* trace=nullptr;
FILE* events=nullptr;
ULONGLONG started=0;
void Pump(DWORD duration);
bool closing=false;
bool RunChecks(){
#ifdef BASELINE_GALLERY
    return false;
#else
    FILE* report=nullptr;_wfopen_s(&report,L"checks.txt",L"w");if(!report)return false;
    bool passed=true;unsigned checks=0;
    try{
        // The consumer may retain only the native tree, without C++ wrappers.
        KillTimer(window,1);gallery=nullptr;headers=nullptr;viewport=nullptr;Pump(60);
        gallery=contentRoot.Child().as<c::Grid>();
        headers=gallery.FindName(L"ControlPages").as<c::Pivot>();viewport=gallery.FindName(L"ControlPagesViewport").as<c::ScrollViewer>();SetTimer(window,1,16,nullptr);
        CheckPageStrip(gallery,Pump,[&](bool value,char const* message){
        std::fprintf(report,"%s %s\n",value?"PASS":"FAIL",message);++checks;
        if(!value)throw std::runtime_error(message);
        });
        const auto selected=headers.SelectedIndex();
        contentRoot.Child(nullptr);Pump(60);contentRoot.Child(gallery);Pump(200);
        if(headers.SelectedIndex()!=selected||std::abs(viewport.HorizontalOffset()-selected*viewport.ViewportWidth())>1)throw std::runtime_error("reload changed page");
        std::fputs("PASS unload/reload preserves page selection\n",report);++checks;
        // A short outer host must retain vertical bring-into-view, as NativeFlyout does.
        c::ScrollViewer outer;outer.HorizontalScrollMode(c::ScrollMode::Disabled);outer.HorizontalScrollBarVisibility(c::ScrollBarVisibility::Disabled);outer.VerticalScrollMode(c::ScrollMode::Enabled);outer.VerticalScrollBarVisibility(c::ScrollBarVisibility::Auto);outer.Height(180);
        contentRoot.Child(nullptr);outer.Content(gallery);contentRoot.Child(outer);Pump(250);
        headers.SelectedIndex(4);Pump(750);
        auto target=gallery.FindName(L"DialogAction").as<c::Button>();
        const auto rect=target.TransformToVisual(outer).TransformBounds({0,0,static_cast<float>(target.ActualWidth()),static_cast<float>(target.ActualHeight())});
        if(rect.Y+rect.Height<=outer.ViewportHeight())throw std::runtime_error("outer test target is already visible");
        target.StartBringIntoView();const auto until=GetTickCount64()+2500;
        do{Pump(20);}while(outer.VerticalOffset()<=0&&GetTickCount64()<until);
        if(outer.VerticalOffset()<=0||std::abs(viewport.HorizontalOffset()-4*viewport.ViewportWidth())>0.01){std::fprintf(report,"FAIL outer offset=%.3f scrollable=%.3f viewport=%.3f targetY=%.3f horizontal=%.3f pageWidth=%.3f\n",outer.VerticalOffset(),outer.ScrollableHeight(),outer.ViewportHeight(),rect.Y,viewport.HorizontalOffset(),viewport.ViewportWidth());throw std::runtime_error("outer vertical bring-into-view failed");}
        std::fputs("PASS outer vertical host scrolls without changing the page edge\n",report);++checks;
        outer.Content(nullptr);contentRoot.Child(gallery);Pump(200);
    }catch(...){std::fputs("FAIL gallery checks or reload\n",report);passed=false;}
    std::fprintf(report,"%s: %u checks at %u DPI\n",passed?"PASS":"FAIL",checks,GetDpiForWindow(window));
    fclose(report);return passed;
#endif
}
bool Swipe(double pages){
    auto failure=[](wchar_t const* stage){const DWORD error=GetLastError();FILE* file=nullptr;_wfopen_s(&file,L"input-errors.txt",L"a, ccs=UTF-8");if(file){fwprintf(file,L"%s error=%lu foreground=%p expected=%p\n",stage,error,GetForegroundWindow(),window);fclose(file);}return false;};
    if(!viewport||GetAncestor(GetForegroundWindow(),GA_ROOT)!=window)return failure(L"foreground");
    const double scale=GetDpiForWindow(window)/96.0;
    const auto origin=viewport.TransformToVisual(contentRoot).TransformPoint({0,0});
    POINT start{static_cast<LONG>(std::lround((origin.X+(pages>0?viewport.ActualWidth()-20:20))*scale)),static_cast<LONG>(std::lround((origin.Y+viewport.ActualHeight()/2)*scale))};
    ClientToScreen(window,&start);
    if(GetAncestor(WindowFromPoint(start),GA_ROOT)!=window)return failure(L"hit-test");
    const int left=GetSystemMetrics(SM_XVIRTUALSCREEN)+4,right=left+GetSystemMetrics(SM_CXVIRTUALSCREEN)-8;
    const int end=std::clamp(static_cast<int>(start.x)-static_cast<int>(std::lround(pages*viewport.ViewportWidth()*scale)),left,right);
    if(!InitializeTouchInjection(1,TOUCH_FEEDBACK_NONE))return failure(L"initialize");
    POINTER_TOUCH_INFO touch{};touch.pointerInfo.pointerType=PT_TOUCH;touch.pointerInfo.pointerId=0;
    touch.touchMask=TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;touch.orientation=90;touch.pressure=512;
    auto send=[&](int px,POINTER_FLAGS flags){
        touch.pointerInfo.ptPixelLocation={px,start.y};touch.pointerInfo.pointerFlags=flags;
        touch.rcContact={px-2,start.y-2,px+2,start.y+2};
        SetLastError(0);return InjectTouchInput(1,&touch)!=FALSE;
    };
    if(!send(start.x,POINTER_FLAG_DOWN|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT))return failure(L"down");
    int last=start.x;bool ok=true;
    for(int frame=1;frame<=120;++frame){
        Pump(8);last=start.x+(end-start.x)*frame/120;
        if(!send(last,POINTER_FLAG_UPDATE|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT)){ok=false;break;}
    }
    for(int frame=0;ok&&frame<20;++frame){Pump(8);ok=send(last,POINTER_FLAG_UPDATE|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT);}
    Pump(8);ok=send(last,ok?POINTER_FLAG_UP:POINTER_FLAG_UP|POINTER_FLAG_CANCELED)&&ok;Pump(1000);
    return ok?true:failure(L"move/up");
}

template<class T>T Named(x::DependencyObject const& node,wchar_t const* name){
    if(auto fe=node.try_as<x::FrameworkElement>();fe&&fe.Name()==name)if(auto result=node.try_as<T>())return result;
    for(int i=0;i<x::Media::VisualTreeHelper::GetChildrenCount(node);++i)
        if(auto result=Named<T>(x::Media::VisualTreeHelper::GetChild(node,i),name))return result;
    return nullptr;
}
bool Visible(x::FrameworkElement node){
    while(node){
        if(node.Visibility()!=x::Visibility::Visible||node.Opacity()==0)return false;
        node=x::Media::VisualTreeHelper::GetParent(node).try_as<x::FrameworkElement>();
    }
    return true;
}
unsigned RealizedPages(){
    if(auto strip=gallery.FindName(L"ControlPageStrip").try_as<c::StackPanel>()){
        unsigned count=0;for(auto const& item:strip.Children())if(Visible(item.as<x::FrameworkElement>()))++count;
        return count;
    }
    unsigned count=0;
    for(auto const& entry:headers.Items()){
        auto item=entry.as<c::PivotItem>();
        if(x::Media::VisualTreeHelper::GetChildrenCount(item)){
            auto content=x::Media::VisualTreeHelper::GetChild(item,0).as<x::FrameworkElement>();
            if(Visible(content))++count;
        }
    }
    return count;
}
void Snapshot(){
    if(trace&&viewport){
        double itemWidth=0,itemActual=0;
        if(auto panel=gallery.FindName(L"ControlPageStrip").try_as<c::StackPanel>();panel&&panel.Children().Size()){
            auto item=panel.Children().GetAt(0).as<x::FrameworkElement>();itemWidth=item.Width();itemActual=item.ActualWidth();
        }
        std::fprintf(trace,"%llu,%d,%.3f,%.3f,%.3f,%u,%.3f,%.3f\n",GetTickCount64()-started,headers.SelectedIndex(),viewport.HorizontalOffset(),viewport.ViewportWidth(),viewport.ScrollableWidth(),RealizedPages(),itemWidth,itemActual);
        std::fflush(trace);
    }
}
void Layout(){
    if(!island||!contentRoot)return;
    RECT client{};GetClientRect(window,&client);
    SetWindowPos(child,nullptr,0,0,client.right,client.bottom,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    const double scale=GetDpiForWindow(window)/96.0;
    contentRoot.Width(client.right/scale);contentRoot.Height(client.bottom/scale);
}
LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    try{
        if(message==WM_SIZE){Layout();return 0;}
        if(message==WM_SETFOCUS&&child){SetFocus(child);return 0;}
        if(message==WM_DPICHANGED){auto bounds=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,bounds->left,bounds->top,bounds->right-bounds->left,bounds->bottom-bounds->top,SWP_NOZORDER|SWP_NOACTIVATE);Layout();return 0;}
        if(message==WM_ERASEBKGND)return 1;
        if(message==WM_PAINT){PAINTSTRUCT p{};BeginPaint(hwnd,&p);EndPaint(hwnd,&p);return 0;}
        if(message==WM_APP+1&&headers){headers.SelectedIndex(static_cast<int>(wp));return 0;}
        if(message==WM_APP+2&&viewport){viewport.ChangeView(viewport.ViewportWidth()*static_cast<int>(wp)/1000.0,nullptr,nullptr,lp!=0);return 0;}
        if(message==WM_APP+3){Snapshot();return 0;}
        if(message==WM_APP+4&&viewport)return static_cast<LRESULT>(std::lround(viewport.HorizontalOffset()*1000));
        if(message==WM_APP+5&&viewport)return static_cast<LRESULT>(std::lround(viewport.ViewportWidth()*1000));
        if(message==WM_APP+6)return headers?headers.SelectedIndex():-1;
        if(message==WM_APP+7)return gallery?RealizedPages():0;
        if(message==WM_APP+8)return Swipe(static_cast<int>(wp)/1000.0)?1:0;
        if(message==WM_APP+9)return RunChecks()?1:0;
        if(message==WM_APP+10)return GetDpiForWindow(window);
        if(message==WM_APP+11){contentRoot.RequestedTheme(wp?x::ElementTheme::Dark:x::ElementTheme::Light);contentRoot.Background(x::Media::SolidColorBrush(wp?winrt::Windows::UI::Color{255,32,32,32}:winrt::Windows::UI::Color{255,245,245,245}));return 0;}
        if(message==WM_APP+12){SetFocus(child);const bool focused=gallery.FindName(L"TextInput").as<c::TextBox>().Focus(x::FocusState::Keyboard);return focused&&(GetFocus()==child||IsChild(child,GetFocus()))?1:0;}
        if(message==WM_APP+13){
            auto panel=gallery.FindName(L"ControlPageStrip").try_as<c::StackPanel>();if(!panel)return -1;
            for(auto node=x::Input::FocusManager::GetFocusedElement(gallery.XamlRoot()).try_as<x::DependencyObject>();node;node=x::Media::VisualTreeHelper::GetParent(node))
                for(unsigned index=0;index<panel.Children().Size();++index)if(node==panel.Children().GetAt(index))return index;
            return -1;
        }
        if(message==WM_TIMER){Snapshot();return 0;}
        if(message==WM_CLOSE){DestroyWindow(hwnd);return 0;}
        if(message==WM_DESTROY){closing=true;PostQuitMessage(0);return 0;}
    }catch(...){OutputDebugStringW(L"GallerySwipeBench callback failed\n");}
    return DefWindowProcW(hwnd,message,wp,lp);
}
void Pump(DWORD duration){
    auto until=GetTickCount64()+duration;
    do{
        MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){
            if(m.message==WM_QUIT){closing=true;return;}
            BOOL handled=FALSE;
            if(island)island.as<IDesktopWindowXamlSourceNative2>()->PreTranslateMessage(&m,&handled);
            if(!handled){TranslateMessage(&m);DispatchMessageW(&m);}
        }
        if(closing)return;Sleep(5);
    }while(GetTickCount64()<until);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int){
    int result=0;
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        const auto manager=x::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WindowProc;wc.lpszClassName=L"Yata.GallerySwipeBench";RegisterClassW(&wc);
        window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Gallery swipe test",WS_OVERLAPPEDWINDOW,950,50,700,1380,nullptr,nullptr,instance,nullptr);
        winrt::check_bool(window!=nullptr);
        island=x::Hosting::DesktopWindowXamlSource();auto native=island.as<IDesktopWindowXamlSourceNative>();winrt::check_hresult(native->AttachToWindow(window));winrt::check_hresult(native->get_WindowHandle(&child));
        gallery=NativeControls::Create();contentRoot=c::Border();contentRoot.Padding({24,24,24,24});contentRoot.Child(gallery);contentRoot.Background(x::Media::SolidColorBrush({255,245,245,245}));
        SendMessageW(window,WM_APP+11,std::wstring_view(command).find(L"--dark")!=std::wstring_view::npos,0);
        headers=gallery.FindName(L"ControlPages").as<c::Pivot>();island.Content(contentRoot);Layout();ShowWindow(window,SW_SHOW);SetForegroundWindow(window);Pump(900);
        viewport=Named<c::ScrollViewer>(gallery,L"ControlPagesViewport");
        if(!viewport)viewport=Named<c::ScrollViewer>(headers,L"ScrollViewer");
        winrt::check_bool(viewport!=nullptr);
        _wfopen_s(&events,L"events.txt",L"w");
        headers.SelectionChanged([](auto const&,auto const&){if(events){
            auto inner=Named<c::ScrollViewer>(headers,L"ScrollViewer");
            std::fprintf(events,"%llu selection=%d body=%.3f pivot=%.3f\n",GetTickCount64()-started,headers.SelectedIndex(),viewport.HorizontalOffset(),inner?inner.HorizontalOffset():-1);std::fflush(events);
        }});
        gallery.GettingFocus([](auto const&,auto const& args){if(events){
            auto next=args.NewFocusedElement().template try_as<x::FrameworkElement>();
            std::fprintf(events,"%llu focus selected=%d direction=%d state=%d cancel=%d target=%ls\n",GetTickCount64()-started,headers.SelectedIndex(),static_cast<int>(args.Direction()),static_cast<int>(args.FocusState()),args.Cancel(),next?next.Name().c_str():L"null");std::fflush(events);
        }});
        _wfopen_s(&trace,L"gallery-trace.csv",L"w");winrt::check_bool(trace!=nullptr);
        std::fputs("Ms,Selected,Offset,Viewport,Scrollable,RealizedPages,ItemWidth,ItemActual\n",trace);started=GetTickCount64();SetTimer(window,1,16,nullptr);Snapshot();
        while(!closing)Pump(100);
        island.Content(nullptr);island.Close();island=nullptr;contentRoot=nullptr;gallery=nullptr;headers=nullptr;viewport=nullptr;manager.Close();
    }catch(winrt::hresult_error const& error){
        if(error.code()!=E_ABORT){FILE* file=nullptr;_wfopen_s(&file,L"gallery-error.txt",L"a, ccs=UTF-8");if(file){fwprintf(file,L"0x%08X %s\n",static_cast<unsigned>(error.code().value),error.message().c_str());fclose(file);}result=1;}
    }
    if(events)fclose(events);if(trace)fclose(trace);
    return result;
}
