[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$probeOutput=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $projectRoot 'evidence\exit-material-20261105'}
[IO.Directory]::CreateDirectory($probeOutput)|Out-Null
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration Release -OutputDirectory (Join-Path $probeOutput 'bin')
Push-Location $objects
try{Compile (Join-Path $PSScriptRoot 'ExitMaterialProbe.cpp') (Join-Path $probeOutput 'bin\ExitMaterialProbe.exe') ($xamlLibs+@('/MANIFEST:EMBED',"/MANIFESTINPUT:$projectRoot\source\NativeTray\SelfTest.manifest",'gdi32.lib','gdiplus.lib','dwmapi.lib'))}finally{Pop-Location}
Write-Output (Join-Path $probeOutput 'bin\ExitMaterialProbe.exe')
