[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',[string]$OutputDirectory,[switch]$SelfTest)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$source=Join-Path $PSScriptRoot 'source\NativeTray'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installations=@()
if(Test-Path -LiteralPath $vswhere){$installations+=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath}
# Also support installations without the optional vswhere utility.
foreach($root in @("$env:ProgramFiles\Microsoft Visual Studio","${env:ProgramFiles(x86)}\Microsoft Visual Studio")){
 if(Test-Path -LiteralPath $root){foreach($version in (Get-ChildItem -LiteralPath $root -Directory)){$installations+=Get-ChildItem -LiteralPath $version.FullName -Directory|Select-Object -ExpandProperty FullName}}
}
$toolchains=foreach($installation in $installations){
 $tools=Join-Path $installation 'VC\Tools\MSVC'
 if(Test-Path -LiteralPath $tools){Get-ChildItem -LiteralPath $tools -Directory|Where-Object{Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Hostx64\x64\cl.exe')}}
}
$toolchain=$toolchains|Sort-Object{[version]$_.Name} -Descending|Select-Object -First 1
if(!$toolchain){throw 'Install Visual Studio Desktop development with C++ (x64).'}
$sdkRoot=(Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
$sdk=Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory|Where-Object{
 (Test-Path -LiteralPath (Join-Path $_.FullName 'cppwinrt\winrt\Windows.UI.Xaml.h')) -and
 (Test-Path -LiteralPath (Join-Path $sdkRoot "Lib\$($_.Name)\um\x64\WindowsApp.lib"))
}|Sort-Object{[version]$_.Name} -Descending|Select-Object -First 1
if(!$sdk){throw 'Install a Windows SDK with C++/WinRT and x64 libraries.'}
$sdkVersion=$sdk.Name
$env:PATH="$($toolchain.FullName)\bin\Hostx64\x64;$sdkRoot\bin\$sdkVersion\x64;$env:PATH"
$env:INCLUDE="$($toolchain.FullName)\include;$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\shared;$sdkRoot\Include\$sdkVersion\um;$sdkRoot\Include\$sdkVersion\winrt;$sdkRoot\Include\$sdkVersion\cppwinrt"
$env:LIB="$($toolchain.FullName)\lib\x64;$sdkRoot\Lib\$sdkVersion\um\x64;$sdkRoot\Lib\$sdkVersion\ucrt\x64"
$output=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $PSScriptRoot "bin\$Configuration"}
$objects=Join-Path $PSScriptRoot "artifacts\obj\$Configuration"
$tests=Join-Path $PSScriptRoot "artifacts\tests\$Configuration"
foreach($path in @($output,$objects,$tests)){[IO.Directory]::CreateDirectory($path)|Out-Null}
$compiler=Join-Path $toolchain.FullName 'bin\Hostx64\x64\cl.exe'
$flags=@('/nologo','/std:c++20','/EHsc','/permissive-','/W4','/WX','/utf-8','/bigobj','/DUNICODE','/D_UNICODE','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX')
if($Configuration -eq 'Debug'){$flags+=@('/Od','/MTd','/Zi','/RTC1','/D_DEBUG')}else{$flags+=@('/O2','/MT','/DNDEBUG')}
$xamlLibs=@('WindowsApp.lib','shcore.lib','user32.lib','shell32.lib','ole32.lib','oleaut32.lib')
function Compile([string]$inputFile,[string]$destination,[string[]]$libraries,[switch]$Dll){
 $arguments=$flags+@($inputFile,"/Fe:$destination");if($Dll){$arguments+='/LD'}
 $stem=[IO.Path]::GetFileNameWithoutExtension($destination)
 & $compiler @arguments /link @libraries "/PDB:$objects\$stem.pdb" "/ILK:$objects\$stem.ilk"
 if($LASTEXITCODE -ne 0){throw "Build failed: $destination"}
}
function Run-Test([string]$name){& (Join-Path $tests $name);if($LASTEXITCODE -ne 0){throw "Test failed: $name (exit $LASTEXITCODE)"}}
Push-Location $objects
try{
 & $compiler @flags /c "$source\..\ShellPanels\ShellPanels.cpp" "/Fo:$objects\ShellPanels.obj"
 if($LASTEXITCODE -ne 0){throw 'ShellPanels compilation failed'}
 & lib.exe /nologo "/OUT:$objects\ShellPanels.lib" "$objects\ShellPanels.obj"
 if($LASTEXITCODE -ne 0){throw 'ShellPanels library failed'}
 Compile "$source\NativeTrayTap.cpp" "$output\YataNativeFramework.Patch.dll" ($xamlLibs+@("/DEF:$source\NativeTrayTap.def","/IMPLIB:$objects\YataNativeFramework.Patch.lib")) -Dll
 Compile "$source\..\TrayStub.cpp" "$output\YataNativeFramework.exe" ($xamlLibs+@("$objects\ShellPanels.lib",'/SUBSYSTEM:WINDOWS','/MANIFEST:EMBED',"/MANIFESTINPUT:$source\..\app.manifest",'d2d1.lib','dwrite.lib','dwmapi.lib','gdi32.lib','advapi32.lib'))
 if($SelfTest){
  Compile "$source\NativeTraySelfTest.cpp" "$tests\NativeTraySelfTest.exe" ($xamlLibs+@('/MANIFEST:EMBED',"/MANIFESTINPUT:$source\SelfTest.manifest"));Run-Test 'NativeTraySelfTest.exe'
  Compile "$source\FlyoutPlacementSelfTest.cpp" "$tests\PlacementSelfTest.exe" @();Run-Test 'PlacementSelfTest.exe'
  Compile "$source\TrayCommandsSelfTest.cpp" "$tests\CommandsSelfTest.exe" @();Run-Test 'CommandsSelfTest.exe'
  Compile "$source\FrameworkSelfTest.cpp" "$tests\FrameworkSelfTest.exe" ($xamlLibs+@('/MANIFEST:EMBED',"/MANIFESTINPUT:$source\SelfTest.manifest",'dwmapi.lib'));Run-Test 'FrameworkSelfTest.exe'
  Compile "$PSScriptRoot\tools\shell-parity\HostChecks.cpp" "$tests\ShellPanelsChecks.exe" ($xamlLibs+@("$objects\ShellPanels.lib",'psapi.lib','/MANIFEST:EMBED',"/MANIFESTINPUT:$source\SelfTest.manifest"));Run-Test 'ShellPanelsChecks.exe'
 }
}finally{Pop-Location}
"Built ${Configuration}: MSVC $($toolchain.Name), Windows SDK $sdkVersion -> $output"
