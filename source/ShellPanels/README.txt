ShellPanels static C++ library — experimental Windows 11 profile 26100

BUILD
Run tools\shell-parity\Build.ps1 from the project. It produces ShellPanels.lib
and an independent consumer ShellBench.exe in artifacts\shell-panels\Release.
No additional runtime DLL is introduced. Windows.UI.Xaml, Composition and fonts
are provided by Windows. The library uses C++20, C++/WinRT and /MT (/MTd in Debug).
Consumer libraries: WindowsApp, shcore, user32, shell32, ole32, oleaut32, dwmapi.
Use a PerMonitorV2 manifest and WindowsXamlManager-compatible maxversiontested.
See ShellBench.cpp for the complete message pump and window lifetime contract.

OWNERSHIP AND INPUT
Create Host on an initialized STA. Calls, callbacks and destruction use that STA.
The owner HWND must outlive Host. Process every MSG before normal dispatch.
Forward WM_ACTIVATE/WA_INACTIVE to Deactivated; ownTrayPress is true only for
the originating tray gesture. The client toggles the panel on button release.
Coordinates are PHYSICAL desktop pixels; layout sizes and widths are DIP.
Model is copied. Action callbacks are synchronous intents, not OS operations.
Callback exceptions are contained at the UI event boundary. Do not destroy the
Host inside a callback; queue destruction to the owning message loop.
WindowsXamlManager shutdown is asynchronous. If destroying and recreating the
last Host on the same thread, drain the dispatcher between lifetimes; immediate
reinitialization is rejected by Windows. Prefer a persistent Host per UI thread.

DATA
SetModel updates Quick Settings sliders and enabled tile states while visible,
without generating user commands. Values are clamped to 0..100. The application
acknowledges toggle intents by publishing a new Model. The standalone bench uses
only an in-memory fixture; it does not toggle Windows networking or VPN.
Structural lists (languages, notifications), media metadata and calendar layout
are rebuilt on the next opening. These are not yet a full observable data model.
Theme/tint changes refresh materials immediately. Some manually styled child
brushes/templates still require reopening; see the open theme acceptance item.
DateSelected is a Gregorian day count since 1970-01-01, not a local timestamp.
Context menu Settings callbacks are currently coarse; per-command dispatch is
an outstanding API refinement. No settings pages or external URLs are launched.

MATERIAL AND MOTION
CreateHostBackdropBrush plus a composition effect graph and embedded noise keep
the background independent of foreground activation during slide-out. System
advanced-effects, high-contrast and power-saving policies choose solid fallback.
XAML/Composition completion owns visibility; there is no rendering timer loop.
Quick Settings and generic content use vertical motion (250 ms in / 167 ms out).
Date uses horizontal motion from the right (167 / 167 ms); Language uses a faster
vertical profile (83 / 83 ms). These are measured prototype profile values, not
a promise of exact trajectory parity. Capture results are in evidence.

TRAY-WIDGET POLICY
All context menus, INCLUDING BATTERY, share the work-area gap. The installed
Windows battery popup's pointer-relative Y placement is deliberately excluded
at the user's request. Language has no context menu in the reference profile.
Legacy Menu(mode,width) is a compatibility adapter for the existing sample app;
the private transport still contains that menu definition. It is not a generic
menu schema API. UI layout code does not depend on Explorer injection.

ACCESSIBILITY AND LIMITS
Real native Button/Slider/ListView controls retain their UI Automation providers.
Date uses OS CalendarView with native Table/Selection UIA providers. Composition clips round its day chrome; localization, keyboard navigation and virtualization stay in the native control. Language activation, localization,
notification grouping, secondary pages, live theme switching, mixed DPI and
all Shell hover/press states are not certified as 1:1. Read REPORT.txt before
treating the profile as production-complete.

SOURCE ATTRIBUTION
Noise.png / Noise.h derive from the MIT-licensed Microsoft WinUI noise asset;
the copyright and permission notice are retained in Noise-LICENSE.txt.
Effect recipe reference: microsoft-ui-xaml Materials/Acrylic/AcrylicBrush.cpp.
The panel tree is constructed in C++; no XBF, private Shell binaries or extracted
Shell assets are redistributed.

FULL-HEIGHT CARD STACK (v15)
Call SetSections(vector<CardSection>, widthDips) while Host is Closed, then
Show(anchor) or Toggle(anchor). The stack uses the anchor monitor work area,
8 DIP at the top, 12 DIP at bottom/right and 12 DIP between cards.
A section supplies native UIElements: header, body, footer. Fixed bodyHeight is
in DIP. flexible=true gives the body a share of the remaining space; place a
ScrollViewer/ListView there when content can overflow. Multiple flexible cards
share equally. Flexible cards stay expanded. Fixed bodies may start collapsed.
Header/footer keep their measured heights; only the body is clipped/collapsed.
Each card has its own material, external stroke and shadow. It is not a window
per card. The entire layout remains inside one XAML Island and one HWND.
ExpandSection(index, expanded, animate) supports mid-transition reversal.
SectionExpanded returns the requested state. State survives Hide/Show; replacing
content or selecting a Shell scheme releases it. Keyboard focus inside a closing
body moves to the first focusable header/footer control in that Island's XamlRoot.
Host observes sent DPI/display/work-area messages and XamlRoot scale changes. Keep forwarding MSG to Host::Process. Reposition accepts raw physical anchor coordinates.
The 300 ms height transition is a dependent XAML animation, not a refresh timer.
It preserves real layout/hit testing; materials and shadows reuse their resources.
This has a UI-thread layout cost and is not a claim of 240 rendered frames/s.
AnimationsEnabled=false or animate=false commits geometry directly. No global
policy or setting is written. Ensure fixed content fits the intended work area;
short displays and mixed-DPI hot moves remain separate acceptance cases.
See key 4 / ShowCards in the standalone ShellBench example. Its three cards,
list, switch and slider use only local state. The public API introduces no DLL.