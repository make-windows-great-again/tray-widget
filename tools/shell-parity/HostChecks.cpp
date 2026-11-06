#include "../../source/ShellPanels/ShellPanels.h"
#include <cmath>
#include <cstdio>
#include <psapi.h>
#include <stdexcept>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
ShellPanels::Host *active = nullptr;
LRESULT CALLBACK Window(HWND w, UINT m, WPARAM p, LPARAM l)
{
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT)
    {
        PAINTSTRUCT paint{};
        BeginPaint(w, &paint);
        EndPaint(w, &paint);
        return 0;
    }
    return DefWindowProcW(w, m, p, l);
}
void Pump(DWORD duration)
{
    const auto end = GetTickCount64() + duration;
    do
    {
        MSG m{};
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE))
        {
            if (!active || !active->Process(m))
            {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
        }
        Sleep(1);
    } while (GetTickCount64() < end);
}
void Check(bool passed, char const *name)
{
    if (!passed)
        throw std::runtime_error(name);
    std::printf("PASS %s\n", name);
}
void WaitPhase(ShellPanels::Host &host, ShellPanels::Host::Phase phase)
{
    const auto until = GetTickCount64() + 1500;
    while (host.CurrentPhase() != phase && GetTickCount64() < until)
        Pump(8);
    if (host.CurrentPhase() != phase)
        throw std::runtime_error("composition phase timeout");
}
SIZE_T Memory()
{
    PROCESS_MEMORY_COUNTERS_EX info{};
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&info), sizeof(info));
    return info.PrivateUsage;
}
uint64_t Cpu()
{
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return k.QuadPart + u.QuadPart;
}
int wmain()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        WNDCLASSW c{};
        c.hInstance = GetModuleHandleW(nullptr);
        c.lpfnWndProc = Window;
        c.lpszClassName = L"Yata.ShellPanels.Tests";
        RegisterClassW(&c);
        HWND window =
            CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP, c.lpszClassName,
                            L"ShellPanels checks", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, c.hInstance, nullptr);
        Check(window != nullptr, "independent consumer creates a native host");
        {
            ShellPanels::Host host;
            active = &host;
            unsigned callbacks = 0;
            host.Initialize(window, [&](bool) { ++callbacks; }, [](UINT) {});
            POINT anchor{2200, 1528};
            const bool animation = winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
            for (auto kind : {ShellPanels::Kind::Language, ShellPanels::Kind::QuickSettings, ShellPanels::Kind::Date})
            {
                host.Show(kind, anchor);
                Pump(400);
                Check(host.CurrentPhase() == ShellPanels::Host::Phase::Open,
                      "scheme reaches Open after composition completion");
                RECT before{}, after{};
                GetWindowRect(window, &before);
                for (int pass = 0; pass < 3; ++pass)
                {
                    SendMessageW(window, WM_DISPLAYCHANGE, 32, 0);
                    Pump(40);
                }
                GetWindowRect(window, &after);
                Check(EqualRect(&before, &after), "repeated display relayout does not accumulate anchor offsets");
                SetWindowPos(window, nullptr, before.left, before.top, before.right - before.left + 19,
                             before.bottom - before.top, SWP_NOACTIVATE | SWP_NOZORDER);
                SendMessageW(window, WM_DISPLAYCHANGE, 32, 0);
                Pump(80);
                GetWindowRect(window, &after);
                Check(EqualRect(&before, &after), "sent display message repairs native host geometry");
                host.Deactivated(FindWindowW(L"Shell_TrayWnd", nullptr), true);
                Check(host.Visible(), "tray press retains the visible surface until release");
                host.Toggle(kind, anchor);
                if (animation)
                    Check(host.CurrentPhase() == ShellPanels::Host::Phase::Closing,
                          "tray release starts an exit animation");
                Pump(250);
                Check(!host.Visible(), "tray release completes exactly one close");
                host.Show(kind, anchor);
                Pump(350);
                host.Deactivated(FindWindowW(L"Shell_TrayWnd", nullptr));
                if (animation)
                    Check(host.Visible() && host.CurrentPhase() == ShellPanels::Host::Phase::Closing,
                          "foreign activation keeps surface alive during exit");
                Pump(250);
                Check(!host.Visible(), "outside activation reaches Closed");
                host.Show(kind, anchor);
                Pump(30);
                host.Hide();
                Pump(300);
                Check(!host.Visible() && host.CurrentPhase() == ShellPanels::Host::Phase::Closed,
                      "dismiss during entrance cannot resurrect the panel");
            }
            namespace x = winrt::Windows::UI::Xaml;
            namespace c = x::Controls;
            unsigned actions = 0;
            host.SetHandler([&](ShellPanels::Action, double) { ++actions; });
            host.Show(ShellPanels::Kind::QuickSettings, anchor);
            Pump(350);
            ShellPanels::Model model;
            model.volume = 72;
            model.brightness = 37;
            model.wifiEnabled = false;
            host.SetModel(model);
            Pump(50);
            Check(actions == 0, "live model updates do not echo application commands");
            Check(host.CurrentPhase() == ShellPanels::Host::Phase::Open, "live state update preserves the open panel");
            host.Hide();
            Pump(250);
            for (auto part : {ShellPanels::TrayPart::Network, ShellPanels::TrayPart::Volume,
                              ShellPanels::TrayPart::Battery, ShellPanels::TrayPart::Clock})
            {
                host.ContextMenu(part, anchor);
                Pump(300);
                Check(host.MenuOpen(), "context menu opens with an independent material");
                host.Hide();
                Pump(250);
                Check(!host.MenuOpen(), "context menu releases its popup on Hide");
            }
            c::Button button;
            button.Content(winrt::box_value(L"Invoke"));
            unsigned invoked = 0;
            button.Click([&](auto const &, auto const &) { ++invoked; });
            host.SetContent(button, L"Library content slot", 300);
            host.Show(anchor);
            Pump(350);
            auto peer = x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(button);
            Check(peer != nullptr, "custom content retains its real UI Automation peer");
            peer.GetPattern(x::Automation::Peers::PatternInterface::Invoke)
                .as<x::Automation::Provider::IInvokeProvider>()
                .Invoke();
            Pump(20);
            Check(invoked == 1, "UI Automation invokes the client action once");
            host.Hide();
            Pump(250);
            c::Grid flexible;
            c::Button sectionHeader, sectionBody, sectionFooter;
            sectionHeader.Content(winrt::box_value(L"Section header"));
            sectionHeader.Height(32);
            sectionBody.Content(winrt::box_value(L"Section body"));
            sectionFooter.Content(winrt::box_value(L"Section footer"));
            sectionFooter.Height(24);
            c::Border thirdBody;
            host.SetSections({{nullptr, flexible, nullptr, 0, true},
                              {sectionHeader, sectionBody, sectionFooter, 180},
                              {nullptr, thirdBody, nullptr, 64}});
            host.Show(anchor);
            WaitPhase(host, ShellPanels::Host::Phase::Open);
            Pump(30);
            auto position = [](x::FrameworkElement const &element) {
                return element.TransformToVisual(nullptr).TransformPoint({0, 0}).Y;
            };
            const auto beforeFlexible = flexible.ActualHeight();
            const auto beforeFooter = position(sectionFooter);
            Check(host.SectionExpanded(1) && sectionBody.ActualHeight() == 180,
                  "three independent cards measure fixed and flexible bodies");
            Check(sectionBody.Focus(x::FocusState::Keyboard), "a card body can receive native keyboard focus");
            host.ExpandSection(1, false);
            Pump(55);
            if (animation && x::Media::Animation::Timeline::AllowDependentAnimations())
                Check(flexible.ActualHeight() > beforeFlexible && flexible.ActualHeight() < beforeFlexible + 180,
                      "collapse produces intermediate layout without jumping to the endpoint");
            Check(sectionHeader.FocusState() != x::FocusState::Unfocused,
                  "collapsing a focused body returns focus to its header");
            host.ExpandSection(1, true);
            Pump(350);
            Check(host.SectionExpanded(1) && std::abs(flexible.ActualHeight() - beforeFlexible) < 1,
                  "reversal cancels old completion and restores the requested expansion");
            host.ExpandSection(1, false);
            Pump(350);
            Check(std::abs(flexible.ActualHeight() - beforeFlexible - 180) < 1 &&
                      std::abs(position(sectionFooter) - beforeFooter) < 1,
                  "collapse grows the flexible card while retaining the footer position");
            host.Hide();
            WaitPhase(host, ShellPanels::Host::Phase::Closed);
            host.Show(anchor);
            WaitPhase(host, ShellPanels::Host::Phase::Open);
            Check(!host.SectionExpanded(1) && std::abs(flexible.ActualHeight() - beforeFlexible - 180) < 1,
                  "card state survives hide and reopen");
            host.ExpandSection(1, true, false);
            Pump(20);
            Check(std::abs(flexible.ActualHeight() - beforeFlexible) < 1,
                  "nonanimated update reaches final geometry immediately");
            bool rejected = false;
            try
            {
                host.ExpandSection(0, false);
            }
            catch (winrt::hresult_error const &e)
            {
                rejected = e.code() == E_INVALIDARG;
            }
            Check(rejected && host.SectionExpanded(0), "flexible body cannot collapse into an invalid star layout");
            host.ExpandSection(1, false);
            Pump(30);
            host.Hide();
            WaitPhase(host, ShellPanels::Host::Phase::Closed);
            host.Show(ShellPanels::Kind::QuickSettings, anchor);
            WaitPhase(host, ShellPanels::Host::Phase::Open);
            host.Hide();
            WaitPhase(host, ShellPanels::Host::Phase::Closed);
            Check(true, "switching schema releases a card stack with pending animation safely");
            for (int i = 0; i < 8; ++i)
            {
                host.Show(ShellPanels::Kind::QuickSettings, anchor);
                WaitPhase(host, ShellPanels::Host::Phase::Open);
                host.Hide();
                WaitPhase(host, ShellPanels::Host::Phase::Closed);
            }
            const auto before = Memory();
            const DWORD handlesBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            for (int i = 0; i < 100; ++i)
            {
                host.Show(ShellPanels::Kind::QuickSettings, anchor);
                WaitPhase(host, ShellPanels::Host::Phase::Open);
                host.Hide();
                WaitPhase(host, ShellPanels::Host::Phase::Closed);
            }
            const auto after = Memory();
            const DWORD handlesAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            std::printf("MEASURE cycles=100 private_before=%llu private_after=%llu gdi_before=%lu gdi_after=%lu\n",
                        static_cast<unsigned long long>(before), static_cast<unsigned long long>(after), handlesBefore,
                        handlesAfter);
            Check(after < before + 16 * 1024 * 1024, "100 cycles stay within the bounded memory allowance");
            Check(handlesAfter <= handlesBefore + 2, "100 cycles do not accumulate GDI handles");
            const auto cpu = Cpu();
            Pump(2000);
            std::printf("MEASURE idle_cpu_one_core_percent=%.3f\n", static_cast<double>(Cpu() - cpu) / 200000.0);
            std::printf("MEASURE visibility_callbacks=%u\n", callbacks);
            Check(callbacks >= 200, "visibility transitions are delivered to the consuming application");
            host.Close();
            active = nullptr;
        }
        DestroyWindow(window);
        winrt::uninit_apartment();
        return 0;
    }
    catch (winrt::hresult_error const &e)
    {
        std::printf("FAIL 0x%08X %ls\n", static_cast<unsigned>(e.code().value), e.message().c_str());
    }
    catch (std::exception const &e)
    {
        std::printf("FAIL %s\n", e.what());
    }
    active = nullptr;
    return 1;
}
