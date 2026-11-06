[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$Baseline)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
$destination=Join-Path $projectRoot ('artifacts\gallery-swipe\'+$(if($Baseline){'baseline'}else{$Configuration}))
. ([scriptblock]::Create($setup)) -Configuration $Configuration -OutputDirectory $destination
$include=if($Baseline){Join-Path $projectRoot 'artifacts\gallery-baseline'}else{Join-Path $projectRoot 'source\NativeTray'}
if(!(Test-Path -LiteralPath (Join-Path $include 'NativeControls.h'))){throw 'Baseline header is missing; copy NativeControls.h from the baseline commit to artifacts/gallery-baseline first.'}
$flags+=('/I'+$include)
if($Baseline){$flags+='/DBASELINE_GALLERY'}
Push-Location $objects
try{
 Compile "$PSScriptRoot\GallerySwipeBench.cpp" "$output\GallerySwipeBench.exe" ($xamlLibs+@('/SUBSYSTEM:WINDOWS','/MANIFEST:EMBED',"/MANIFESTINPUT:$projectRoot\source\NativeTray\SelfTest.manifest"))
}finally{Pop-Location}
Write-Output "$output\GallerySwipeBench.exe"
