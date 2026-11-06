#pragma once
#include <windows.h>
#include <string_view>
#include <algorithm>
namespace TrayConsole {
inline void Write(std::string_view text) noexcept {
    HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    if(!output||output==INVALID_HANDLE_VALUE){AttachConsole(ATTACH_PARENT_PROCESS);output=GetStdHandle(STD_OUTPUT_HANDLE);}
    if(output&&output!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(output,text.data(),static_cast<DWORD>(text.size()),&written,nullptr);}
}
inline void Error(std::wstring_view text) noexcept {
    char buffer[2048]{};int n=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(std::min<size_t>(text.size(),600)),buffer,2040,nullptr,nullptr);
    if(n>0)Write({buffer,static_cast<size_t>(n)});Write("\n");
}
}
