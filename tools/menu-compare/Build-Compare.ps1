[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)) 'Download\YataMenuCompare'),[switch]$ReuseReference)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$comparisonOutput=[IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($comparisonOutput)|Out-Null
$reference=Join-Path $comparisonOutput 'reference'
if(!$ReuseReference){& (Join-Path $projectRoot 'tools\visual-audit\Native-Reference.ps1') -OutputDirectory $reference -Target network,volume,battery,clock -MenuOnly}
$menus=Get-Content -LiteralPath (Join-Path $reference 'native-reference.json') -Raw -Encoding UTF8|ConvertFrom-Json
$generated=Join-Path $projectRoot 'artifacts\menu-compare\include';[IO.Directory]::CreateDirectory($generated)|Out-Null
$header=[Collections.Generic.List[string]]::new()
$header.Add('#pragma once')
$header.Add('struct Row { std::wstring text; wchar_t glyph; bool separatorBefore; };')
$header.Add('struct Reference { std::wstring key,title,prefix,screenshot; double width,height; unsigned dpi; double imageWidth; std::vector<Row> rows; };')
$header.Add('inline const std::array<Reference,4> References{{')
function Wide([string]$value){return 'L"'+$value.Replace('\','\\').Replace('"','\"').Replace("`r",'\r').Replace("`n",'\n')+'"'}
$names=@{network=@('Wi-Fi','Network');volume=@('Звук','Volume');battery=@('Батарея','Power');clock=@('Дата и время','Clock')}
foreach($key in @('network','volume','battery','clock')){
 $menu=@($menus|Where-Object{$_.Target-eq$key})[0];if(!$menu){throw "Missing original menu: $key"}
 $body=@($menu.Elements|Where-Object{$_.Type-eq'ControlType.Menu'})[0]
 $items=@($menu.Elements|Where-Object{$_.Type-eq'ControlType.MenuItem'});if(!$items.Count){throw "Missing original rows: $key"}
 $scale=$menu.Window.Dpi/96.0
 $header.Add(('{'+(Wide $key)+','+(Wide $names[$key][0])+','+(Wide $names[$key][1])+','+(Wide $menu.Screenshot)+','+($body.W/$scale).ToString('R',[Globalization.CultureInfo]::InvariantCulture)+','+($body.H/$scale).ToString('R',[Globalization.CultureInfo]::InvariantCulture)+','+$menu.Window.Dpi+','+$body.W+',{'))
 for($i=0;$i-lt$items.Count;$i++){
  $name=$items[$i].Name;$glyph=if($key-eq'volume'-and$i-eq0){$null}elseif($name-like'*Diagnose*'){'e90f'}elseif($name-like'*speed test*'){'f42f'}else{'e713'}
  $glyphLiteral=if($glyph){"L'\u"+$glyph+"'"}else{"L'\0'"}
  $separator=if($i-gt0 -and ($items[$i].Y-$items[$i-1].Y-$items[$i-1].H)-gt(5*$scale)){'true'}else{'false'}
  $header.Add('{'+(Wide $name)+','+$glyphLiteral+','+$separator+'},')
 }
 $header.Add('}},')
}
$header.Add('}};')
[IO.File]::WriteAllLines((Join-Path $generated 'ReferenceMenuData.h'),$header,[Text.UTF8Encoding]::new($false))
$buildText=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$buildText.Substring(0,$buildText.IndexOf('Push-Location $objects')).Replace('$PSScriptRoot','$projectRoot')
. ([scriptblock]::Create($setup)) -Configuration Release -OutputDirectory $comparisonOutput
$flags+="/I$generated"
Push-Location $objects
try{Compile (Join-Path $PSScriptRoot 'MenuCompare.cpp') (Join-Path $comparisonOutput 'YataMenuCompare.exe') ($xamlLibs+@('/SUBSYSTEM:WINDOWS','/MANIFEST:EMBED',"/MANIFESTINPUT:$PSScriptRoot\MenuCompare.manifest",'dwmapi.lib','gdiplus.lib','gdi32.lib','UIAutomationCore.lib','uuid.lib'))}finally{Pop-Location}
Write-Output (Join-Path $comparisonOutput 'YataMenuCompare.exe')
