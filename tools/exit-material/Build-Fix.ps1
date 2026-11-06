[CmdletBinding()]
param([ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
# Compatibility entry point; the canonical build also links ShellPanels.lib.
& (Join-Path $projectRoot 'Build.ps1') -Configuration $Configuration -OutputDirectory (Join-Path $projectRoot "artifacts\exit-material-fix\$Configuration") -SelfTest