#pragma once
#include "App.xaml.g.h"
namespace winrt::WinUI3Tray::implementation {
struct App : AppT<App> {
    App() : manager(Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread()) {}
    void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {}
private:
    Microsoft::UI::Xaml::Hosting::WindowsXamlManager manager{nullptr};
};
}
