Local Windows 11 visual and lifecycle bench

Build.ps1 builds ShellPanels.lib and ShellBench.exe. The bench uses fixture data
only. Keyboard: 1/2/3 selects a root panel, right click opens the audio menu,
Escape dismisses, F10 exits. --dark and --custom select per-host themes.

Test-Host.ps1 builds/runs the public-API lifecycle test, including 100 completed
open/close cycles. Test-Interactions.ps1 uses UIA plus physical pointer input;
-BinaryPath can select a consumer compiled from the exported SDK.

Record-Comparison.ps1 -Target native|cpp|tray [-Dark] [-Custom] [-Interactions]
records the reference desktop, standalone consumer or real attached tray app.
This capture profile is specific to 2560x1600, 150%, taskbar at the bottom.
Update coordinates/profile before using a different monitor configuration.
The native -Dark run temporarily changes the two personalization theme values
and restores them in finally. It does not change networking or VPN.
The tray run requires a healthy attached application in lines mode; it reads
the actual widget rectangle from the versioned shared-memory protocol.

The recorder expects these external test tools (not application dependencies):
artifacts/tools/ffmpeg-9.0.2-essentials_build/bin/ffmpeg.exe and ffprobe.exe.
The current project workspace includes this portable FFmpeg build. The source
archive excludes the downloaded tools; install a compatible build at that path.
DDAGrab and h264_nvenc are used on the measured NVIDIA system. Adapt the capture
and encoder for other hardware. Frames are variable-rate, without duplication.

Analyze-Recording.ps1 -Name native-light (repeat for other recording names)
extracts vertical and horizontal edge traces and frame timestamp statistics.
Summarize-Comparison.ps1 builds comparison.html and pixel-measurements.csv.
Measure-Motion.ps1 derives observed motion segments from the traces; it cannot
replace exact internal animation timing or presentation tracing.

Use lossless PNG for color/geometry and MP4 for motion. Match dynamic fixture
content or mask it before a full image comparison. Reject captures with obscured
targets/backgrounds. Desktop interaction must be exclusive during a capture.

Deploy-Candidate.ps1 is specific to this audited local workspace. It preserves
the pinned active patch, snapshots layout.ini and refuses unexpected instances.
It does not fix Explorer Diagnostics reconnect or restart Explorer automatically.
Do not treat an Explorer restart as a successful reconnect regression test.

See evidence/shell-parity-26100/REPORT.txt, TEST-PLAN.csv and FIX-PLAN.csv for
actual results and open acceptance criteria. Earlier probes are exploratory;
only the final report identifies evidence used for acceptance.

v15: Date / full-height cards
Record-Comparison.ps1 -Target cpp -Revision v15 [-Dark|-Custom]
Record-Comparison.ps1 -Target cpp -Interactions -Revision v15 [-Dark]
Record-Comparison.ps1 -Target cpp -Cards -Revision v15
GUI runs must be sequential. -Revision keeps earlier accepted reference captures.
Measure-CardMotion.ps1 traces the inter-card gap and reports collapse/expand time.
Measure-Motion.ps1 accepts -Tags and -OutputName to compare a newer build without
replacing the baseline CSV. Build-DateReport.ps1 creates the current comparison.
ShellBench key 4 demonstrates public CardSection and Host::SetSections.

Control gallery / long swipes (no Explorer patch or tray process):
  .\tools\shell-parity\Build-GallerySwipe.ps1
  .\tools\shell-parity\Test-GallerySwipe.ps1 -Label accepted-local -Checks -Record
  .\tools\shell-parity\Test-GalleryDpi.ps1 -Record
The last command requires Display Settings open and restores the original scale.
It tests 100/150/200/250% in light/dark themes; -Record records the 150% cases.
The bench uses real XAML Islands and InjectTouchInput, not a physical touchpad.
Ctrl+Tab/PageUp/PageDown use injected keyboard input directed to the owned island.
The shared NativePageStripChecks also run under Build.ps1 -SelfTest.
The narrow 500 px window leaves desktop space for multi-page contact motion;
-Width 700 provides a wider visual comparison. Capture profile: 2560x1600 desktop.
The trace records layout offsets, not compositor presentation timestamps.
For an old Pivot reference, copy the previous NativeControls.h to the ignored
artifacts/gallery-baseline directory, then Build-GallerySwipe.ps1 -Baseline and
Test-GallerySwipe.ps1 -Variant baseline -Label baseline-reference -Record.
Do not pass -Checks to the old baseline; finite-scroll assertions target the fix.
Videos require the same FFmpeg/NVIDIA setup described above. GUI tests are serial.
