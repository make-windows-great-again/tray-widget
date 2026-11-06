[CmdletBinding()]
param([switch]$Live,[switch]$Record,[ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Label='standalone')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$exe=Join-Path $root 'bin\WinUI3Tray\Release\YataWinUI3Tray.exe'
$out=Join-Path $root ('evidence\winui3-tray-20261106\'+$Label)
[IO.Directory]::CreateDirectory($out)|Out-Null
if(Get-Process YataWinUI3Tray -ErrorAction SilentlyContinue){throw 'Close the existing WinUI 3 test instance first'}
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing,System.Windows.Forms
if(!('W3Test'-as[type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class W3Test{
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int L,T,R,B;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [DllImport("user32.dll")]static extern bool GetCursorPos(out Point point);
 [DllImport("user32.dll")]static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 public static void Click(int x,int y,bool right){Point old;GetCursorPos(out old);try{SetCursorPos(x,y);mouse_event(right?8u:2u,0,0,0,UIntPtr.Zero);mouse_event(right?16u:4u,0,0,0,UIntPtr.Zero);}finally{SetCursorPos(old.X,old.Y);}}
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr p,IntPtr l);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern uint RegisterWindowMessage(string name);
}
'@}
[void][W3Test]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$checks=[Collections.Generic.List[string]]::new();$process=$null;$window=[IntPtr]::Zero;$passed=$false;$dpi=0;$recorder=$null;$recordLog=$null;$backdrop=$null
function Run-Client([string[]]$Arguments){
 $info=[Diagnostics.ProcessStartInfo]::new($exe);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 foreach($arg in $Arguments){$info.ArgumentList.Add($arg)}
 $child=[Diagnostics.Process]::Start($info)
 try{$stdout=$child.StandardOutput.ReadToEndAsync();$stderr=$child.StandardError.ReadToEndAsync();if(!$child.WaitForExit(8000)){ $child.Kill();throw 'Client command timed out' };[pscustomobject]@{Exit=$child.ExitCode;Output=$stdout.GetAwaiter().GetResult();Error=$stderr.GetAwaiter().GetResult()}}finally{$child.Dispose()}
}
function State { $result=Run-Client @('--status');if($result.Exit){throw 'Status failed'};$result.Output|ConvertFrom-Json }
function Command([string[]]$Arguments){$result=Run-Client $Arguments;if($result.Exit){throw "Command failed ($($result.Exit)): $Arguments $($result.Output) $($result.Error)"}}
function Check([bool]$Condition,[string]$Name){if(!$Condition){throw $Name};$checks.Add($Name);Write-Output "PASS $Name"}
function Wait-State([scriptblock]$Predicate,[string]$Description){
 $deadline=[DateTime]::UtcNow.AddSeconds(12)
 do{[Windows.Forms.Application]::DoEvents();$value=State;if(&$Predicate $value){return $value};if($process.HasExited){throw "Application exited $($process.ExitCode): $Description"};Start-Sleep -Milliseconds 60}while([DateTime]::UtcNow-lt$deadline)
 throw "Timed out: $Description; last state: $($value|ConvertTo-Json -Compress)"
}
function Element([string]$Id){
 $owner=[Windows.Automation.AutomationElement]::FromHandle($window)
 $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,$Id)
 $owner.FindFirst([Windows.Automation.TreeScope]::Descendants,$condition)
}
function Menu-Item([string]$Name){
 $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty,$process.Id)
 $windows=[Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,$condition)
 foreach($candidate in $windows){
  $item=$candidate.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$Name))
  if($item-and!$item.Current.IsOffscreen){return $item}
 }
 return $null
}
function Capture([string]$Name){
 Command @('--show');$null=Wait-State {param($s) $s.phase-eq2-and$s.visible} 'show for capture'
 $title=Element 'HelloTitle';$bounds=$title.Current.BoundingRectangle
 [W3Test]::Click([int]($bounds.Left+$bounds.Width/2),[int]($bounds.Top+$bounds.Height/2),$false);Start-Sleep -Milliseconds 250
 if([W3Test]::GetForegroundWindow()-ne$window){throw 'Owned window is not foreground; rejecting capture'}
 $rect=[W3Test+Rect]::new();if(![W3Test]::GetWindowRect($window,[ref]$rect)){throw 'No window to capture'}
 $bitmap=[Drawing.Bitmap]::new($rect.R-$rect.L,$rect.B-$rect.T);$graphics=[Drawing.Graphics]::FromImage($bitmap)
 try{$graphics.CopyFromScreen($rect.L,$rect.T,0,0,$bitmap.Size);$bitmap.Save((Join-Path $out ($Name+'.png')))}finally{$graphics.Dispose();$bitmap.Dispose()}
}
try{
 # Own neutral surface limits desktop contents in captures, including the frames
 # where the panel slides away. Review recordings before publishing. Closes in finally.
 $backdrop=[Windows.Forms.Form]::new();$backdrop.FormBorderStyle='None';$backdrop.ShowInTaskbar=$false;$backdrop.StartPosition='Manual';$backdrop.TopMost=$true
 $backdrop.Bounds=[Windows.Forms.Screen]::PrimaryScreen.WorkingArea;$backdrop.BackColor=[Drawing.Color]::FromArgb(110,120,130)
 $backdrop.Show();$backdrop.Refresh();[Windows.Forms.Application]::DoEvents()
 $args=@('--show','--width','88');if(!$Live){$args+='--standalone'}
 $process=Start-Process $exe -ArgumentList $args -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $out 'app.log') -RedirectStandardError (Join-Path $out 'app-error.log')
 $null=Wait-State {param($s) $s.running-and$s.phase-eq2} 'first Open'
 $window=[W3Test]::FindWindow('YataWinUI3Tray.Window','VPN — WinUI 3');$dpi=[W3Test]::GetDpiForWindow($window)
 Check ($window-ne[IntPtr]::Zero) 'WinUI 3 host created'
 $modules=(Get-Process -Id $process.Id).Modules|Where-Object ModuleName -Match 'Microsoft.UI.Xaml|Windows.UI.Xaml'|Select-Object ModuleName,FileName,FileVersionInfo
 $modules|ConvertTo-Json -Depth 2|Set-Content (Join-Path $out 'ui-modules.json')
 Check (@($modules|Where-Object ModuleName -EQ 'Microsoft.UI.Xaml.dll').Count-gt0) 'client loads the actual WinUI 3 runtime'
 $title=Element 'HelloTitle';Check ($null-ne$title-and$title.Current.Name-eq'Hello World'-and!$title.Current.IsOffscreen) 'Hello World is visible through UI Automation'
 if($Live){$null=Wait-State {param($s) $s.applied-eq1-and[Math]::Abs($s.actualWidth-88)-lt0.1} 'patch attaches at 88 DIP';Check $true 'new patch applies 88 DIP in Explorer'}
 if($Record){
  $rect=[W3Test+Rect]::new();[void][W3Test]::GetWindowRect($window,[ref]$rect)
  # A change in tray width moves the anchor; retain room for every tested width.
  $area=[Windows.Forms.Screen]::FromHandle($window).WorkingArea
  $captureWidth=2*[int][Math]::Floor([Math]::Min($area.Width,$rect.R-$rect.L+640*$dpi/96)/2)
  $captureHeight=2*[int][Math]::Floor([Math]::Min($area.Height,$rect.B-$rect.T+256*$dpi/96)/2)
  $captureLeft=$area.Right-$captureWidth;$captureTop=$area.Bottom-$captureHeight
  $ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
  $info=[Diagnostics.ProcessStartInfo]::new($ff);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
  foreach($arg in @('-hide_banner','-y','-f','lavfi','-i',"ddagrab=framerate=120:draw_mouse=1:video_size=${captureWidth}x${captureHeight}:offset_x=${captureLeft}:offset_y=${captureTop}:dup_frames=0",'-vf','hwdownload,format=bgra','-c:v','h264_nvenc','-preset','p2','-cq','18','-fps_mode','passthrough',(Join-Path $out 'panel.mp4'))){$info.ArgumentList.Add($arg)}
  $recorder=[Diagnostics.Process]::Start($info);$recordLog=$recorder.StandardError.ReadToEndAsync();Start-Sleep -Milliseconds 500
  if($recorder.HasExited){throw 'Screen recorder failed'}
 }
 $slider=Element 'TrayWidth';Check ($null-ne$slider) 'native WinUI slider exposes AutomationId'
 $slider.GetCurrentPattern([Windows.Automation.RangeValuePattern]::Pattern).SetValue(144)
 $null=Wait-State {param($s) $s.width-eq144-and(!$Live-or[Math]::Abs($s.actualWidth-144)-lt0.1)} 'UI slider changes tray width'
 Check $true 'UIA slider updates the width request'
 foreach($width in @(32,320,64,88,0,144)){
  Command @('--width',"$width")
  $null=Wait-State {param($s) $s.width-eq$width-and(!$Live-or($s.applied-eq1-and($width-eq0-or[Math]::Abs($s.actualWidth-$width)-lt0.1)))} "width $width"
  Check $true "width $width applied"
 }
 $bad=Run-Client @('--width','321');Check ($bad.Exit-ne0) 'invalid width is rejected'
 Check ((State).width-eq144) 'rejected command does not modify width'
 foreach($theme in @('light','dark')){Command @('--theme',$theme);Capture $theme;Check $true "$theme theme stays visible"}
 for($i=0;$i-lt8;$i++){
  Command @('--hide');$null=Wait-State {param($s) $s.phase-eq0-and!$s.visible} 'Closed';
  Command @('--show');$null=Wait-State {param($s) $s.phase-eq2-and$s.visible} 'Open'
 }
 Check $true 'eight complete hide/show cycles'
 [void][W3Test]::SetForegroundWindow($window)
 Check ([W3Test]::GetForegroundWindow()-eq$window) 'owned panel has foreground before keyboard test'
 [W3Test]::keybd_event(27,0,0,[UIntPtr]::Zero);[W3Test]::keybd_event(27,0,2,[UIntPtr]::Zero)
 $null=Wait-State {param($s) $s.phase-eq0-and!$s.visible} 'Escape dismisses the focused WinUI island'
 Check $true 'Escape dismisses WinUI content'
 if($Live){
  $state=State;$rect=$state.rect
  Check ($rect[2]-gt$rect[0]-and$rect[3]-gt$rect[1]) 'patch reports a nonempty tray target'
  [W3Test]::Click([int](($rect[0]+$rect[2])/2),[int](($rect[1]+$rect[3])/2),$false)
  $null=Wait-State {param($s) $s.phase-eq2-and$s.visible} 'left tray click opens panel'
  Check $true 'real left click on patched tray opens Hello World'
  [W3Test]::Click([int](($rect[0]+$rect[2])/2),[int](($rect[1]+$rect[3])/2),$false)
  $null=Wait-State {param($s) $s.phase-eq0-and!$s.visible} 'second tray click closes panel'
  Check $true 'second real tray click closes Hello World'
 }
 Command @('--hide');$null=Wait-State {param($s) $s.phase-eq0} 'hide before menu'
 if($Live){$rect=(State).rect;[W3Test]::Click([int](($rect[0]+$rect[2])/2),[int](($rect[1]+$rect[3])/2),$true)}else{Command @('--menu')}
 Start-Sleep -Milliseconds 350
 $hiddenTitle=Element 'HelloTitle';Check ($null-eq$hiddenTitle-or$hiddenTitle.Current.IsOffscreen) 'context menu does not show the Hello World card'
 $auto=Menu-Item 'Автоматическая ширина';Check ($null-ne$auto) 'WinUI context menu is visible'
 $auto.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
 $null=Wait-State {param($s) $s.phase-eq0-and$s.width-eq0} 'menu action applies automatic width and dismisses host'
 Check $true 'context action changes width and closes its host'
 Command @('--show');$null=Wait-State {param($s) $s.phase-eq2} 'reopen after context'
 if($Live){Check ((State).callbackErrors-eq0) 'no patch callback failures'}
 $passed=$true
}finally{
 if($recorder){
  if(!$recorder.HasExited){$recorder.StandardInput.WriteLine('q');if(!$recorder.WaitForExit(8000)){$recorder.Kill();$recorder.WaitForExit()}}
  $lines=$recordLog.GetAwaiter().GetResult()-split'\r\n|\r|\n'|ForEach-Object {$_.TrimEnd()}
  [IO.File]::WriteAllText((Join-Path $out 'recorder.log'),(($lines-join"`n").TrimEnd()+"`n"))
  if($recorder.ExitCode-ne0){$passed=$false};$recorder.Dispose()
 }
 if($process){
  if(!$process.HasExited){$null=Run-Client @('--exit');if(!$process.WaitForExit(8000)){$process.Kill();$process.WaitForExit();$passed=$false}}
  if($process.ExitCode-ne0){$passed=$false}
  $exit=$process.ExitCode;$process.Dispose()
 }
 if($backdrop){$backdrop.Close();$backdrop.Dispose()}
 if($passed-and$Live){
  $deadline=[DateTime]::UtcNow.AddSeconds(5)
  do{$after=State;if(!$after.running-and!$after.applied){break};Start-Sleep -Milliseconds 80}while([DateTime]::UtcNow-lt$deadline)
  $after|ConvertTo-Json|Set-Content (Join-Path $out 'after-exit.json')
  if($after.running-or$after.applied){$passed=$false}else{$checks.Add('exit detaches decoration and stops the client')}
 }
 $checks|Set-Content (Join-Path $out 'checks.txt')
 [pscustomobject]@{Passed=$passed;Live=$Live.IsPresent;Dpi=$dpi;ExitCode=$exit;Checks=$checks.Count;EXE=(Get-FileHash $exe).Hash;Patch=$(if(Test-Path (Join-Path (Split-Path $exe) 'YataWinUI3Tray.Patch.dll')){(Get-FileHash (Join-Path (Split-Path $exe) 'YataWinUI3Tray.Patch.dll')).Hash})}|ConvertTo-Json|Set-Content (Join-Path $out 'results.json')
}
if(!$passed){throw 'Test process failed'}
