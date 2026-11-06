#define NATIVE_FLYOUT_SELFTEST
#include "../../source/NativeTray/NativeFlyout.h"
#include "../../source/NativeTray/NativeControls.h"
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include <cmath>

namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace media=x::Media;
std::filesystem::path audit_output;
std::ofstream audit_metrics;
NativeFlyout* audit_active=nullptr;
HWND audit_host=nullptr;
std::string audit_scenario;
std::set<void*> audit_enlarged;
std::string Utf8(std::wstring_view value){
    if(value.empty())return {};
    std::string result(static_cast<size_t>(WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr)),'\0');
    WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),static_cast<int>(result.size()),nullptr,nullptr);return result;
}
std::string Q(std::wstring_view text){
    std::string result="\"";for(char ch:Utf8(text)){if(ch=='"'||ch=='\\')result+='\\';if(ch=='\n')result+="\\n";else if(ch=='\r')result+="\\r";else if(ch=='\t')result+="\\t";else result+=ch;}return result+'"';
}
std::string Q(char const* text){return Q(winrt::to_hstring(text));}
std::string Number(double value){return std::isfinite(value)?std::to_string(value):"null";}
std::wstring Color(winrt::Windows::UI::Color value){wchar_t text[20]{};swprintf_s(text,L"#%02X%02X%02X%02X",value.A,value.R,value.G,value.B);return text;}
std::wstring Brush(media::Brush const& value){
    if(!value)return L"null";
    if(auto solid=value.try_as<media::SolidColorBrush>())return L"solid:"+Color(solid.Color());
    if(auto acrylic=value.try_as<media::AcrylicBrush>())return L"acrylic:tint="+Color(acrylic.TintColor())+L" tintOpacity="+std::to_wstring(acrylic.TintOpacity())+L" fallback="+Color(acrylic.FallbackColor())+L" alwaysFallback="+(acrylic.AlwaysUseFallback()?L"true":L"false");
    return std::wstring(winrt::get_class_name(value));
}
void Pump(DWORD milliseconds){
    const auto end=GetTickCount64()+milliseconds;
    do{MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))if(!audit_active||!audit_active->Process(message)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(4);}while(GetTickCount64()<end);
}
LRESULT CALLBACK Proc(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
    if(message==WM_PAINT){PAINTSTRUCT paint{};BeginPaint(window,&paint);EndPaint(window,&paint);return 0;}
    if(message==WM_ERASEBKGND)return 1;
    // The audit audit_host stays open during measurement; no input is sent to other apps.
    return DefWindowProcW(window,message,wp,lp);
}
CLSID PngEncoder(){
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> storage(size);
    auto encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());Gdiplus::GetImageEncoders(count,size,encoders);
    for(UINT i=0;i<count;++i)if(!wcscmp(encoders[i].MimeType,L"image/png"))return encoders[i].Clsid;
    winrt::throw_hresult(E_FAIL);
}
void Screenshot(HWND window,std::wstring const& name){
    RECT rect{};if(!GetWindowRect(window,&rect)||rect.right-rect.left<2||rect.bottom-rect.top<2)return;
    if(window==audit_host&&audit_active&&audit_active->Visible())rect=audit_active->SurfaceForTest();
    HDC screen=GetDC(nullptr);if(!screen)return;
    Gdiplus::Bitmap bitmap(rect.right-rect.left,rect.bottom-rect.top,PixelFormat24bppRGB);
    {Gdiplus::Graphics graphics(&bitmap);HDC target=graphics.GetHDC();BitBlt(target,0,0,rect.right-rect.left,rect.bottom-rect.top,screen,rect.left,rect.top,SRCCOPY|CAPTUREBLT);graphics.ReleaseHDC(target);}
    ReleaseDC(nullptr,screen);auto encoder=PngEncoder();bitmap.Save((audit_output/name).c_str(),&encoder,nullptr);
}
void Walk(x::DependencyObject const& node,x::UIElement const& root,std::string const& tree,int depth=0){
    if(depth>45)return;
    auto fe=node.try_as<x::FrameworkElement>();
    auto text=node.try_as<c::TextBlock>();auto control=node.try_as<c::Control>();auto border=node.try_as<c::Border>();
    auto type=std::wstring(winrt::get_class_name(node));
    const bool important=fe&&(text||control||(border&&(audit_scenario.rfind("nav-",0)==0||fe.Name()==L"Background"||fe.Name()==L"ListInputSurface"||x::Automation::AutomationProperties::GetAutomationId(fe)==L"VpnPanelSurface")));
    if(important){
        auto name=std::wstring(fe.Name());auto value=text?std::wstring(text.Text()):std::wstring(x::Automation::AutomationProperties::GetName(fe));
        if(value.size()>180)value.resize(180);
        winrt::Windows::Foundation::Rect bounds{};try{bounds=fe.TransformToVisual(root).TransformBounds({0,0,static_cast<float>(fe.ActualWidth()),static_cast<float>(fe.ActualHeight())});}catch(...){}
        const double size=text?text.FontSize():control?control.FontSize():0;
        const auto family=text?text.FontFamily().Source():control?control.FontFamily().Source():L"";
        media::Brush foreground=text?text.Foreground():control?control.Foreground():nullptr;
        media::Brush background=control?control.Background():border?border.Background():nullptr;
        const auto corners=control?control.CornerRadius():border?border.CornerRadius():x::CornerRadius{};
        auto scroll=node.try_as<c::ScrollViewer>();
        audit_metrics<<"{\"case\":"<<Q(audit_scenario.c_str())<<",\"tree\":"<<Q(tree.c_str())<<",\"type\":"<<Q(type)<<",\"name\":"<<Q(name)<<",\"text\":"<<Q(value)
            <<",\"w\":"<<fe.ActualWidth()<<",\"h\":"<<fe.ActualHeight()<<",\"desiredH\":"<<fe.DesiredSize().Height<<",\"x\":"<<bounds.X<<",\"y\":"<<bounds.Y
            <<",\"font\":"<<Q(family)<<",\"fontDip\":"<<size<<",\"theme\":"<<static_cast<int>(fe.ActualTheme())<<",\"requestedTheme\":"<<static_cast<int>(fe.RequestedTheme())
            <<",\"foreground\":"<<Q(Brush(foreground))<<",\"background\":"<<Q(Brush(background))<<",\"corner\":"<<corners.TopLeft
            <<",\"minH\":"<<fe.MinHeight()<<",\"extentH\":"<<Number(scroll?scroll.ExtentHeight():0)<<",\"viewportH\":"<<Number(scroll?scroll.ViewportHeight():0)
            <<",\"textScaling\":"<<(text?(text.IsTextScaleFactorEnabled()?"true":"false"):control?(control.IsTextScaleFactorEnabled()?"true":"false"):"null")<<"}\n";
    }
    for(int i=0;i<media::VisualTreeHelper::GetChildrenCount(node);++i)Walk(media::VisualTreeHelper::GetChild(node,i),root,tree,depth+1);
}
BOOL CALLBACK CaptureWindow(HWND window,LPARAM){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||!IsWindowVisible(window))return TRUE;
    wchar_t name[180]{};GetClassNameW(window,name,180);RECT r{};GetWindowRect(window,&r);
    if(r.right-r.left<2||r.bottom-r.top<2)return TRUE;
    auto file=winrt::to_hstring(audit_scenario)+L"-"+winrt::to_hstring(reinterpret_cast<uint64_t>(window))+L".png";
    Screenshot(window,std::wstring(file));
    RECT imageBounds=r;if(window==audit_host&&audit_active&&audit_active->Visible())imageBounds=audit_active->SurfaceForTest();
    audit_metrics<<"{\"case\":"<<Q(audit_scenario.c_str())<<",\"window\":true,\"class\":"<<Q(name)<<",\"file\":"<<Q(file)<<",\"dpi\":"<<GetDpiForWindow(window)<<",\"x\":"<<r.left<<",\"y\":"<<r.top<<",\"w\":"<<r.right-r.left<<",\"h\":"<<r.bottom-r.top
        <<",\"imageW\":"<<imageBounds.right-imageBounds.left<<",\"imageH\":"<<imageBounds.bottom-imageBounds.top<<"}\n";return TRUE;
}
void Capture(NativeFlyout& flyout,std::string const& label){
    audit_scenario=label;Pump(220);auto root=flyout.ContentRoot();root.UpdateLayout();Pump(50);
    audit_metrics<<"{\"case\":"<<Q(label.c_str())<<",\"environment\":true,\"rasterScale\":"<<root.XamlRoot().RasterizationScale()<<",\"textScale\":"<<winrt::Windows::UI::ViewManagement::UISettings().TextScaleFactor()<<",\"highContrast\":"<<(winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast()?"true":"false")<<"}\n";
    Walk(root,root,"audit_host");int n=0;
    for(auto const& popup:media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root.XamlRoot()))if(auto child=popup.Child())Walk(child,child,"popup"+std::to_string(n++));
    EnumWindows(CaptureWindow,0);audit_metrics.flush();
}
template<class T>T Find(x::DependencyObject const& root){
    if(auto item=root.try_as<T>())return item;
    for(int i=0;i<media::VisualTreeHelper::GetChildrenCount(root);++i)if(auto item=Find<T>(media::VisualTreeHelper::GetChild(root,i)))return item;
    return nullptr;
}
template<class T>T Named(x::DependencyObject const& node,wchar_t const* name){
    if(auto fe=node.try_as<x::FrameworkElement>();fe&&fe.Name()==name)if(auto found=node.try_as<T>())return found;
    for(int i=0;i<media::VisualTreeHelper::GetChildrenCount(node);++i)if(auto found=Named<T>(media::VisualTreeHelper::GetChild(node,i),name))return found;
    return nullptr;
}
void CaptureNavigation(NativeFlyout& flyout,c::NavigationView const& navigation,std::string const& label){
    Pump(300);auto split=Find<c::SplitView>(navigation);
    audit_metrics<<"{\"case\":"<<Q(label.c_str())<<",\"navigationState\":true,\"paneOpen\":"<<(navigation.IsPaneOpen()?"true":"false")
        <<",\"paneDisplayMode\":"<<static_cast<int>(navigation.PaneDisplayMode())<<",\"displayMode\":"<<static_cast<int>(navigation.DisplayMode())
        <<",\"openLength\":"<<navigation.OpenPaneLength()<<",\"splitMode\":"<<(split?static_cast<int>(split.DisplayMode()):-1)
        <<",\"paneBackground\":"<<Q(split?Brush(split.PaneBackground()):L"null")<<"}\n";
    Capture(flyout,label);
}
c::Button FindButton(x::DependencyObject const& root,wchar_t const* content){
    if(auto button=root.try_as<c::Button>();button&&winrt::unbox_value_or<winrt::hstring>(button.Content(),L"")==content)return button;
    for(int i=0;i<media::VisualTreeHelper::GetChildrenCount(root);++i)if(auto item=FindButton(media::VisualTreeHelper::GetChild(root,i),content))return item;
    return nullptr;
}
void Enlarge(x::DependencyObject const& root,double factor){
    if(auto text=root.try_as<c::TextBlock>();text&&audit_enlarged.insert(winrt::get_abi(text)).second)text.FontSize(text.FontSize()*factor);
    for(int i=0;i<media::VisualTreeHelper::GetChildrenCount(root);++i)Enlarge(media::VisualTreeHelper::GetChild(root,i),factor);
}
void CustomPalette(c::Grid const& root){
    auto add=[&](wchar_t const* key,winrt::Windows::UI::Color color){root.Resources().Insert(winrt::box_value(key),media::SolidColorBrush(color));};
    add(L"SystemControlAcrylicWindowBrush",{255,32,24,39});add(L"FlyoutBorderThemeBrush",{255,82,214,201});
    add(L"SystemControlForegroundBaseHighBrush",{255,232,219,255});add(L"SystemControlHighlightAccentBrush",{255,82,214,201});
}
int wmain(int argc,wchar_t** argv){
    if(argc<2)return 2;
    ULONG_PTR graphicsToken=0;Gdiplus::GdiplusStartupInput graphicsInput;Gdiplus::GdiplusStartup(&graphicsToken,&graphicsInput,nullptr);
    try{
        audit_output=argv[1];std::filesystem::create_directories(audit_output);audit_metrics.open(audit_output/L"probe.jsonl",std::ios::trunc);audit_metrics<<std::setprecision(8);
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"YataNativeFramework.VisualAudit";RegisterClassW(&wc);
        audit_host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"Yata — visual audit",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);winrt::check_bool(audit_host!=nullptr);
        {NativeFlyout flyout;audit_active=&flyout;flyout.Initialize(audit_host,[](bool){},[](UINT){});flyout.AnimationsForTest(false);
         MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(audit_host,MONITOR_DEFAULTTOPRIMARY),&monitor);
         POINT anchor{monitor.rcWork.left+500,monitor.rcWork.bottom-120};
         if(argc>2&&!wcscmp(argv[2],L"--navigation-only")){
            for(bool light:{true,false}){
                flyout.Theme(light);auto gallery=NativeControls::Create();flyout.SetContent(gallery,L"Navigation — visual audit",600);flyout.Show(anchor);
                gallery.FindName(L"ControlPages").as<c::Pivot>().SelectedIndex(5);Pump(500);
                auto navigation=gallery.FindName(L"NavigationInput").as<c::NavigationView>();
                const std::string prefix=light?"nav-light":"nav-dark";
                CaptureNavigation(flyout,navigation,prefix+"-initial");
                auto toggle=Named<c::Button>(navigation,L"TogglePaneButton");winrt::check_bool(toggle!=nullptr);
                auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(toggle);
                auto invoke=peer.GetPattern(x::Automation::Peers::PatternInterface::Invoke).as<x::Automation::Provider::IInvokeProvider>();
                for(int cycle=1;cycle<=3;++cycle){
                    invoke.Invoke();CaptureNavigation(flyout,navigation,prefix+"-toggle"+std::to_string(cycle)+"-first");
                    invoke.Invoke();CaptureNavigation(flyout,navigation,prefix+"-toggle"+std::to_string(cycle)+"-second");
                }
                for(int cycle=1;cycle<=2;++cycle){flyout.Hide();Pump(120);flyout.Show(anchor);CaptureNavigation(flyout,navigation,prefix+"-host"+std::to_string(cycle));}
                flyout.Hide();Pump(120);
            }
         }else if(argc>2&&!wcscmp(argv[2],L"--tray-only")){
            for(bool light:{true,false}){
                flyout.Theme(light);flyout.Menu(anchor,2,88);
                const std::string prefix=light?"tray-light":"tray-dark";
                Pump(220);
                c::MenuFlyoutPresenter menu{nullptr};
                for(auto const& popup:media::VisualTreeHelper::GetOpenPopupsForXamlRoot(flyout.ContentRoot().XamlRoot()))
                    if(auto child=popup.Child())if(auto found=Find<c::MenuFlyoutPresenter>(child)){menu=found;break;}
                winrt::check_bool(menu!=nullptr);
                for(auto const& value:menu.Items())if(!value.try_as<c::MenuFlyoutSeparator>())
                    winrt::check_bool(x::VisualStateManager::GoToState(value.as<c::Control>(),L"NarrowPadding",false));
                Capture(flyout,prefix);
                auto first=menu.Items().GetAt(0).as<c::MenuFlyoutItem>();first.FontSize(28);
                Capture(flyout,prefix+"-text200");first.FontSize(14);
                auto sub=menu.Items().GetAt(5).as<c::MenuFlyoutSubItem>();
                auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(sub);
                winrt::check_bool(peer!=nullptr);
                peer.GetPattern(x::Automation::Peers::PatternInterface::ExpandCollapse).as<x::Automation::Provider::IExpandCollapseProvider>().Expand();
                Capture(flyout,prefix+"-submenu");
                flyout.Hide();Pump(80);
            }
         }else if(argc>2&&!wcscmp(argv[2],L"--list-only")){
            for(bool light:{true,false}){
                flyout.Theme(light);
                auto gallery=NativeControls::Create();flyout.SetContent(gallery,L"Native controls — list verification",600);flyout.Show(anchor);
                gallery.FindName(L"ControlPages").as<c::Pivot>().SelectedIndex(2);Pump(180);
                const std::string prefix=light?"list-light":"list-dark";
                Capture(flyout,prefix);
                auto list=gallery.FindName(L"ListInput").as<c::ListBox>();
                for(double factor:{1.5,2.0}){
                    for(auto const& item:list.Items())Find<c::TextBlock>(item.as<c::ListBoxItem>()).FontSize(list.FontSize()*factor);
                    Capture(flyout,prefix+"-text"+std::to_string(static_cast<int>(100*factor)));
                }
                for(auto const& item:list.Items())Find<c::TextBlock>(item.as<c::ListBoxItem>()).ClearValue(c::TextBlock::FontSizeProperty());
                list.SelectedIndex(2);
                Capture(flyout,prefix+"-selected");
                flyout.Hide();Pump(80);
            }
         }else{
         for(int theme=0;theme<3;++theme){
            const std::string prefix=theme==0?"light":theme==1?"dark":"custom";
            flyout.Theme(theme==0);if(theme==2)CustomPalette(flyout.ContentRoot());
            auto gallery=NativeControls::Create();flyout.SetContent(gallery,L"Native controls — audit",600);flyout.Show(anchor);Pump(150);
            auto pages=gallery.FindName(L"ControlPages").as<c::Pivot>();
            for(int page=0;page<6;++page){pages.SelectedIndex(page);Pump(100);Capture(flyout,prefix+"-page"+std::to_string(page));}
            pages.SelectedIndex(2);Pump(100);auto combo=gallery.FindName(L"ComboInput").as<c::ComboBox>();combo.IsDropDownOpen(true);Capture(flyout,prefix+"-combo");
            flyout.Theme(theme!=0);Capture(flyout,prefix+"-open-combo-theme-flip");combo.IsDropDownOpen(false);flyout.Theme(theme==0);Pump(60);
            pages.SelectedIndex(4);Pump(100);auto button=FindButton(gallery,L"Всплывающее меню");winrt::check_bool(button!=nullptr);auto menu=button.Flyout().as<c::MenuFlyout>();menu.ShowAt(button);Capture(flyout,prefix+"-gallery-menu");
            try{auto sub=menu.Items().GetAt(menu.Items().Size()-1).as<c::MenuFlyoutSubItem>();auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(sub);peer.GetPattern(x::Automation::Peers::PatternInterface::ExpandCollapse).as<x::Automation::Provider::IExpandCollapseProvider>().Expand();Capture(flyout,prefix+"-gallery-submenu");}catch(...){}
            menu.Hide();Pump(70);
            auto ordinary=FindButton(gallery,L"Всплывающая панель");ordinary.Flyout().ShowAt(ordinary);Capture(flyout,prefix+"-flyout");ordinary.Flyout().Hide();
            auto dialogButton=gallery.FindName(L"DialogAction").as<c::Button>();
            auto dialogPeer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(dialogButton);
            dialogPeer.GetPattern(x::Automation::Peers::PatternInterface::Invoke).as<x::Automation::Provider::IInvokeProvider>().Invoke();Pump(100);Capture(flyout,prefix+"-dialog");
            auto dialog=Find<c::ContentDialog>(flyout.ContentRoot());
            if(!dialog)for(auto const& popup:media::VisualTreeHelper::GetOpenPopupsForXamlRoot(flyout.ContentRoot().XamlRoot()))if(auto child=popup.Child())if(auto found=Find<c::ContentDialog>(child)){dialog=found;break;}
            if(dialog)dialog.Hide();Pump(100);
            // Compare a theme change while an existing inner menu remains open.
            menu.ShowAt(button);Pump(120);flyout.Theme(theme!=0);Capture(flyout,prefix+"-open-menu-theme-flip");menu.Hide();flyout.Theme(theme==0);Pump(70);
            flyout.Menu(anchor,2,88);Capture(flyout,prefix+"-tray-menu");flyout.Hide();Pump(70);
            std::cout<<"Captured "<<prefix<<"\n";
         }
         // Width and text stress are explicitly synthetic; OS DPI is unchanged.
         flyout.ContentRoot().Resources().Clear();flyout.Theme(true);
         auto gallery=NativeControls::Create();flyout.SetContent(gallery,L"Layout stress — audit",600);flyout.Show(anchor);auto pages=gallery.FindName(L"ControlPages").as<c::Pivot>();pages.SelectedIndex(2);Pump(100);
         for(float width:{320.0f,480.0f,600.0f}){flyout.PreferredWidth(width);Capture(flyout,"width-"+std::to_string(static_cast<int>(width)));}
         Enlarge(gallery,1.5);Capture(flyout,"synthetic-text150");
         auto list=gallery.FindName(L"ListInput").as<c::ListBox>();auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(list);
         auto scroll=peer.GetPattern(x::Automation::Peers::PatternInterface::Scroll).try_as<x::Automation::Provider::IScrollProvider>();
         if(scroll){audit_metrics<<"{\"case\":\"list-scrollability\",\"verticallyScrollable\":"<<(scroll.VerticallyScrollable()?"true":"false")<<",\"viewSize\":"<<scroll.VerticalViewSize()<<"}\n";}
         }
         flyout.Close();audit_active=nullptr;}
        DestroyWindow(audit_host);audit_host=nullptr;winrt::uninit_apartment();audit_metrics.close();Gdiplus::GdiplusShutdown(graphicsToken);return 0;
    }catch(winrt::hresult_error const& error){std::wcerr<<L"HRESULT "<<std::hex<<static_cast<unsigned long>(error.code().value)<<L" "<<error.message().c_str()<<L"\n";}
    catch(std::exception const& error){std::cerr<<error.what()<<"\n";}
    audit_active=nullptr;if(audit_host)DestroyWindow(audit_host);Gdiplus::GdiplusShutdown(graphicsToken);return 1;
}
