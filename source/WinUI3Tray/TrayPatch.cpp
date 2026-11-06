#include <windows.h>
#include <ocidl.h>
#include <shellapi.h>
#include <xamlom.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <chrono>
#include <functional>
#include <mutex>
#include <string_view>
#include "TrayProtocol.h"

namespace wux = winrt::Windows::UI::Xaml;
namespace controls = wux::Controls;
using XamlObject = winrt::Windows::Foundation::IInspectable;
constexpr CLSID TapClsid=NativeTray::TapClsid;

namespace
{
void Log(std::wstring_view message) noexcept
{
    try{
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring path = std::wstring(temp) +
#ifdef NATIVE_TRAY_SELFTEST
        L"YataWinUI3TraySelfTest.log";
#else
        L"YataWinUI3TrayPatch.log";
#endif
    const HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t prefix[96]{};
        swprintf_s(prefix, L"%02u:%02u:%02u pid=%lu tid=%lu ", time.wHour, time.wMinute, time.wSecond,
            GetCurrentProcessId(), GetCurrentThreadId());
        const std::wstring line = std::wstring(prefix) + std::wstring(message) + L"\r\n";
        DWORD written = 0;
        WriteFile(file, line.data(), static_cast<DWORD>(line.size() * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(file);
    }
    }catch(...){}
}

struct SavedValue
{
    wux::DependencyObject object{nullptr};
    wux::DependencyProperty property{nullptr};
    XamlObject local{nullptr};
    wux::Data::BindingBase binding{nullptr};

    SavedValue(wux::DependencyObject const& target, wux::DependencyProperty const& key)
        : object(target), property(key), local(target.ReadLocalValue(key))
    {
        if (const auto framework = target.try_as<wux::FrameworkElement>())
            if (const auto expression = framework.GetBindingExpression(key)) binding = expression.ParentBinding();
    }
    void Restore() const
    {
        if (binding) wux::Data::BindingOperations::SetBinding(object, property, binding);
        else if (local == wux::DependencyProperty::UnsetValue()) object.ClearValue(property);
        else object.SetValue(property, local);
    }
};

controls::ContentPresenter FindPresenter(wux::DependencyObject const& parent, int depth = 0)
{
    if (depth > 12) return nullptr;
    const int count = wux::Media::VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; ++i)
    {
        const auto child = wux::Media::VisualTreeHelper::GetChild(parent, i);
        if (auto presenter = child.try_as<controls::ContentPresenter>(); presenter && presenter.Name() == L"ContentPresenter")
            return presenter;
        if (auto found = FindPresenter(child, depth + 1)) return found;
    }
    return nullptr;
}

void DumpTree(wux::DependencyObject const& node, int depth = 0)
{
    if (depth > 10) return;
    std::wstring line(static_cast<size_t>(depth) * 2, L' ');
    line += winrt::get_class_name(node);
    if (auto fe = node.try_as<wux::FrameworkElement>())
        line += L"#" + std::wstring(fe.Name()) + L" actual=" + std::to_wstring(fe.ActualWidth()) + L"x" + std::to_wstring(fe.ActualHeight()) +
            L" width=" + std::to_wstring(fe.Width()) + L" min=" + std::to_wstring(fe.MinWidth()) + L" max=" + std::to_wstring(fe.MaxWidth());
    Log(line);
    const int count = wux::Media::VisualTreeHelper::GetChildrenCount(node);
    for (int index = 0; index < count; ++index) DumpTree(wux::Media::VisualTreeHelper::GetChild(node, index), depth + 1);
}

controls::TextBlock FindClock(wux::DependencyObject const& node, int depth = 0)
{
    if(depth>24) return nullptr;
    if(auto text=node.try_as<controls::TextBlock>();text && text.Name()==L"TimeInnerTextBlock")return text;
    const int count=wux::Media::VisualTreeHelper::GetChildrenCount(node);
    for(int i=0;i<count;++i)if(auto found=FindClock(wux::Media::VisualTreeHelper::GetChild(node,i),depth+1))return found;
    return nullptr;
}

void UseClockTypography(wux::DependencyObject const& node,controls::TextBlock const& clock)
{
    if(auto text=node.try_as<controls::TextBlock>())
    {
        text.Style(clock.Style());
        text.FontFamily(clock.FontFamily());text.FontSize(clock.FontSize());
        text.FontWeight(clock.FontWeight());text.FontStyle(clock.FontStyle());text.FontStretch(clock.FontStretch());
        // Foreground deliberately remains inherited from ContentPresenter.
    }
    const int count=wux::Media::VisualTreeHelper::GetChildrenCount(node);
    for(int i=0;i<count;++i)UseClockTypography(wux::Media::VisualTreeHelper::GetChild(node,i),clock);
}

void BindNativeForeground(wux::DependencyObject const& node,controls::ContentPresenter const& presenter)
{
    if(auto text=node.try_as<controls::TextBlock>())
    {
        wux::Data::Binding binding;
        binding.Source(presenter);binding.Path(wux::PropertyPath(L"Foreground"));
        binding.Mode(wux::Data::BindingMode::OneWay);
        text.SetBinding(controls::TextBlock::ForegroundProperty(),binding);
    }
    const int count=wux::Media::VisualTreeHelper::GetChildrenCount(node);
    for(int i=0;i<count;++i)BindNativeForeground(wux::Media::VisualTreeHelper::GetChild(node,i),presenter);
}

#include "../NativeTray/NativeChrome.h"
#include "../NativeTray/NativeTooltip.h"

struct Decoration
{
    wux::FrameworkElement view{nullptr};
    controls::ContentPresenter presenter{nullptr};
    XamlObject replacement{nullptr};
    controls::Grid contentGrid{nullptr};
    std::vector<wux::UIElement> originalChildren;
    NativeChrome chrome;
    NativeTooltip tooltip;
    std::vector<SavedValue> saved;
    LONG mode = NativeTray::Off;
    UINT requestedWidth = 0;

    bool IsCurrent() const
    {
        if(!presenter)return false;
        if(contentGrid)return presenter.Content()==contentGrid && contentGrid.Children().Size()==1 && contentGrid.Children().GetAt(0)==replacement;
        return presenter.Content()==replacement;
    }

    void SetPanelOpen(bool value)
    {
        chrome.SetActive(value);
    }

    void Restore() noexcept
    {
        try {
            chrome.Close();
            tooltip.Close();
            if(contentGrid){contentGrid.Children().Clear();for(auto const& child:originalChildren)contentGrid.Children().Append(child);}
        }catch(...){Log(L"Restore native content grid failed");}
        originalChildren.clear();contentGrid=nullptr;
        for (auto it = saved.rbegin(); it != saved.rend(); ++it)
        {
            try { it->Restore(); } catch (...) { Log(L"Restore property failed"); }
        }
        saved.clear();
        view = nullptr;
        presenter = nullptr;
        replacement = nullptr;
        mode = NativeTray::Off;
    }

    void Apply(wux::FrameworkElement const& element, LONG nextMode, UINT width = 0)
    {
        // The WinUI 3 client can change width continuously. Keep the existing
        // native subtree, hover/pressed chrome and bindings while it does so.
        if(view==element && mode==nextMode && IsCurrent()){
            Resize(width);
            return;
        }
        Restore();
        const auto content = FindPresenter(element);
        if (!content) { Log(L"Apply: ContentPresenter was not found"); return; }
        view = element;
        presenter = content;
        saved.emplace_back(view, wux::FrameworkElement::WidthProperty());
        saved.emplace_back(view, wux::FrameworkElement::MinWidthProperty());
        saved.emplace_back(view, wux::FrameworkElement::MaxWidthProperty());
        contentGrid=presenter.Content().try_as<controls::Grid>();
        if(contentGrid && contentGrid.Name()==L"ContentGrid")
        {
            for(auto const& child:contentGrid.Children())originalChildren.push_back(child);
        }
        else {contentGrid=nullptr;saved.emplace_back(presenter, controls::ContentPresenter::ContentProperty());}
        saved.emplace_back(presenter, wux::FrameworkElement::HorizontalAlignmentProperty());
        saved.emplace_back(presenter, wux::FrameworkElement::MarginProperty());
        // Match the clock's caption metrics; keep native foreground inheritance.
        const wchar_t* xaml = nextMode == NativeTray::Lines
            ? LR"(<StackPanel xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" IsHitTestVisible="False" Orientation="Vertical" Margin="0,-2,-1,0" HorizontalAlignment="Center" VerticalAlignment="Center">
<Grid><Grid.ColumnDefinitions><ColumnDefinition Width="12"/><ColumnDefinition Width="24"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
<TextBlock Text="&#x2191;" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False"/>
<TextBlock Grid.Column="1" Text="100" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False" TextAlignment="Right" Typography.NumeralAlignment="Tabular"/>
<TextBlock Grid.Column="2" Text="Мбит/с" Margin="4,0,0,0" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False"/></Grid>
<Grid><Grid.ColumnDefinitions><ColumnDefinition Width="12"/><ColumnDefinition Width="24"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
<TextBlock Text="&#x2193;" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False"/>
<TextBlock Grid.Column="1" Text="100" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False" TextAlignment="Right" Typography.NumeralAlignment="Tabular"/>
<TextBlock Grid.Column="2" Text="Кбит/с" Margin="4,0,0,0" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False"/></Grid></StackPanel>)"
            : LR"(<TextBlock xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" IsHitTestVisible="False" Text="VPN" Style="{StaticResource CaptionTextBlockStyle}" IsTextScaleFactorEnabled="False" HorizontalAlignment="Center" VerticalAlignment="Center"/>)";
        replacement = wux::Markup::XamlReader::Load(xaml);
        BindNativeForeground(replacement.as<wux::DependencyObject>(),presenter);
        if(auto root=view.XamlRoot();root && root.Content())
            if(auto clock=FindClock(root.Content()))
            {
                UseClockTypography(replacement.as<wux::DependencyObject>(),clock);
                Log(L"Clock typography family="+std::wstring(clock.FontFamily().Source())+L" size="+std::to_wstring(clock.FontSize()));
            }
        mode=nextMode;
        Resize(width);
        presenter.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
        presenter.Margin({4, 0, 4, 0});
        if(contentGrid){contentGrid.Children().Clear();contentGrid.Children().Append(replacement.as<wux::UIElement>());}
        else presenter.Content(replacement);
        chrome.Attach(view,presenter);
        tooltip.Attach(view);
        mode = nextMode;
        requestedWidth = width;
        Log(L"Applied mode=" + std::to_wstring(mode) + L" requestedWidth=" + std::to_wstring(width) + L" name=" + std::wstring(wux::Automation::AutomationProperties::GetName(view)));
    }

    void Resize(UINT width)
    {
        if(width!=0 && (width<32 || width>320))winrt::throw_hresult(E_INVALIDARG);
        view.MinWidth(width ? static_cast<double>(width) : mode==NativeTray::Lines ? 104.0 : 44.0);
        view.MaxWidth(width ? static_cast<double>(width) : std::numeric_limits<double>::infinity());
        view.Width(width ? static_cast<double>(width) : std::numeric_limits<double>::quiet_NaN());
        requestedWidth=width;
    }
};

#include "../NativeTray/NativeEventBackend.h"
}
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void** value)
{
    if (!value) return E_POINTER;
    *value = nullptr;
    if (clsid != TapClsid) return CLASS_E_CLASSNOTAVAILABLE;
    try { return winrt::make<Factory>().as(iid, value); } catch (...) { return winrt::to_hresult(); }
}

// Windows XAML diagnostics pins a loaded TAP. "Off" restores all local values;
// the inert DLL is released by Windows when Explorer exits.
extern "C" HRESULT __stdcall DllCanUnloadNow() { return S_FALSE; }

// Initialize diagnostics on a fresh thread inside the shell. A Windows hook is
// kept only until the TAP acknowledges subscription; it never handles clicks.
DWORD WINAPI InstallDiagnostics(void*) noexcept
{
    HMODULE xaml=nullptr;HRESULT result=E_FAIL;
    try{
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&InstallDiagnostics), &module);
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    xaml = LoadLibraryExW(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!xaml) return GetLastError();
    const auto initialize = reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(xaml, "InitializeXamlDiagnosticsEx"));
    if (!initialize){FreeLibrary(xaml);return ERROR_PROC_NOT_FOUND;}
    for (int endpoint = 1; endpoint <= 32; ++endpoint)
    {
        const std::wstring connection = L"VisualDiagConnection" + std::to_wstring(endpoint);
        std::thread([&]() noexcept {try{result = initialize(connection.c_str(), GetCurrentProcessId(), nullptr, path, TapClsid, nullptr);}catch(...){result=winrt::to_hresult();}}).join();
        Log(L"In-process diagnostics " + connection + L" HRESULT=" + std::to_wstring(static_cast<unsigned long>(result)));
        if (SUCCEEDED(result)) break;
    }
    }catch(...){result=winrt::to_hresult();Log(L"Diagnostics bootstrap failed");}
    if(xaml)FreeLibrary(xaml);
    return static_cast<DWORD>(result);
}

extern "C" LRESULT CALLBACK NativeTrayHook(int code, WPARAM wp, LPARAM lp) noexcept
{
    try{
    static std::atomic_flag started = ATOMIC_FLAG_INIT;
    if (code >= 0 && !started.test_and_set())
    {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&NativeTrayHook), &module);
        Log(L"NativeTrayHook entered");
        bool initialized=false;{std::lock_guard lock(runtimeMutex);initialized=runtimeDiagnostics!=nullptr;}
        if(!initialized)if (const HANDLE thread = CreateThread(nullptr, 0, InstallDiagnostics, nullptr, 0, nullptr)) CloseHandle(thread);
    }
    }catch(...){Log(L"Native hook bootstrap failed");}
    return CallNextHookEx(nullptr, code, wp, lp);
}
