param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration $Configuration -OutputDirectory (Join-Path $projectRoot 'artifacts\candidate')
Push-Location $objects
try {
 Compile "$PSScriptRoot\HostChecks.cpp" "$tests\ShellPanelsChecks.exe" ($xamlLibs+@("$objects\ShellPanels.lib",'psapi.lib','/MANIFEST:EMBED',"/MANIFESTINPUT:$source\SelfTest.manifest"))
 Run-Test 'ShellPanelsChecks.exe'
}finally{Pop-Location}
