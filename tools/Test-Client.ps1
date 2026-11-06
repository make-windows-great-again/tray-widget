[CmdletBinding()]
param([string]$BinaryDirectory=(Join-Path $PSScriptRoot '..\bin\Release'),[switch]$Capture)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$app=Join-Path ([IO.Path]::GetFullPath($BinaryDirectory)) 'YataNativeFramework.exe'
$evidence=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\evidence'))
[IO.Directory]::CreateDirectory($evidence)|Out-Null
$checks=[Collections.Generic.List[string]]::new();$mapping=$null;$view=$null;$application=$null;$owned=$false
function Assert([bool]$ok,[string]$text){if(!$ok){throw $text};$checks.Add("PASS $text")}
function Run-Command([string]$arguments,[int]$expected=0){
 $info=[Diagnostics.ProcessStartInfo]::new($app,$arguments);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $process=[Diagnostics.Process]::Start($info)
 try{
  $output=$process.StandardOutput.ReadToEndAsync();$errors=$process.StandardError.ReadToEndAsync()
  if(!$process.WaitForExit(5000)){$process.Kill();throw "CLI timeout: $arguments"}
  if($process.ExitCode-ne $expected){throw "Unexpected exit $($process.ExitCode): $arguments $($errors.GetAwaiter().GetResult())"}
  return $output.GetAwaiter().GetResult()
 }finally{$process.Dispose()}
}
 $layoutFile=Join-Path $env:TEMP 'YataNativeFramework.layout.ini'
 $layoutExisted=Test-Path -LiteralPath $layoutFile
 $layoutSnapshot=if($layoutExisted){[IO.File]::ReadAllBytes($layoutFile)}else{$null}
try{
 try{$mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13')}catch{}
 if($mapping){throw 'Close the running framework instance before starting the client test.'}
 $info=[Diagnostics.ProcessStartInfo]::new($app,'--standalone --controls --show --mode label --width 44');$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 $application=[Diagnostics.Process]::Start($info);$owned=$true
 $watch=[Diagnostics.Stopwatch]::StartNew()
 while(!$view){
  if($application.HasExited){throw "Startup failure: $($application.StandardOutput.ReadToEnd())"}
  try{$mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$mapping.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)}catch{if($watch.ElapsedMilliseconds-gt 5000){throw};Start-Sleep -Milliseconds 30}
 }
 Start-Sleep -Milliseconds 600
 Assert (!$application.HasExited) 'production executable hosts the gallery without Explorer patching'
 Assert ($view.ReadInt32(16)-eq 1 -and $view.ReadInt32(88)-eq 44) 'startup applies the selected label and width'
 Run-Command '--native-speed --native-width=88'|Out-Null
 Assert ($view.ReadInt32(16)-eq 2 -and $view.ReadInt32(88)-eq 44 -and $view.ReadInt32(92)-eq 88) 'actual WM_COPYDATA command updates mode and width together'
 Run-Command '--width 160 --mode label'|Out-Null
 Assert ($view.ReadInt32(16)-eq 1 -and $view.ReadInt32(88)-eq 160 -and $view.ReadInt32(92)-eq 88) 'actual reversed CLI option order preserves the other width'
 $before=$view.ReadInt32(84)
 Run-Command '--width 88garbage' 2|Out-Null
 Assert ($view.ReadInt32(84)-eq $before) 'invalid client CLI cannot change the running request'
 Run-Command '--sample --show'|Out-Null;Start-Sleep -Milliseconds 250
 Run-Command '--controls --show'|Out-Null;Start-Sleep -Milliseconds 350
 Assert (!$application.HasExited -and $view.ReadInt32(56)-eq 1) 'live client content can switch between sample and gallery'
 $profile=Run-Command '--profile'|ConvertFrom-Json
 Assert ($profile.pollingTimers-eq 0 -and $profile.tooltipTimer-eq 0) 'standalone client does not create backend polling timers'
 Assert ((Run-Command '--status')-match 'mode=1 applied=0') 'status reports that standalone mode has no Explorer decoration'
 if($Capture){
  try{
  Add-Type -AssemblyName System.Drawing
  Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class ClientCapture {
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int left,top,right,bottom;}
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
}
'@
  $old=[ClientCapture]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
  try{
   $r=New-Object ClientCapture+Rect
   if(![ClientCapture]::GetWindowRect([IntPtr]::new($view.ReadInt64(8)),[ref]$r)){throw 'Cannot locate client window'}
   $image=[Drawing.Bitmap]::new($r.right-$r.left,$r.bottom-$r.top);$graphics=[Drawing.Graphics]::FromImage($image)
   try{$graphics.CopyFromScreen($r.left,$r.top,0,0,$image.Size);$image.Save((Join-Path $evidence 'gallery-current.png'),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$image.Dispose()}
  }finally{[void][ClientCapture]::SetThreadDpiAwarenessContext($old)}
  }catch{$checks.Add("CAPTURE UNAVAILABLE: $($_.Exception.Message)")}
 }
 Run-Command '--exit'|Out-Null
 Assert ($application.WaitForExit(5000)) 'the production executable exits cleanly through its own CLI'
 $checks.Add('ALL CLIENT CHECKS PASSED')
}catch{$checks.Add("FAIL $($_.Exception.Message)");throw}
finally{
 if($owned -and $application -and !$application.HasExited){try{Run-Command '--exit'|Out-Null}catch{}}
 if($view){$view.Dispose()};if($mapping){$mapping.Dispose()};if($application){$application.Dispose()}
 if($owned){if($layoutExisted){[IO.File]::WriteAllBytes($layoutFile,$layoutSnapshot)}elseif(Test-Path -LiteralPath $layoutFile){Remove-Item -LiteralPath $layoutFile}}
 $checks|Set-Content -LiteralPath (Join-Path $evidence 'client-results-current.txt') -Encoding UTF8
 $checks|Write-Output
}
