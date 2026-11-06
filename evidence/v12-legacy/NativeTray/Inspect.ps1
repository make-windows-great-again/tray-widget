[CmdletBinding()]
param([string]$Label = 'snapshot', [switch]$Click, [switch]$RightClick)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing
if (-not ('TrayInspection' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class TrayInspection {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int x,y; }
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
}
'@
}
[void][TrayInspection]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
$taskbarWindow = [TrayInspection]::FindWindow('Shell_TrayWnd',$null)
if ($taskbarWindow -eq [IntPtr]::Zero) { throw 'The taskbar is unavailable on this desktop. Run Inspect.ps1 in the interactive Windows session.' }
$taskbar = [Windows.Automation.AutomationElement]::FromHandle($taskbarWindow)
$all = $taskbar.FindAll([Windows.Automation.TreeScope]::Descendants, [Windows.Automation.Condition]::TrueCondition)
$items = @()
$vpn = $null
foreach ($element in $all) {
    $current = $element.Current
    if ($current.ClassName -like 'SystemTray.*' -or $current.Name -like '*Yata Native Framework*' -or $current.Name -eq 'VPN') {
        $rect = $current.BoundingRectangle
        $items += [pscustomobject]@{ Name=$current.Name; Class=$current.ClassName; Id=$current.AutomationId; X=$rect.X; Y=$rect.Y; Width=$rect.Width; Height=$rect.Height; Dpi=[TrayInspection]::GetDpiForWindow($taskbarWindow); Offscreen=$current.IsOffscreen }
        if ($current.AutomationId -eq 'NotifyItemIcon' -and $current.Name -like 'Yata Native Framework*') { $vpn = $element }
    }
}
$items | ConvertTo-Json -Depth 4
$oldCursor = New-Object TrayInspection+Point
[void][TrayInspection]::GetCursorPos([ref]$oldCursor)
$panel = [TrayInspection]::FindWindow('YataNativeFramework.Window',$null)
try {
    $stateMapping = [IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataVpn.NativeTray.v2',[IO.MemoryMappedFiles.MemoryMappedFileRights]::Read)
    $stateView = $stateMapping.CreateViewAccessor(0,56,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
    $panel = [IntPtr]::new($stateView.ReadInt64(8))
    $stateView.Dispose();$stateMapping.Dispose()
} catch {}
$beforeVisible = [TrayInspection]::IsWindowVisible($panel)
if (($Click -or $RightClick) -and $vpn) {
    $rect = $vpn.Current.BoundingRectangle
    [void][TrayInspection]::SetCursorPos([int]($rect.X+$rect.Width/2),[int]($rect.Y+$rect.Height/2))
    $down = if ($RightClick) { 8 } else { 2 }
    $up = if ($RightClick) { 16 } else { 4 }
    [TrayInspection]::mouse_event($down,0,0,0,[UIntPtr]::Zero)
    [TrayInspection]::mouse_event($up,0,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 800
}
Write-Output "PanelVisible=$([TrayInspection]::IsWindowVisible($panel))"
$evidence = Join-Path $env:TEMP 'YataVpnNativeTrayBuild\evidence'
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$items | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence "$Label.json") -Encoding UTF8
[pscustomobject]@{ BeforeVisible=$beforeVisible; PanelVisible=[TrayInspection]::IsWindowVisible($panel); OwnerWindow=$panel.ToInt64(); TargetFound=[bool]$vpn; ExplorerPid=$taskbar.Current.ProcessId } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence "$Label-state.json") -Encoding UTF8
$bounds = $taskbar.Current.BoundingRectangle
$captureWidth = [int][Math]::Min(1300,$bounds.Width)
$captureHeight = [int][Math]::Min(600,$bounds.Y+$bounds.Height)
$bitmap = New-Object Drawing.Bitmap($captureWidth,$captureHeight)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen([int]($bounds.Right-$captureWidth),[int]($bounds.Bottom-$captureHeight),0,0,$bitmap.Size)
    $bitmap.Save((Join-Path $evidence "$Label.png"),[Drawing.Imaging.ImageFormat]::Png)
} finally { $graphics.Dispose();$bitmap.Dispose() }
if ($Click -or $RightClick) { [void][TrayInspection]::SetCursorPos($oldCursor.x,$oldCursor.y) }
