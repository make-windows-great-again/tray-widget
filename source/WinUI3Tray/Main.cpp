#include "pch.h"
#include "App.xaml.h"
#include "TrayBridge.h"
#include "TrayAttach.h"
#include "HelloPanel.h"
#include "../NativeTray/TrayConsole.h"
#include <Microsoft.UI.Dispatching.Interop.h>

namespace {
struct Options {
    bool standalone=false,show=false,attach=false,status=false,exit=false,hide=false,menu=false;
    int width=-1,theme=-1;
};
Options Arguments(){
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);winrt::check_bool(args!=nullptr);
    auto cleanup=std::unique_ptr<wchar_t*,decltype(&LocalFree)>(args,LocalFree);
    Options result;
    for(int i=1;i<count;++i){
        const std::wstring_view arg=args[i];
        if(arg==L"--standalone")result.standalone=true;
        else if(arg==L"--show")result.show=true;
        else if(arg==L"--attach")result.attach=true;
        else if(arg==L"--status")result.status=true;
        else if(arg==L"--exit")result.exit=true;
        else if(arg==L"--hide")result.hide=true;
        else if(arg==L"--menu")result.menu=true;
        else if(arg==L"--width"){
            if(++i==count)winrt::throw_hresult(E_INVALIDARG);
            wchar_t* end=nullptr;long value=wcstol(args[i],&end,10);
            if(end==args[i]||*end||(value!=0&&(value<32||value>320)))winrt::throw_hresult(E_INVALIDARG);
            result.width=static_cast<int>(value);
        }else if(arg==L"--theme"){
            if(++i==count)winrt::throw_hresult(E_INVALIDARG);
            std::wstring_view value=args[i];result.theme=value==L"system"?0:value==L"light"?1:value==L"dark"?2:-1;
            if(result.theme<0)winrt::throw_hresult(E_INVALIDARG);
        }else winrt::throw_hresult(E_INVALIDARG);
    }
    return result;
}
int Status(){
    HANDLE mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,NativeTray::MappingName);
    if(!mapping){TrayConsole::Write("{\"running\":false}\n");return 0;}
    auto state=static_cast<NativeTray::State const*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(NativeTray::State)));
    if(!state||!NativeTray::Valid(*state)){if(state)UnmapViewOfFile(state);CloseHandle(mapping);return 2;}
    DWORD owner=0;const HWND window=reinterpret_cast<HWND>(state->appWindow);GetWindowThreadProcessId(window,&owner);
    char text[1000]{};std::snprintf(text,sizeof(text),
        "{\"running\":%s,\"pid\":%lu,\"applied\":%ld,\"width\":%ld,\"actualWidth\":%.3f,\"actualHeight\":%.3f,\"phase\":%ld,\"visible\":%s,\"error\":%ld,\"attachResult\":%ld,\"callbackErrors\":%lld,\"rect\":[%ld,%ld,%ld,%ld]}\n",
        owner==state->appPid?"true":"false",state->appPid,state->applied,state->labelWidth,state->actualWidth,state->actualHeight,state->panelPhase,IsWindowVisible(window)?"true":"false",state->lastError,state->attachResult,static_cast<long long>(state->callbackErrors),state->actualBounds.left,state->actualBounds.top,state->actualBounds.right,state->actualBounds.bottom);
    TrayConsole::Write(text);UnmapViewOfFile(state);CloseHandle(mapping);return 0;
}
bool Send(HWND target,UINT message,WPARAM value=0){DWORD_PTR result=0;return SendMessageTimeoutW(target,message,value,0,SMTO_ABORTIFHUNG,3000,&result)!=0;}

class Client {
    HWND window=nullptr;
    TrayBridge bridge;
    HelloPanel panel;
    bool ready=false,standalone=false,closing=false;
    UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
public:
    int failure=0;
    ~Client(){panel.Close();bridge.Close();if(window&&IsWindow(window))DestroyWindow(window);}
    void Initialize(Options const& options){
        standalone=options.standalone;
        WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=Procedure;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=NativeTray::WindowClass;
        winrt::check_bool(RegisterClassExW(&wc)!=0);
        window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"VPN — WinUI 3",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,this);winrt::check_bool(window!=nullptr);
        COLORREF none=DWMWA_COLOR_NONE;DwmSetWindowAttribute(window,DWMWA_BORDER_COLOR,&none,sizeof(none));
        DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_DONOTROUND;DwmSetWindowAttribute(window,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
        bridge.Initialize(window,options.width<0?88:static_cast<UINT>(options.width),standalone);
        panel.Initialize(window,[this](int phase){bridge.Phase(phase);},[this](UINT width){bridge.Width(width);},[this]{PostMessageW(window,NativeTray::ExitMessage(),0,0);});
        panel.SetWidth(bridge.Width());if(options.theme>=0)panel.Theme(options.theme);ready=true;
        if(options.show||standalone)panel.Show(bridge.Bounds());if(options.menu)panel.Context(bridge.Bounds());
    }
    HWND Window() const noexcept{return window;}
    LRESULT Message(UINT message,WPARAM wp,LPARAM lp){
        if(message==NativeTray::ExitMessage()){closing=true;DestroyWindow(window);return 1;}
        if(!ready)return DefWindowProcW(window,message,wp,lp);
        if(message==taskbarCreated&&!standalone){bridge.RegisterIcon();bridge.Attach();return 0;}
        if(message==NativeTray::ReadyMessage()){bridge.Publish();return 0;}
        if(message==NativeTray::ReportMessage()){bridge.Report(wp);return 0;}
        if(message==NativeTray::WidthMessage()){
            if(wp!=0&&(wp<32||wp>320))return 0;bridge.Width(static_cast<UINT>(wp));panel.SetWidth(static_cast<UINT>(wp));return 1;
        }
        if(message==NativeTray::ThemeMessage()){if(wp>2)return 0;panel.Theme(static_cast<int>(wp));bridge.Theme();return 1;}
        if(message==NativeTray::ShowMessage()){panel.Show(bridge.Bounds());return 1;}
        if(message==NativeTray::HideMessage()){panel.Hide();return 1;}
        if(message==NativeTray::MenuMessage()){panel.Context(bridge.Bounds());return 1;}
        if(message==NativeTray::TrayMessage()){
            switch(LOWORD(lp)){
            case NIN_SELECT:case NIN_KEYSELECT:panel.Toggle(bridge.Bounds());break;
            case WM_CONTEXTMENU:panel.Context(bridge.Bounds());break;
            }return 0;
        }
        switch(message){
        case WM_SIZE:panel.ResizeIsland();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT paint{};BeginPaint(window,&paint);EndPaint(window,&paint);return 0;}
        case WM_ACTIVATE:
            if(LOWORD(wp)==WA_INACTIVE){POINT point{};GetPhysicalCursorPos(&point);const auto rect=bridge.Bounds();panel.Deactivated(reinterpret_cast<HWND>(lp),(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&PtInRect(&rect,point));}
            return 0;
        case WM_KEYDOWN:if(wp==VK_ESCAPE){panel.Hide();return 0;}break;
        case WM_DPICHANGED:case WM_DISPLAYCHANGE:if(panel.Phase())panel.Place(bridge.Bounds());return 0;
        case WM_SETTINGCHANGE:case WM_THEMECHANGED:panel.RefreshTheme();bridge.Theme();return 0;
        case WM_CLOSE:panel.Hide();return 0;
        case WM_DESTROY:closing=true;ready=false;panel.Close();bridge.Close();PostQuitMessage(failure);return 0;
        }return DefWindowProcW(window,message,wp,lp);
    }
    static LRESULT CALLBACK Procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) noexcept {
        auto client=reinterpret_cast<Client*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){client=static_cast<Client*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);client->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(client));}
        try{return client?client->Message(message,wp,lp):DefWindowProcW(hwnd,message,wp,lp);}
        catch(winrt::hresult_error const& error){TrayConsole::Error(error.message());if(client){client->failure=static_cast<int>(error.code().value);if(!client->closing)PostMessageW(hwnd,NativeTray::ExitMessage(),0,0);}return 0;}
        catch(...){if(client){client->failure=1;PostMessageW(hwnd,NativeTray::ExitMessage(),0,0);}return 0;}
    }
};
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    try{
        const auto options=Arguments();
        if(options.status)return Status();
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        if(options.attach)return static_cast<int>(TrayAttach::Run());
        HANDLE mutex=CreateMutexW(nullptr,TRUE,NativeTray::MutexName);winrt::check_bool(mutex!=nullptr);
        const auto existed=GetLastError()==ERROR_ALREADY_EXISTS;
        auto release=std::unique_ptr<void,decltype(&CloseHandle)>(mutex,CloseHandle);
        if(existed||options.exit||options.hide){
            const auto target=FindWindowW(NativeTray::WindowClass,nullptr);if(!target)return options.exit?0:2;
            bool ok=true;
            if(options.width>=0)ok=Send(target,NativeTray::WidthMessage(),options.width)&&ok;
            if(options.theme>=0)ok=Send(target,NativeTray::ThemeMessage(),options.theme)&&ok;
            if(options.show)ok=Send(target,NativeTray::ShowMessage())&&ok;
            if(options.hide)ok=Send(target,NativeTray::HideMessage())&&ok;
            if(options.menu)ok=Send(target,NativeTray::MenuMessage())&&ok;
            if(options.exit)ok=Send(target,NativeTray::ExitMessage())&&ok;
            return ok?0:3;
        }
        auto queue=winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();
        auto app=winrt::make<winrt::WinUI3Tray::implementation::App>();
        int result=0;
        {
            Client client;client.Initialize(options);
            auto token=app.UnhandledException([&](auto const&,ui::UnhandledExceptionEventArgs const& args){TrayConsole::Error(args.Message());args.Handled(true);client.failure=1;PostMessageW(client.Window(),NativeTray::ExitMessage(),0,0);});
            MSG message{};BOOL status=0;
            while((status=GetMessageW(&message,nullptr,0,0))>0){if(!ContentPreTranslateMessage(&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
            result=status<0?1:client.failure;app.UnhandledException(token);
        }
        queue.ShutdownQueue();return result;
    }catch(winrt::hresult_error const& error){TrayConsole::Error(error.message());return static_cast<int>(error.code().value);}
    catch(...){TrayConsole::Write("WinUI 3 tray initialization failed\n");return 1;}
}
