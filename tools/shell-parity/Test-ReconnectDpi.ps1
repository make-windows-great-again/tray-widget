[CmdletBinding()]
param(
 [string]$BinaryDirectory=(Join-Path $PSScriptRoot '..\..\bin\Release'),
 [ValidateSet(100,125,150,175,200,225,250)][int[]]$Scales=@(100,150,200,250),
 [string]$EvidenceDirectory=(Join-Path $PSScriptRoot '..\..\evidence\reconnect-dpi')
)
$ErrorActionPreference='Stop'
$BinaryDirectory=[IO.Path]::GetFullPath($BinaryDirectory)
$EvidenceDirectory=[IO.Path]::GetFullPath($EvidenceDirectory)
if(Get-Process YataNativeFramework -ErrorAction SilentlyContinue){throw 'Close the running framework before starting this owned-process test.'}
$explorer=Get-Process explorer -ErrorAction Stop
if(@($explorer).Count-ne1){throw 'Expected one Explorer process'}
$patch=@($explorer.Modules | Where-Object ModuleName -eq 'YataNativeFramework.Patch.dll')
if($patch.Count-ne1){throw 'Install the candidate patch into Explorer before running this test.'}
$expectedHash=(Get-FileHash -LiteralPath (Join-Path $BinaryDirectory 'YataNativeFramework.Patch.dll')).Hash
if((Get-FileHash -LiteralPath $patch[0].FileName).Hash-ne$expectedHash){throw 'Explorer is using a different patch file than the test candidate.'}
. (Join-Path $PSScriptRoot 'DpiHarness.ps1')
if(!('ReconnectWindowState'-as[type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class ReconnectWindowState {
 [DllImport("user32.dll")]public static extern bool IsIconic(IntPtr window);
}
'@}
$settings=Get-SettingsWindow
$settingsHandle=[IntPtr]$settings.Current.NativeWindowHandle
$wasMinimized=[ReconnectWindowState]::IsIconic($settingsHandle)
$originalScale=Get-DisplayScale
$layout=Join-Path $env:TEMP 'YataNativeFramework.layout.ini'
$layoutHash=if(Test-Path -LiteralPath $layout){(Get-FileHash -LiteralPath $layout).Hash}else{$null}
$results=[Collections.Generic.List[object]]::new()
[IO.Directory]::CreateDirectory($EvidenceDirectory)|Out-Null
try{
 foreach($scale in $Scales){
  Set-DisplayScale $scale
  $caseDirectory=Join-Path $EvidenceDirectory ([string]$scale)
  & (Join-Path $PSScriptRoot '..\Test-Live.ps1') -BinaryDirectory $BinaryDirectory -IdleSeconds 1 -CrashTest -EvidenceDirectory $caseDirectory
  if(!$?){throw "Live test failed at $scale percent"}
  $sameExplorer=(Get-Process explorer -ErrorAction Stop).Id-eq$explorer.Id
  if(!$sameExplorer){throw 'Explorer restarted during the reconnect test'}
  $checks=@(Get-Content -LiteralPath (Join-Path $caseDirectory 'live-results-current.txt'))
  $results.Add([pscustomobject]@{Scale=$scale;Dpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());Pass=@($checks|Where-Object {$_-like'PASS *'}).Count;SameExplorer=$sameExplorer})
 }
}finally{
 try{Set-DisplayScale $originalScale}finally{
  if($wasMinimized){[void][DpiDesktop]::ShowWindow($settingsHandle,6)}
  $currentHash=if(Test-Path -LiteralPath $layout){(Get-FileHash -LiteralPath $layout).Hash}else{$null}
  $record=[pscustomobject]@{Explorer=$explorer.Id;PatchSHA256=$expectedHash;InitialScale=$originalScale;FinalDpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());SettingsUnchanged=($currentHash-eq$layoutHash);Results=$results.ToArray()}
  $record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'results.json') -Encoding utf8
  if(!$record.SettingsUnchanged){throw 'Layout settings were not restored'}
 }
}
$results
