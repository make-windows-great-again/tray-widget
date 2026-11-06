#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace NativeTray
{
#ifdef NATIVE_TRAY_SELFTEST
inline constexpr wchar_t MappingName[] = L"Local\\YataTray.SelfTest.v13";
#else
inline constexpr wchar_t MappingName[] = L"Local\\YataNativeFramework.v13";
#endif
inline constexpr wchar_t BrokerClass[] = L"YataNativeFramework.Broker.v13";
inline constexpr wchar_t ModuleName[] = L"YataNativeFramework.Patch.dll";
inline constexpr GUID TapClsid{0x6d772a20,0x37ce,0x4ac8,{0xa1,0x38,0x84,0xcb,0x99,0xb6,0x21,0xf3}};
inline UINT StateMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.State.v13");return m;}
inline UINT ReadyMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Ready.v13");return m;}
inline UINT ReportMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Report.v13");return m;}
inline UINT ReconnectMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Reconnect.v13");return m;}
inline UINT DetachMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Detach.v13");return m;}
inline UINT WorkMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Work.v13");return m;}
inline UINT EventMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Event.v13");return m;}
inline UINT OwnerGoneMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.OwnerGone.v13");return m;}
inline UINT FaultMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Fault.v13");return m;}
inline UINT ErrorMessage(){static UINT m=RegisterWindowMessageW(L"YataTray.Error.v13");return m;}
inline constexpr wchar_t WindowClass[] = L"YataNativeFramework.Window";
inline constexpr wchar_t MutexName[] = L"Local\\YataNativeFramework.CE31FE29";
inline UINT TrayMessage(){static UINT m=RegisterWindowMessageW(L"YataVpn.Tray.Notify.v4");return m;}
inline UINT ShowMessage(){static UINT m=RegisterWindowMessageW(L"YataVpn.Tray.Show.v4");return m;}
inline UINT ExitMessage(){static UINT m=RegisterWindowMessageW(L"YataVpn.Tray.Exit.v4");return m;}
inline UINT CommandMessage(){static UINT m=RegisterWindowMessageW(L"YataVpn.Tray.Command.v4");return m;}
inline constexpr wchar_t Tooltip[] = L"Yata Native Framework\nWindows controls";
inline UINT ModeMessage() { static const UINT value=RegisterWindowMessageW(L"YataVpn.NativeTray.SetMode.v4");return value; }
inline UINT WidthMessage() { static const UINT value=RegisterWindowMessageW(L"YataVpn.NativeTray.SetWidth.v4");return value; }
inline constexpr GUID IconGuid{0xce31fe29, 0x112b, 0x4c67, {0xa3, 0xb0, 0xdc, 0x24, 0x59, 0x42, 0x07, 0x96}};
inline constexpr std::uint32_t Magic = 0x59544e32;
inline constexpr std::uint32_t ProtocolVersion=13;
enum Lifecycle : LONG { Detached=0, Connecting=1, Ready=2, Faulted=3, Stopping=4 };
enum Mode : LONG { Off=0, Label=1, Lines=2, Vpn=Label, Speed=Lines };
inline std::wstring SettingsPath()
{
    wchar_t path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, path);
    return std::wstring(path) + L"YataNativeFramework.layout.ini";
}
inline UINT ReadWidth(LONG mode)
{
    const UINT width = GetPrivateProfileIntW(L"Layout", mode == Lines ? L"LinesWidth" : L"LabelWidth", 0, SettingsPath().c_str());
    return width >= 32 && width <= 320 ? width : 0;
}
inline bool WriteWidth(LONG mode, UINT width)
{
    if (width != 0 && (width < 32 || width > 320)) return false;
    return WritePrivateProfileStringW(L"Layout", mode == Lines ? L"LinesWidth" : L"LabelWidth",
        std::to_wstring(width).c_str(), SettingsPath().c_str()) != FALSE;
}
struct State
{
    std::uint32_t magic;
    DWORD appPid;
    std::uint64_t appWindow;
    volatile LONG mode;
    volatile LONG applied;
    volatile LONG uiThread;
    volatile LONG lastError;
    volatile LONG explorerPid;
    volatile LONG attachResult;
    double actualWidth;
    double actualHeight;
    volatile LONG panelOpen;
    volatile LONG panelPhase;
    DWORD size;
    DWORD version;
    alignas(8) volatile LONG64 session;
    volatile LONG requestSequence;
    volatile LONG requestRevision;
    volatile LONG labelWidth;
    volatile LONG linesWidth;
    volatile LONG themeRevision;
    volatile LONG acknowledgedRevision;
    alignas(8) volatile LONG64 controllerWindow;
    volatile LONG lifecycle;
    volatile LONG subscriptions;
    volatile LONG tooltipTimer;
    volatile LONG pollingTimers;
    alignas(8) volatile LONG64 refreshes;
    volatile LONG64 notifications;
    volatile LONG64 pointerEvents;
    volatile LONG64 chromeUpdates;
    volatile LONG64 tooltipOpens;
    volatile LONG64 callbackErrors;
    volatile LONG64 updateTicks;
    volatile LONG64 maxUpdateTicks;
    volatile LONG64 qpcFrequency;
    volatile LONG64 connection;
    RECT actualBounds;
};
struct Request {LONG mode=Off,panelOpen=0,panelPhase=0,labelWidth=0,linesWidth=0,themeRevision=0,revision=0;};
inline LONG Read(volatile LONG& v) noexcept{return InterlockedCompareExchange(&v,0,0);}
inline LONG64 Read64(volatile LONG64& v) noexcept{return InterlockedCompareExchange64(&v,0,0);}
inline bool ReadRequest(State& state,Request& result) noexcept {
    for(int attempt=0;attempt<8;++attempt){
        LONG before=Read(state.requestSequence);if(before&1)continue;
        Request r{Read(state.mode),Read(state.panelOpen),Read(state.panelPhase),Read(state.labelWidth),Read(state.linesWidth),Read(state.themeRevision),Read(state.requestRevision)};
        MemoryBarrier();if(before==Read(state.requestSequence)){result=r;return true;}
    }return false;
}
inline void Publish(State& s,Request const& r) noexcept {
    InterlockedIncrement(&s.requestSequence);
    InterlockedExchange(&s.mode,r.mode);InterlockedExchange(&s.panelOpen,r.panelOpen);InterlockedExchange(&s.panelPhase,r.panelPhase);
    InterlockedExchange(&s.labelWidth,r.labelWidth);InterlockedExchange(&s.linesWidth,r.linesWidth);InterlockedExchange(&s.themeRevision,r.themeRevision);
    InterlockedExchange(&s.requestRevision,r.revision);InterlockedIncrement(&s.requestSequence);
}
inline bool Valid(State const& s) noexcept{return s.magic==Magic&&s.size==sizeof(State)&&s.version==ProtocolVersion;}
inline void AttachEventName(wchar_t (&name)[96],LONG64 session) noexcept {swprintf_s(name,L"Local\\YataTray.Attach.%llX",static_cast<unsigned long long>(session));}
inline void SignalAttached(State& state) noexcept {wchar_t name[96]{};AttachEventName(name,Read64(state.session));if(HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name)){SetEvent(event);CloseHandle(event);}}
}
