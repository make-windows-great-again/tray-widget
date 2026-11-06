#pragma once
#include <unknwn.h>
#include <windows.h>
#undef GetCurrentTime
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <winrt/Windows.UI.Xaml.h>
namespace ShellPanels
{
enum class Kind
{
    Language,
    QuickSettings,
    Date
};
enum class TrayPart
{
    Language,
    Network,
    Volume,
    Battery,
    Clock
};
enum class Action
{
    LanguageSelected,
    NetworkDetails,
    BluetoothDetails,
    Airplane,
    Accessibility,
    VpnDetails,
    EnergySaver,
    Brightness,
    Volume,
    AudioDetails,
    Settings,
    Focus,
    ClearNotifications,
    DateSelected,
    DismissNotification,
    NotificationAction,
    MediaPrevious,
    MediaToggle,
    MediaNext,
    MediaOpen,
    NetworkToggle,
    BluetoothToggle
};
struct Language
{
    std::wstring code, name, layout;
};
struct Notification
{
    std::wstring app, title, body, time, action;
    int grouped = 0;
};
struct Model
{
    std::vector<Language> languages{{L"ENG", L"English (United States)", L"US"}, {L"РУС", L"Russian", L"Russian"}};
    int selectedLanguage = 1, volume = 27, brightness = 100, battery = 100;
    std::wstring network = L"Wi-Fi", bluetooth = L"Not connected";
    bool light = true;
    bool wifiEnabled = true, bluetoothEnabled = true, airplaneEnabled = false, energySaverEnabled = false;
    bool mediaVisible = false;
    std::wstring mediaTitle = L"Media", mediaSubtitle = L"Test provider";
    std::vector<Notification> notifications;
    // ARGB customization is scoped to this Host; accessibility policy wins.
    std::optional<uint32_t> tintArgb, accentArgb;
    std::optional<double> luminosity;
};
// A full-height stack of independently framed cards. Header and footer keep
// their measured height; only Body collapses. A flexible body uses the space
// left by fixed cards. Supply a ScrollViewer when its contents can overflow.
struct CardSection
{
    winrt::Windows::UI::Xaml::UIElement header{nullptr}, body{nullptr}, footer{nullptr};
    double bodyHeight = 0;
    bool flexible = false, expanded = true;
};
// Owning UI thread only. No application singletons, tray injection, network
// operations, global setting writes, or demo command IDs in the public API.
class Host
{
  public:
    enum class Phase
    {
        Closed,
        Opening,
        Open,
        Closing
    };
    using Handler = std::function<void(Action, double)>;
    Host();
    explicit Host(HWND owner, Handler handler = {});
    ~Host();
    Host(Host const &) = delete;
    Host &operator=(Host const &) = delete;
    void SetModel(Model model);
    void SetHandler(Handler handler);
    void Show(Kind kind, POINT anchor);
    void Toggle(Kind kind, POINT anchor);
    void Hide();
    void Deactivated(HWND next, bool ownTrayPress = false);
    bool Process(MSG const &message);
    bool Visible() const;
    int PhaseValue() const;
    void ContextMenu(Kind kind, POINT anchor);
    void ContextMenu(TrayPart part, POINT anchor);
    void Initialize(HWND owner, std::function<void(bool)> visibility, std::function<void(UINT)> command,
                    std::function<void(Phase)> phase = {});
    void SetContent(winrt::Windows::UI::Xaml::UIElement const &content, winrt::hstring const &title, float width = 520);
    // Configure while Closed. State and content survive hide/show cycles.
    void SetSections(std::vector<CardSection> sections, float width = 336);
    void ExpandSection(size_t index, bool expanded, bool animate = true);
    bool SectionExpanded(size_t index) const;
    void Select(Kind kind);
    void Show(POINT anchor);
    void Toggle(POINT anchor);
    void Theme(bool light);
    void Menu(POINT anchor, LONG mode, UINT width);
    void OwnerDeactivated(HWND next, bool ownTrayPress = false)
    {
        Deactivated(next, ownTrayPress);
    }
    void Reposition(POINT anchor);
    void PreferredWidth(float width);
    bool Transitioning() const;
    Phase CurrentPhase() const;
    bool MenuOpen() const;
    void Close() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace ShellPanels
