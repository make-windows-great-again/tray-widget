[CmdletBinding()]
param([ValidateSet('Release','Debug')][string]$Configuration='Release',[switch]$Restore,[switch]$SelfTest,[switch]$ClientOnly)
$ErrorActionPreference='Stop'
$runPatchTests=$SelfTest.IsPresent
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$packages=Join-Path $projectRoot 'artifacts\winui3-packages'
if($Restore-or!(Test-Path (Join-Path $packages 'Microsoft.WindowsAppSDK.WinUI.2.6.5\build\native\Microsoft.WindowsAppSDK.WinUI.props'))){& (Join-Path $PSScriptRoot 'Restore-Packages.ps1')|Out-Host}
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration $Configuration -OutputDirectory (Join-Path $projectRoot "bin\WinUI3Tray\$Configuration")
$msbuild=Join-Path $toolchain.Parent.Parent.Parent.FullName 'MSBuild\Current\Bin\MSBuild.exe'
if(!(Test-Path $msbuild)){$msbuild=Join-Path ($installations|Select-Object -First 1) 'MSBuild\Current\Bin\MSBuild.exe'}
& $msbuild (Join-Path $projectRoot 'source\WinUI3Tray\WinUI3Tray.vcxproj') "/p:Configuration=$Configuration" /p:Platform=x64 /m:2 /v:minimal /nologo
if($LASTEXITCODE){throw 'WinUI 3 client build failed'}
if(!$ClientOnly){
 $patchSource=Join-Path $projectRoot 'source\WinUI3Tray'
 $objects=Join-Path $projectRoot "artifacts\winui3-patch-obj\$Configuration"
 [IO.Directory]::CreateDirectory($objects)|Out-Null
 Push-Location $objects
 try{
  Compile "$patchSource\TrayPatch.cpp" "$output\YataWinUI3Tray.Patch.dll" ($xamlLibs+@("/DEF:$patchSource\TrayPatch.def","/IMPLIB:$objects\YataWinUI3Tray.Patch.lib")) -Dll
  if($runPatchTests){
   Compile "$patchSource\PatchSelfTest.cpp" "$objects\PatchSelfTest.exe" ($xamlLibs+@('/MANIFEST:EMBED',"/MANIFESTINPUT:$projectRoot\source\NativeTray\SelfTest.manifest"))
   & "$objects\PatchSelfTest.exe"
   if($LASTEXITCODE){throw 'WinUI 3 tray patch checks failed'}
  }
 }finally{Pop-Location}
}
Write-Output "WinUI 3 client and separate native patch: $output"
