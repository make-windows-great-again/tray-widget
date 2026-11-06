$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$sdk=Join-Path $root 'sdk\Release'
foreach($dir in @($sdk,(Join-Path $sdk 'include'),(Join-Path $sdk 'lib'))){[IO.Directory]::CreateDirectory($dir)|Out-Null}
Copy-Item -LiteralPath (Join-Path $root 'source\ShellPanels\ShellPanels.h') -Destination (Join-Path $sdk 'include') -Force
Copy-Item -LiteralPath (Join-Path $root 'artifacts\obj\Release\ShellPanels.lib') -Destination (Join-Path $sdk 'lib') -Force
Copy-Item -LiteralPath (Join-Path $root 'source\ShellPanels\README.txt'),(Join-Path $root 'source\ShellPanels\Noise-LICENSE.txt') -Destination $sdk -Force
$guide="SDK PACKAGE: open an x64 Visual Studio Developer Command Prompt, then run Build-Example.cmd.`r`nThe example consumes include/ShellPanels.h and lib/ShellPanels.lib only. Its controls use local fixtures.`r`nMSVC 14.51, Windows SDK 26100, C++20 and /MT were used for this binary library.`r`nSource-repository paths mentioned below apply to the full project archive.`r`n`r`n"
[IO.File]::WriteAllText((Join-Path $sdk 'README.txt'),$guide+[IO.File]::ReadAllText((Join-Path $sdk 'README.txt')),[Text.UTF8Encoding]::new($true))
Copy-Item -LiteralPath (Join-Path $root 'source\NativeTray\SelfTest.manifest') -Destination (Join-Path $sdk 'app.manifest') -Force
$sample=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ShellBench.cpp')).Replace('../../source/ShellPanels/ShellPanels.h','ShellPanels.h')
[IO.File]::WriteAllText((Join-Path $sdk 'ShellBench.cpp'),$sample,[Text.UTF8Encoding]::new($true))
$build=@'
@echo off
rem Run from an x64 Visual Studio Developer Command Prompt (MSVC 14.51 / SDK 26100).
cd /d "%~dp0"
cl /nologo /std:c++20 /EHsc /utf-8 /MT /DNOMINMAX /DUNICODE /D_UNICODE /I include ShellBench.cpp lib\ShellPanels.lib WindowsApp.lib shcore.lib user32.lib shell32.lib ole32.lib oleaut32.lib dwmapi.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest /OUT:ShellBench.exe
'@
[IO.File]::WriteAllText((Join-Path $sdk 'Build-Example.cmd'),$build,[Text.ASCIIEncoding]::new())
$archive=Join-Path $root 'ShellPanels-v16-sdk-x64-Release.zip'
Compress-Archive -LiteralPath (@('include','lib','README.txt','Noise-LICENSE.txt','app.manifest','ShellBench.cpp','Build-Example.cmd')|ForEach-Object {Join-Path $sdk $_}) -DestinationPath $archive -Force
$archive
