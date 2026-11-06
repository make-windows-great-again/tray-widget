[CmdletBinding()]
param([string]$BinaryDirectory=(Join-Path $PSScriptRoot '..\bin\Release'),[int]$IdleSeconds=10,[switch]$CrashTest,
 [string]$EvidenceDirectory=(Join-Path $PSScriptRoot '..\evidence'))
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$BinaryDirectory=[IO.Path]::GetFullPath($BinaryDirectory)
$app=Join-Path $BinaryDirectory 'YataNativeFramework.exe'
$evidence=[IO.Path]::GetFullPath($EvidenceDirectory)
[IO.Directory]::CreateDirectory($evidence)|Out-Null
$checks=[Collections.Generic.List[string]]::new();$mapping=$null;$view=$null;$owned=$false;$application=$null
$layoutObservations=[Collections.Generic.List[object]]::new()
function Run-Command([string]$arguments,[int]$expected=0){
 $info=[Diagnostics.ProcessStartInfo]::new($app,$arguments);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $process=[Diagnostics.Process]::Start($info)
 try{
  $outputTask=$process.StandardOutput.ReadToEndAsync();$errorTask=$process.StandardError.ReadToEndAsync()
  if(!$process.WaitForExit(15000)){$process.Kill();throw "CLI timeout: $arguments"}
  $output=$outputTask.GetAwaiter().GetResult();$errors=$errorTask.GetAwaiter().GetResult()
  if($process.ExitCode -ne $expected){throw "CLI $arguments returned $($process.ExitCode): $output $errors"};return $output
 }finally{$process.Dispose()}
}
function Assert([bool]$ok,[string]$message){if(!$ok){throw $message};$checks.Add("PASS $message")}
function Wait-For([scriptblock]$condition,[string]$message,[switch]$AllowExited){
 $watch=[Diagnostics.Stopwatch]::StartNew()
 while(!(& $condition)){
  if(!$AllowExited-and$application-and$application.HasExited){throw "Application exited ($($application.ExitCode)): $($application.StandardOutput.ReadToEnd()) $($application.StandardError.ReadToEnd())"}
  if($view -and $view.ReadInt32(36)-lt 0 -and $view.ReadInt32(20)-eq 0){throw ('Shell attachment failed: 0x{0:X8}' -f $view.ReadInt32(36))}
  if($watch.ElapsedMilliseconds -gt 12000){throw "Timeout: $message; $(Run-Command '--status')"};Start-Sleep -Milliseconds 30
 }
}
function Wait-Applied([int]$mode){
 Wait-For {
  if($view.ReadInt32(20)-ne$mode-or$view.ReadInt32(100)-ne$view.ReadInt32(84)){return $false}
  $widthOffset=if($mode-eq2){92}else{88}
  $requested=$view.ReadInt32($widthOffset);$actual=$view.ReadDouble(40)
  if($requested-eq0-or[Math]::Abs($actual-$requested)-lt1){return $true}
  # Request acknowledgement precedes XAML's asynchronous arrange/SizeChanged.
  $layoutObservations.Add([pscustomobject]@{Mode=$mode;Revision=$view.ReadInt32(84);RequestedWidth=$requested;ActualWidth=$actual})
  return $false
 } "applied $mode, current revision and arranged width"
}
function Start-App {
 $info=[Diagnostics.ProcessStartInfo]::new($app,'--controls --show --mode label --width 44');$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $script:application=[Diagnostics.Process]::Start($info);$script:owned=$true
}
 $layoutFile=Join-Path $env:TEMP 'YataNativeFramework.layout.ini'
 $layoutExisted=Test-Path -LiteralPath $layoutFile
 $layoutSnapshot=if($layoutExisted){[IO.File]::ReadAllBytes($layoutFile)}else{$null}
try{
 # Never disrupt another running instance: this test owns the application it starts.
 try{$mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13')}catch{}
 if($mapping){throw 'Close the running framework instance before starting the live test.'}
 Assert ((Run-Command '--version').Trim()-eq 'Yata Native Framework 16') 'version role runs without a UI'
 Start-App
 Wait-For {try{$script:mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$script:view=$mapping.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read);$true}catch{$false}} 'application state creation'
 Wait-Applied 1
 Assert ($view.ReadInt32(28)-eq 0) 'patch is applied in the real shell without a callback failure'
 Run-Command '--mode label --width 44'|Out-Null;Wait-Applied 1
 Run-Command '--native-speed --native-width=88'|Out-Null;Wait-Applied 2
 Assert ($view.ReadInt32(88)-eq 44 -and $view.ReadInt32(92)-eq 88 -and [Math]::Abs($view.ReadDouble(40)-88)-lt 1) 'combined mode and width are applied to the intended presentation'
 Run-Command '--width 160 --mode label'|Out-Null;Wait-Applied 1
 Assert ($view.ReadInt32(88)-eq 160 -and $view.ReadInt32(92)-eq 88) 'reversed option order preserves the other presentation width'
 $revision=$view.ReadInt32(84)
 foreach($arguments in '--width abc','--width -1','--width 31','--width 321','--width 88junk','--width 4294967296','--width','--status --controls'){
  Run-Command $arguments 2|Out-Null;Assert ($view.ReadInt32(84)-eq $revision) "invalid CLI leaves the request unchanged: $arguments"
 }
 foreach($cycle in 1..20){
  Run-Command '--detach'|Out-Null
  Wait-For {$view.ReadInt32(112)-eq 0 -and $view.ReadInt32(116)-eq 0 -and $view.ReadInt64(104)-eq 0} 'detach cleanup'
  Assert ($view.ReadInt32(20)-eq 0 -and $view.ReadInt32(120)-eq 0) "detach $cycle releases subscriptions and timers"
  Run-Command '--mode label --width 44'|Out-Null;Wait-Applied 1
 }
 Run-Command '--test-fault'|Out-Null
 Wait-For {$view.ReadInt32(112)-eq 3 -and $view.ReadInt32(20)-eq 0} 'contained failure'
 Assert ($view.ReadInt32(116)-eq 0 -and $view.ReadInt32(120)-eq 0) 'fault restores native content and releases session resources'
 Run-Command '--mode label --width 44'|Out-Null;Wait-Applied 1
 # The gallery intentionally contains an animated ProgressRing. Backend counters
 # measure the patch separately from client-side control animations.
 $before=Run-Command '--profile'|ConvertFrom-Json
 Start-Sleep -Seconds $IdleSeconds
 $after=Run-Command '--profile'|ConvertFrom-Json
 Assert ($after.pollingTimers-eq 0 -and $after.tooltipTimer-eq 0) 'idle backend has no periodic timer'
 [pscustomobject]@{Seconds=$IdleSeconds;Before=$before;After=$after;RefreshDelta=$after.refreshes-$before.refreshes}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $evidence 'live-profile-current.json') -Encoding UTF8
 if($CrashTest){
  Assert ($application.Id-eq $view.ReadInt32(4)) 'crash test targets only the process started by this test'
  $application.Kill();$application.WaitForExit()
  Wait-For {$view.ReadInt32(112)-eq 0 -and $view.ReadInt32(116)-eq 0 -and $view.ReadInt64(104)-eq 0} 'process-exit cleanup' -AllowExited
  $application.Dispose();$application=$null
  $view.Dispose();$view=$null;$mapping.Dispose();$mapping=$null;Start-App
  Wait-For {try{$script:mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$script:view=$mapping.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read);$true}catch{$false}} 'new session'
  Wait-Applied 1;Assert ($view.ReadInt32(28)-eq 0) 'new process reconnects to the inactive broker'
 }
 $checks.Add('ALL LIVE FRAMEWORK CHECKS PASSED')
}catch{
 $checks.Add("FAIL $($_.Exception.Message)")
 if($owned){try{Run-Command '--status'|Set-Content -LiteralPath (Join-Path $evidence 'live-failure-status.txt') -Encoding UTF8}catch{}}
 throw
}
finally{
 if($owned){try{Run-Command '--exit'|Out-Null}catch{}}
 $shutdownFailed=$false
 if($owned-and$application-and!$application.HasExited-and!$application.WaitForExit(8000)){
  $shutdownFailed=$true;$application.Kill();$null=$application.WaitForExit(5000)
  $checks.Add('FAIL owned application did not exit cleanly')
 }
 if($view){$view.Dispose()};if($mapping){$mapping.Dispose()};if($application){$application.Dispose()}
 if($owned){if($layoutExisted){[IO.File]::WriteAllBytes($layoutFile,$layoutSnapshot)}elseif(Test-Path -LiteralPath $layoutFile){Remove-Item -LiteralPath $layoutFile}}
 $checks|Set-Content -LiteralPath (Join-Path $evidence 'live-results-current.txt') -Encoding UTF8
 if($layoutObservations.Count){$layoutObservations|Export-Csv -LiteralPath (Join-Path $evidence 'live-layout-observations.csv') -NoTypeInformation -Encoding UTF8}
 $checks|Write-Output
 if($shutdownFailed){throw 'Owned application shutdown timed out'}
}
