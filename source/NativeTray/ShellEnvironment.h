#pragma once
#include <windows.h>
#include <string>
#include <string_view>

// Read-only desktop inspection. Never switches desktops or launches through a
// different security context. The caller decides whether standalone UI is useful.
namespace TrayEnvironment {
enum class Availability { Ready, OtherDesktop, NoShell };
struct Snapshot {
    std::wstring station,desktop,inputDesktop;
    HWND taskbar=nullptr;
    DWORD shellPid=0,sessionId=0;
};
inline std::wstring ObjectName(HANDLE object){
    wchar_t name[512]{};DWORD needed=0;
    if(object&&GetUserObjectInformationW(object,UOI_NAME,name,sizeof(name),&needed))return name;
    return L"unknown";
}
inline Snapshot Inspect(){
    Snapshot result;
    result.station=ObjectName(GetProcessWindowStation());
    result.desktop=ObjectName(GetThreadDesktop(GetCurrentThreadId()));
    HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(input){try{result.inputDesktop=ObjectName(input);}catch(...){CloseDesktop(input);throw;}CloseDesktop(input);}
    else result.inputDesktop=L"unknown";
    result.taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);
    if(result.taskbar)GetWindowThreadProcessId(result.taskbar,&result.shellPid);
    ProcessIdToSessionId(GetCurrentProcessId(),&result.sessionId);
    return result;
}
inline Availability Classify(Snapshot const& value) noexcept {
    if(value.taskbar&&value.shellPid)return Availability::Ready;
    if(value.desktop!=L"unknown"&&value.inputDesktop!=L"unknown"&&value.desktop!=value.inputDesktop)
        return Availability::OtherDesktop;
    return Availability::NoShell;
}
inline std::wstring Describe(Snapshot const& value){
    return L"session="+std::to_wstring(value.sessionId)+L" station="+value.station+
        L" desktop="+value.desktop+L" input-desktop="+value.inputDesktop+
        L" shell-pid="+std::to_wstring(value.shellPid)+L" taskbar="+(value.taskbar?L"found":L"not-found");
}
inline std::wstring Problem(Snapshot const& value){
    if(Classify(value)==Availability::OtherDesktop)
        return L"Explorer is on a different desktop. This process is on '"+value.desktop+
            L"'; the input desktop is '"+value.inputDesktop+
            L"'. Launch the EXE from File Explorer on your normal desktop. Use --standalone only for an isolated UI preview.";
    if(Classify(value)==Availability::NoShell)
        return L"Explorer taskbar was not found on this desktop. Start Explorer and try again, or use --standalone for a UI preview.";
    return {};
}
inline void Log(std::wstring_view stage,HRESULT result) noexcept {
    try{
        wchar_t temp[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,temp))return;
        const auto path=std::wstring(temp)+L"YataNativeFrameworkConnection.log";
        SYSTEMTIME time{};GetLocalTime(&time);wchar_t prefix[160]{};
        swprintf_s(prefix,L"%04u-%02u-%02u %02u:%02u:%02u pid=%lu tid=%lu result=0x%08lX stage=",
            time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,
            GetCurrentProcessId(),GetCurrentThreadId(),static_cast<unsigned long>(result));
        const auto line=std::wstring(prefix)+std::wstring(stage)+L" "+Describe(Inspect())+L"\r\n";
        char bytes[8192]{};const int length=WideCharToMultiByte(CP_UTF8,0,line.data(),static_cast<int>(line.size()),bytes,sizeof(bytes),nullptr,nullptr);
        if(length<=0)return;
        HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return;
        DWORD written=0;WriteFile(file,bytes,static_cast<DWORD>(length),&written,nullptr);CloseHandle(file);
    }catch(...){}
}
}
