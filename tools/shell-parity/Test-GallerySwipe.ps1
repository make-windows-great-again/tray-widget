[CmdletBinding()]
param([ValidateSet('baseline','Release','Debug')][string]$Variant='Release',[ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Label='current',[switch]$Record,[switch]$Checks,[switch]$Dark,[ValidateRange(400,1000)][int]$Width=500,[int]$ExpectedDpi=0)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root ('evidence\gallery-swipe-20261106\'+$Label)
[IO.Directory]::CreateDirectory($out)|Out-Null
if(Get-Process GallerySwipeBench -ErrorAction SilentlyContinue){throw 'Another gallery bench is already running'}
Add-Type -AssemblyName System.Drawing
if(!('GalleryDriver'-as[type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class GalleryDriver {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int L,T,R,B;}
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;public Point(int x,int y){X=x;Y=y;}}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string cls,string name);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr w,int cmd);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")]public static extern IntPtr GetAncestor(IntPtr w,uint flags);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]public static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")]public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
}
'@}
[void][GalleryDriver]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$exe=Join-Path $root ('artifacts\gallery-swipe\'+$Variant+'\GallerySwipeBench.exe')
$bench=Start-Process -FilePath $exe -WorkingDirectory $out -WindowStyle Hidden -PassThru
$window=[IntPtr]::Zero;$recorder=$null;$recordLog=$null;$passed=0;$dpi=0
$results=[Collections.Generic.List[object]]::new()
function Message([uint32]$id,[int]$value=0,[int]$extra=0){[GalleryDriver]::SendMessage($window,$id,[IntPtr]::new($value),[IntPtr]::new($extra)).ToInt64()}
function Wait-Settled([int]$expected=-1){
 $until=[DateTime]::UtcNow.AddSeconds(5);$stable=0
 do{
  Start-Sleep -Milliseconds 60
  $width=(Message 0x8005)/1000;$offset=(Message 0x8004)/1000;$selected=Message 0x8006
  $aligned=$width-gt0-and[Math]::Abs($offset/$width-[Math]::Round($offset/$width))-lt0.004
  if($aligned-and($expected-lt0-or$selected-eq$expected)){$stable++}else{$stable=0}
 }while($stable-lt3-and[DateTime]::UtcNow-lt$until)
 if($stable-lt3){throw "Viewport did not settle: index=$selected offset=$offset width=$width expected=$expected"}
}
function Control-Key([byte]$key){
 Focus-Bench
 [GalleryDriver]::keybd_event(0x11,0,0,[UIntPtr]::Zero)
 try{[GalleryDriver]::keybd_event($key,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 50}
 finally{[GalleryDriver]::keybd_event($key,0,2,[UIntPtr]::Zero);[GalleryDriver]::keybd_event(0x11,0,2,[UIntPtr]::Zero)}
}
function Check-FocusedPage([int]$expected){
 $page=Message 0x800D
 if($page-ne$expected){throw "Keyboard focus is on page $page instead of visible page $expected"}
}
function Focus-Bench {
 [void][GalleryDriver]::SetWindowPos($window,[IntPtr]::new(-1),0,0,0,0,0x53)
 [void][GalleryDriver]::SetForegroundWindow($window)
 Start-Sleep -Milliseconds 100
 if([GalleryDriver]::GetForegroundWindow()-ne$window){
  $rect=[GalleryDriver+Rect]::new();[void][GalleryDriver]::GetWindowRect($window,[ref]$rect)
  $captionY=$rect.T+[int][Math]::Round(20*(Message 0x800A)/96)
  if([GalleryDriver]::GetAncestor([GalleryDriver]::WindowFromPoint([GalleryDriver+Point]::new($rect.L+100,$captionY)),2)-ne$window){throw 'Owned title bar is occluded; refusing mouse click'}
  [void][GalleryDriver]::SetCursorPos($rect.L+100,$captionY)
  [GalleryDriver]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[GalleryDriver]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 200
 }
 if([GalleryDriver]::GetForegroundWindow()-ne$window){throw 'Gallery bench could not acquire foreground input'}
}
function Snapshot([string]$name){
 $rect=[GalleryDriver+Rect]::new();[void][GalleryDriver]::GetWindowRect($window,[ref]$rect)
 $bitmap=[Drawing.Bitmap]::new($rect.R-$rect.L,$rect.B-$rect.T);$g=[Drawing.Graphics]::FromImage($bitmap)
 try{$g.CopyFromScreen($rect.L,$rect.T,0,0,$bitmap.Size);$bitmap.Save((Join-Path $out ($name+'.png')))}finally{$g.Dispose();$bitmap.Dispose()}
}
try{
 Start-Sleep -Milliseconds 1500
 $window=[GalleryDriver]::FindWindow('Yata.GallerySwipeBench','Gallery swipe test')
 if($window-eq[IntPtr]::Zero){throw 'Gallery bench did not create its window'}
 [void][GalleryDriver]::ShowWindow($window,9);Focus-Bench
 [void][GalleryDriver]::SetWindowPos($window,[IntPtr]::Zero,1000,50,$Width,1380,0x14)
 $null=Message 0x800B ([int]$Dark.IsPresent)
 $dpi=Message 0x800A
 if($ExpectedDpi-and$dpi-ne$ExpectedDpi){throw "Expected $ExpectedDpi DPI, but the gallery uses $dpi"}
 Start-Sleep -Milliseconds 700
 if($Record){
  $rect=[GalleryDriver+Rect]::new();[void][GalleryDriver]::GetWindowRect($window,[ref]$rect)
  $width=2*[int][Math]::Floor(($rect.R-$rect.L)/2);$height=2*[int][Math]::Floor(($rect.B-$rect.T)/2)
  $ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
  $info=[Diagnostics.ProcessStartInfo]::new($ff);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
  foreach($arg in @('-hide_banner','-y','-f','lavfi','-i',"ddagrab=framerate=120:draw_mouse=1:video_size=${width}x${height}:offset_x=$($rect.L):offset_y=$($rect.T):dup_frames=0",'-vf','hwdownload,format=bgra','-c:v','h264_nvenc','-preset','p2','-cq','18','-fps_mode','passthrough',(Join-Path $out 'gestures.mp4'))){$info.ArgumentList.Add($arg)}
  $recorder=[Diagnostics.Process]::Start($info);$recordLog=$recorder.StandardError.ReadToEndAsync();Start-Sleep -Milliseconds 500
  if($recorder.HasExited){throw 'Screen recorder failed'}
 }
 $null=Message 0x8001 2;Start-Sleep -Milliseconds 700;Snapshot 'selected-2'
 foreach($gesture in @([pscustomobject]@{Name='long-left';Pages=3400;Start=-1},[pscustomobject]@{Name='long-right';Pages=-3400;Start=-1},[pscustomobject]@{Name='first-edge';Pages=-3400;Start=0},[pscustomobject]@{Name='last-edge';Pages=3400;Start=5})){
  if($gesture.Start-ge0){$null=Message 0x8001 $gesture.Start;Start-Sleep -Milliseconds 700}
  Focus-Bench
  $before=Message 0x8006;$injected=Message 0x8008 $gesture.Pages;Start-Sleep -Milliseconds 300
  if($injected-ne1){throw 'Touch injection failed; see input-errors.txt'}
  if($Variant-ne'baseline'){Wait-Settled $gesture.Start}
  $results.Add([pscustomobject]@{Gesture=$gesture.Name;Before=$before;InjectedTouch=($injected-eq1);After=(Message 0x8006);RealizedPages=(Message 0x8007);Offset=(Message 0x8004)/1000;Viewport=(Message 0x8005)/1000})
  Snapshot $gesture.Name
 }
 if($Checks){
  if($Variant-eq'baseline'){throw 'Regression assertions apply to the finite gallery only'}
  $passed=Message 0x8009
  if($passed-ne1){throw "Gallery regression failed; see $out\checks.txt"}
  $passed=0
  $null=Message 0x8001 1;Wait-Settled 1
  if((Message 0x800C)-ne1){throw 'Keyboard focus did not reach the owned XAML island'}
  Control-Key 0x09;Wait-Settled 2;Check-FocusedPage 2
  Control-Key 0x22;Wait-Settled 3;Check-FocusedPage 3
  Control-Key 0x21;Wait-Settled 2;Check-FocusedPage 2
  $null=Message 0x8001 5;Wait-Settled 5;Control-Key 0x22;Wait-Settled 5
  $null=Message 0x8001 0;Wait-Settled 0;Control-Key 0x21;Wait-Settled 0
  'PASS Ctrl+Tab, Ctrl+PageDown, Ctrl+PageUp, focus on visible page and finite keyboard edges'|Add-Content (Join-Path $out 'checks.txt')
  if((Message 0x800A)-ne$dpi){throw 'Desktop DPI changed during the test'}
  $passed=1
 }
}finally{
 if($recorder){
  if(!$recorder.HasExited){$recorder.StandardInput.WriteLine('q');if(!$recorder.WaitForExit(8000)){$recorder.Kill();$recorder.WaitForExit()}}
  # Preserve every diagnostic; expand console carriage returns into text lines.
  $lines=$recordLog.GetAwaiter().GetResult() -split '\r\n|\r|\n' | ForEach-Object { $_.TrimEnd() }
  [IO.File]::WriteAllText((Join-Path $out 'recorder.log'),(($lines -join "`n").TrimEnd()+"`n"))
  $recorder.Dispose()
 }
 if(!$bench.HasExited){[void][GalleryDriver]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero);if(!$bench.WaitForExit(5000)){$bench.Kill();$bench.WaitForExit()}}
 $bench.Dispose()
 [pscustomobject]@{Variant=$Variant;SHA256=(Get-FileHash -LiteralPath $exe).Hash;Dpi=$dpi;Input='Injected touch, not physical precision touchpad';RequestedCaptureRate=$(if($Record){120}else{0});ChecksRequested=$Checks.IsPresent;ChecksPassed=($passed-eq1);Dark=$Dark.IsPresent;Results=$results.ToArray()}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'results.json')
}
$results
