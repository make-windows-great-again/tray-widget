[CmdletBinding()]
param([string]$Directory=(Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)) 'Download\YataMenuCompare'))
$ErrorActionPreference='Stop'
$Directory=[IO.Path]::GetFullPath($Directory)
$info=[Diagnostics.ProcessStartInfo]::new((Join-Path $Directory 'YataMenuCompare.exe'),'--dump-all')
$info.UseShellExecute=$false;$info.CreateNoWindow=$true
$process=[Diagnostics.Process]::Start($info)
try{
 if(!$process.WaitForExit(45000)){throw 'The comparison capture has not completed; inspect the test window.'}
 if($process.ExitCode-ne0){throw "The comparison executable failed: $($process.ExitCode)"}
}finally{$process.Dispose()}
$rows=@(Get-Content -LiteralPath (Join-Path $Directory 'measurements.jsonl') -Encoding UTF8|Where-Object{$_}|ForEach-Object{$_|ConvertFrom-Json})
if($rows.Count-ne16){throw "Expected 16 measured native menus, got $($rows.Count)"}
foreach($row in $rows){if($row.widthPx-le0-or$row.heightPx-le0-or!(Test-Path -LiteralPath (Join-Path $Directory ('captures\'+$row.file)))){throw 'A menu capture or measurement is missing'}}
& (Join-Path $PSScriptRoot 'Write-Comparison.ps1') -Directory $Directory
