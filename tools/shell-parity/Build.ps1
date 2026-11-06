param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$OnlyBench)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration $Configuration -OutputDirectory (Join-Path $projectRoot "artifacts\shell-panels\$Configuration")
Push-Location $objects
try {
 if(!$OnlyBench){
 & $compiler @flags /c "$projectRoot\source\ShellPanels\ShellPanels.cpp" "/Fo:$objects\ShellPanels.obj"
 if($LASTEXITCODE){throw 'ShellPanels compilation failed'}
 & lib.exe /nologo "/OUT:$output\ShellPanels.lib" "$objects\ShellPanels.obj"
 if($LASTEXITCODE){throw 'ShellPanels library failed'}
 }elseif(!(Test-Path -LiteralPath "$output\ShellPanels.lib")){throw 'Build the static library before using OnlyBench'}
 Compile "$PSScriptRoot\ShellBench.cpp" "$output\ShellBench.exe" ($xamlLibs+@("$output\ShellPanels.lib",'dwmapi.lib','gdi32.lib','advapi32.lib','/SUBSYSTEM:WINDOWS','/MANIFEST:EMBED',"/MANIFESTINPUT:$projectRoot\source\NativeTray\SelfTest.manifest"))
}finally{Pop-Location}
Write-Output "$output\ShellBench.exe"
