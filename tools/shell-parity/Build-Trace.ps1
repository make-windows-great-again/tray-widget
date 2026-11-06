param([string]$Directory='artifacts\dpi-trace-v16')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration Release -OutputDirectory (Join-Path $projectRoot $Directory)
Push-Location $output
try {
 $flags+='/DSHELLPANELS_LAYOUT_TRACE'
 & $compiler @flags /c "$projectRoot\source\ShellPanels\ShellPanels.cpp" /Fo:ShellPanelsTrace.obj
 if($LASTEXITCODE){throw 'Trace library compilation failed'}
 & lib.exe /nologo /OUT:ShellPanelsTrace.lib ShellPanelsTrace.obj
 if($LASTEXITCODE){throw 'Trace library archiving failed'}
 Compile "$PSScriptRoot\ShellBench.cpp" "$output\ShellBench.exe" ($xamlLibs+@("$output\ShellPanelsTrace.lib",'dwmapi.lib','gdi32.lib','advapi32.lib','/SUBSYSTEM:WINDOWS','/MANIFEST:EMBED',"/MANIFESTINPUT:$projectRoot\source\NativeTray\SelfTest.manifest"))
}finally{Pop-Location}
