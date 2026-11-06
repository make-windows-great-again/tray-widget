[CmdletBinding()]
param([string]$Directory=(Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)) 'Download\YataMenuCompare'))
$ErrorActionPreference='Stop'
$Directory=[IO.Path]::GetFullPath($Directory)
$references=Get-Content -LiteralPath (Join-Path $Directory 'reference\native-reference.json') -Raw -Encoding UTF8|ConvertFrom-Json
$measurements=Get-Content -LiteralPath (Join-Path $Directory 'measurements.jsonl') -Encoding UTF8|Where-Object{$_}|ForEach-Object{$_|ConvertFrom-Json}
function Html([string]$value){[Net.WebUtility]::HtmlEncode($value)}
function Picture([string]$path,[double]$width){
 $base64=[Convert]::ToBase64String([IO.File]::ReadAllBytes($path))
 return '<img alt="Снимок настоящего меню" data-pixel-width="'+$width.ToString([Globalization.CultureInfo]::InvariantCulture)+'" src="data:image/png;base64,'+$base64+'">'
}
$sections=[Collections.Generic.List[string]]::new();$summary=[Collections.Generic.List[object]]::new()
$names=@{network='Wi-Fi';volume='Звук';battery='Батарея';clock='Дата и время'}
foreach($key in @('network','volume','battery','clock')){
 $reference=@($references|Where-Object{$_.Target-eq$key})[0]
 $body=@($reference.Elements|Where-Object{$_.Type-eq'ControlType.Menu'})[0]
 foreach($theme in @('light','dark')){
  $current=@($measurements|Where-Object{$_.category-eq$key-and$_.variant-eq'current'-and$_.theme-eq$theme-and$_.fontScale-eq1})[-1]
  $copy=@($measurements|Where-Object{$_.category-eq$key-and$_.variant-eq'copy'-and$_.theme-eq$theme-and$_.fontScale-eq1})[-1]
  if(!$current-or!$copy){throw "Missing capture: $key $theme"}
  $summary.Add([pscustomobject]@{Category=$key;Theme=$theme;ReferenceWidthPx=$body.W;ReferenceHeightPx=$body.H;CurrentWidthPx=$current.widthPx;CurrentHeightPx=$current.heightPx;CopyWidthPx=$copy.widthPx;CopyHeightPx=$copy.heightPx;CopyHeightDeltaPx=($copy.heightPx-$body.H);Dpi=$copy.dpi})
  $nativeImage=Picture (Join-Path $Directory ('reference\'+$reference.Screenshot)) $body.W
  $currentImage=Picture (Join-Path $Directory ('captures\'+$current.file)) $current.widthPx
  $copyImage=Picture (Join-Path $Directory ('captures\'+$copy.file)) $copy.widthPx
  $delta=$copy.heightPx-$body.H
  $sections.Add(@"
<article data-theme="$theme"><h2>$(Html $names[$key])</h2><div class="samples">
<section><h3>Оригинал Windows</h3><p>$($body.W) × $($body.H) px</p><div class="stage" data-ref-height="$($body.H)">$nativeImage<i class="ruler"></i></div></section>
<section><h3>Текущий стиль проекта</h3><p>$($current.widthPx) × $($current.heightPx) px</p><div class="stage" data-ref-height="$($body.H)">$currentImage<i class="ruler"></i></div></section>
<section><h3>По образцу Windows</h3><p>$($copy.widthPx) × $($copy.heightPx) px · разница высоты: $delta px</p><div class="stage" data-ref-height="$($body.H)">$copyImage<i class="ruler"></i></div></section>
</div></article>
"@)
 }
}
$html=@"
<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Сравнение меню Windows 11</title>
<style>body{font:14px 'Segoe UI Variable','Segoe UI',sans-serif;background:#f3f3f3;color:#202020;margin:28px}h1{font-size:24px;margin:0 0 10px}h2{font-size:19px;margin:0 0 14px}h3{font-size:14px;margin:0}p{margin:8px 0 14px;color:#555}.toolbar{display:flex;gap:24px;margin:18px 0}article{padding:20px;margin:16px 0;background:#fff;border:1px solid #ddd;border-radius:8px}.samples{display:grid;grid-template-columns:repeat(3,minmax(250px,1fr));gap:20px}.stage{position:relative;min-height:180px;background:#e8e8e8;padding:0;overflow:visible}img{display:block;max-width:none}.ruler{position:absolute;left:0;right:0;border-top:1px dashed #dc3030;pointer-events:none;display:none}body.lines .ruler{display:block}article[data-theme=dark]{display:none}body.dark article[data-theme=light]{display:none}body.dark article[data-theme=dark]{display:block}button,select{font:inherit;padding:5px}aside{max-width:1000px;line-height:1.5}</style>
<h1>Одни и те же меню: оригинал, текущий стиль и тестовая копия</h1><aside>Снимки настоящих XAML-меню, одинаковые подписи, значки и разделители. Изображения показываются пиксель в пиксель с учётом масштаба экрана. Красная линия отмечает высоту оригинала. Системные эталоны сняты в светлой теме; тёмная тема показана для тестовых копий.</aside>
<div class="toolbar"><label>Тема копий <select id="theme"><option value="light">Светлая</option><option value="dark">Тёмная</option></select></label><label><input id="lines" type="checkbox" checked> Линия высоты оригинала</label></div>
$($sections-join"`n")
<script>function size(){const d=window.devicePixelRatio||1;document.querySelectorAll('img').forEach(i=>i.style.width=(Number(i.dataset.pixelWidth)/d)+'px');document.querySelectorAll('.stage').forEach(s=>{s.querySelector('.ruler').style.top=(Number(s.dataset.refHeight)/d)+'px';s.style.minHeight=(Number(s.dataset.refHeight)/d+12)+'px'})}document.getElementById('theme').onchange=e=>document.body.classList.toggle('dark',e.target.value==='dark');document.getElementById('lines').onchange=e=>document.body.classList.toggle('lines',e.target.checked);document.body.classList.add('lines');window.addEventListener('resize',size);size();</script></html>
"@
[IO.File]::WriteAllText((Join-Path $Directory 'comparison.html'),$html,[Text.UTF8Encoding]::new($false))
$summary|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Directory 'comparison.json') -Encoding UTF8
$summary|ConvertTo-Json
