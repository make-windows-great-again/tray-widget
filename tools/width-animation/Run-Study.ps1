[CmdletBinding()]
param([ValidateRange(1,5)][int]$Runs=3,[switch]$NoBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $projectRoot 'evidence\navigation-width-20261105\width'
if(!$NoBuild){& (Join-Path $PSScriptRoot 'Build-Probe.ps1') -OutputDirectory $output}
for($run=1;$run-le$Runs;$run++){
 $directory=Join-Path $output "study-$run"
 & (Join-Path $output 'bin\WidthAnimationProbe.exe') $directory
 if($LASTEXITCODE-ne0){throw "Width animation study failed: $run"}
 $data=Get-Content -LiteralPath (Join-Path $directory 'summary.json') -Raw -Encoding UTF8|ConvertFrom-Json
 $data.runs|ForEach-Object{[pscustomobject]@{Run=$run;MonitorHz=$data.monitorHz;Phase=$_.phase;UiCallbackHz=$_.uiCallbackHz;LayoutWidthDip=$_.finalWidthDip;NeighborXDip=$_.neighborXDip}}
}
