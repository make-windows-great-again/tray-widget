$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$active=Join-Path $env:TEMP 'YataNativeFramework\bin\Release'
$app=Join-Path $active 'YataNativeFramework.exe';$patch=Join-Path $active 'YataNativeFramework.Patch.dll'
$settings=Join-Path $env:TEMP 'YataNativeFramework.layout.ini'
$settingsHash=(Get-FileHash -LiteralPath $settings).Hash;$patchHash=(Get-FileHash -LiteralPath $patch).Hash
if($patchHash-ne'E79814D9C387E31EC5335990BF2380651B1251A0E7E23E0AEF054F5A6D766961'){throw 'The active patch differs from the audited pinned module'}
$running=@(Get-Process YataNativeFramework -ErrorAction Stop)
if($running.Count-ne1-or$running[0].Path-ne$app){throw 'Unexpected application instance'}
$backup=Join-Path $root 'artifacts\deployment-backup';[IO.Directory]::CreateDirectory($backup)|Out-Null
Copy-Item -LiteralPath $app -Destination (Join-Path $backup ('YataNativeFramework-before-v16-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'.exe')) -Force
& $app --detach
Start-Sleep -Milliseconds 600
& $app --exit
if(!$running[0].WaitForExit(8000)){throw 'Application did not exit cleanly'}
& (Join-Path $root 'tools\Test-Client.ps1') -BinaryDirectory (Join-Path $root 'artifacts\candidate')
if(!$?){throw 'Candidate client test failed'}
if((Get-FileHash -LiteralPath $settings).Hash-ne$settingsHash){throw 'Client test did not restore layout.ini'}
$runtime=Join-Path $root 'bin\Release';[IO.Directory]::CreateDirectory($runtime)|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'artifacts\candidate\YataNativeFramework.exe') -Destination $app -Force
Copy-Item -LiteralPath $app,$patch -Destination $runtime -Force
$process=Start-Process -FilePath $app -ArgumentList '--mode lines' -WorkingDirectory $active -WindowStyle Hidden -PassThru
Start-Sleep -Milliseconds 4500
$status=(& $app --status|Out-String).Trim();$profile=(& $app --profile|Out-String).Trim()
[pscustomobject]@{AppPid=$process.Id;Status=$status;Profile=$profile;ExeSHA256=(Get-FileHash -LiteralPath $app).Hash;PatchSHA256=(Get-FileHash -LiteralPath $patch).Hash;SettingsSHA256=(Get-FileHash -LiteralPath $settings).Hash;SettingsUnchanged=((Get-FileHash -LiteralPath $settings).Hash-eq$settingsHash)}|ConvertTo-Json|Set-Content (Join-Path $root 'evidence\shell-parity-26100\deployment.json')
$status
if((Get-FileHash -LiteralPath $patch).Hash-ne$patchHash){throw 'Pinned patch changed'}
if((Get-FileHash -LiteralPath $settings).Hash-ne$settingsHash){throw 'Layout changed'}
if($status-notmatch'applied=2'-or$status-notmatch'error=0x00000000'-or$status-notmatch'attach=0x00000000'){throw 'Reconnect requires diagnosis; no automatic retry or Explorer restart was performed'}
