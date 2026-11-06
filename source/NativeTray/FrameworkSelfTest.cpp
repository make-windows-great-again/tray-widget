#define NATIVE_FLYOUT_SELFTEST
#include "NativeFlyout.h"
#include "NativeControls.h"
#include "NativePageStripChecks.h"
#include "SamplePanel.h"
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <cstdio>
#include <stdexcept>

namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
NativeFlyout* active=nullptr;
void Check(bool value,char const* message){if(!value)throw std::runtime_error(message);std::printf("PASS %s\n",message);}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
    if(message==WM_PAINT){PAINTSTRUCT paint{};BeginPaint(window,&paint);EndPaint(window,&paint);return 0;}
    if(message==WM_ERASEBKGND)return 1;
    return DefWindowProcW(window,message,wp,lp);
}
void Pump(DWORD milliseconds){
    auto until=GetTickCount64()+milliseconds;
    do{MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
        if(!active||!active->Process(message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }Sleep(5);}while(GetTickCount64()<until);
}
void Invoke(c::Button const& button){
    auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(button);
    Check(peer!=nullptr,"a real button exposes an AutomationPeer");
    peer.GetPattern(x::Automation::Peers::PatternInterface::Invoke).as<x::Automation::Provider::IInvokeProvider>().Invoke();
    Pump(60);
}
template<class T>T Find(x::DependencyObject const& node){
    if(auto found=node.try_as<T>())return found;
    for(int n=0;n<x::Media::VisualTreeHelper::GetChildrenCount(node);++n)
        if(auto found=Find<T>(x::Media::VisualTreeHelper::GetChild(node,n)))return found;
    return nullptr;
}
template<class T>T Named(x::DependencyObject const& node,wchar_t const* name){
    if(auto fe=node.try_as<x::FrameworkElement>();fe&&fe.Name()==name)if(auto value=node.try_as<T>())return value;
    for(int i=0;i<x::Media::VisualTreeHelper::GetChildrenCount(node);++i)
        if(auto value=Named<T>(x::Media::VisualTreeHelper::GetChild(node,i),name))return value;
    return nullptr;
}
c::ContentDialog Dialog(x::FrameworkElement const& root){
    if(auto found=Find<c::ContentDialog>(root))return found;
    for(auto const& popup:x::Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root.XamlRoot()))
        if(auto child=popup.Child())if(auto found=Find<c::ContentDialog>(child))return found;
    return nullptr;
}
bool RowsFit(c::ListBox const& list){
    auto viewport=Find<c::ScrollViewer>(list);
    if(!viewport||viewport.ActualHeight()<=0||viewport.ScrollableHeight()>0.5){
        std::printf("List viewport: loaded=%d height=%.2f scrollable=%.2f\n",list.IsLoaded(),viewport?viewport.ActualHeight():0,viewport?viewport.ScrollableHeight():0);return false;
    }
    for(auto const& value:list.Items()){
        auto row=value.as<c::ListBoxItem>();
        if(!row.IsLoaded()||row.ActualHeight()<=0){std::printf("List row: loaded=%d height=%.2f\n",row.IsLoaded(),row.ActualHeight());return false;}
        auto bounds=row.TransformToVisual(viewport).TransformBounds({0,0,static_cast<float>(row.ActualWidth()),static_cast<float>(row.ActualHeight())});
        if(bounds.Y< -0.5||bounds.Y+bounds.Height>viewport.ActualHeight()+0.5){std::printf("List row: y=%.2f height=%.2f viewport=%.2f\n",bounds.Y,bounds.Height,viewport.ActualHeight());return false;}
    }
    return true;
}
int main(){
    HWND window=nullptr;
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"YataFramework.IsolatedTest";
        RegisterClassW(&wc);
        window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"Framework verification",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);
        winrt::check_bool(window!=nullptr);
        {
            NativeFlyout flyout;active=&flyout;
            unsigned changes=0;bool visible=false;
            flyout.Initialize(window,[&](bool value){visible=value;++changes;},[](UINT){});
            auto gallery=NativeControls::Create();
            flyout.SetContent(gallery,L"Native controls",600);flyout.Theme(true);
            MONITORINFO monitor{sizeof(monitor)};winrt::check_bool(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTOPRIMARY),&monitor));
            POINT anchor{monitor.rcWork.right-100,monitor.rcWork.bottom};
            flyout.Show(anchor);Pump(450);
            Check(visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Open,"arbitrary client content opens in a real transparent XAML island");
            Check(gallery.IsLoaded()&&gallery.ActualWidth()>0,"control gallery is loaded and arranged");
            auto pages=gallery.FindName(L"ControlPages").as<c::Pivot>();
            Check(pages.Items().Size()==6,"all six native control categories are instantiated");
            CheckPageStrip(gallery,Pump,Check);
            auto status=gallery.FindName(L"ActionStatus").as<c::TextBlock>();auto before=status.Text();
            Invoke(gallery.FindName(L"ActionButton").as<c::Button>());
            Check(status.Text()!=before,"UI Automation Invoke reaches the button action");
            auto toggle=gallery.FindName(L"SwitchAction").as<c::ToggleSwitch>();before=status.Text();toggle.IsOn(!toggle.IsOn());Pump(50);
            Check(status.Text()!=before,"native toggle events reach the client");
            pages.SelectedIndex(1);Pump(70);
            auto input=gallery.FindName(L"TextInput").as<c::TextBox>();
            Check(input.Focus(x::FocusState::Keyboard),"text input accepts keyboard focus");
            before=status.Text();input.Text(L"Keyboard input");Pump(40);
            Check(status.Text()!=before,"text input emits native change notifications");
            gallery.FindName(L"LevelInput").as<c::Slider>().Value(73);Pump(40);
            Check(gallery.FindName(L"LevelProgress").as<c::ProgressBar>().Value()==73,"slider events update the real progress control");
            pages.SelectedIndex(2);Pump(70);
            before=status.Text();gallery.FindName(L"ComboInput").as<c::ComboBox>().SelectedIndex(2);Pump(40);
            Check(status.Text()!=before,"native selection changes reach the client");
            auto list=gallery.FindName(L"ListInput").as<c::ListBox>();
            Pump(350);gallery.UpdateLayout();
            Check(RowsFit(list),"all three list rows fit without clipping or an inner scrollbar");
            auto normalHeight=list.ActualHeight();
            // Increase the actual text used by the native item template, without changing OS settings.
            for(auto const& value:list.Items())Find<c::TextBlock>(value.as<c::ListBoxItem>()).FontSize(28);
            Pump(80);
            Check(list.ActualHeight()>normalHeight&&RowsFit(list),"the list grows to fit doubled text instead of clipping the last row");
            for(auto const& value:list.Items())Find<c::TextBlock>(value.as<c::ListBoxItem>()).ClearValue(c::TextBlock::FontSizeProperty());
            Pump(80);
            Check(RowsFit(list),"restoring system typography keeps every row visible");
            before=status.Text();
            auto last=list.Items().GetAt(2).as<c::ListBoxItem>();
            list.SelectedIndex(2);Pump(40);
            Check(list.SelectedIndex()==2&&status.Text()!=before,"selecting the fully visible last row notifies the client");
            Check(last.Focus(x::FocusState::Keyboard),"the last list row accepts keyboard focus");
            auto focused=GetFocus();Check(focused!=nullptr,"native list keyboard focus reaches the island window");
            PostMessageW(focused,WM_KEYDOWN,VK_HOME,1);PostMessageW(focused,WM_KEYUP,VK_HOME,1);Pump(80);
            Check(list.SelectedIndex()==0,"Home selects the first row through native keyboard handling");
            for(bool light:{false,true}){
                flyout.Theme(light);Pump(60);
                Check(gallery.ActualTheme()==(light?x::ElementTheme::Light:x::ElementTheme::Dark),"client controls inherit the selected light or dark theme");
                Check(RowsFit(list),"all list rows remain visible after a live theme change");
            }
            pages.SelectedIndex(5);Pump(350);
            auto navigation=gallery.FindName(L"NavigationInput").as<c::NavigationView>();
            Check(!navigation.IsPaneOpen(),"compact navigation starts closed so its first opening runs the native transition");
            struct PaneEvents{unsigned opened=0,closed=0;};auto paneEvents=std::make_shared<PaneEvents>();
            const auto paneChanged=navigation.RegisterPropertyChangedCallback(c::NavigationView::IsPaneOpenProperty(),[paneEvents](auto const& sender,auto const&){
                if(sender.template as<c::NavigationView>().IsPaneOpen())++paneEvents->opened;else ++paneEvents->closed;
            });
            auto paneButton=Named<c::Button>(navigation,L"TogglePaneButton");Check(paneButton!=nullptr,"navigation exposes its native pane toggle");
            for(int cycle=0;cycle<3;++cycle){
                Invoke(paneButton);Pump(300);
                auto split=Find<c::SplitView>(navigation);
                Check(navigation.IsPaneOpen()&&split&&split.IsPaneOpen()&&split.DisplayMode()==c::SplitViewDisplayMode::CompactOverlay,"opening navigation preserves its compact overlay mode");
                Invoke(paneButton);Pump(300);
                Check(!navigation.IsPaneOpen()&&!split.IsPaneOpen(),"the native navigation toggle completes closing on every cycle");
            }
            navigation.UnregisterPropertyChangedCallback(c::NavigationView::IsPaneOpenProperty(),paneChanged);
            Check(paneEvents->opened==3&&paneEvents->closed==3,"first and repeated navigation toggles emit all native pane-state notifications");
            pages.SelectedIndex(4);Pump(80);
            Invoke(gallery.FindName(L"DialogAction").as<c::Button>());Pump(150);
            auto dialog=Dialog(gallery);
            Check(dialog!=nullptr,"ContentDialog is hosted in the island XamlRoot");
            // PreTranslate must not synchronously dismiss the entire host on Escape.
            HWND child=FindWindowExW(window,nullptr,nullptr,nullptr);
            MSG escape{};escape.hwnd=child;escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;
            flyout.Process(escape);
            Check(flyout.CurrentPhase()==NativeFlyout::Phase::Open,"Escape is offered to child controls before the flyout host");
            dialog.Hide();Pump(180);
            Check(visible,"closing a modal dialog preserves its owning panel");
            // Replacing content while a dialog is open must release its asynchronous UI.
            Invoke(gallery.FindName(L"DialogAction").as<c::Button>());Pump(100);
            auto custom=c::TextBox();custom.Header(winrt::box_value(L"Application content"));
            flyout.SetContent(custom,L"Custom application",420);Pump(180);
            Check(custom.IsLoaded()&&!gallery.IsLoaded(),"replacing client content unloads the old page and its dialog");
            flyout.PreferredWidth(100000);Pump(100);RECT bounds{};GetWindowRect(window,&bounds);
            Check(bounds.left>=monitor.rcWork.left&&bounds.right<=monitor.rcWork.right&&bounds.top>=monitor.rcWork.top&&bounds.bottom<=monitor.rcWork.bottom,"oversize content and shadow remain inside the monitor work area");
            flyout.Hide();Pump(260);Check(!visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Closed,"animated closing completes and clears visibility");
            flyout.AnimationsForTest(false);flyout.SetContent(TraySample::Create(),L"Legacy visual sample",362);
            flyout.Show(anchor);Pump(80);Check(visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Open,"disabled animations produce a completed opening");
            flyout.Hide();Check(!visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Closed,"disabled animations produce a completed closing");
            flyout.AnimationsForTest(true);flyout.Show(anchor);Pump(350);
            flyout.OwnerDeactivated(window);
            Check(visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Open,"same-process activation preserves the owning panel and its popups");
            flyout.OwnerDeactivated(GetShellWindow(),true);
            Check(visible,"the tray press is left to its release toggle without reopening the panel");
            flyout.OwnerDeactivated(GetShellWindow());
            Check(visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Closing&&IsWindowVisible(window),"external deactivation retains the surface until its exit completes");
            flyout.Toggle(anchor);
            Check(visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Closing,"a tray callback during exit cannot reverse the committed close");Pump(250);
            flyout.Show(anchor);Pump(20);
            flyout.OwnerDeactivated(GetShellWindow());
            Check(flyout.CurrentPhase()==NativeFlyout::Phase::Closing,"deactivation during opening interrupts the entrance with an exit");
            Pump(350);
            Check(!visible&&flyout.CurrentPhase()==NativeFlyout::Phase::Closed,"cancelled entrance completion cannot reopen a dismissed panel");
            Check(changes>=4,"visibility callbacks track complete panel cycles");
            flyout.Close();active=nullptr;
        }
        DestroyWindow(window);window=nullptr;winrt::uninit_apartment();
        std::puts("All isolated framework and native-control checks passed. Explorer was not modified.");return 0;
    }catch(winrt::hresult_error const& error){std::printf("FAIL HRESULT 0x%08lX: %ls\n",static_cast<unsigned long>(error.code().value),error.message().c_str());}
    catch(std::exception const& error){std::printf("FAIL %s\n",error.what());}
    active=nullptr;if(window)DestroyWindow(window);return 1;
}
