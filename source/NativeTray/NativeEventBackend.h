#pragma once
#include "XamlConnections.h"

enum Dirty : unsigned { Discover=1, Geometry=2, Chrome=4, Tooltip=8, Theme=16, CancelTip=32, PointerSignal=64, All=31 };
struct Watcher;
struct Controller;
thread_local Controller* controller=nullptr;
std::mutex runtimeMutex;
winrt::com_ptr<IXamlDiagnostics> runtimeDiagnostics;
winrt::com_ptr<Watcher> runtimeWatcher;
bool runtimeWorker=false,runtimeWanted=false;
uint64_t runtimeGeneration=0,runtimeRevision=0;
std::atomic<uint64_t> nextGeneration{1};
std::atomic<uint64_t> desiredGeneration{0};
std::atomic<HWND> brokerWindow{nullptr};
void Subscription(bool wanted,uint64_t generation,winrt::com_ptr<IXamlDiagnostics> diagnostics={}) noexcept;

struct Controller {
    struct Candidate {InstanceHandle handle{};wux::FrameworkElement view{nullptr};std::unique_ptr<XamlConnections> events;bool waitingLoaded=false;};
    struct WaitContext {HWND window=nullptr;uint64_t generation=0;} waitContext;
    HANDLE mapping=nullptr,process=nullptr;
    PTP_WAIT wait=nullptr;
    HWINEVENTHOOK foreground=nullptr;
    HWND timerWindow=nullptr;
    NativeTray::State* state=nullptr;
    uint64_t session=0,generation=0;
    std::vector<Candidate> candidates;
    std::vector<winrt::weak_ref<wux::Hosting::DesktopWindowXamlSource>> sources;
    Decoration decoration;
    XamlConnections activeEvents;
    NativeTray::Request request{};
    RECT targetBounds{};
    double actualWidth=0,actualHeight=0;
    LONG sentRevision=-1,sentMode=-1,sentLifecycle=-1,sentError=0;
    double sentWidth=-1,sentHeight=-1;
    RECT sentBounds{};
#ifdef NATIVE_TRAY_SELFTEST
    std::function<bool(wux::FrameworkElement const&)> matchForTest;
#endif
    bool busy=false,queued=false,blocked=false,stopping=false,dumped=false,tipTimer=false;
    UINT_PTR tooltipTimerId=0,nextTooltipTimer=100;
    unsigned dirty=0;
    std::wstring lastTrace;
    void Trace(std::wstring value){if(value!=lastTrace){Log(value);lastTrace=std::move(value);}}
    bool Live() const noexcept{return state&&NativeTray::Read64(state->session)==static_cast<LONG64>(session);}
    void Count(volatile LONG64 NativeTray::State::*field) noexcept {if(Live())InterlockedIncrement64(&(state->*field));}
    static bool TrayThread() noexcept {
#ifdef NATIVE_TRAY_SELFTEST
        if(controller&&controller->matchForTest)return true;
#endif
        HWND bar=FindWindowW(L"Shell_TrayWnd",nullptr);
        HWND bridge=FindWindowExW(bar,nullptr,L"Windows.UI.Composition.DesktopWindowContentBridge",nullptr);
        return GetCurrentThreadId()==GetWindowThreadProcessId(bridge ? bridge : bar,nullptr);
    }
    Controller(){
        WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&WindowProc),&wc.hInstance);
        wc.lpszClassName=NativeTray::BrokerClass;RegisterClassW(&wc);
        timerWindow=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,this);
        winrt::check_bool(timerWindow!=nullptr);
        brokerWindow=timerWindow;
    }
    ~Controller(){
        Disconnect(false);
        if(timerWindow){if(brokerWindow.load()==timerWindow)brokerWindow=nullptr;DestroyWindow(timerWindow);}
    }
    static void CALLBACK OwnerExited(PTP_CALLBACK_INSTANCE,void* data,PTP_WAIT,TP_WAIT_RESULT) noexcept {
        auto context=static_cast<WaitContext*>(data);
        PostMessageW(context->window,NativeTray::OwnerGoneMessage(),static_cast<WPARAM>(context->generation),0);
    }
    static void CALLBACK ForegroundChanged(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) noexcept {
        if(controller)controller->Queue(CancelTip);
    }
    void Notify(UINT message) noexcept {
        if(Live())PostMessageW(reinterpret_cast<HWND>(static_cast<UINT_PTR>(state->appWindow)),message,static_cast<WPARAM>(session),0);
    }
    void Report(LONG lifecycle=NativeTray::Ready,LONG error=0) noexcept {
        if(!Live())return;
        const LONG applied=decoration.view?decoration.mode:0;
        const bool moved=memcmp(&sentBounds,&targetBounds,sizeof(RECT))!=0;
        const bool changed=sentRevision!=request.revision||sentMode!=applied||sentLifecycle!=lifecycle||sentError!=error||sentWidth!=actualWidth||sentHeight!=actualHeight||moved;
        sentRevision=request.revision;sentMode=applied;sentLifecycle=lifecycle;sentError=error;sentWidth=actualWidth;sentHeight=actualHeight;
        sentBounds=targetBounds;state->actualBounds=targetBounds;
        InterlockedExchange(&state->lifecycle,lifecycle);InterlockedExchange(&state->lastError,error);
        InterlockedExchange(&state->acknowledgedRevision,request.revision);
        InterlockedExchange(&state->applied,decoration.view ? decoration.mode : 0);
        InterlockedExchange(&state->uiThread,static_cast<LONG>(GetCurrentThreadId()));
        InterlockedExchange(&state->subscriptions,static_cast<LONG>(activeEvents.undo.size()+CandidateSubscriptions()));
        InterlockedExchange(&state->pollingTimers,0);InterlockedExchange(&state->tooltipTimer,tipTimer?1:0);
        state->actualWidth=actualWidth;state->actualHeight=actualHeight;
        if(changed){Notify(NativeTray::ReportMessage());if(lifecycle==NativeTray::Ready||lifecycle==NativeTray::Faulted)NativeTray::SignalAttached(*state);}
    }
    size_t CandidateSubscriptions() const noexcept {size_t n=0;for(auto const& c:candidates)if(c.events)n+=c.events->undo.size();return n;}
    bool Connect(uint64_t token){
        Disconnect(false);
        mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,NativeTray::MappingName);if(!mapping)return false;
        state=static_cast<NativeTray::State*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(NativeTray::State)));
        if(!state||!NativeTray::Valid(*state)){Disconnect(false);return false;}
        session=static_cast<uint64_t>(NativeTray::Read64(state->session));generation=token;
        HWND owner=reinterpret_cast<HWND>(static_cast<UINT_PTR>(state->appWindow));DWORD pid=0;GetWindowThreadProcessId(owner,&pid);
        wchar_t name[100]{};
        if(!session||pid!=state->appPid||!GetClassNameW(owner,name,100)||wcscmp(name,NativeTray::WindowClass)){Disconnect(false);return false;}
        process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);winrt::check_bool(process!=nullptr);
        waitContext={timerWindow,generation};wait=CreateThreadpoolWait(OwnerExited,&waitContext,nullptr);winrt::check_bool(wait!=nullptr);
        SetThreadpoolWait(wait,process,nullptr);
        foreground=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,ForegroundChanged,0,0,WINEVENT_OUTOFCONTEXT);
        winrt::check_bool(foreground!=nullptr);
        stopping=false;blocked=false;sentRevision=-1;actualWidth=0;actualHeight=0;
        InterlockedExchange64(&state->controllerWindow,reinterpret_cast<LONG64>(timerWindow));
        InterlockedExchange64(&state->connection,static_cast<LONG64>(generation));
        InterlockedExchange(&state->explorerPid,static_cast<LONG>(GetCurrentProcessId()));
        LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);InterlockedExchange64(&state->qpcFrequency,frequency.QuadPart);
        NativeTray::ReadRequest(*state,request);Report(NativeTray::Connecting);Notify(NativeTray::ReadyMessage());
        Queue(All);return true;
    }
    void CancelDelay() noexcept {
        if(tipTimer){KillTimer(timerWindow,tooltipTimerId);tipTimer=false;tooltipTimerId=0;if(Live())InterlockedExchange(&state->tooltipTimer,0);}
    }
    void Disconnect(bool unsubscribe=true,bool fault=false,HRESULT error=0) noexcept {
        stopping=true;CancelDelay();
        if(foreground){UnhookWinEvent(foreground);foreground=nullptr;}
        if(wait){SetThreadpoolWait(wait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(wait,TRUE);CloseThreadpoolWait(wait);wait=nullptr;}
        if(process){CloseHandle(process);process=nullptr;}
        activeEvents.Clear();candidates.clear();sources.clear();decoration.Restore();
        if(Live()){
            InterlockedExchange(&state->applied,0);InterlockedExchange(&state->subscriptions,0);InterlockedExchange(&state->tooltipTimer,0);
            InterlockedExchange(&state->lifecycle,fault?NativeTray::Faulted:NativeTray::Detached);InterlockedExchange(&state->lastError,error);InterlockedExchange64(&state->controllerWindow,0);
            state->actualWidth=0;state->actualHeight=0;
            InterlockedExchange(&state->acknowledgedRevision,request.revision);Notify(NativeTray::ReportMessage());NativeTray::SignalAttached(*state);
        }
        if(state){UnmapViewOfFile(state);state=nullptr;}if(mapping){CloseHandle(mapping);mapping=nullptr;}
        session=0;queued=false;dirty=0;
        if(unsubscribe)Subscription(false,generation);
    }
    void Fail(HRESULT error,wchar_t const* stage=L"window-dispatch") noexcept {
        wchar_t failure[192]{};
        swprintf_s(failure,L"Native backend failure stage=%s HRESULT=0x%08lX",stage,static_cast<unsigned long>(error));
        Log(failure);
        Count(&NativeTray::State::callbackErrors);
        blocked=true;Disconnect(true,true,error);actualWidth=0;actualHeight=0;
        Log(L"Native backend disabled after callback failure");
    }
    void Queue(unsigned flags) noexcept {
        if(stopping||blocked||!state)return;
        dirty|=flags;
        if(!queued){queued=true;if(!PostMessageW(timerWindow,NativeTray::WorkMessage(),static_cast<WPARAM>(generation),0))queued=false;}
    }
    void External(unsigned flags) noexcept {if(!busy)Queue(flags);}
    void WatchLoaded(Candidate& c){
        if(c.waitingLoaded||c.view.IsLoaded())return;
        const auto view=c.view;const auto hwnd=timerWindow;const auto token=generation;
        auto loaded=view.Loaded([hwnd,token](auto const&,auto const&) noexcept {PostMessageW(hwnd,NativeTray::EventMessage(),static_cast<WPARAM>(token),Discover);});
        c.events->undo.emplace_back([view,loaded]{view.Loaded(loaded);});c.waitingLoaded=true;
    }
    void Add(InstanceHandle handle,wux::FrameworkElement const& view){
        for(auto const& c:candidates)if(c.handle==handle)return;
        Candidate c;c.handle=handle;c.view=view;c.events=std::make_unique<XamlConnections>();
        HWND hwnd=timerWindow;const auto token=generation;
        auto wake=[hwnd,token]{PostMessageW(hwnd,NativeTray::EventMessage(),static_cast<WPARAM>(token),Discover);};
        c.events->Property(view,wux::Automation::AutomationProperties::NameProperty(),wake);
        WatchLoaded(c);
        auto unloaded=view.Unloaded([wake](auto const&,auto const&) noexcept {wake();});c.events->undo.emplace_back([view,unloaded]{view.Unloaded(unloaded);});
        candidates.push_back(std::move(c));Queue(Discover);
    }
    void Remove(InstanceHandle handle){
        auto found=std::find_if(candidates.begin(),candidates.end(),[handle](auto const& c){return c.handle==handle;});
        if(found!=candidates.end()){candidates.erase(found);Queue(Discover);}
    }
    bool MatchesOwner(wux::FrameworkElement const& element)
    {
        const std::wstring actualName(wux::Automation::AutomationProperties::GetName(element));
        if (actualName != NativeTray::Tooltip)
        {
            if (actualName.starts_with(L"Yata VPN"))
            {
                std::wstring codes;
                for (const auto ch : actualName) codes += std::to_wstring(static_cast<unsigned>(ch)) + L",";
                Trace(L"Match: tooltip mismatch codes=" + codes);
            }
            return false;
        }
        if (!dumped) { DumpTree(element); dumped = true; }
        if (!element.IsLoaded()) { Trace(L"Match: target IsLoaded=false"); return false; }
        NOTIFYICONIDENTIFIER identity{sizeof(identity)};
        identity.hWnd = reinterpret_cast<HWND>(static_cast<UINT_PTR>(state->appWindow));
        identity.uID = 1;
        identity.guidItem = NativeTray::IconGuid;
        RECT icon{};
        const HRESULT iconResult = Shell_NotifyIconGetRect(&identity, &icon);
        if (FAILED(iconResult)) { Trace(L"Match: GUID rectangle HRESULT=" + std::to_wstring(static_cast<unsigned long>(iconResult))); return false; }
        const auto root = element.XamlRoot();
        if (!root || !root.Content()) return false;
        HWND host = nullptr;
        for (auto it = sources.begin(); it != sources.end();)
        {
            const auto source = it->get();
            if (!source) { it = sources.erase(it); continue; }
            if (source.Content() == root.Content())
            {
                winrt::check_hresult(source.as<IDesktopWindowXamlSourceNative>()->get_WindowHandle(&host));
                break;
            }
            ++it;
        }
        // Some shell builds omit the source from diagnostics. Accept the primary
        // bridge only if its physical client size matches this XamlRoot.
        if (!host)
        {
            const HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
            const HWND bridge = FindWindowExW(taskbar, nullptr, L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr);
            RECT client{};
            if (!bridge || GetWindowThreadProcessId(bridge, nullptr) != GetCurrentThreadId() || !GetClientRect(bridge, &client)) return false;
            if (std::abs((client.right - client.left) - root.Size().Width * root.RasterizationScale()) > 2 ||
                std::abs((client.bottom - client.top) - root.Size().Height * root.RasterizationScale()) > 2) return false;
            host = bridge;
        }
        const auto logical = element.TransformToVisual(root.Content()).TransformBounds({0, 0,
            static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
        const double scale = root.RasterizationScale();
        POINT corners[2]{{static_cast<LONG>(std::lround(logical.X * scale)), static_cast<LONG>(std::lround(logical.Y * scale))},
            {static_cast<LONG>(std::lround((logical.X + logical.Width) * scale)), static_cast<LONG>(std::lround((logical.Y + logical.Height) * scale))}};
        MapWindowPoints(host, nullptr, corners, 2);
        const winrt::Windows::Foundation::Rect bounds{static_cast<float>(corners[0].x), static_cast<float>(corners[0].y),
            static_cast<float>(corners[1].x - corners[0].x), static_cast<float>(corners[1].y - corners[0].y)};
        Trace(L"Match: GUID=" + std::to_wstring(icon.left) + L"," + std::to_wstring(icon.top) + L"," + std::to_wstring(icon.right) + L"," + std::to_wstring(icon.bottom) +
            L" native=" + std::to_wstring(bounds.X) + L"," + std::to_wstring(bounds.Y) + L"," + std::to_wstring(bounds.Width) + L"," + std::to_wstring(bounds.Height));
        const double centerX = (static_cast<double>(icon.left) + icon.right) / 2.0;
        const double centerY = (static_cast<double>(icon.top) + icon.bottom) / 2.0;
        targetBounds={corners[0].x,corners[0].y,corners[1].x,corners[1].y};
        return bounds.Width > 0 && bounds.Height > 0 && centerX >= bounds.X && centerX <= bounds.X + bounds.Width &&
            centerY >= bounds.Y && centerY <= bounds.Y + bounds.Height;
    }


    void BindEvents(){
        auto view=decoration.view;const auto hwnd=timerWindow;const auto token=generation;
        auto post=[hwnd,token](unsigned flags){PostMessageW(hwnd,NativeTray::EventMessage(),static_cast<WPARAM>(token),flags);};
        for(auto event:{wux::UIElement::PointerEnteredEvent(),wux::UIElement::PointerExitedEvent(),wux::UIElement::PointerMovedEvent(),wux::UIElement::PointerReleasedEvent()})
            activeEvents.Pointer(view,event,[post]{post(PointerSignal|Chrome|Tooltip);});
        for(auto event:{wux::UIElement::PointerPressedEvent(),wux::UIElement::PointerCanceledEvent(),wux::UIElement::PointerCaptureLostEvent()})
            activeEvents.Pointer(view,event,[post]{post(PointerSignal|Chrome|CancelTip);});
        auto theme=view.ActualThemeChanged([post](auto const&,auto const&) noexcept {post(Theme|Chrome|Tooltip);});
        activeEvents.undo.emplace_back([view,theme]{view.ActualThemeChanged(theme);});
        auto size=view.SizeChanged([post](auto const&,auto const&) noexcept {post(Geometry);});
        activeEvents.undo.emplace_back([view,size]{view.SizeChanged(size);});
        auto layout=view.LayoutUpdated([post](auto const&,auto const&) noexcept {post(Geometry);});
        activeEvents.undo.emplace_back([view,layout]{view.LayoutUpdated(layout);});
        auto root=view.XamlRoot();if(root){auto changed=root.Changed([post](auto const&,auto const&) noexcept {post(Geometry|Theme);});activeEvents.undo.emplace_back([root,changed]{root.Changed(changed);});}
        activeEvents.Property(decoration.presenter,controls::ContentPresenter::ContentProperty(),[this]{External(Discover);});
        activeEvents.Property(decoration.presenter,controls::ContentPresenter::ForegroundProperty(),[this]{External(Chrome);});
        if(decoration.chrome.native)activeEvents.Property(decoration.chrome.native,wux::UIElement::OpacityProperty(),[this]{External(Chrome);});
        auto settings=winrt::Windows::UI::ViewManagement::AccessibilitySettings();
        auto contrast=settings.HighContrastChanged([post](auto const&,auto const&) noexcept {post(Theme|Chrome|Tooltip);});
        activeEvents.undo.emplace_back([settings,contrast]{settings.HighContrastChanged(contrast);});
    }
    bool TipAllowed() const noexcept {
        DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return !request.panelOpen && Live() && pid!=state->appPid;
    }
    void Refresh(unsigned flags){
        if(!Live()){Disconnect();return;}
        NativeTray::Request latest;if(!NativeTray::ReadRequest(*state,latest))return;
        auto validWidth=[](LONG width){return width==0||(width>=32&&width<=320);};
        if(latest.mode<0||latest.mode>2||!validWidth(latest.labelWidth)||!validWidth(latest.linesWidth))winrt::throw_hresult(E_INVALIDARG);
        if(latest.themeRevision!=request.themeRevision)flags|=Theme|Chrome|Tooltip;
        request=latest;
        if(request.mode==NativeTray::Off){Disconnect();return;}
        const auto width=static_cast<UINT>(request.mode==NativeTray::Lines ? request.linesWidth : request.labelWidth);
        bool change=!decoration.view || decoration.mode!=request.mode || decoration.requestedWidth!=width;
        if((flags&Discover)||!decoration.view){
            wux::FrameworkElement target{nullptr};
            for(auto& c:candidates){WatchLoaded(c);if(c.view.IsLoaded()&&
#ifdef NATIVE_TRAY_SELFTEST
                (matchForTest ? matchForTest(c.view) : MatchesOwner(c.view))
#else
                MatchesOwner(c.view)
#endif
                ){target=c.view;break;}}
            if(!target){activeEvents.Clear();decoration.Restore();actualWidth=0;actualHeight=0;Report(NativeTray::Connecting);return;}
            if(decoration.view && decoration.view!=target)change=true;
            if(!change && !decoration.IsCurrent())winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
            if(change){
                if(!FindPresenter(target))winrt::throw_hresult(E_NOINTERFACE);
                activeEvents.Clear();CancelDelay();decoration.Apply(target,request.mode,width);
                if(!decoration.view || !decoration.chrome.overlay)winrt::throw_hresult(E_NOINTERFACE);
                BindEvents();flags|=All;
            }
        }else if(change){activeEvents.Clear();CancelDelay();auto target=decoration.view;decoration.Apply(target,request.mode,width);BindEvents();flags|=All;}
        if(!decoration.view)return;
        NOTIFYICONIDENTIFIER identity{sizeof(identity)};identity.guidItem=NativeTray::IconGuid;
#ifdef NATIVE_TRAY_SELFTEST
        if(!matchForTest)
#endif
        if(FAILED(Shell_NotifyIconGetRect(&identity,&targetBounds)))winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
        decoration.chrome.bounds=targetBounds;
        if(flags&Theme){
            decoration.chrome.light=decoration.view.ActualTheme()!=wux::ElementTheme::Dark;
            decoration.chrome.contrast=winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
            decoration.chrome.Palette();decoration.tooltip.paletteValid=false;
        }
        decoration.SetPanelOpen(request.panelOpen!=0);
        if(flags&(Chrome|Theme|Geometry)){decoration.chrome.Update();Count(&NativeTray::State::chromeUpdates);}
        if(flags&CancelTip){CancelDelay();decoration.tooltip.Cancel();}
        else{
            const bool arm=decoration.tooltip.Update(targetBounds,TipAllowed());
            if(!decoration.tooltip.hovered)CancelDelay();
            else if(arm){CancelDelay();tooltipTimerId=++nextTooltipTimer;winrt::check_bool(SetTimer(timerWindow,tooltipTimerId,std::max(1u,decoration.tooltip.delay),nullptr)!=0);tipTimer=true;}
        }
        actualWidth=decoration.view.ActualWidth();actualHeight=decoration.view.ActualHeight();Report();
    }
    void Work(unsigned flags){
        if(stopping||blocked||busy)return;
        busy=true;LARGE_INTEGER start{},end{};QueryPerformanceCounter(&start);
        try{Refresh(flags);}
        catch(...){Fail(winrt::to_hresult(),L"refresh");}
        QueryPerformanceCounter(&end);
        if(Live()){
            InterlockedIncrement64(&state->refreshes);InterlockedAdd64(&state->updateTicks,end.QuadPart-start.QuadPart);
            const auto old=NativeTray::Read64(state->maxUpdateTicks);if(end.QuadPart-start.QuadPart>old)InterlockedExchange64(&state->maxUpdateTicks,end.QuadPart-start.QuadPart);
        }
        busy=false;
    }
    static LRESULT CALLBACK WindowProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) noexcept {
        auto self=reinterpret_cast<Controller*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<Controller*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        try{
            if(self){
                if(message==NativeTray::ReconnectMessage()){
                    const auto token=nextGeneration.fetch_add(1);if(self->Connect(token))Subscription(true,token);return 0;
                }
                if(message==NativeTray::StateMessage() && wp==self->session){self->Count(&NativeTray::State::notifications);self->Queue(All);return 0;}
                if(message==NativeTray::DetachMessage() && wp==self->session){self->Disconnect();return 0;}
                if(message==NativeTray::OwnerGoneMessage() && wp==self->generation){self->Disconnect();return 0;}
                if(message==NativeTray::FaultMessage() && wp==self->session){winrt::throw_hresult(E_FAIL);}
                if(message==NativeTray::ErrorMessage() && self->state && wp==self->generation){self->Fail(static_cast<HRESULT>(lp));return 0;}
                if(message==NativeTray::EventMessage() && wp==self->generation){if(static_cast<unsigned>(lp)&PointerSignal)self->Count(&NativeTray::State::pointerEvents);self->Queue(static_cast<unsigned>(lp));return 0;}
                if(message==NativeTray::WorkMessage() && wp==self->generation){
                    if(self->busy)return 0;
                    const unsigned flags=self->dirty|static_cast<unsigned>(lp);self->dirty=0;self->queued=false;
                    self->Work(flags);return 0;
                }
                if(message==WM_TIMER && self->tipTimer && wp==self->tooltipTimerId){self->CancelDelay();if(self->Live()&&self->decoration.tooltip.Open(self->targetBounds,self->TipAllowed()))self->Count(&NativeTray::State::tooltipOpens);self->Report();return 0;}
            }
        }catch(...){if(self)self->Fail(winrt::to_hresult());}
        return DefWindowProcW(hwnd,message,wp,lp);
    }
};

struct Watcher : winrt::implements<Watcher,IVisualTreeServiceCallback2> {
    winrt::com_ptr<IXamlDiagnostics> diagnostics;
    const uint64_t generation;
    std::atomic_bool stopped{false};
    std::atomic_bool advised{false};
    bool loggedExpiredHandle=false; // Accessed only on the tray XAML thread.
    Watcher(winrt::com_ptr<IXamlDiagnostics> d,uint64_t token):diagnostics(std::move(d)),generation(token){}
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation,VisualElement element,VisualMutationType mutation) noexcept override {
        std::unique_ptr<wchar_t,decltype(&SysFreeString)> name(element.Name,SysFreeString),type(element.Type,SysFreeString),file(element.SrcInfo.FileName,SysFreeString),hash(element.SrcInfo.Hash,SysFreeString);
        wchar_t const* stage=L"visual-tree-connect";
        try{
            if(stopped||generation!=desiredGeneration.load()||!Controller::TrayThread())return S_OK;
            const std::wstring_view kind=type ? std::wstring_view(type.get()):std::wstring_view{};
            if(!controller)controller=new Controller();
            if(controller->stopping && controller->generation==generation)return S_OK;
            if(controller->generation!=generation && !controller->Connect(generation))return S_OK;
            if(controller->busy)return S_OK;
            if(mutation==Remove){controller->Remove(element.Handle);return S_OK;}
            if(mutation!=Add || (kind!=L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource"&&kind!=L"SystemTray.NotifyIconView"))return S_OK;
            stage=L"visual-tree-resolve-handle";
            XamlObject object{nullptr};
            const HRESULT resolved=diagnostics->GetIInspectableFromHandle(element.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(object)));
            // Enumeration can outlive a Diagnostics cache entry. A disappearing
            // element is local to this handle, not a failed tray subscription.
            // Preserve fault containment for every other resolution failure.
            if(resolved==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)){
                controller->Remove(element.Handle);
                if(!loggedExpiredHandle){Log(L"Skipped expired XAML diagnostics handle; discovery remains active");loggedExpiredHandle=true;}
                return S_OK;
            }
            winrt::check_hresult(resolved);
            stage=L"visual-tree-watch-element";
            if(auto source=object.try_as<wux::Hosting::DesktopWindowXamlSource>())controller->sources.emplace_back(source);
            if(kind==L"SystemTray.NotifyIconView")if(auto view=object.try_as<wux::FrameworkElement>())controller->Add(element.Handle,view);
            return S_OK;
        }catch(...){if(controller)controller->Fail(winrt::to_hresult(),stage);return S_OK;}
    }
    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR) noexcept override{return S_OK;}
};

void Subscription(bool wanted,uint64_t generation,winrt::com_ptr<IXamlDiagnostics> diagnostics) noexcept {
    try{
        {std::lock_guard lock(runtimeMutex);if(diagnostics&&!runtimeDiagnostics)runtimeDiagnostics=std::move(diagnostics);
            runtimeWanted=wanted;runtimeGeneration=generation;desiredGeneration=wanted?generation:0;++runtimeRevision;if(runtimeWorker)return;runtimeWorker=true;}
        std::thread([]() noexcept {
            const auto apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            try{
                for(;;){
                    bool wanted;uint64_t token,revision;winrt::com_ptr<IXamlDiagnostics> diagnostics;winrt::com_ptr<Watcher> watcher;
                    {std::lock_guard lock(runtimeMutex);wanted=runtimeWanted;token=runtimeGeneration;revision=runtimeRevision;diagnostics=runtimeDiagnostics;watcher=runtimeWatcher;}
                    if(watcher && (!wanted||watcher->generation!=token)){
                        watcher->stopped=true;
                        if(watcher->advised.exchange(false))winrt::check_hresult(diagnostics.as<IVisualTreeService3>()->UnadviseVisualTreeChange(watcher.get()));
                        {std::lock_guard lock(runtimeMutex);runtimeWatcher=nullptr;}watcher=nullptr;
                    }
                    if(wanted&&!watcher&&diagnostics){
                        watcher=winrt::make_self<Watcher>(diagnostics,token);
                        {std::lock_guard lock(runtimeMutex);runtimeWatcher=watcher;}
                        winrt::check_hresult(diagnostics.as<IVisualTreeService3>()->AdviseVisualTreeChange(watcher.get()));
                        watcher->advised=true;
                    }
                    {std::lock_guard lock(runtimeMutex);if(revision==runtimeRevision){runtimeWorker=false;break;}}
                }
            }catch(...){const auto error=winrt::to_hresult();{std::lock_guard lock(runtimeMutex);if(runtimeWatcher)runtimeWatcher->stopped=true;runtimeWorker=false;}
                PostMessageW(brokerWindow.load(),NativeTray::ErrorMessage(),static_cast<WPARAM>(desiredGeneration.load()),error);Log(L"Diagnostics subscription failed");}
            if(SUCCEEDED(apartment))CoUninitialize();
        }).detach();
    }catch(...){const auto error=winrt::to_hresult();{std::lock_guard lock(runtimeMutex);runtimeWorker=false;}
        PostMessageW(brokerWindow.load(),NativeTray::ErrorMessage(),static_cast<WPARAM>(generation),error);Log(L"Diagnostics worker creation failed");}
}

struct Site : winrt::implements<Site,IObjectWithSite> {
    winrt::com_ptr<IUnknown> site;
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* value) noexcept override {
        try{
            if(!value){site=nullptr;return S_OK;}
            site.copy_from(value);
            const auto mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,NativeTray::MappingName);
            auto state=mapping ? static_cast<NativeTray::State*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(NativeTray::State))):nullptr;
            if(state&&NativeTray::Valid(*state)){Subscription(true,nextGeneration.fetch_add(1),site.as<IXamlDiagnostics>());}
            if(state)UnmapViewOfFile(state);if(mapping)CloseHandle(mapping);return S_OK;
        }catch(...){return winrt::to_hresult();}
    }
    HRESULT STDMETHODCALLTYPE GetSite(REFIID iid,void** value) noexcept override {try{return site.as(iid,value);}catch(...){return winrt::to_hresult();}}
};
struct Factory : winrt::implements<Factory,IClassFactory> {
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** value) noexcept override {
        if(!value)return E_POINTER;*value=nullptr;if(outer)return CLASS_E_NOAGGREGATION;
        try{return winrt::make<Site>().as(iid,value);}catch(...){return winrt::to_hresult();}
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override{return S_OK;}
};
