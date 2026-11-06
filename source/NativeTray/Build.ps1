param([ValidateSet('Debug','Release')][string]$Configuration="Release",[string]$OutputDirectory,[switch]$SelfTest)
& (Join-Path $PSScriptRoot "..\..\Build.ps1") @PSBoundParameters
