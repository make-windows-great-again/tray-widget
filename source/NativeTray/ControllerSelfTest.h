#pragma once
// Includes real event bindings, window dispatch, process waits and cleanup.
unsigned legacyTicks=0;
LRESULT CALLBACK LegacyWindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
    if(message==WM_TIMER){++legacyTicks;return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
void VerifyEventController(){
    WNDCLASSW legacyClass{};legacyClass.lpfnWndProc=LegacyWindowProc;legacyClass.hInstance=GetModuleHandleW(nullptr);
    legacyClass.lpszClassName=L"YataVpn.NativeTray.Controller.v10";RegisterClassW(&legacyClass);
    HWND legacyWindow=CreateWindowExW(0,legacyClass.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,legacyClass.hInstance,nullptr);
    winrt::check_bool(legacyWindow&&SetTimer(legacyWindow,1,25,nullptr)!=0);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=NativeTray::WindowClass;
    RegisterClassW(&wc);
    HWND host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,NativeTray::WindowClass,L"Event controller verification",WS_POPUP,100,100,500,120,nullptr,nullptr,wc.hInstance,nullptr);
    winrt::check_bool(host!=nullptr);
    wux::Hosting::DesktopWindowXamlSource island;auto native=island.as<IDesktopWindowXamlSourceNative>();winrt::check_hresult(native->AttachToWindow(host));
    HWND child=nullptr;winrt::check_hresult(native->get_WindowHandle(&child));SetWindowPos(child,nullptr,0,0,500,120,SWP_NOZORDER|SWP_SHOWWINDOW);
    controls::Grid root;root.MinWidth(32);root.Height(48);
    controls::Border background;background.Name(L"BackgroundBorder");background.Opacity(0.75);root.Children().Append(background);
    controls::ContentPresenter presenter;presenter.Name(L"ContentPresenter");controls::Grid content;content.Name(L"ContentGrid");
    controls::TextBlock original;original.Text(L"original");content.Children().Append(original);presenter.Content(content);root.Children().Append(presenter);
    island.Content(root);ShowWindow(host,SW_SHOWNOACTIVATE);SetWindowPos(child,nullptr,0,0,500,120,SWP_NOZORDER|SWP_SHOWWINDOW);root.UpdateLayout();PumpFor(150);
    Require(root.IsLoaded(),"controller fixture uses a loaded real XAML owner");
    HANDLE mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(NativeTray::State),NativeTray::MappingName);winrt::check_bool(mapping!=nullptr);
    auto state=static_cast<NativeTray::State*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(NativeTray::State)));winrt::check_bool(state!=nullptr);ZeroMemory(state,sizeof(*state));
    state->magic=NativeTray::Magic;state->size=sizeof(*state);state->version=NativeTray::ProtocolVersion;state->appPid=GetCurrentProcessId();state->appWindow=reinterpret_cast<UINT_PTR>(host);state->session=771;
    NativeTray::Request wanted;wanted.mode=1;wanted.labelWidth=44;wanted.linesWidth=88;wanted.revision=1;NativeTray::Publish(*state,wanted);
    {
        Controller engine;engine.matchForTest=[root](auto const& candidate){return candidate==root;};
        auto send=[&]{NativeTray::Publish(*state,wanted);PostMessageW(engine.timerWindow,NativeTray::StateMessage(),771,0);PumpFor(150);};
        winrt::check_bool(engine.Connect(1001));engine.Add(1,root);PumpFor(250);
        Require(legacyTicks>0,"a new framework session leaves legacy application timers untouched");
        Require(state->applied==1&&state->acknowledgedRevision==1&&root.Width()==44,"event controller applies the initial request without polling");
        const auto idle=state->refreshes;PumpFor(200);
        Require(state->refreshes==idle&&state->pollingTimers==0,"idle controller performs zero periodic refreshes");
        wanted.mode=2;wanted.revision++;send();
        Require(state->applied==2&&state->acknowledgedRevision==wanted.revision&&root.Width()==88,"mode and target width apply as a single acknowledged snapshot");
        auto before=state->refreshes;
        for(int i=0;i<1000;++i)PostMessageW(engine.timerWindow,NativeTray::EventMessage(),1001,Chrome);
        PumpFor(100);Require(state->refreshes-before<=2,"one thousand queued visual events coalesce into bounded updates");
        wanted.panelOpen=1;wanted.panelPhase=2;wanted.revision++;send();
        Require(background.Opacity()==0&&engine.decoration.chrome.active,"open state activates native selected chrome through notifications");
        PostMessageW(engine.timerWindow,NativeTray::FaultMessage(),771,0);PumpFor(100);
        Require(state->lifecycle==NativeTray::Faulted&&state->lastError==E_FAIL&&state->applied==0,"an exception at the window boundary disables only the decorator");
        Require(background.Opacity()==0.75&&content.Children().GetAt(0)==original,"fault containment restores the original XAML content and chrome");
        before=state->refreshes;PostMessageW(engine.timerWindow,NativeTray::StateMessage(),771,0);PumpFor(100);
        Require(state->refreshes==before,"a fault does not enter a reapply loop");
        engine.Disconnect(false);
        Require(state->subscriptions==0&&state->tooltipTimer==0&&state->controllerWindow==0,"disconnect releases subscriptions, waits, timers and the session mapping");
        wanted.panelOpen=0;wanted.panelPhase=0;
        for(int cycle=0;cycle<20;++cycle){
            wanted.revision++;NativeTray::Publish(*state,wanted);winrt::check_bool(engine.Connect(2000+cycle));engine.Add(1,root);PumpFor(35);engine.Disconnect(false);
            Require(state->subscriptions==0&&background.Opacity()==0.75&&content.Children().GetAt(0)==original,"repeated attach/detach restores resources and original bindings");
        }
        wanted.revision++;NativeTray::Publish(*state,wanted);winrt::check_bool(engine.Connect(4000));engine.Add(1,root);PumpFor(80);
        island.Content(nullptr);PumpFor(80);
        Require(state->applied==0,"an unloaded owner releases its decoration without polling");
        island.Content(root);root.UpdateLayout();PumpFor(150);
        Require(state->applied==2&&root.Width()==88,"Loaded rediscovers the same owner after it re-enters the tree");
        HANDLE exited=CreateEventW(nullptr,TRUE,FALSE,nullptr);SetThreadpoolWait(engine.wait,exited,nullptr);SetEvent(exited);PumpFor(120);
        Require(state->lifecycle==NativeTray::Detached&&state->subscriptions==0,"a signalled owner wait schedules cleanup on the XAML thread");CloseHandle(exited);
        DestroyWindow(engine.timerWindow);engine.timerWindow=nullptr;
    }
    UnmapViewOfFile(state);CloseHandle(mapping);island.Close();DestroyWindow(host);
    KillTimer(legacyWindow,1);DestroyWindow(legacyWindow);
}
