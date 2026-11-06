[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$feed=Join-Path $root 'artifacts\winui3-feed'
$packages=Join-Path $root 'artifacts\winui3-packages'
[IO.Directory]::CreateDirectory($feed)|Out-Null
[IO.Directory]::CreateDirectory($packages)|Out-Null
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$resolved=[Collections.Generic.List[object]]::new()
function Restore-Package([string]$Id,[string]$Version){
 if($Id-notmatch'^[A-Za-z0-9_.-]+$'-or$Version-notmatch'^[A-Za-z0-9_.-]+$'){throw 'Invalid package identity'}
 $key="$Id.$Version"
 if(!$seen.Add($key)){return}
 $destination=Join-Path $packages $key
 $archive=Join-Path $feed ($key.ToLowerInvariant()+'.nupkg')
 $url='https://api.nuget.org/v3-flatcontainer/'+$Id.ToLowerInvariant()+'/'+$Version.ToLowerInvariant()+'/'+$key.ToLowerInvariant()+'.nupkg'
 if(!(Test-Path -LiteralPath $archive)){Write-Host "Download $key";Invoke-WebRequest $url -OutFile $archive}
 $registration=Invoke-RestMethod ('https://api.nuget.org/v3/registration5-gz-semver2/'+$Id.ToLowerInvariant()+'/'+$Version.ToLowerInvariant()+'.json')
 $catalog=Invoke-RestMethod $registration.catalogEntry
 if($catalog.packageHashAlgorithm-ne'SHA512'){throw "Unsupported package hash: $key"}
 $expected=$catalog.packageHash
 $actual=[Convert]::ToBase64String([Convert]::FromHexString((Get-FileHash -LiteralPath $archive -Algorithm SHA512).Hash))
 if($actual-ne$expected){throw "Package integrity check failed: $key"}
 if(!(Test-Path -LiteralPath $destination)){[IO.Compression.ZipFile]::ExtractToDirectory($archive,$destination)}
 $nuspec=Get-ChildItem -LiteralPath $destination -Filter '*.nuspec'|Select-Object -First 1
 if(!$nuspec){throw "Missing package metadata: $key"}
 [xml]$metadata=[IO.File]::ReadAllText($nuspec.FullName)
 $resolved.Add([pscustomobject]@{Id=$Id;Version=$Version;SHA512=$actual})
 foreach($dependency in $metadata.SelectNodes('//*[local-name()="dependencies"]//*[local-name()="dependency"]')){
  # Use the declared minimum; exact resolved versions are recorded in the build manifest.
  $minimum=([string]$dependency.version).TrimStart('[','(').Split(',')[0].TrimEnd(']',')')
  Restore-Package ([string]$dependency.id) $minimum
 }
}
Restore-Package 'Microsoft.WindowsAppSDK.WinUI' '2.6.5'
Restore-Package 'Microsoft.WindowsAppSDK.Runtime' '2.6.5'
Restore-Package 'Microsoft.Windows.CppWinRT' '3.0.260818.1'
$resolved|ConvertTo-Json|Set-Content (Join-Path $packages 'resolved.json')
Write-Output $packages
