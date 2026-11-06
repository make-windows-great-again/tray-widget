param([string]$Label='visual-v6')
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class VisualFixTest {
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int left,top,right,bottom;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
}
'@
[void][VisualFixTest]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13')
$view=$map.CreateViewAccessor()
$owner=[IntPtr]::new($view.ReadInt64(8))
$bar=[VisualFixTest]::FindWindow('Shell_TrayWnd',$null)
$barRect=New-Object VisualFixTest+Rect
[void][VisualFixTest]::GetWindowRect($bar,[ref]$barRect)
$checks=[Collections.Generic.List[string]]::new()
$frames=[Collections.Generic.List[object]]::new()
function Assert([bool]$ok,[string]$text){if(!$ok){throw $text};$checks.Add("PASS $text")}
function Find-Control([string]$id){
 $root=[Windows.Automation.AutomationElement]::FromHandle($owner)
 $filter=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,$id)
 return $root.FindFirst([Windows.Automation.TreeScope]::Descendants,$filter)
}
function Show-Panel {
 [void][VisualFixTest]::PostMessage($owner,[VisualFixTest]::RegisterWindowMessage('YataVpn.Tray.Show.v4'),[IntPtr]::Zero,[IntPtr]::Zero)
 Start-Sleep -Milliseconds 450
}
function Capture([string]$name){
 $b=New-Object Drawing.Bitmap(750,720);$g=[Drawing.Graphics]::FromImage($b)
 try{$g.CopyFromScreen($barRect.right-750,$barRect.bottom-720,0,0,$b.Size);$b.Save("$PSScriptRoot\$name.png",[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$b.Dispose()}
}
try {
 Show-Panel
 Assert ($view.ReadInt32(56) -eq 1) 'panel publishes its open state'
 $close=Find-Control 'ClosePanel'
 Assert ($null -ne $close -and $close.Current.ControlType -eq [Windows.Automation.ControlType]::Button) 'close is an accessible native Button'
 Assert ([bool](Find-Control 'UploadValue') -and [bool](Find-Control 'DownloadValue')) 'both values are accessible'
 $rect=New-Object VisualFixTest+Rect
 [void][VisualFixTest]::GetWindowRect($owner,[ref]$rect)
 Assert ($rect.bottom -le $barRect.top) 'panel HWND is entirely above the taskbar'
 Capture "$Label-open"
 $close.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
 $watch=[Diagnostics.Stopwatch]::StartNew()
 while($watch.ElapsedMilliseconds -lt 280){
  $r=New-Object VisualFixTest+Rect
  [void][VisualFixTest]::GetWindowRect($owner,[ref]$r)
  $frames.Add([pscustomobject]@{Ms=$watch.Elapsed.TotalMilliseconds;X=$r.left;Y=$r.top;Right=$r.right;Bottom=$r.bottom;Visible=[VisualFixTest]::IsWindowVisible($owner)})
  Start-Sleep -Milliseconds 5
 }
 Assert ($view.ReadInt32(56) -eq 0 -and ![VisualFixTest]::IsWindowVisible($owner)) 'close Invoke completes the slide down and clears open state'
 Assert (@($frames | Where-Object {$_.Bottom -gt $barRect.top -or $_.X -ne $rect.left -or $_.Y -ne $rect.top}).Count -eq 0) 'HWND remains fixed above the taskbar throughout closing'
 foreach($i in 1..10){
  Show-Panel
  Assert ($view.ReadInt32(56) -eq 1) "open cycle $i"
  (Find-Control 'ClosePanel').GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
  Start-Sleep -Milliseconds 300
  Assert ($view.ReadInt32(56) -eq 0) "close cycle $i"
 }
 Show-Panel
 $close=Find-Control 'ClosePanel';$close.SetFocus();Start-Sleep -Milliseconds 100
 Assert ($close.Current.HasKeyboardFocus) 'close accepts keyboard focus'
 [VisualFixTest]::keybd_event(32,0,0,[UIntPtr]::Zero);[VisualFixTest]::keybd_event(32,0,2,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 400
 Assert ($view.ReadInt32(56) -eq 0) 'Space invokes the focused close button'
 Show-Panel
 [VisualFixTest]::keybd_event(27,0,0,[UIntPtr]::Zero);[VisualFixTest]::keybd_event(27,0,2,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 400
 Assert ($view.ReadInt32(56) -eq 0) 'Escape dismisses the panel'
 $checks.Add('ALL PANEL CHECKS PASSED')
}catch{
 $checks.Add("FAIL $($_.Exception.Message)");throw
}finally{
 $checks | Set-Content -LiteralPath "$PSScriptRoot\$Label-results.txt" -Encoding UTF8
 $frames | ConvertTo-Json | Set-Content -LiteralPath "$PSScriptRoot\$Label-frames.json" -Encoding UTF8
 $checks
 $view.Dispose();$map.Dispose()
}
