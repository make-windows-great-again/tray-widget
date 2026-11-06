[CmdletBinding()]
param([string]$BinaryDirectory = $PSScriptRoot, [switch]$SkipClicks)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$app = Join-Path $BinaryDirectory 'YataNativeFramework.exe'
$hostTool = Join-Path $BinaryDirectory 'YataVpnNativeTrayHost.exe'
$status = & $hostTool --status
if ($LASTEXITCODE -ne 0 -or $status -notmatch '^mode=([012]) ') { throw 'Start the app in the interactive Windows session first.' }
$originalMode = @('off','vpn','speed')[[int]$Matches[1]]
$explorerProcessIds = @(Get-Process explorer | Select-Object -ExpandProperty Id)
$evidence = Join-Path $env:TEMP 'YataVpnNativeTrayBuild\evidence'
$settings = Join-Path $env:TEMP 'YataVpnNativeTray.layout.ini'
$settingsExisted = Test-Path -LiteralPath $settings
$settingsBytes = if ($settingsExisted) { [IO.File]::ReadAllBytes($settings) } else { $null }
$results = @()
function Command([string]$Option,[string]$Value) {
    & $hostTool $Option $Value
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $Option $Value" }
}
function Snapshot([string]$Label, [switch]$Click) {
    & "$PSScriptRoot\Inspect.ps1" -Label $Label -Click:$Click | Out-Null
    $items = Get-Content -LiteralPath (Join-Path $evidence "$Label.json") -Encoding UTF8 -Raw | ConvertFrom-Json
    $target = @($items | Where-Object {$_.Id -eq 'NotifyItemIcon' -and $_.Name -like 'Yata Native Framework*'})
    if ($target.Count -ne 1 -or $target[0].Offscreen) { throw "One visible VPN item expected: $Label" }
    return [pscustomobject]@{Target=$target[0];Items=$items}
}
try {
    $baseline = $null
    $neighbor = $null
    foreach ($test in @(
        @{Mode='off'; Width=0; Dip=32; Label='verify-off-before'},
        @{Mode='vpn'; Width=0; Dip=44; Label='verify-vpn-auto'},
        @{Mode='vpn'; Width=88; Dip=88; Label='verify-vpn-88'},
        @{Mode='speed'; Width=0; Dip=104; Label='verify-speed-auto'},
        @{Mode='speed'; Width=160; Dip=160; Label='verify-speed-160'},
        @{Mode='speed'; Width=0; Dip=104; Label='verify-speed-restored'},
        @{Mode='off'; Width=0; Dip=32; Label='verify-off-restored'}
    )) {
        Command --mode $test.Mode
        if ($test.Mode -ne 'off') { Command --width $test.Width }
        Start-Sleep -Milliseconds 800
        $snapshot = Snapshot $test.Label
        $target = $snapshot.Target
        $dip = $target.Width * 96 / $target.Dpi
        if ([Math]::Abs($dip-$test.Dip) -gt 0.5) { throw "$($test.Label): expected $($test.Dip) DIP, got $dip" }
        if (!$baseline) {
            $baseline = $snapshot
            $neighbor = $snapshot.Items | Where-Object {$_.Id -eq 'NotifyItemIcon' -and $_.X -lt $target.X} | Sort-Object X -Descending | Select-Object -First 1
        } elseif ($neighbor) {
            $currentNeighbor = @($snapshot.Items | Where-Object {$_.Id -eq $neighbor.Id -and $_.Name -eq $neighbor.Name})
            $delta = $target.Width - $baseline.Target.Width
            if ($currentNeighbor.Count -ne 1 -or [Math]::Abs($currentNeighbor[0].Width-$neighbor.Width) -gt 0.5 -or
                [Math]::Abs($currentNeighbor[0].X-$neighbor.X+$delta) -gt 1) { throw 'Neighbor did not move with the native tray layout.' }
        }
        $results += [pscustomobject]@{Case=$test.Label;WidthDIP=$dip;WidthPixels=$target.Width;X=$target.X;Y=$target.Y;Passed=$true}
        Write-Output "PASS $($test.Label): $dip DIP / $($target.Width) px; native layout"
    }
    if (!$SkipClicks) {
        Command --mode speed
        Command --width 0
        Start-Sleep -Milliseconds 800
        foreach($label in @('verify-click-one','verify-click-two')) {
            $null = Snapshot $label -Click
            $state = Get-Content -LiteralPath (Join-Path $evidence "$label-state.json") -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($state.BeforeVisible -eq $state.PanelVisible) { throw 'Native click did not toggle the panel.' }
            Write-Output "PASS ${label}: $($state.BeforeVisible) -> $($state.PanelVisible)"
        }
    }
    if (Compare-Object $explorerProcessIds @(Get-Process explorer | Select-Object -ExpandProperty Id)) { throw 'Explorer restarted during verification.' }
    Write-Output "PASS Explorer unchanged. Evidence: $evidence"
} finally {
    if ($settingsExisted) { [IO.File]::WriteAllBytes($settings,$settingsBytes) }
    elseif (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings }
    & $hostTool --mode $originalMode
    $results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'live-layout-results.json') -Encoding UTF8
}
