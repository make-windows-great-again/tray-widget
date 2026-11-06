#pragma once
#include <windows.h>
#include <xamlom.h>
#include <string>
#include <thread>
#include "TrayProtocol.h"
#include "../NativeTray/ShellEnvironment.h"
namespace TrayAttach {
struct Handle{HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct View{NativeTray::State* value=nullptr;~View(){if(value)UnmapViewOfFile(value);}};
struct Module{HMODULE value=nullptr;~Module(){if(value)FreeLibrary(value);}};
struct Hook{HHOOK value=nullptr;~Hook(){if(value)UnhookWindowsHookEx(value);}};
inline HRESULT Run(){
    Handle mapping{OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,NativeTray::MappingName)};
    if(!mapping.value)return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    View state{static_cast<NativeTray::State*>(MapViewOfFile(mapping.value,FILE_MAP_ALL_ACCESS,0,0,sizeof(NativeTray::State)))};
    if(!state.value||!NativeTray::Valid(*state.value))return E_INVALIDARG;
    const auto session=NativeTray::Read64(state.value->session);
    wchar_t const* stage=L"shell-detection";
    auto finish=[&](HRESULT result){TrayEnvironment::Log(stage,result);if(NativeTray::Read64(state.value->session)==session)InterlockedExchange(&state.value->attachResult,result);return result;};
    const auto environment=TrayEnvironment::Inspect();
    if(TrayEnvironment::Classify(environment)!=TrayEnvironment::Availability::Ready)
        return finish(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    stage=L"ready-event";
    wchar_t name[96]{};NativeTray::AttachEventName(name,session);
    Handle ready{CreateEventW(nullptr,TRUE,FALSE,name)};if(!ready.value)return finish(HRESULT_FROM_WIN32(GetLastError()));
    HWND taskbar=environment.taskbar;const DWORD pid=environment.shellPid;
    HWND broker=FindWindowExW(HWND_MESSAGE,nullptr,NativeTray::BrokerClass,nullptr);DWORD brokerPid=0;GetWindowThreadProcessId(broker,&brokerPid);
    if(broker&&brokerPid==pid&&state.value->lifecycle==NativeTray::Ready&&state.value->applied==state.value->mode&&state.value->lastError==0)return finish(S_OK);
    ResetEvent(ready.value);
    auto await=[&]{
        stage=L"controller-acknowledgment";
        const DWORD waited=WaitForSingleObject(ready.value,10000);
        if(waited==WAIT_FAILED)return finish(HRESULT_FROM_WIN32(GetLastError()));
        if(waited!=WAIT_OBJECT_0)return finish(HRESULT_FROM_WIN32(WAIT_TIMEOUT));
        if(NativeTray::Read64(state.value->session)!=session)return finish(HRESULT_FROM_WIN32(ERROR_CANCELLED));
        return finish(static_cast<HRESULT>(state.value->lastError));
    };
    if(broker&&brokerPid==pid){if(!PostMessageW(broker,NativeTray::ReconnectMessage(),0,0))return finish(HRESULT_FROM_WIN32(GetLastError()));return await();}
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    std::wstring path(executable);path.resize(path.find_last_of(L"\\/")+1);path+=NativeTray::ModuleName;
    stage=L"patch-file";
    if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)return finish(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    stage=L"xaml-diagnostics-entrypoint";
    Module xaml{LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
    if(!xaml.value)return finish(HRESULT_FROM_WIN32(GetLastError()));
    const auto initialize=reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(xaml.value,"InitializeXamlDiagnosticsEx"));
    if(!initialize)return finish(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
    HRESULT result=E_FAIL;
    stage=L"xaml-diagnostics-initialize";
    for(int endpoint=1;endpoint<=32;++endpoint){
        const auto connection=L"VisualDiagConnection"+std::to_wstring(endpoint);
        std::thread([&]{try{result=initialize(connection.c_str(),pid,nullptr,path.c_str(),NativeTray::TapClsid,nullptr);}catch(...){result=E_FAIL;}}).join();
        if(SUCCEEDED(result))break;
    }
    TrayEnvironment::Log(stage,result);
    if(WaitForSingleObject(ready.value,0)==WAIT_OBJECT_0)return await();
    stage=L"patch-load";
    Module module{LoadLibraryW(path.c_str())};if(!module.value)return finish(HRESULT_FROM_WIN32(GetLastError()));
    const auto callback=reinterpret_cast<HOOKPROC>(GetProcAddress(module.value,"NativeTrayHook"));
    const auto island=FindWindowExW(taskbar,nullptr,L"Windows.UI.Composition.DesktopWindowContentBridge",nullptr);
    const HWND target=island?island:taskbar;const DWORD tid=GetWindowThreadProcessId(target,nullptr);
    stage=L"bootstrap-hook";
    if(!callback)return finish(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
    if(!tid)return finish(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    Hook sent{SetWindowsHookExW(WH_CALLWNDPROC,callback,module.value,tid)};
    Hook queued{SetWindowsHookExW(WH_GETMESSAGE,callback,module.value,tid)};
    const DWORD hookError=GetLastError();
    if(!sent.value&&!queued.value){
        // A successful direct Diagnostics connection does not require a hook.
        if(SUCCEEDED(result))return await();
        return finish(hookError?HRESULT_FROM_WIN32(hookError):E_FAIL);
    }
    PostMessageW(target,WM_NULL,0,0);PostThreadMessageW(tid,WM_NULL,0,0);
    DWORD_PTR ignored=0;SendMessageTimeoutW(target,WM_NULL,0,0,SMTO_ABORTIFHUNG,2000,&ignored);
    return await();
}
}
