param([string]$App=(Join-Path $PSScriptRoot '..\..\..\YataNativeFramework.exe'),[string]$Label='architecture-v12',[switch]$NoCrashTest)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class ArchitectureTest {
 [DllImport("user32.dll")]public static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
}
'@
$App=[IO.Path]::GetFullPath($App);$checks=[Collections.Generic.List[string]]::new();$events=[Collections.Generic.List[object]]::new()
function Run-Command([string]$arguments,[int]$expected=0){
 $start=[Diagnostics.ProcessStartInfo]::new($App,$arguments);$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 $process=[Diagnostics.Process]::Start($start);$output=$process.StandardOutput.ReadToEnd();$errorOutput=$process.StandardError.ReadToEnd()
 if(!$process.WaitForExit(6000)){throw "CLI timeout: $arguments"};$code=$process.ExitCode;$process.Dispose()
 if($code -ne $expected){throw "CLI $arguments returned $code, expected $expected; $output $errorOutput"};return $output
}
function Assert([bool]$ok,[string]$text){if(!$ok){throw $text};$checks.Add("PASS $text")}
function Open-State{
 $script:map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$script:view=$map.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
}
function Wait-Condition([scriptblock]$condition,[string]$description){
 $timer=[Diagnostics.Stopwatch]::StartNew();while(!(& $condition)){if($timer.ElapsedMilliseconds -gt 5000){throw "Timeout: $description; $(Run-Command '--status')"};Start-Sleep -Milliseconds 20}
}
function Wait-Applied([int]$mode){Wait-Condition {$view.ReadInt32(20) -eq $mode -and $view.ReadInt32(100) -eq $view.ReadInt32(84)} "applied mode $mode and latest revision"}
$map=$null;$view=$null;$original=$null
try{
 Open-State
 $original=[pscustomobject]@{Mode=$view.ReadInt32(16);VpnWidth=$view.ReadInt32(88);SpeedWidth=$view.ReadInt32(92)}
 Run-Command '--mode vpn --width 44' | Out-Null;Wait-Applied 1
 Run-Command '--native-speed --native-width=88' | Out-Null;Wait-Applied 2
 Assert ($view.ReadInt32(88) -eq 44 -and $view.ReadInt32(92) -eq 88 -and [Math]::Abs($view.ReadDouble(40)-88) -lt 1) 'combined legacy CLI updates speed and leaves VPN width intact'
 Run-Command '--width 160 --mode vpn' | Out-Null;Wait-Applied 1
 Assert ($view.ReadInt32(88) -eq 160 -and $view.ReadInt32(92) -eq 88) 'reversed option order targets VPN width atomically'
 $revision=$view.ReadInt32(84)
 foreach($args in '--width abc','--width -1','--width 31','--width 321','--width 88junk','--width 4294967296','--width','--mode unknown','--status --mode vpn'){
  Run-Command $args 2 | Out-Null;Assert ($view.ReadInt32(84) -eq $revision) "invalid CLI does not mutate settings: $args"
 }
 foreach($cycle in 1..20){
  Run-Command '--detach' | Out-Null
  Wait-Condition {$view.ReadInt32(112) -eq 0 -and $view.ReadInt32(116) -eq 0 -and $view.ReadInt64(104) -eq 0} 'fully detached resources'
  Assert ($view.ReadInt32(20) -eq 0 -and $view.ReadInt32(120) -eq 0) "detach cycle $cycle releases native subscriptions and timer"
  Run-Command '--mode speed --width 88' | Out-Null;Wait-Applied 2
  $events.Add((Run-Command '--profile' | ConvertFrom-Json))
 }
 Run-Command '--test-fault' | Out-Null
 Wait-Condition {$view.ReadInt32(112) -eq 3 -and $view.ReadInt32(20) -eq 0} 'contained fault'
 Assert ($view.ReadInt32(116) -eq 0 -and $view.ReadInt32(120) -eq 0) 'callback exception restores the icon and releases active resources'
 Run-Command '--mode speed --width 88' | Out-Null;Wait-Applied 2
 Assert ($view.ReadInt32(28) -eq 0) 'explicit reconnect recovers after a contained fault'
 if(!$NoCrashTest){
  $oldId=$view.ReadInt32(4);$oldSession=$view.ReadInt64(72)
  (Get-Process -Id $oldId).Kill();(Get-Process -Id $oldId -ErrorAction SilentlyContinue) | Wait-Process -ErrorAction SilentlyContinue
  Wait-Condition {$view.ReadInt32(112) -eq 0 -and $view.ReadInt32(116) -eq 0 -and $view.ReadInt64(104) -eq 0} 'process-exit cleanup'
  Assert ($view.ReadInt32(20) -eq 0) 'abrupt application exit restores the native icon through a process wait'
  $view.Dispose();$view=$null;$map.Dispose();$map=$null
  $start=[Diagnostics.ProcessStartInfo]::new($App,'--native-speed');$start.UseShellExecute=$false;$start.CreateNoWindow=$true;[void][Diagnostics.Process]::Start($start)
  $limit=[Diagnostics.Stopwatch]::StartNew();while(!$view){try{Open-State}catch{if($limit.ElapsedMilliseconds -gt 5000){throw};Start-Sleep -Milliseconds 50}}
  Wait-Applied 2
  Assert ($view.ReadInt64(72) -ne $oldSession -and $view.ReadInt32(4) -ne $oldId) 'new application session reconnects to the existing inactive broker'
 }
 $checks.Add('ALL LIVE ARCHITECTURE CHECKS PASSED')
}catch{$checks.Add("FAIL $($_.Exception.Message)");$checks.Add($_.ScriptStackTrace);throw}
finally{
 if($view -and $original){
  try{
   Run-Command "--mode vpn --width $($original.VpnWidth)" | Out-Null
   Run-Command "--mode speed --width $($original.SpeedWidth)" | Out-Null
   Run-Command "--mode $(@('off','vpn','speed')[$original.Mode])" | Out-Null
  }catch{$checks.Add("RESTORE ERROR $($_.Exception.Message)")}
 }
 if($view){$view.Dispose()};if($map){$map.Dispose()}
 $checks|Set-Content -LiteralPath "$PSScriptRoot\$Label-results.txt" -Encoding UTF8
 $events|ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$PSScriptRoot\$Label-profile.json" -Encoding UTF8
}
