[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$auditOutput=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $projectRoot 'evidence\visual-audit-20261105'}
[IO.Directory]::CreateDirectory($auditOutput)|Out-Null
# Reuse only the reviewed toolchain setup and compiler functions; do not build
# or modify either production binary and do not execute production test suites.
$buildText=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$buildText.Substring(0,$buildText.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration Release -OutputDirectory (Join-Path $auditOutput 'bin')
Push-Location $objects
try{
 Compile (Join-Path $PSScriptRoot 'VisualAuditProbe.cpp') (Join-Path $auditOutput 'bin\VisualAuditProbe.exe') ($xamlLibs+@('/MANIFEST:EMBED',"/MANIFESTINPUT:$projectRoot\source\NativeTray\SelfTest.manifest",'gdi32.lib','gdiplus.lib','dwmapi.lib'))
}finally{Pop-Location}
Write-Output (Join-Path $auditOutput 'bin\VisualAuditProbe.exe')
