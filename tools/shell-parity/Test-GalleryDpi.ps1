[CmdletBinding()]
param([ValidateSet(100,150,200,250)][int[]]$Scales=@(100,150,200,250),[switch]$Record,[switch]$Resume)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DpiHarness.ps1')
if(!('GallerySettingsState'-as[type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class GallerySettingsState {
 [DllImport("user32.dll")]public static extern bool IsIconic(IntPtr window);
}
'@}
$settings=Get-SettingsWindow
$settingsHandle=[IntPtr]$settings.Current.NativeWindowHandle
$wasMinimized=[GallerySettingsState]::IsIconic($settingsHandle)
$initial=Get-DisplayScale
$summary=[Collections.Generic.List[object]]::new()
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\gallery-swipe-20261106'
$binaryHash=(Get-FileHash -LiteralPath (Join-Path $root 'artifacts\gallery-swipe\Release\GallerySwipeBench.exe')).Hash
try{
 foreach($scale in $Scales){
  Set-DisplayScale $scale
  foreach($dark in @($false,$true)){
   $label='accepted-'+$scale+'-'+$(if($dark){'dark'}else{'light'})
   $resultFile=Join-Path $out "$label\results.json"
   $report=if($Resume-and(Test-Path -LiteralPath $resultFile)){Get-Content -LiteralPath $resultFile -Raw|ConvertFrom-Json}else{$null}
   $reuse=$report-and$report.ChecksPassed-and$report.SHA256-eq$binaryHash-and$report.Dpi-eq($scale*96/100)-and$report.Dark-eq$dark
   if(!$reuse){
    & (Join-Path $PSScriptRoot 'Test-GallerySwipe.ps1') -Label $label -Dark:$dark -Checks -ExpectedDpi ($scale*96/100) -Record:($Record-and$scale-eq150)
    if(!$?){throw "Gallery failed at $label"}
    $report=Get-Content -LiteralPath $resultFile -Raw|ConvertFrom-Json
   }
   if(!$report.ChecksPassed){throw "Gallery assertions failed at $label"}
   if($report.Dpi-ne($scale*96/100)){throw "Gallery window did not use the expected DPI at $label"}
   $summary.Add([pscustomobject]@{Scale=$scale;Dpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());Theme=$(if($dark){'dark'}else{'light'});Passed=$report.ChecksPassed;Directory=$label})
  }
 }
}finally{
 try{if([DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray())-ne($initial*96/100)){Set-DisplayScale $initial}}finally{
  if($wasMinimized){[void][DpiDesktop]::ShowWindow($settingsHandle,6)}
  [pscustomobject]@{InitialScale=$initial;FinalDpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());Cases=$summary.ToArray()}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'dpi-results.json')
 }
}
