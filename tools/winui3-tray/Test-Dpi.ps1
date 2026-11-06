[CmdletBinding()]
param([ValidateSet(100,150,200,250)][int[]]$Scales=@(100,150,200,250),[switch]$Record)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\shell-parity\DpiHarness.ps1')
$initial=Get-DisplayScale
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\winui3-tray-20261106'
$cases=[Collections.Generic.List[object]]::new()
try {
    foreach($scale in $Scales) {
        Set-DisplayScale $scale
        $label="accepted-$scale"
        & (Join-Path $PSScriptRoot 'Test.ps1') -Live -Label $label -Record:($Record-and$scale-eq150)
        if(!$?){throw "WinUI 3 checks failed at $scale%"}
        $report=Get-Content (Join-Path $out "$label\results.json") -Raw|ConvertFrom-Json
        if(!$report.Passed-or$report.Dpi-ne($scale*96/100)){throw "WinUI 3 DPI verification failed at $scale%"}
        $cases.Add([pscustomobject]@{Scale=$scale;Dpi=$report.Dpi;Passed=$report.Passed;Checks=$report.Checks;Directory=$label})
    }
} finally {
    try {Set-DisplayScale $initial} finally {
        [pscustomobject]@{InitialScale=$initial;FinalDpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());Cases=$cases.ToArray()}|
            ConvertTo-Json -Depth 4|Set-Content (Join-Path $out 'dpi-results.json')
    }
}
