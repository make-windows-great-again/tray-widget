#include "TrayCommands.h"
#include "NativeTrayProtocol.h"
#include "ShellEnvironment.h"
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <atomic>
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);std::printf("PASS %s\n",text);}
int main(){try{
    auto c=TrayCommands::Parse({L"--native-speed",L"--native-width=88"});Check(c.error.empty()&&c.mode==2&&c.width==88,"legacy combined mode and width command is one parsed transaction");
    c=TrayCommands::Parse({L"--width",L"160",L"--mode",L"vpn"});Check(c.error.empty()&&c.mode==1&&c.width==160,"option order does not change the target mode");
    for(auto bad:{L"abc",L"-1",L"+88",L"88x",L" 88",L"4294967296",L"31",L"321"})Check(!TrayCommands::Parse({L"--width",bad}).error.empty(),"invalid and overflowing widths are rejected");
    Check(!TrayCommands::Parse({L"--mode"}).error.empty()&&!TrayCommands::Parse({L"--width"}).error.empty(),"missing values are rejected");
    Check(!TrayCommands::Parse({L"--status",L"--attach"}).error.empty()&&!TrayCommands::Parse({L"--attach",L"--mode",L"vpn"}).error.empty(),"service roles cannot accidentally enter application startup");
    Check(TrayCommands::Parse({L"--attach"}).role==TrayCommands::Role::Attach,"attach is an explicit early service role");
    Check(TrayCommands::Parse({L"--help"}).role==TrayCommands::Role::Help&&TrayCommands::Parse({L"--version"}).role==TrayCommands::Role::Version,"help and version never start a UI or attach helper");
    Check(TrayCommands::Parse({L"--controls",L"--mode",L"label"}).content==1&&TrayCommands::Parse({L"--sample"}).content==0,"generic content selection is independent from tray presentation");
    Check(!TrayCommands::Parse({L"--status",L"--controls"}).error.empty(),"service roles reject content selection");
    Check(TrayCommands::Parse({L"--standalone",L"--controls"}).standalone&&!TrayCommands::Parse({L"--status",L"--standalone"}).error.empty(),"standalone is a startup option and cannot alter a service role");
    Check(TrayCommands::Parse({L"--diagnose"}).role==TrayCommands::Role::Diagnose&&!TrayCommands::Parse({L"--diagnose",L"--show"}).error.empty(),"desktop diagnostics are a side-effect-free service role");
    TrayEnvironment::Snapshot environment;environment.desktop=L"Isolated";environment.inputDesktop=L"Default";
    Check(TrayEnvironment::Classify(environment)==TrayEnvironment::Availability::OtherDesktop,"an isolated desktop without Explorer is diagnosed before patch loading");
    environment.desktop=L"Default";
    Check(TrayEnvironment::Classify(environment)==TrayEnvironment::Availability::NoShell,"an absent shell on the current desktop is distinguished from desktop isolation");
    environment.taskbar=reinterpret_cast<HWND>(1);environment.shellPid=42;environment.inputDesktop=L"Winlogon";
    Check(TrayEnvironment::Classify(environment)==TrayEnvironment::Availability::Ready,"a shell on the current desktop remains usable when the input desktop changes");
    environment.taskbar=nullptr;environment.desktop=L"unknown";
    Check(TrayEnvironment::Classify(environment)==TrayEnvironment::Availability::NoShell,"missing desktop-name permission does not fabricate a desktop mismatch");
    Check(TrayCommands::Parse({L"--lang"}).content==2&&TrayCommands::Parse({L"--quick"}).content==3&&TrayCommands::Parse({L"--date"}).content==4,"shell schemas are independent content choices");
    TrayCommands::Packet packet;Check(TrayCommands::Valid(packet),"default command packet is accepted");packet.content=5;Check(!TrayCommands::Valid(packet),"untrusted content indices are rejected");packet.content=4;Check(TrayCommands::Valid(packet),"last supported content index is accepted");packet.content=1;packet.version=1;Check(!TrayCommands::Valid(packet),"old command packet versions are rejected");
    NativeTray::State state{};state.magic=NativeTray::Magic;state.size=sizeof(state);state.version=NativeTray::ProtocolVersion;
    Check(NativeTray::Valid(state),"protocol size and version are validated");state.version++;Check(!NativeTray::Valid(state),"incompatible protocols are rejected");
    std::atomic_bool done=false;std::atomic_bool failed=false;
    std::thread writer([&]{for(LONG n=1;n<=100000;++n){NativeTray::Request r;r.mode=n%3;r.labelWidth=n;r.linesWidth=n;r.revision=n;NativeTray::Publish(state,r);}done=true;});
    while(!done){NativeTray::Request r;if(NativeTray::ReadRequest(state,r)&&(r.labelWidth!=r.linesWidth||r.revision!=r.labelWidth))failed=true;}
    writer.join();Check(!failed,"concurrent snapshot readers never observe mixed settings revisions");
    Check(offsetof(NativeTray::State,panelOpen)==56&&offsetof(NativeTray::State,session)%8==0,"diagnostic prefix and interlocked alignment remain stable");
    std::puts("All CLI and protocol checks passed");return 0;
}catch(std::exception const& e){std::printf("FAIL %s\n",e.what());return 1;}}
