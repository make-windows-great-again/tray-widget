param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..\..\evidence\shell-parity-26100\recordings'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory);[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
$ff=(Get-ChildItem (Join-Path $root 'artifacts\tools') -Recurse -Filter ffmpeg.exe|Select-Object -First 1).FullName
if(!$ff){throw 'Portable ffmpeg is required in artifacts/tools'}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms
$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Capture-Reference.ps1'))
$begin=$source.IndexOf("using System;using System.Text;");$end=$source.IndexOf("'@",$begin)
Add-Type $source.Substring($begin,$end-$begin)
[void][RefNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$buttons=Get-Content (Join-Path $root 'evidence\shell-parity-26100\taskbar.json')|ConvertFrom-Json
$back=[Windows.Forms.Form]::new();$back.FormBorderStyle='None';$back.StartPosition='Manual';$back.Bounds=[Drawing.Rectangle]::new(1440,0,1120,1528);$back.BackColor=[Drawing.Color]::FromArgb(96,96,96);$back.Show();$back.Refresh();[Windows.Forms.Application]::DoEvents()
$info=[Diagnostics.ProcessStartInfo]::new($ff,('-hide_banner -y -f lavfi -i "ddagrab=framerate=240:draw_mouse=1:video_size=1120x1600:offset_x=1440:offset_y=0:dup_frames=0" -vf "hwdownload,format=bgra" -c:v h264_nvenc -preset p2 -cq 16 -fps_mode passthrough "'+(Join-Path $OutputDirectory 'native.mp4')+'"'))
$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
$record=[Diagnostics.Process]::Start($info);$stderr=$record.StandardError.ReadToEndAsync()
$clock=[Diagnostics.Stopwatch]::StartNew();$events=[Collections.Generic.List[object]]::new()
function Pause([int]$ms){$until=$clock.ElapsedMilliseconds+$ms;while($clock.ElapsedMilliseconds-lt$until){[Windows.Forms.Application]::DoEvents();Start-Sleep -Milliseconds 5}}
function Event([string]$name){$events.Add([pscustomobject]@{Ms=$clock.Elapsed.TotalMilliseconds;Event=$name})}
try{
 Pause 1400;if($record.HasExited){throw 'Recorder exited before interaction'}
 foreach($name in @('lang','quick','date')){
  $pattern=@{lang='Tray Input Indicator*';quick='Volume*';date='Clock*'}[$name];$button=$buttons|Where-Object Name -like $pattern|Select-Object -First 1
  $x=[int]($button.X+$button.W/2);$y=[int]($button.Y+$button.H/2)
  Event "$name-hover";[void][RefNative]::SetCursorPos($x,$y);Pause 900
  Event "$name-open";[RefNative]::Click($x,$y,$false);Pause 1200
  Event "$name-toggle-close";[RefNative]::Click($x,$y,$false);Pause 700
  Event "$name-open-again";[RefNative]::Click($x,$y,$false);Pause 700
  Event "$name-outside-close";[RefNative]::Click(1600,200,$false);Pause 700
  Event "$name-context";[RefNative]::Click($x,$y,$true);Pause 900
  Event "$name-context-escape";[RefNative]::Escape();Pause 500
 }
 Pause 400
}finally{
 if(!$record.HasExited){$record.StandardInput.WriteLine('q');if(!$record.WaitForExit(10000)){$record.Kill()}}
 $stderr.Result|Set-Content (Join-Path $OutputDirectory 'native-recorder.log') -Encoding UTF8
 $events|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'native-events.json') -Encoding UTF8
 $back.Close();$back.Dispose();$record.Dispose()
}
$probe=Join-Path ([IO.Path]::GetDirectoryName($ff)) 'ffprobe.exe'
& $probe -v error -select_streams v:0 -show_entries 'stream=width,height,avg_frame_rate,r_frame_rate,duration,nb_frames:frame=best_effort_timestamp_time,pkt_duration_time' -of json (Join-Path $OutputDirectory 'native.mp4') | Set-Content (Join-Path $OutputDirectory 'native-frames.json') -Encoding UTF8
