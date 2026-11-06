[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$auditRoot=Join-Path $projectRoot 'evidence\visual-audit-20261105'
$manifest=Get-Content (Join-Path $projectRoot 'evidence\SHA256-Release.json') -Raw|ConvertFrom-Json
foreach($record in $manifest){if((Get-FileHash -LiteralPath (Join-Path $projectRoot "bin\Release\$($record.File)")).Hash-ne $record.SHA256){throw "Production binary changed: $($record.File)"}}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceZip=[IO.Compression.ZipFile]::OpenRead((Join-Path $projectRoot 'YataNativeFramework-v13-source.zip'))
$sourceCount=0
try{
 foreach($entry in $sourceZip.Entries){
  $relative=$entry.FullName.Replace('\','/')
  if(!$relative.StartsWith('source/')-or $relative.EndsWith('/')){continue}
  $path=[IO.Path]::GetFullPath((Join-Path $projectRoot $relative))
  if(!$path.StartsWith((Join-Path $projectRoot 'source')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected source path'}
  $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
  try{$expected=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}
  if((Get-FileHash -LiteralPath $path).Hash-ne $expected){throw "Production source changed: $relative"};$sourceCount++
 }
}finally{$sourceZip.Dispose()}
$app=Join-Path $projectRoot 'bin\Release\YataNativeFramework.exe'
$info=[Diagnostics.ProcessStartInfo]::new($app,'--status');$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true
$process=[Diagnostics.Process]::Start($info)
try{if(!$process.WaitForExit(3000)){throw 'Status timed out'};$status=$process.StandardOutput.ReadToEnd()}finally{$process.Dispose()}
$os=Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$theme=Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
[ordered]@{CapturedUtc=[DateTime]::UtcNow.ToString('o');WindowsBuild="$($os.CurrentBuild).$($os.UBR)";DisplayVersion=$os.DisplayVersion;Status=$status;Theme=@{AppsUseLightTheme=$theme.AppsUseLightTheme;SystemUsesLightTheme=$theme.SystemUsesLightTheme;EnableTransparency=$theme.EnableTransparency};ProductionSourceFilesVerified=$sourceCount;ProductionBinaries=$manifest;ProductionUnchanged=$true;Monitors=(Get-Content (Join-Path $auditRoot 'monitors.json') -Raw|ConvertFrom-Json)}|ConvertTo-Json -Depth 7|Set-Content (Join-Path $auditRoot 'environment.json') -Encoding UTF8
$rows=Get-Content (Join-Path $auditRoot 'probe\probe.jsonl') -Encoding UTF8|ForEach-Object{$_|ConvertFrom-Json}
$pictures=@($rows|Where-Object{$_.window}|Select-Object -ExpandProperty file)
foreach($name in $pictures){if(!(Test-Path -LiteralPath (Join-Path $auditRoot "probe\$name"))){throw "Missing image: $name"}}
if(@($rows|Where-Object{$_.environment}).Count-ne 46-or $pictures.Count-ne 64){throw 'Unexpected scenario/frame count'}
$reference=Get-Content (Join-Path $auditRoot 'native-reference.json') -Raw|ConvertFrom-Json
$relativeFiles=@('VISUAL-AUDIT.txt','VISUAL-AUDIT.html','summary.json','environment.json','monitors.json','native-reference.json','user-reference.png','live-uia-Selection.json','live-uia-Menus.json','probe-build.log','probe-run.log','probe\probe.jsonl')
$relativeFiles+=@($pictures|ForEach-Object{'probe\'+$_})
$relativeFiles+=@($reference|Where-Object Target -eq network|Select-Object -ExpandProperty Screenshot)
$stage=Join-Path $projectRoot ('artifacts\visual-audit-package-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stage)|Out-Null
foreach($relative in $relativeFiles){
 $from=[IO.Path]::GetFullPath((Join-Path $auditRoot $relative));$to=[IO.Path]::GetFullPath((Join-Path $stage $relative))
 if(!$from.StartsWith($auditRoot+'\',[StringComparison]::OrdinalIgnoreCase)-or !$to.StartsWith($stage+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected evidence path'}
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to))|Out-Null;Copy-Item -LiteralPath $from -Destination $to
}
$tools=Join-Path $stage 'tools\visual-audit';[IO.Directory]::CreateDirectory($tools)|Out-Null
Get-ChildItem -LiteralPath $PSScriptRoot -File|Where-Object{$_.Extension-in '.ps1','.cpp'}|ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination $tools}
[IO.File]::WriteAllText((Join-Path $stage 'README.txt'),"Open VISUAL-AUDIT.html or VISUAL-AUDIT.txt.`r`nThis is an evidence archive, not a replacement runtime package.`r`nAudit tools require the existing YataNativeFramework project sources and Windows C++ toolchain.`r`n",[Text.UTF8Encoding]::new($false))
$archive=Join-Path $projectRoot 'YataNativeFramework-visual-audit-20261105.zip'
if(Test-Path -LiteralPath $archive){Remove-Item -LiteralPath $archive}
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$archive)
"PASS production binaries and $sourceCount source files unchanged"
"PASS 46 scenarios and 64 referenced frames present"
$status
Get-Item -LiteralPath $archive|Select-Object FullName,Length
