param([ValidateSet('native','cpp','tray')][string]$Target='cpp',[switch]$Dark,[switch]$Custom,[switch]$Interactions,[switch]$Cards,[ValidatePattern('^[a-zA-Z0-9-]*$')][string]$Revision='')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100\recordings';[IO.Directory]::CreateDirectory($out)|Out-Null
$tag=$Target+'-'+$(if($Custom){'custom'}elseif($Dark){'dark'}else{'light'})
if($Interactions){$tag+='-interactions'}
if($Cards){if($Target-ne'cpp'){throw 'Cards requires the C++ bench'};$tag+='-cards'}
if($Revision){$tag+='-'+$Revision}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms
foreach($file in @('Test-Bench.ps1','Capture-Reference.ps1')){
 $type=if($file-eq'Test-Bench.ps1'){'BenchNative'}else{'RefNative'}
 if(!($type-as[type])){$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot $file));$begin=$source.IndexOf('using System;');$end=$source.IndexOf("'@",$begin);Add-Type $source.Substring($begin,$end-$begin)}
}
[void][BenchNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
if(!('ThemeBroadcast'-as[type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class ThemeBroadcast {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr SendMessageTimeout(IntPtr w,uint m,UIntPtr p,string value,uint flags,uint timeout,out UIntPtr result);
 public static void Changed(){UIntPtr result;SendMessageTimeout(new IntPtr(65535),0x1a,UIntPtr.Zero,"ImmersiveColorSet",2,2000,out result);}
}
'@
}
$themeSnapshot=@{};$themeKey=$null
$mapping=$null;$state=$null;$trayApp=$null
if($Target-eq'tray'){$mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$state=$mapping.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read);$trayApp=(Get-Process -Id $state.ReadInt32(4)).Path;if($state.ReadInt32(20)-ne2-or$state.ReadInt32(28)-ne0){throw 'Attached healthy tray is required'}}
$exe=Join-Path $root 'artifacts\shell-panels\Release\ShellBench.exe';$benchArguments=if($Custom){'--custom'}elseif($Dark){'--dark'}else{'--light'}
[pscustomobject]@{Captured=(Get-Date).ToString('o');Target=$Target;Theme=$benchArguments;BenchSHA256=(Get-FileHash -LiteralPath $exe).Hash;AppSHA256=$(if($trayApp){(Get-FileHash -LiteralPath $trayApp).Hash}else{$null});RequestedCaptureRate=240;DuplicateFrames=$false;Coordinates='Physical pixels';OpenTrigger=$(if($Target-eq'cpp'){'WM_APP command'}else{'Physical pointer click'});OutsideTrigger='Physical pointer click'}|ConvertTo-Json|Set-Content (Join-Path $out ($tag+'-manifest.json'))
$bench=Start-Process -FilePath $exe -ArgumentList $benchArguments -WorkingDirectory $out -WindowStyle Hidden -PassThru
$ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
$record=$null;$clock=[Diagnostics.Stopwatch]::StartNew();$events=[Collections.Generic.List[object]]::new()
function Pause([int]$ms){Start-Sleep -Milliseconds $ms}
function Event([string]$name){$phase=if($state){$state.ReadInt32(60)}else{[BenchNative]::SendMessage($hostHandle,0x8004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64()};$events.Add([pscustomobject]@{Ms=$clock.Elapsed.TotalMilliseconds;Event=$name;Phase=$phase})}
function Snapshot([string]$name){$bitmap=[Drawing.Bitmap]::new(1120,1600);$g=[Drawing.Graphics]::FromImage($bitmap);try{$g.CopyFromScreen(1440,0,0,0,$bitmap.Size);$sample=$bitmap.GetPixel(50,600);if([Math]::Abs($sample.R-96)-gt2-or[Math]::Abs($sample.G-96)-gt2){throw 'Neutral background was obscured; capture rejected'};$bitmap.Save((Join-Path $out ($tag+'-'+$name+'-lossless.png')))}finally{$g.Dispose();$bitmap.Dispose()}}
function PrepareBackground {
 [void][BenchNative]::SetWindowPos($backHandle,[IntPtr]::new(-1),0,0,0,0,0x53)
 [RefNative]::Click(1600,700,$false)
 [void][BenchNative]::SetWindowPos($backHandle,[IntPtr]::new(-2),0,0,0,0,0x13)
 Pause 300
 if([RefNative]::GetForegroundWindow()-ne$backHandle){throw 'Neutral test window did not obtain focus; input aborted'}
}
function Trigger([int]$kind,[bool]$right=$false){
 if($Target-eq'tray'){$offset=$state.ReadInt32(64)-16;$x=[int](($state.ReadInt32($offset)+$state.ReadInt32($offset+8))/2);$y=[int](($state.ReadInt32($offset+4)+$state.ReadInt32($offset+12))/2);if(![RefNative]::IsTrayPoint($x,$y)){throw 'Application tray target is obscured'};[RefNative]::Click($x,$y,$right)}
 elseif($Target-eq'native'){$x=@(2264,2363,2488)[$kind];if(![RefNative]::IsTrayPoint($x,1564)){throw ("Expected taskbar target is obscured: "+[RefNative]::RootAtPoint($x,1564))};[RefNative]::Click($x,1564,$right)}
 else{[void][BenchNative]::PostMessage($hostHandle,$(if($right){0x8002}else{0x8001}),[IntPtr]::new($kind),[IntPtr]::Zero)}
}
function CalendarButton([string]$pattern){
 $window=[Windows.Automation.AutomationElement]::FromHandle([RefNative]::GetForegroundWindow())
 $buttons=$window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Button))
 $button=$buttons|Where-Object {$_.Current.Name-like$pattern-and!$_.Current.IsOffscreen}|Select-Object -First 1
 if(!$button){throw "Calendar control missing: $pattern"}
 $bounds=$button.Current.BoundingRectangle
 [void][RefNative]::SetPhysicalCursorPos([int]($bounds.X+$bounds.Width/2),[int]($bounds.Y+$bounds.Height/2));Pause 300
 Event ('press-'+$button.Current.Name);[RefNative]::Click([int]($bounds.X+$bounds.Width/2),[int]($bounds.Y+$bounds.Height/2),$false);Pause 850
}
try{
 if($Target-eq'native'-and$Dark){
  $themeKey=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Themes\Personalize',$true)
  foreach($key in @('AppsUseLightTheme','SystemUsesLightTheme')){$themeSnapshot[$key]=$themeKey.GetValue($key,$null);$themeKey.SetValue($key,0,[Microsoft.Win32.RegistryValueKind]::DWord)}
  [ThemeBroadcast]::Changed();Pause 1600
 }
 Pause 1800;$hostHandle=[BenchNative]::FindWindow('Yata.ShellBench.Host','ShellPanels test host');$backHandle=[BenchNative]::FindWindow('Yata.ShellBench.Background','C++ ShellPanels test bench')
 if($hostHandle-eq[IntPtr]::Zero){throw 'Test host missing'}
 [void][BenchNative]::SetForegroundWindow($backHandle)
 $info=[Diagnostics.ProcessStartInfo]::new($ff,('-hide_banner -y -f lavfi -i "ddagrab=framerate=240:draw_mouse=1:video_size=1120x1600:offset_x=1440:offset_y=0:dup_frames=0" -vf "hwdownload,format=bgra" -c:v h264_nvenc -preset p2 -cq 16 -fps_mode passthrough "'+(Join-Path $out ($tag+'.mp4'))+'"'))
 $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
 $record=[Diagnostics.Process]::Start($info);$stderr=$record.StandardError.ReadToEndAsync();$clock.Restart();Pause 1400
 if($record.HasExited){throw 'Recorder failed'}
 if($Cards){
  Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
  PrepareBackground
  Event 'cards-open';[void][BenchNative]::PostMessage($hostHandle,0x8008,[IntPtr]::Zero,[IntPtr]::Zero);Pause 1000;Snapshot 'cards'
  CalendarButton 'Tasks*';Snapshot 'cards-collapsed'
  CalendarButton 'Tasks*';Snapshot 'cards-expanded'
  Event 'cards-outside-close';[RefNative]::Click(1600,700,$false);Pause 500
 }elseif($Interactions){
  Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
  PrepareBackground
  Event 'date-open';Trigger 2;Pause 1000
  CalendarButton '*calendar';Snapshot 'date-collapsed'
  CalendarButton '*calendar';Snapshot 'date-expanded'
  CalendarButton 'Next*'
  CalendarButton 'Previous*'
  Event 'date-escape';[RefNative]::Escape();Pause 450
  Event 'quick-open';Trigger 1;Pause 900
  foreach($position in @(@(2073,1001),@(2145,1001),@(2240,1001),@(2070,1308),@(2250,1380))){Event 'pointer-hover';[void][RefNative]::SetPhysicalCursorPos($position[0],$position[1]);Pause 650}
  Event 'quick-outside-close';[RefNative]::Click(1600,700,$false);Pause 500
 }else{
 for($kind=0;$kind-lt$(if($Target-eq'tray'){4}else{3});$kind++){
  $name=@('lang','quick','date','gallery')[$kind]
  PrepareBackground
  if($Target-eq'tray'){$flag=@('--lang','--quick','--date','--controls')[$kind];& $trayApp $flag --show;Pause 650;[void][BenchNative]::PostMessage([IntPtr]::new($state.ReadInt64(8)),0x10,[IntPtr]::Zero,[IntPtr]::Zero);Pause 400;PrepareBackground;if($state.ReadInt32(60)-ne0){throw 'Panel setup did not reach Closed'}}
  Event "$name-open";Trigger $kind;Pause 1100;Snapshot $name
  if($state-and$state.ReadInt32(60)-ne2){throw 'Physical tray click did not open the panel'}
  Event "$name-toggle-close";Trigger $kind;Pause 600
  Event "$name-open-again";Trigger $kind;Pause 700
  Event "$name-outside-close";[RefNative]::Click(1600,700,$false);Pause 600
  if($state-and$state.ReadInt32(60)-ne0){throw 'Physical outside click did not close the panel'}
  Event "$name-context";Trigger $kind $true;Pause 800;Snapshot ($name+'-context')
  Event "$name-context-escape";[RefNative]::Escape();Pause 500
 }
 if($Target-ne'tray'){foreach($part in @(@('network',1,2320),@('battery',3,2408))){
  Event ($part[0]+'-context');if($Target-eq'native'){if(![RefNative]::IsTrayPoint($part[2],1564)){throw ("Expected taskbar target is obscured: "+[RefNative]::RootAtPoint($x,1564))};[RefNative]::Click($part[2],1564,$true)}else{[void][BenchNative]::PostMessage($hostHandle,0x8005,[IntPtr]::new($part[1]),[IntPtr]::Zero)};Pause 800;Snapshot ($part[0]+'-context')
  Event ($part[0]+'-context-escape');[RefNative]::Escape();Pause 400
 }
 }
 }
 Pause 400
}finally{
 if($themeKey){foreach($key in $themeSnapshot.Keys){if($null-eq$themeSnapshot[$key]){$themeKey.DeleteValue($key,$false)}else{$themeKey.SetValue($key,$themeSnapshot[$key],[Microsoft.Win32.RegistryValueKind]::DWord)}};$themeKey.Dispose();[ThemeBroadcast]::Changed()}
 if($record){if(!$record.HasExited){$record.StandardInput.WriteLine('q');if(!$record.WaitForExit(10000)){$record.Kill()}};$stderr.Result|Set-Content (Join-Path $out ($tag+'-recorder.log'));$record.Dispose()}
 $events|ConvertTo-Json|Set-Content (Join-Path $out ($tag+'-events.json'))
 if(!$bench.HasExited){[void][BenchNative]::PostMessage($backHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero);[void]$bench.WaitForExit(5000)}
 if($state){$state.Dispose()};if($mapping){$mapping.Dispose()}
}
& (Join-Path ([IO.Path]::GetDirectoryName($ff)) 'ffprobe.exe') -v error -select_streams v:0 -show_entries 'stream=width,height,avg_frame_rate,r_frame_rate,duration,nb_frames:frame=best_effort_timestamp_time' -of json (Join-Path $out ($tag+'.mp4')) | Set-Content (Join-Path $out ($tag+'-frames.json'))
