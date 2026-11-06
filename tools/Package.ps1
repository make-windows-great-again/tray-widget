[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$NoBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
[IO.Directory]::CreateDirectory((Join-Path $root 'evidence'))|Out-Null
if(!$NoBuild){& (Join-Path $root 'Build.ps1') -Configuration $Configuration -SelfTest}
$runtime=Join-Path $root "bin\$Configuration"
$expected=@('YataNativeFramework.exe','YataNativeFramework.Patch.dll')
$actual=@(Get-ChildItem -LiteralPath $runtime -File|Select-Object -ExpandProperty Name)
if(@(Compare-Object $actual $expected).Count){throw 'Runtime directory must contain exactly one EXE and one patch DLL.'}
$files=$expected|ForEach-Object{Join-Path $runtime $_}
$archive=Join-Path $root "YataNativeFramework-v16-win11-x64-$Configuration.zip"
Compress-Archive -LiteralPath $files -DestinationPath $archive -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($archive)
try{if(@(Compare-Object @($zip.Entries|Select-Object -ExpandProperty FullName) $expected).Count){throw 'Unexpected file in runtime archive'}}finally{$zip.Dispose()}
$records=foreach($file in $files){[pscustomobject]@{File=[IO.Path]::GetFileName($file);SHA256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash;Bytes=(Get-Item -LiteralPath $file).Length}}
$records|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root "evidence\SHA256-$Configuration.json") -Encoding UTF8
$staging=Join-Path $root ('artifacts\source-package-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($staging)|Out-Null
foreach($name in @('source','tools','Build.ps1','Start-Framework.cmd','README.txt','README.md','CHANGELOG.txt','.gitignore','.gitattributes')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $staging -Recurse}
$verification=Join-Path $staging 'evidence';[IO.Directory]::CreateDirectory($verification)|Out-Null
foreach($name in @('VERIFICATION.txt','EXIT-MATERIAL-FIX-20261105.txt','exit-material-Release.log','exit-material-Debug.log','LIST-FIX-20261105.txt','MENU-SPACING-20261105.txt','TRAY-MENU-INTEGRATION-20261105.txt','NAVIGATION-STARTUP-FIX-20261105.txt','WIDTH-ANIMATION-RESEARCH-20261105.txt','navigation-fix-Release.log','navigation-fix-Debug.log','tray-menu-integration-Release.log','tray-menu-integration-Debug.log','list-fix-framework-Release.log','list-fix-framework-Debug.log','menu-spacing-framework-Release.log','menu-spacing-framework-Debug.log','SHA256-Release.json','SHA256-Debug.json','build-release-current.log','build-debug-current.log','client-results-current.txt','live-results-current.txt','desktop-preflight-current.json','capture-limitation.txt','dependencies-current.txt','exports-current.txt')){
 $file=Join-Path $root "evidence\$name";if(Test-Path -LiteralPath $file){Copy-Item -LiteralPath $file -Destination $verification}
}
$parity=Join-Path $root 'evidence\shell-parity-26100'
$parityCopy=Join-Path $verification 'shell-parity-26100'
[IO.Directory]::CreateDirectory($parityCopy)|Out-Null
Get-ChildItem -LiteralPath $parity -File|Where-Object {$_.Extension-in'.txt','.csv','.json','.html','.log'}|Copy-Item -Destination $parityCopy
foreach($folder in @('frames','recordings')){
 $destination=Join-Path $parityCopy $folder;[IO.Directory]::CreateDirectory($destination)|Out-Null
 Get-ChildItem -LiteralPath (Join-Path $parity $folder) -File|Where-Object {if($folder-eq'frames'){$_.Extension-eq'.png'}else{$_.Extension-in'.mp4','.json','.csv','.log'-and$_.Name-ne'native.mp4'}}|Copy-Item -Destination $destination
}
$dpiCopy=Join-Path $parityCopy 'dpi-v16'
[IO.Directory]::CreateDirectory($dpiCopy)|Out-Null
foreach($name in 'baseline-matrix-core','native225-retry','accepted-light','accepted-dark','accepted-hot','accepted-context','acceptance.csv','comparison.html','REPORT.txt'){
 $path=Join-Path $parity ('dpi-v16\'+$name)
 if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path -Destination $dpiCopy -Recurse -Force}
}
$sourceArchive=Join-Path $root 'YataNativeFramework-v16-source.zip'
if(Test-Path -LiteralPath $sourceArchive){Remove-Item -LiteralPath $sourceArchive}
[IO.Compression.ZipFile]::CreateFromDirectory($staging,$sourceArchive)
"Runtime (exactly 2 files): $archive"
"Source: $sourceArchive"
