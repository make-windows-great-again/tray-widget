// Exercises the real decoration/restoration code in a separate desktop XAML island.
#define NATIVE_TRAY_SELFTEST
#include "NativeTrayTap.cpp"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Documents.h>
#include <winrt/Windows.UI.Text.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <cstdio>
#include <stdexcept>

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
    std::printf("PASS %s\n", message);
}

void PumpFor(ULONGLONG milliseconds)
{
    const auto until=GetTickCount64()+milliseconds;
    do{
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }while(GetTickCount64()<until);
}

void VerifyVisibleTooltipPlacement()
{
    MONITORINFO monitor{sizeof(MONITORINFO)};
    winrt::check_bool(GetMonitorInfoW(MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY),&monitor));
    const HWND host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Tooltip geometry verification",
        WS_POPUP,monitor.rcWork.right-400,monitor.rcWork.bottom,200,72,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    winrt::check_bool(host!=nullptr);
    wux::Hosting::DesktopWindowXamlSource island;
    auto native=island.as<IDesktopWindowXamlSourceNative>();native->AttachToWindow(host);
    HWND child=nullptr;winrt::check_hresult(native->get_WindowHandle(&child));
    controls::Grid root;root.Height(48);island.Content(root);
    const double scale=GetDpiForWindow(host)/96.0;
    for(const double width:{44.0,88.0,160.0}){
        const auto pixelWidth=static_cast<int>(std::lround(width*scale));
        const auto pixelHeight=static_cast<int>(std::lround(48*scale));
        root.Width(width);
        SetWindowPos(host,nullptr,monitor.rcWork.right-400,monitor.rcWork.bottom,pixelWidth,pixelHeight,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        SetWindowPos(child,nullptr,0,0,pixelWidth,pixelHeight,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        root.UpdateLayout();PumpFor(100);
        NativeTooltip tooltip;tooltip.Attach(root);Require(tooltip.ShowOwned(),"owned tooltip opens without an automatic pointer placement");PumpFor(250);
        struct PopupResult{RECT rect{};int count=0;} result;
        EnumDesktopWindows(nullptr,[](HWND window,LPARAM data)->BOOL{
            DWORD pid=0;GetWindowThreadProcessId(window,&pid);wchar_t name[128]{};GetClassNameW(window,name,128);
            if(pid==GetCurrentProcessId() && IsWindowVisible(window) && std::wstring_view(name)==L"Xaml_WindowedPopupClass"){
                auto& found=*reinterpret_cast<PopupResult*>(data);GetWindowRect(window,&found.rect);++found.count;
            }return TRUE;
        },reinterpret_cast<LPARAM>(&result));
        RECT anchor{};GetWindowRect(host,&anchor);
        std::printf("tooltip width=%.0f DIP anchorTop=%ld popup=%ld,%ld,%ld,%ld\n",width,anchor.top,
            result.rect.left,result.rect.top,result.rect.right,result.rect.bottom);
        Require(result.count==1,"opening a loaded tooltip creates exactly one visible native popup");
        Require(result.rect.bottom<anchor.top && result.rect.top>=monitor.rcWork.top,
            "visible tooltip including its shadow stays above the taskbar in the work area");
        Require(std::abs((result.rect.left+result.rect.right)-(anchor.left+anchor.right))<=3,
            "tooltip remains centered over the complete tray button");
        tooltip.Close();PumpFor(200);
    }
    island.Close();DestroyWindow(host);
}

#include "ControllerSelfTest.h"

int wmain()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        const auto manager = wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        const HWND host = CreateWindowExW(0, L"STATIC", L"Native tray verification", WS_OVERLAPPEDWINDOW,
            0, 0, 500, 120, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        winrt::check_bool(host != nullptr);
        {
            wux::Hosting::DesktopWindowXamlSource island;
            island.as<IDesktopWindowXamlSourceNative>()->AttachToWindow(host);
            controls::Grid root;
            controls::Border background;
            background.Name(L"BackgroundBorder");
            background.CornerRadius({4,4,4,4});
            background.Opacity(0.75);
            root.Children().Append(background);
            controls::ContentPresenter presenter;
            presenter.Name(L"ContentPresenter");
            controls::Grid original;
            original.Name(L"ContentGrid");
            controls::TextBlock bitmapPlaceholder;
            bitmapPlaceholder.Text(L"original icon content");
            original.Children().Append(bitmapPlaceholder);
            presenter.Content(original);
            root.Children().Append(presenter);
            root.MinWidth(32);
            root.Height(40);
            auto originalTooltip=winrt::box_value(L"original native tooltip");
            controls::ToolTipService::SetToolTip(root,originalTooltip);
            island.Content(root);
            const auto originalWidth = root.ReadLocalValue(wux::FrameworkElement::WidthProperty());
            const auto originalMax = root.ReadLocalValue(wux::FrameworkElement::MaxWidthProperty());
            const auto originalAlignment = presenter.ReadLocalValue(wux::FrameworkElement::HorizontalAlignmentProperty());
            Decoration decoration;
            auto tooltipFill=wux::Media::SolidColorBrush(winrt::Windows::UI::Color{255,245,241,233});
            auto tooltipText=wux::Media::SolidColorBrush(winrt::Windows::UI::Color{255,25,25,25});
            auto tooltipStroke=wux::Media::SolidColorBrush(winrt::Windows::UI::Color{40,0,0,0});
            root.Resources().Insert(winrt::box_value(L"ToolTipBackgroundBrush"),tooltipFill);
            root.Resources().Insert(winrt::box_value(L"ToolTipForegroundBrush"),tooltipText);
            root.Resources().Insert(winrt::box_value(L"ToolTipBorderBrush"),tooltipStroke);
            decoration.Apply(root, NativeTray::Label, 44);
            Require(root.Width() == 44 && root.MinWidth() == 44, "VPN reserves 44 DIP");
            Require(controls::ToolTipService::GetToolTip(root)==decoration.tooltip.tip &&
                controls::ToolTipService::GetPlacement(root)==controls::Primitives::PlacementMode::Top,
                "a single owned tooltip is registered above the decorated icon");
            Require(decoration.tooltip.tip.Placement()==controls::Primitives::PlacementMode::Top &&
                decoration.tooltip.tip.PlacementTarget()==root,
                "tooltip is anchored to the complete tray button");
            Require(decoration.tooltip.tip.FontSize()==12 && decoration.tooltip.tip.Padding().Left==8 && decoration.tooltip.tip.Padding().Top==5,
                "tooltip keeps native caption typography and padding");
            Require(decoration.tooltip.tip.Background()==tooltipFill && decoration.tooltip.tip.Foreground()==tooltipText &&
                decoration.tooltip.tip.BorderBrush()==tooltipStroke,"tooltip inherits the shell palette from its owner resource scope");
            Require(decoration.replacement.as<controls::TextBlock>().Text() == L"UI", "label uses a real XAML TextBlock");
            Require(presenter.Content()==original && original.Children().Size()==1 && original.Children().GetAt(0)==decoration.replacement,
                "native ContentGrid identity and transform target are preserved");
            Require(!decoration.replacement.as<wux::UIElement>().IsHitTestVisible(), "replacement leaves pointer handling to outer control");
            root.Measure({500, 40});
            root.Arrange({0, 0, 44, 40});
            Require(root.ActualWidth() == 44, "VPN measures and arranges at 44 DIP");
            decoration.Apply(root, NativeTray::Lines, 104);
            Require(root.Width() == 104, "speed mode reserves 104 DIP");
            const auto rows = decoration.replacement.as<controls::StackPanel>();
            Require(rows.Children().Size() == 2, "speed mode contains two text rows");
            const auto row=rows.Children().GetAt(0).as<controls::Grid>();
            const auto number=row.Children().GetAt(1).as<controls::TextBlock>();
            Require(number.FontSize()==12 && row.ColumnDefinitions().Size()==3,"caption metrics and separate direction/number/unit columns");
            Require(wux::Documents::Typography::GetNumeralAlignment(number)==wux::FontNumeralAlignment::Tabular,
                "numeric column uses tabular figures");
            auto stateBrush=wux::Media::SolidColorBrush(winrt::Windows::UI::Color{255,12,34,56});
            presenter.Foreground(stateBrush);
            root.UpdateLayout();
            Require(number.Foreground()==stateBrush,"native presenter foreground propagates to custom text");
            decoration.SetPanelOpen(true);
            Require(decoration.chrome.overlay && !decoration.chrome.overlay.IsHitTestVisible() && background.Opacity()==0,
                "open panel owns a separate selected background without intercepting clicks");
            Require(decoration.chrome.fills[0]!=decoration.chrome.fills[1] && decoration.chrome.fills[1]!=decoration.chrome.fills[2],
                "selected normal hover and pressed states have separate native brushes");
            decoration.SetPanelOpen(false);
            Require(background.Opacity()==0.75 && presenter.Foreground()==stateBrush &&
                decoration.chrome.overlay.Visibility()==wux::Visibility::Collapsed,
                "closing restores native hover chrome and foreground");
            for(int i=0;i<10;++i){decoration.SetPanelOpen(true);decoration.SetPanelOpen(false);}
            Require(background.Opacity()==0.75 && presenter.Foreground()==stateBrush,
                "repeated open close retains the original native highlight");
            decoration.Restore();
            Require(controls::ToolTipService::GetToolTip(root)==originalTooltip,"original tooltip is restored on mode restoration");
            Require(presenter.Content() == original, "original content object is restored");
            Require(original.Children().Size()==1 && original.Children().GetAt(0)==bitmapPlaceholder,"original ContentGrid children are restored");
            Require(root.MinWidth() == 32 && root.ReadLocalValue(wux::FrameworkElement::WidthProperty()) == originalWidth &&
                root.ReadLocalValue(wux::FrameworkElement::MaxWidthProperty()) == originalMax, "original local dimensions and unset values are restored");
            Require(presenter.ReadLocalValue(wux::FrameworkElement::HorizontalAlignmentProperty()) == originalAlignment,
                "original alignment local value is restored");
            decoration.Apply(root, NativeTray::Label, 88);
            root.Measure({500, 40});
            root.Arrange({0, 0, 88, 40});
            Require(root.ActualWidth() == 88, "arbitrary requested native width is applied");
            decoration.Apply(root, NativeTray::Label, 0);
            Require(std::isnan(root.Width()) && std::isinf(root.MaxWidth()), "automatic width follows XAML measurement");
            root.Measure({500, 40});
            const auto shortWidth = root.DesiredSize().Width;
            decoration.replacement.as<controls::TextBlock>().Text(L"A substantially longer VPN status label");
            root.Measure({500, 40});
            Require(root.DesiredSize().Width > shortWidth + 30, "automatic width grows with the text content");
            decoration.Restore();
            controls::Grid neighbor;
            neighbor.MinWidth(32);
            Require(neighbor.MinWidth() == 32 && neighbor.ReadLocalValue(wux::FrameworkElement::WidthProperty()) == wux::DependencyProperty::UnsetValue(),
                "unrelated element stays unchanged");

            wux::Data::Binding binding;
            binding.Source(winrt::box_value(L"bound original content"));
            presenter.SetBinding(controls::ContentPresenter::ContentProperty(), binding);
            const auto originalBinding = presenter.GetBindingExpression(controls::ContentPresenter::ContentProperty()).ParentBinding();
            decoration.Apply(root, NativeTray::Label);
            decoration.Restore();
            Require(presenter.GetBindingExpression(controls::ContentPresenter::ContentProperty()).ParentBinding() == originalBinding,
                "original Content binding is restored");
            for (int iteration = 0; iteration < 10; ++iteration)
            {
                decoration.Apply(root, iteration % 2 ? NativeTray::Lines : NativeTray::Label);
                decoration.Restore();
            }
            Require(presenter.GetBindingExpression(controls::ContentPresenter::ContentProperty()).ParentBinding() == originalBinding,
                "repeated mode switches retain the original binding");
            island.Close();
        }
        DestroyWindow(host);
        VerifyVisibleTooltipPlacement();
        VerifyEventController();
        manager.Close();
        std::puts("All XAML integration checks passed. Explorer was not loaded or modified by this test.");
        return 0;
    }
    catch (const winrt::hresult_error& error)
    {
        std::printf("FAIL HRESULT=0x%08lX\n", static_cast<unsigned long>(error.code().value));
        return 1;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL %s\n", error.what());
        return 1;
    }
}
