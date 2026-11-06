[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$evidence=Join-Path $projectRoot 'evidence\exit-material-20261105'
$app=(Get-Process YataNativeFramework -ErrorAction Stop).Path
$shell=New-Object -ComObject Shell.Application
$folders=@($shell.Windows()|ForEach-Object {try{if($_.LocationURL-like'file:*'){[pscustomobject]@{Location=$_.LocationURL;Left=$_.Left;Top=$_.Top;Width=$_.Width;Height=$_.Height}}}catch{}})
$settings=Join-Path $env:TEMP 'YataNativeFramework.layout.ini'
$settingsHash=(Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash
$patch=Join-Path ([IO.Path]::GetDirectoryName($app)) 'YataNativeFramework.Patch.dll'
$patchHash=(Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash
$explorers=@(Get-Process explorer -ErrorAction Stop)
if($explorers.Count-ne1){throw 'Expected exactly one Explorer process'}
$explorer=$explorers[0]
if($explorer.Path-ne(Join-Path $env:WINDIR 'Explorer.EXE')){throw 'Unexpected shell process path'}
$oldId=$explorer.Id
Stop-Process -Id $oldId -Force
$null=$explorer.WaitForExit(5000)
Start-Sleep -Milliseconds 1200
if(!(Get-Process explorer -ErrorAction SilentlyContinue)){Start-Process -FilePath (Join-Path $env:WINDIR 'explorer.exe') -WindowStyle Hidden}
$watch=[Diagnostics.Stopwatch]::StartNew()
do{
 Start-Sleep -Milliseconds 500
 $status=(& $app --status|Out-String).Trim()
 if($status-match'applied=2'-and$status-match'error=0x00000000'){break}
}while($watch.ElapsedMilliseconds-lt18000)
$recovered=$status-match'applied=2'-and$status-match'error=0x00000000'
$shell=New-Object -ComObject Shell.Application
foreach($folder in $folders){
 $existing=@($shell.Windows()|Where-Object {$_.LocationURL-eq$folder.Location})
 if(!$existing.Count){$shell.Open($folder.Location)}
}
Start-Sleep -Milliseconds 600
foreach($folder in $folders){foreach($entry in @($shell.Windows()|Where-Object {$_.LocationURL-eq$folder.Location})){
 try{$entry.Left=$folder.Left;$entry.Top=$folder.Top;$entry.Width=$folder.Width;$entry.Height=$folder.Height}catch{}
}}
if((Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash-ne$settingsHash){throw 'Layout changed'}
if((Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash-ne$patchHash){throw 'Patch changed'}
if(!$recovered){throw "Shell recovery failed after restoring folder windows: $status"}
$record=Join-Path $evidence 'shell-recovery.json'
if(Test-Path -LiteralPath $record){Copy-Item -LiteralPath $record -Destination (Join-Path $evidence ('shell-recovery-'+[DateTime]::UtcNow.ToString('HHmmss')+'.json'))}
[pscustomobject]@{OldExplorerId=$oldId;NewExplorerIds=@(Get-Process explorer|Select-Object -ExpandProperty Id);Status=$status;RestoredFolders=$folders;SettingsUnchanged=$true;PatchUnchanged=$true}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $record -Encoding UTF8
$status
