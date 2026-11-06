#pragma once
#include "TrayProtocol.h"
#include <limits>

// HWND/shared-memory boundary only. No XAML objects cross into Explorer.
class TrayBridge {
    HANDLE mapping=nullptr,helper=nullptr;
    HWND owner=nullptr;
    NativeTray::State* state=nullptr;
    NativeTray::Request request{};
    uint64_t session=0;
    HICON icon=nullptr;
    bool registered=false;
public:
    ~TrayBridge(){Close();}
    void Initialize(HWND window,UINT width,bool standalone){
        owner=window;
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(NativeTray::State),NativeTray::MappingName);
        winrt::check_bool(mapping!=nullptr);
        state=static_cast<NativeTray::State*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(NativeTray::State)));
        winrt::check_bool(state!=nullptr);
        ZeroMemory(state,sizeof(*state));
        FILETIME time{};GetSystemTimeAsFileTime(&time);
        session=(uint64_t(time.dwHighDateTime)<<32)|time.dwLowDateTime;
        session^=uint64_t(GetCurrentProcessId())<<17;
        state->magic=NativeTray::Magic;state->version=NativeTray::ProtocolVersion;state->size=sizeof(*state);
        state->appPid=GetCurrentProcessId();state->appWindow=reinterpret_cast<UINT_PTR>(owner);
        InterlockedExchange64(&state->session,session);
        request.mode=NativeTray::Label;request.labelWidth=static_cast<LONG>(width);Publish();
        if(!standalone){RegisterIcon();Attach();}
    }
    HWND Controller() const noexcept {
        if(!state)return nullptr;
        auto window=reinterpret_cast<HWND>(NativeTray::Read64(state->controllerWindow));
        DWORD pid=0,shell=0;GetWindowThreadProcessId(window,&pid);GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd",nullptr),&shell);
        wchar_t name[100]{};
        return pid&&pid==shell&&GetClassNameW(window,name,100)&&!wcscmp(name,NativeTray::BrokerClass)?window:nullptr;
    }
    void Publish() noexcept {
        if(!state)return;
        request.revision=request.revision==LONG_MAX?1:request.revision+1;
        NativeTray::Publish(*state,request);
        if(auto controller=Controller())PostMessageW(controller,NativeTray::StateMessage(),static_cast<WPARAM>(session),request.revision);
    }
    void Width(UINT width){
        winrt::check_bool(width==0||(width>=32&&width<=320));
        request.labelWidth=static_cast<LONG>(width);Publish();
    }
    UINT Width() const noexcept{return static_cast<UINT>(request.labelWidth);}
    void Phase(int value) noexcept {request.panelPhase=value;request.panelOpen=value!=0;Publish();}
    void Theme() noexcept {++request.themeRevision;Publish();}
    void Attach(){
        if(!registered)return;
        if(helper){if(WaitForSingleObject(helper,0)==WAIT_TIMEOUT)return;CloseHandle(helper);helper=nullptr;}
        if(auto controller=FindWindowExW(HWND_MESSAGE,nullptr,NativeTray::BrokerClass,nullptr)){
            DWORD pid=0,shell=0;GetWindowThreadProcessId(controller,&pid);GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd",nullptr),&shell);
            if(pid&&pid==shell){winrt::check_bool(PostMessageW(controller,NativeTray::ReconnectMessage(),0,0));return;}
        }
        wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
        std::wstring command=L"\""+std::wstring(executable)+L"\" --attach";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        winrt::check_bool(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
        CloseHandle(process.hThread);helper=process.hProcess;
    }
    void RegisterIcon(){
        if(!icon)icon=CopyIcon(LoadIconW(nullptr,IDI_SHIELD));
        NOTIFYICONDATAW data{sizeof(data)};data.hWnd=owner;data.uID=1;data.guidItem=NativeTray::IconGuid;
        data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_GUID|NIF_SHOWTIP;data.hIcon=icon;
        data.uCallbackMessage=NativeTray::TrayMessage();wcscpy_s(data.szTip,NativeTray::Tooltip);
        winrt::check_bool(Shell_NotifyIconW(NIM_ADD,&data));registered=true;
        data.uVersion=NOTIFYICON_VERSION_4;winrt::check_bool(Shell_NotifyIconW(NIM_SETVERSION,&data));
    }
    void Report(WPARAM token){
        if(token!=session||!registered)return;
        NOTIFYICONDATAW data{sizeof(data)};data.hWnd=owner;data.uID=1;data.guidItem=NativeTray::IconGuid;
        data.uFlags=NIF_TIP|NIF_GUID;if(!state->applied)data.uFlags|=NIF_SHOWTIP;
        wcscpy_s(data.szTip,NativeTray::Tooltip);Shell_NotifyIconW(NIM_MODIFY,&data);
    }
    RECT Bounds() const noexcept {
        NOTIFYICONIDENTIFIER id{sizeof(id)};id.hWnd=owner;id.uID=1;id.guidItem=NativeTray::IconGuid;
        RECT rect{};if(registered&&SUCCEEDED(Shell_NotifyIconGetRect(&id,&rect)))return rect;
        POINT point{};GetPhysicalCursorPos(&point);MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTOPRIMARY),&info);
        return {info.rcWork.right-100,info.rcWork.bottom,info.rcWork.right-60,info.rcWork.bottom+1};
    }
    NativeTray::State const* State() const noexcept{return state;}
    void Close() noexcept {
        if(state){request.mode=NativeTray::Off;request.panelOpen=0;request.panelPhase=0;Publish();if(auto controller=Controller())PostMessageW(controller,NativeTray::DetachMessage(),static_cast<WPARAM>(session),0);}
        if(registered){NOTIFYICONDATAW data{sizeof(data)};data.hWnd=owner;data.uID=1;data.guidItem=NativeTray::IconGuid;data.uFlags=NIF_GUID;Shell_NotifyIconW(NIM_DELETE,&data);registered=false;}
        if(icon){DestroyIcon(icon);icon=nullptr;}
        if(helper){CloseHandle(helper);helper=nullptr;}
        if(state){UnmapViewOfFile(state);state=nullptr;}
        if(mapping){CloseHandle(mapping);mapping=nullptr;}
    }
};
