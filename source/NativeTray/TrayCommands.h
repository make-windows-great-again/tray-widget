#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <limits>

namespace TrayCommands {
enum class Role { App, Attach, Status, Detach, Exit, Profile, Help, Version, Diagnose };
struct Options {
    Role role=Role::App;
    int mode=-1;
    int width=-1;
    bool show=false;
    bool fault=false;
    int content=-1;
    bool standalone=false;
    std::wstring error;
};
inline bool Number(std::wstring_view text,unsigned& result) noexcept {
    if(text.empty())return false;
    unsigned value=0;
    for(wchar_t c:text){if(c<L'0'||c>L'9')return false;unsigned digit=c-L'0';if(value>(std::numeric_limits<unsigned>::max()-digit)/10)return false;value=value*10+digit;}
    result=value;return true;
}
inline Options Parse(std::vector<std::wstring> const& args) {
    Options o;bool roleSet=false;
    auto role=[&](Role value){if(roleSet){o.error=L"Only one service role is allowed";return;}o.role=value;roleSet=true;};
    for(size_t i=0;i<args.size() && o.error.empty();++i){
        const std::wstring_view a=args[i];
        if(a==L"--attach")role(Role::Attach);
        else if(a==L"--status")role(Role::Status);
        else if(a==L"--profile")role(Role::Profile);
        else if(a==L"--detach")role(Role::Detach);
        else if(a==L"--exit")role(Role::Exit);
        else if(a==L"--help" || a==L"-h")role(Role::Help);
        else if(a==L"--version")role(Role::Version);
        else if(a==L"--diagnose")role(Role::Diagnose);
        else if(a==L"--controls")o.content=1;
        else if(a==L"--sample")o.content=0;
        else if(a==L"--lang")o.content=2;
        else if(a==L"--quick")o.content=3;
        else if(a==L"--date")o.content=4;
        else if(a==L"--standalone")o.standalone=true;
        else if(a==L"--show")o.show=true;
        else if(a==L"--native-vpn")o.mode=1;
        else if(a==L"--native-speed")o.mode=2;
        else if(a==L"--native-off")o.mode=0;
        else if(a==L"--test-fault")o.fault=true;
        else if(a==L"--mode"){
            if(++i==args.size()){o.error=L"Missing mode";break;}
            if(args[i]==L"vpn"||args[i]==L"label")o.mode=1;else if(args[i]==L"speed"||args[i]==L"lines")o.mode=2;else if(args[i]==L"off")o.mode=0;else o.error=L"Invalid mode";
        }else if(a==L"--width" || a.starts_with(L"--native-width=") || a.starts_with(L"--width=")){
            std::wstring_view value;
            if(a==L"--width"){if(++i==args.size()){o.error=L"Missing width";break;}value=args[i];}
            else value=a.substr(a.find(L'=')+1);
            unsigned width=0;
            if(!Number(value,width) || (width!=0 && (width<32 || width>320)))o.error=L"Width must be 0 or 32..320 DIP";
            else o.width=static_cast<int>(width);
        }else o.error=L"Unknown argument: "+std::wstring(a);
    }
    if(roleSet && o.role!=Role::App && (o.mode!=-1 || o.width!=-1 || o.show || o.fault || o.content!=-1 || o.standalone))o.error=L"Service roles cannot be mixed with application commands";
    return o;
}
inline constexpr ULONG_PTR CommandTag=0x59544332;
struct Packet {DWORD size=sizeof(Packet);DWORD version=2;int mode=-1;int width=-1;BOOL show=FALSE;BOOL fault=FALSE;int content=-1;};
inline bool Valid(Packet const& p) noexcept {return p.size==sizeof(p)&&p.version==2&&p.mode>=-1&&p.mode<=2&&p.width>=-1&&(p.width<=0||(p.width>=32&&p.width<=320))&&(p.show==0||p.show==1)&&(p.fault==0||p.fault==1)&&p.content>=-1&&p.content<=4;}
}
