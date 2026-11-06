#include "../../source/ShellPanels/ShellPanels.h"
#include <cstdio>
#include <shellapi.h>
#include <string>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/base.h>
std::unique_ptr<ShellPanels::Host> panels;
HWND hostWindow = nullptr, backWindow = nullptr;
bool dark = false;
ShellPanels::Model benchModel;
unsigned actionCount = 0;
POINT measuredAnchor{2264, 1528};
bool useMeasuredAnchor = false;
bool keepOpenForDpiTest = false;
void ShowCards()
{
    if (panels->Visible())
    {
        panels->Hide();
        return;
    }
    namespace x = winrt::Windows::UI::Xaml;
    namespace c = x::Controls;
    auto caption = [](wchar_t const *value, double height) {
        c::TextBlock text;
        text.Text(value);
        text.FontSize(14);
        text.Height(height);
        text.Margin({16, 0, 16, 0});
        text.VerticalAlignment(x::VerticalAlignment::Center);
        return text;
    };
    c::ListView activity;
    for (int i = 1; i <= 30; ++i)
        activity.Items().Append(winrt::box_value(L"Activity " + std::to_wstring(i)));
    c::Button toggle;
    toggle.Content(winrt::box_value(L"Tasks — collapse / expand"));
    toggle.Height(48);
    toggle.HorizontalAlignment(x::HorizontalAlignment::Stretch);
    toggle.Click([](auto const &, auto const &) { panels->ExpandSection(1, !panels->SectionExpanded(1)); });
    c::StackPanel controls;
    controls.Margin({16, 8, 16, 8});
    controls.Spacing(12);
    c::ToggleSwitch enabled;
    enabled.Header(winrt::box_value(L"Local example"));
    c::Slider amount;
    amount.Minimum(0);
    amount.Maximum(100);
    amount.Value(50);
    controls.Children().Append(enabled);
    controls.Children().Append(amount);
    panels->SetSections(
        {{caption(L"Activity", 48), activity, nullptr, 0, true},
         {toggle, caption(L"This body has its own expansion state.", 128), caption(L"Footer stays visible", 32), 128},
         {caption(L"Controls", 48), controls, nullptr, 128}});
    panels->Show({2488, 1528});
}
LRESULT CALLBACK HostProc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    try
    {
        if (msg == WM_ERASEBKGND)
            return 1;
        if (msg == WM_NCHITTEST)
            return HTCLIENT;
        if (msg == WM_PAINT)
        {
            PAINTSTRUCT p{};
            BeginPaint(w, &p);
            EndPaint(w, &p);
            return 0;
        }
        if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE && panels && !keepOpenForDpiTest)
        {
            if (reinterpret_cast<HWND>(lp) == backWindow)
                panels->Hide();
            else
                panels->Deactivated(reinterpret_cast<HWND>(lp));
            return 0;
        }
        if (msg == WM_APP + 1 && panels)
        {
            POINT p = measuredAnchor;
            panels->Toggle(static_cast<ShellPanels::Kind>(wp), p);
            return 0;
        }
        if (msg == WM_APP + 2 && panels)
        {
            POINT p{static_cast<LONG>(wp == 1 ? 2363 : 2488), 1528};
            if (useMeasuredAnchor)
                p = measuredAnchor;
            panels->ContextMenu(static_cast<ShellPanels::Kind>(wp), p);
            return 0;
        }
        if (msg == WM_APP + 3 && panels)
        {
            panels->Hide();
            return 0;
        }
        if (msg == WM_APP + 4 && panels)
        {
            return panels->PhaseValue();
        }
        if (msg == WM_APP + 5 && panels)
        {
            POINT p{static_cast<LONG>(wp == 1 ? 2320 : 2408), 1564};
            if (useMeasuredAnchor)
                p = measuredAnchor;
            panels->ContextMenu(static_cast<ShellPanels::TrayPart>(wp), p);
            return 0;
        }
        if (msg == WM_APP + 6 && panels)
        {
            return wp == 0   ? actionCount
                   : wp == 1 ? benchModel.volume
                   : wp == 2 ? benchModel.wifiEnabled
                             : panels->MenuOpen();
        }
        if (msg == WM_APP + 7 && panels)
        {
            benchModel.volume = static_cast<int>(wp);
            panels->SetModel(benchModel);
            return 0;
        }
        if (msg == WM_APP + 8 && panels)
        {
            ShowCards();
            return 0;
        }
        if (msg == WM_APP + 11)
        {
            measuredAnchor = {static_cast<LONG>(wp), static_cast<LONG>(lp)};
            useMeasuredAnchor = true;
            return 0;
        }
        if (msg == WM_APP + 13)
        {
            keepOpenForDpiTest = wp != 0;
            return 0;
        }
        if (msg == WM_CLOSE)
        {
            if (panels)
                panels->Hide();
            return 0;
        }
        if (msg == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
    }
    catch (winrt::hresult_error const &e)
    {
        wchar_t line[512]{};
        swprintf_s(line, L"0x%08X %s\n", static_cast<unsigned>(e.code().value), e.message().c_str());
        OutputDebugStringW(line);
        FILE *f = nullptr;
        _wfopen_s(&f, L"shell-bench-error.txt", L"a, ccs=UTF-8");
        if (f)
        {
            fputws(line, f);
            fclose(f);
        }
    }
    return DefWindowProcW(w, msg, wp, lp);
}
LRESULT CALLBACK Background(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_PAINT)
    {
        PAINTSTRUCT p{};
        auto dc = BeginPaint(w, &p);
        RECT r{};
        GetClientRect(w, &r);
        auto b = CreateSolidBrush(RGB(96, 96, 96));
        FillRect(dc, &r, b);
        DeleteObject(b);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(240, 240, 240));
        DrawTextW(dc,
                  L"C++ ShellPanels bench   |   1 Language   2 Quick Settings   3 Date   4 Cards\nRight click: context "
                  L"menu   |  "
                  L" Esc: close panel   |   F10: quit",
                  -1, &r, DT_TOP | DT_LEFT);
        EndPaint(w, &p);
        return 0;
    }
    if (m == WM_KEYDOWN)
    {
        if (wp >= '1' && wp <= '3')
            PostMessageW(hostWindow, WM_APP + 1, wp - '1', 0);
        if (wp == '4')
            PostMessageW(hostWindow, WM_APP + 8, 0, 0);
        if (wp == VK_F10)
            PostQuitMessage(0);
        return 0;
    }
    if (m == WM_RBUTTONUP)
    {
        PostMessageW(hostWindow, WM_APP + 2, 1, 0);
        return 0;
    }
    if (m == WM_CLOSE)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command, int)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        WNDCLASSW c{};
        c.hInstance = instance;
        c.lpfnWndProc = HostProc;
        c.lpszClassName = L"Yata.ShellBench.Host";
        RegisterClassW(&c);
        hostWindow =
            CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, c.lpszClassName,
                            L"ShellPanels test host", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
        c.lpszClassName = L"Yata.ShellBench.Background";
        c.lpfnWndProc = Background;
        RegisterClassW(&c);
        backWindow = CreateWindowExW(0, c.lpszClassName, L"C++ ShellPanels test bench", WS_POPUP, 1440, 0, 1120, 1528,
                                     nullptr, nullptr, instance, nullptr);
        ShowWindow(backWindow, SW_SHOW);
        SetForegroundWindow(backWindow);
        panels = std::make_unique<ShellPanels::Host>(hostWindow, [](ShellPanels::Action action, double value) {
            ++actionCount;
            switch (action)
            {
            case ShellPanels::Action::NetworkToggle:
                benchModel.wifiEnabled = value != 0;
                break;
            case ShellPanels::Action::BluetoothToggle:
                benchModel.bluetoothEnabled = value != 0;
                break;
            case ShellPanels::Action::Airplane:
                benchModel.airplaneEnabled = value != 0;
                break;
            case ShellPanels::Action::EnergySaver:
                benchModel.energySaverEnabled = value != 0;
                break;
            case ShellPanels::Action::Volume:
                benchModel.volume = static_cast<int>(value);
                break;
            case ShellPanels::Action::Brightness:
                benchModel.brightness = static_cast<int>(value);
                break;
            case ShellPanels::Action::LanguageSelected:
                benchModel.selectedLanguage = static_cast<int>(value);
                break;
            default:
                return;
            }
            panels->SetModel(benchModel);
        });
        auto &model = benchModel;
        model.light = std::wstring(command).find(L"--dark") == std::wstring::npos;
        model.mediaVisible = true;
        model.notifications = {
            {L"Example application", L"Notification title",
             L"A notification body spanning two lines in the reference layout.", L"3:42 PM", L"Dismiss", 1},
            {L"Another application", L"Update ready", L"The new version is ready to install.", L"6:53 AM", L"", 10}};
        if (std::wstring(command).find(L"--custom") != std::wstring::npos)
        {
            model.light = false;
            model.tintArgb = 0xFF183F50;
            model.accentArgb = 0xFF65C8CC;
            model.luminosity = .8;
        }
        panels->SetModel(model);
        MSG m{};
        while (GetMessageW(&m, nullptr, 0, 0) > 0)
        {
            if (!panels->Process(m))
            {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
        }
        panels.reset();
        DestroyWindow(hostWindow);
        DestroyWindow(backWindow);
        winrt::uninit_apartment();
        return 0;
    }
    catch (winrt::hresult_error const &e)
    {
        MessageBoxW(nullptr, e.message().c_str(), L"ShellBench failed", MB_OK | MB_ICONERROR);
        return 1;
    }
}
