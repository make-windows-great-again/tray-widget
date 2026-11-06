#pragma once
// Demo application using the reusable native backend and command protocol.
class TrayApp {
public:
    ~TrayApp(){
        m_stopping=true;
        m_flyout.Close();
        if(m_nativeState){m_request.mode=NativeTray::Off;m_request.panelOpen=0;Publish();Detach();}
        if(m_trayRegistered){NOTIFYICONDATAW icon{sizeof(icon)};icon.hWnd=m_window;icon.uFlags=NIF_GUID;icon.guidItem=kTrayGuid;Shell_NotifyIconW(NIM_DELETE,&icon);}
        if(m_icon)DestroyIcon(m_icon);if(m_helper)CloseHandle(m_helper);
        if(m_nativeState){UnmapViewOfFile(m_nativeState);m_nativeState=nullptr;}if(m_nativeMapping)CloseHandle(m_nativeMapping);
    }
    bool Initialize(HINSTANCE instance,TrayCommands::Options const& options){
        m_standalone=options.standalone;
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,m_factory.GetAddressOf()));
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(m_writeFactory.GetAddressOf())));
        WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=kWindowClass;
        winrt::check_bool(RegisterClassExW(&wc)!=0);
        m_taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
        m_window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP,kWindowClass,L"Yata Native Framework",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,this);
        winrt::check_bool(m_window!=nullptr);m_dpi=GetDpiForWindow(m_window);
        m_request.mode=options.mode<0 ? NativeTray::Label:options.mode;m_request.labelWidth=NativeTray::ReadWidth(NativeTray::Label);m_request.linesWidth=NativeTray::ReadWidth(NativeTray::Lines);
        if(options.width>=0){winrt::check_bool(NativeTray::WriteWidth(m_request.mode,options.width));(m_request.mode==NativeTray::Lines?m_request.linesWidth:m_request.labelWidth)=options.width;}
        InitializeState();
        m_flyout.Initialize(m_window,[this](bool visible){m_request.panelOpen=visible?1:0;Publish();},
            [this](UINT command){PostMessageW(m_window,kCommandMessage,command,0);},
            [this](ShellPanels::Host::Phase phase){m_request.panelPhase=static_cast<LONG>(phase);Publish();});
        SetPanel(options.content<0?1:options.content);
        ApplyTheme();
        if(!m_standalone&&!UpdateTrayIcon(true))TrayConsole::Write("Shell notification icon unavailable; client window remains usable.\n");
        Publish();Attach();return true;
    }
    bool Process(MSG const& message){return m_flyout.Process(message);}
    void ShowPanel(){m_flyout.Show(AnchorPoint());}
    void SetPanel(int content){
        if(content==0)m_flyout.SetContent(TraySample::Create(),L"Скорость VPN",362);
        else if(content==1)m_flyout.SetContent(NativeControls::Create(),L"Yata Native Framework",600);
        else m_flyout.Select(static_cast<ShellPanels::Kind>(content-2));
    }
private:
    void InitializeState(){
        m_nativeMapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(NativeTray::State),NativeTray::MappingName);
        winrt::check_bool(m_nativeMapping!=nullptr);
        m_nativeState=static_cast<NativeTray::State*>(MapViewOfFile(m_nativeMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(NativeTray::State)));winrt::check_bool(m_nativeState!=nullptr);
        InterlockedExchange64(&m_nativeState->session,0);
        ZeroMemory(m_nativeState,sizeof(*m_nativeState));m_nativeState->magic=NativeTray::Magic;m_nativeState->size=sizeof(*m_nativeState);m_nativeState->version=NativeTray::ProtocolVersion;
        m_nativeState->appPid=GetCurrentProcessId();m_nativeState->appWindow=reinterpret_cast<UINT_PTR>(m_window);
        FILETIME now{};GetSystemTimeAsFileTime(&now);m_session=(static_cast<uint64_t>(now.dwHighDateTime)<<32)|now.dwLowDateTime;m_session^=static_cast<uint64_t>(GetCurrentProcessId())<<17;
        InterlockedExchange64(&m_nativeState->session,static_cast<LONG64>(m_session));
    }
    HWND Controller() const noexcept {
        if(!m_nativeState)return nullptr;
        HWND hwnd=reinterpret_cast<HWND>(static_cast<UINT_PTR>(NativeTray::Read64(m_nativeState->controllerWindow)));
        DWORD pid=0,shellPid=0;GetWindowThreadProcessId(hwnd,&pid);GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd",nullptr),&shellPid);
        wchar_t name[100]{};
        return pid&&pid==shellPid&&GetClassNameW(hwnd,name,100)&&!wcscmp(name,NativeTray::BrokerClass) ? hwnd:nullptr;
    }
    void Publish() noexcept {
        if(!m_nativeState)return;
        m_request.revision=m_request.revision==LONG_MAX?1:m_request.revision+1;NativeTray::Publish(*m_nativeState,m_request);
        if(HWND controller=Controller())PostMessageW(controller,NativeTray::StateMessage(),static_cast<WPARAM>(m_session),static_cast<LPARAM>(m_request.revision));
    }
    void Detach() noexcept {if(HWND controller=Controller())PostMessageW(controller,NativeTray::DetachMessage(),static_cast<WPARAM>(m_session),0);}
    void CancelAttachTimer() noexcept {if(m_attachTimer){KillTimer(m_window,m_attachTimer);m_attachTimer=0;}}
    void Attach() noexcept {
        try{
        if(m_request.mode==NativeTray::Off||m_stopping||m_connecting||m_standalone||!m_trayRegistered)return;
        m_connecting=true;CancelAttachTimer();m_attachTimer=++m_nextAttachTimer;
        winrt::check_bool(SetTimer(m_window,m_attachTimer,12000,nullptr)!=0);
        HWND broker=FindWindowExW(HWND_MESSAGE,nullptr,NativeTray::BrokerClass,nullptr);DWORD pid=0,shellPid=0;
        GetWindowThreadProcessId(broker,&pid);GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd",nullptr),&shellPid);
        if(pid&&pid==shellPid){winrt::check_bool(PostMessageW(broker,NativeTray::ReconnectMessage(),0,0)!=FALSE);return;}
        if(m_helper){if(WaitForSingleObject(m_helper,0)==WAIT_TIMEOUT)return;CloseHandle(m_helper);m_helper=nullptr;}
        wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
        std::wstring command=L"\""+std::wstring(executable)+L"\" --attach";STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        if(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)){CloseHandle(process.hThread);m_helper=process.hProcess;}
        else{m_connecting=false;CancelAttachTimer();InterlockedExchange(&m_nativeState->attachResult,HRESULT_FROM_WIN32(GetLastError()));}
        }catch(...){m_connecting=false;CancelAttachTimer();if(m_nativeState)InterlockedExchange(&m_nativeState->attachResult,winrt::to_hresult());}
    }
    bool UpdateTrayIcon(bool add){
        if(TrayEnvironment::Classify(TrayEnvironment::Inspect())!=TrayEnvironment::Availability::Ready){
            if(m_nativeState)InterlockedExchange(&m_nativeState->attachResult,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
            TrayEnvironment::Log(L"notify-icon-shell-unavailable",HRESULT_FROM_WIN32(ERROR_NOT_FOUND));return false;
        }
        HICON next=CreateTrayIcon(m_factory.Get(),m_writeFactory.Get(),m_dpi,UsesLightTheme(L"SystemUsesLightTheme"));
        if(!next){TrayEnvironment::Log(L"notify-icon-render",E_FAIL);return false;}
        NOTIFYICONDATAW data{sizeof(data)};data.hWnd=m_window;data.uID=1;data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_GUID;
        const bool native=m_nativeState&&m_nativeState->applied!=0;if(!native)data.uFlags|=NIF_SHOWTIP;
        data.uCallbackMessage=kTrayMessage;data.guidItem=kTrayGuid;data.hIcon=next;wcscpy_s(data.szTip,NativeTray::Tooltip);
        if(!Shell_NotifyIconW(add?NIM_ADD:NIM_MODIFY,&data)){
            DestroyIcon(next);
            // Shell_NotifyIcon does not document a GetLastError contract.
            if(m_nativeState)InterlockedExchange(&m_nativeState->attachResult,E_FAIL);
            TrayEnvironment::Log(add?L"notify-icon-add":L"notify-icon-modify",E_FAIL);
            return false;
        }
        if(add){if(m_nativeState)InterlockedExchange(&m_nativeState->attachResult,S_OK);TrayEnvironment::Log(L"notify-icon-add",S_OK);}
        m_trayRegistered=true;
        if(m_icon)DestroyIcon(m_icon);m_icon=next;m_nativeTooltip=native;
        if(add){data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);}return true;
    }
    void TooltipPolicy(){
        if(!m_trayRegistered)return;
        const bool native=m_nativeState&&m_nativeState->applied!=0;if(native==m_nativeTooltip)return;
        NOTIFYICONDATAW data{sizeof(data)};data.hWnd=m_window;data.uID=1;data.guidItem=kTrayGuid;data.uFlags=NIF_TIP|NIF_GUID;
        if(!native)data.uFlags|=NIF_SHOWTIP;wcscpy_s(data.szTip,NativeTray::Tooltip);
        if(Shell_NotifyIconW(NIM_MODIFY,&data))m_nativeTooltip=native;
    }
    void ApplyTheme(){
        m_flyout.Theme(UsesLightTheme(L"AppsUseLightTheme"));
        COLORREF border=DWMWA_COLOR_NONE;DwmSetWindowAttribute(m_window,DWMWA_BORDER_COLOR,&border,sizeof(border));
        BOOL disabled=TRUE;DwmSetWindowAttribute(m_window,DWMWA_TRANSITIONS_FORCEDISABLED,&disabled,sizeof(disabled));
        ++m_request.themeRevision;Publish();
    }
    RECT IconRect() const noexcept {
        NOTIFYICONIDENTIFIER identifier{sizeof(identifier)};identifier.hWnd=m_window;identifier.uID=1;identifier.guidItem=kTrayGuid;RECT r{};
        if(!m_trayRegistered||FAILED(Shell_NotifyIconGetRect(&identifier,&r))){
            POINT cursor{};GetPhysicalCursorPos(&cursor);MONITORINFO monitor{sizeof(monitor)};
            if(GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY),&monitor)){cursor.x=monitor.rcWork.right-12;cursor.y=monitor.rcWork.bottom;}
            r={cursor.x,cursor.y,cursor.x+1,cursor.y+1};
        }return r;
    }
    POINT AnchorPoint() const noexcept {auto r=IconRect();return {(r.left+r.right)/2,r.top};}
    bool ApplyCommand(TrayCommands::Packet const& command){
        if(!TrayCommands::Valid(command))return false;
        const LONG mode=command.mode<0?m_request.mode:command.mode;
        if(command.width>=0){if(!NativeTray::WriteWidth(mode,command.width))return false;(mode==NativeTray::Lines?m_request.linesWidth:m_request.labelWidth)=command.width;}
        m_request.mode=mode;Publish();
        if(mode==NativeTray::Off){m_connecting=false;CancelAttachTimer();Detach();}
        else if(!m_nativeState->applied)Attach();
        TooltipPolicy();
        if(command.fault)if(HWND controller=Controller())PostMessageW(controller,NativeTray::FaultMessage(),static_cast<WPARAM>(m_session),0);
        if(command.content>=0)SetPanel(command.content);
        if(command.show)ShowPanel();return true;
    }
    void HandleCommand(UINT command){
        if(command==1){ShowPanel();return;}if(command==2){DestroyWindow(m_window);return;}
        if(command==9){SetPanel(1);ShowPanel();return;}
        TrayCommands::Packet packet;
        if(command>=3&&command<=5)packet.mode=command==3?1:command==4?2:0;
        else if(command>=6&&command<=8){
            auto width=static_cast<UINT>(m_request.mode==NativeTray::Lines?m_request.linesWidth:m_request.labelWidth);
            if(!width)width=static_cast<UINT>(m_nativeState->actualWidth>0?m_nativeState->actualWidth:m_request.mode==NativeTray::Lines?104:44);
            packet.width=command==6?0:command==7?static_cast<int>(std::max(32u,width>8?width-8:32u)):static_cast<int>(std::min(320u,width+8));
        }else return;
        ApplyCommand(packet);
    }
    LRESULT HandleMessage(UINT message,WPARAM wp,LPARAM lp){
        if(message==m_taskbarCreated){InterlockedExchange(&m_nativeState->applied,0);if(!m_standalone)UpdateTrayIcon(true);m_connecting=false;Publish();Attach();return 0;}
        if(message==NativeTray::ReadyMessage()&&wp==m_session){Publish();return 0;}
        if(message==NativeTray::ReportMessage()&&wp==m_session){
            if(m_nativeState->applied||m_nativeState->lifecycle==NativeTray::Faulted||m_nativeState->lifecycle==NativeTray::Detached){m_connecting=false;CancelAttachTimer();if(m_helper){CloseHandle(m_helper);m_helper=nullptr;}}
            if(memcmp(&m_lastBounds,&m_nativeState->actualBounds,sizeof(RECT))){m_lastBounds=m_nativeState->actualBounds;if(m_flyout.Visible())m_flyout.Reposition(AnchorPoint());}
            TooltipPolicy();return 0;
        }
        if(message==kTrayMessage){switch(LOWORD(lp)){case NIN_SELECT:case NIN_KEYSELECT:m_flyout.Toggle(AnchorPoint());break;case WM_CONTEXTMENU:case WM_RBUTTONUP:{POINT cursor{};GetPhysicalCursorPos(&cursor);m_flyout.Menu(cursor,m_request.mode,static_cast<UINT>(m_request.mode==NativeTray::Lines?m_request.linesWidth:m_request.labelWidth));break;}}return 0;}
        if(message==kCommandMessage){HandleCommand(static_cast<UINT>(wp));return 0;}
        if(message==kShowMessage){ShowPanel();return 0;}
        if(message==kExitMessage){DestroyWindow(m_window);return 0;}
        if(message==NativeTray::ModeMessage()){TrayCommands::Packet command;command.mode=static_cast<int>(wp);return ApplyCommand(command)?1:0;}
        if(message==NativeTray::WidthMessage()){TrayCommands::Packet command;command.width=static_cast<int>(wp);return ApplyCommand(command)?1:0;}
        switch(message){
        case WM_COPYDATA:{auto data=reinterpret_cast<COPYDATASTRUCT const*>(lp);if(!data||data->dwData!=TrayCommands::CommandTag||data->cbData!=sizeof(TrayCommands::Packet)||!data->lpData)return 0;
            TrayCommands::Packet command{};memcpy(&command,data->lpData,sizeof(command));return ApplyCommand(command)?1:0;}
        case WM_TIMER:if(m_attachTimer&&wp==m_attachTimer){CancelAttachTimer();m_connecting=false;if(!m_nativeState->applied){InterlockedExchange(&m_nativeState->attachResult,HRESULT_FROM_WIN32(WAIT_TIMEOUT));Detach();TooltipPolicy();}}return 0;
        case WM_ERASEBKGND:return 1;
        case WM_NCHITTEST:return HTCLIENT;
        case WM_PAINT:{PAINTSTRUCT paint{};BeginPaint(m_window,&paint);EndPaint(m_window,&paint);return 0;}
        case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE){POINT cursor{};GetPhysicalCursorPos(&cursor);auto icon=IconRect();bool own=(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&PtInRect(&icon,cursor);m_flyout.OwnerDeactivated(reinterpret_cast<HWND>(lp),own);}return 0;
        case WM_CLOSE:m_flyout.Hide();return 0;
        case WM_KEYDOWN:if(wp==VK_ESCAPE)m_flyout.Hide();return 0;
        case WM_DPICHANGED:m_dpi=HIWORD(wp);if(m_trayRegistered)UpdateTrayIcon(false);if(m_flyout.Visible())m_flyout.Reposition(AnchorPoint());Publish();return 0;
        case WM_SETTINGCHANGE:case WM_THEMECHANGED:ApplyTheme();if(m_trayRegistered)UpdateTrayIcon(false);return 0;
        case WM_DISPLAYCHANGE:if(m_flyout.Visible())m_flyout.Reposition(AnchorPoint());Publish();return 0;
        case WM_DESTROY:m_stopping=true;m_request.mode=NativeTray::Off;Publish();Detach();CancelAttachTimer();m_flyout.Close();PostQuitMessage(0);return 0;
        }return DefWindowProcW(m_window,message,wp,lp);
    }
    static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
        auto app=reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE){app=static_cast<TrayApp*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);app->m_window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
        try{return app?app->HandleMessage(message,wp,lp):DefWindowProcW(window,message,wp,lp);}catch(...){if(app&&app->m_nativeState)InterlockedExchange(&app->m_nativeState->attachResult,winrt::to_hresult());return 0;}
    }
    HWND m_window=nullptr;HANDLE m_nativeMapping=nullptr,m_helper=nullptr;NativeTray::State* m_nativeState=nullptr;NativeTray::Request m_request{};uint64_t m_session=0;
    HICON m_icon=nullptr;UINT m_taskbarCreated=0,m_dpi=96;bool m_stopping=false,m_connecting=false,m_nativeTooltip=false;
    bool m_standalone=false,m_trayRegistered=false;
    UINT_PTR m_attachTimer=0,m_nextAttachTimer=100;RECT m_lastBounds{};
    ShellPanels::Host m_flyout;ComPtr<ID2D1Factory> m_factory;ComPtr<IDWriteFactory> m_writeFactory;
};
