param([string]$BinaryPath)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100\dpi-v16\accepted-hot'
[IO.Directory]::CreateDirectory($out)|Out-Null
. (Join-Path $PSScriptRoot 'DpiHarness.ps1')
[void][DpiDesktop]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
if(!$BinaryPath){$BinaryPath=Join-Path $root 'artifacts\dpi-trace-v16\ShellBench.exe'}
if([DpiDesktop]::FindWindow('Yata.ShellBench.Host','ShellPanels test host')-ne[IntPtr]::Zero){throw 'Bench already running'}
$initial=Get-DisplayScale
$bench=$null;$record=$null;$rows=[Collections.Generic.List[object]]::new();$failure=$null
try {
 Set-DisplayScale 100
 $bench=Start-Process $BinaryPath -WorkingDirectory $out -WindowStyle Hidden -PassThru
 Start-Sleep -Milliseconds 1000
 $hwnd=[DpiDesktop]::FindWindow('Yata.ShellBench.Host','ShellPanels test host')
 $back=[DpiDesktop]::FindWindow('Yata.ShellBench.Background','C++ ShellPanels test bench')
 $display=[DpiDesktop]::Display()
 [DpiDesktop]::Position($back,[IntPtr]::new(-1),$display.Monitor.Right-1120,0,1120,$display.Work.Bottom,0x40)
 [void][DpiDesktop]::SetForegroundWindow($back)
 [DpiDesktop]::Position($back,[IntPtr]::new(-2),0,0,0,0,0x13)
 [void][DpiDesktop]::SendMessage($hwnd,0x800b,[IntPtr]::new($display.Work.Right-20),[IntPtr]::new($display.Work.Bottom))
 [void][DpiDesktop]::SendMessage($hwnd,0x800d,[IntPtr]::new(1),[IntPtr]::Zero)
 [void][DpiDesktop]::PostMessage($hwnd,0x8001,[IntPtr]::new(2),[IntPtr]::Zero)
 Start-Sleep -Milliseconds 1000
 $ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
 $info=[Diagnostics.ProcessStartInfo]::new($ff,('-hide_banner -y -f lavfi -i "ddagrab=framerate=240:draw_mouse=1:video_size=1120x1600:offset_x=1440:offset_y=0:dup_frames=0" -vf "hwdownload,format=bgra" -c:v h264_nvenc -preset p2 -cq 18 -fps_mode passthrough "'+(Join-Path $out 'transition.mp4')+'"'))
 $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
 $record=[Diagnostics.Process]::Start($info);$stderr=$record.StandardError.ReadToEndAsync()
 $collapsed=$false
 foreach($percent in 100,125,150,175,200,225,250,175,100,150){
  if($percent-eq250-and!$collapsed){
   $window=[Windows.Automation.AutomationElement]::FromHandle($hwnd)
   $button=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'Collapse calendar'))
   if(!$button){throw 'Collapse control missing'}
   $button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
   Start-Sleep -Milliseconds 450
   $collapsed=$true
  }
  Set-DisplayScale $percent
  Start-Sleep -Milliseconds 350
  $phase=[DpiDesktop]::SendMessage($hwnd,0x8004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64()
  if($phase-ne2){throw "Panel closed during DPI change to $percent"}
  [void][DpiDesktop]::PostMessage($hwnd,[DpiDesktop]::RegisterWindowMessage('Yata.ShellPanels.TraceLayout.v1'),[IntPtr]::Zero,[IntPtr]::Zero)
  Start-Sleep -Milliseconds 150
  $trace=Import-Csv (Join-Path $out 'layout-trace.csv');$layout=$trace|Select-Object -First 1
  $expected=$percent/100.0
  if([Math]::Abs([double]$layout.scale-$expected)-gt.001){throw 'XamlRoot did not reach target DPI'}
  $display=[DpiDesktop]::Display()
  $expectedX=$display.Work.Right-(12+336)*$expected
  if([Math]::Abs([double]$layout.screen_x-$expectedX)-gt1){throw 'Card anchor drifted'}
  $window=[Windows.Automation.AutomationElement]::FromHandle($hwnd)
  $name=if($collapsed){'Expand calendar'}else{'Collapse calendar'}
  $button=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name))
  if(!$button){throw 'Expansion state lost during DPI change'}
  $rows.Add([pscustomobject]@{Scale=$percent;Phase=$phase;Collapsed=$collapsed;RasterizationScale=$layout.scale;X=$layout.screen_x;ExpectedX=$expectedX;Height=$layout.h;Passed=$true})
  Copy-Item (Join-Path $out 'layout-trace.csv') (Join-Path $out ("step-$($rows.Count)-$percent.csv")) -Force
 }
} catch {$failure=$_.Exception.Message;throw} finally {
 try { Set-DisplayScale $initial } finally {
  if($record-and!$record.HasExited){$record.StandardInput.WriteLine('q');if(!$record.WaitForExit(10000)){$record.Kill()}}
  if($record){$stderr.Result|Set-Content (Join-Path $out 'recorder.log')}
  if($bench-and!$bench.HasExited){[void][DpiDesktop]::PostMessage($back,0x10,[IntPtr]::Zero,[IntPtr]::Zero);if(!$bench.WaitForExit(5000)){$bench.Kill()}}
  $rows|ConvertTo-Json|Set-Content (Join-Path $out 'results.json')
  [pscustomobject]@{InitialScale=$initial;FinalDpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());Steps=$rows.Count;Failure=$failure;BenchSHA256=(Get-FileHash $BinaryPath).Hash}|ConvertTo-Json|Set-Content (Join-Path $out 'manifest.json')
 }
}
$rows|Format-Table -AutoSize