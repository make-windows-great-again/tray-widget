[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root 'evidence\navigation-width-20261105'
function Image([string]$phase,[string]$case){
 $directory=Join-Path $output $phase
 $record=Get-Content -LiteralPath (Join-Path $directory 'probe.jsonl') -Encoding UTF8|ForEach-Object{$_|ConvertFrom-Json}|Where-Object{$_.case-eq$case-and$_.window-eq$true}|Select-Object -First 1
 $bytes=[IO.File]::ReadAllBytes((Join-Path $directory $record.file))
 return '<img src="data:image/png;base64,'+[Convert]::ToBase64String($bytes)+'">'
}
$body=[Collections.Generic.List[string]]::new()
foreach($theme in @('light','dark')){
 $body.Add('<section data-theme="'+$theme+'"><div class="grid">')
 foreach($card in @(@('before',"nav-$theme-initial",'До: первое появление уже раскрытой панели'),@('before',"nav-$theme-toggle1-second",'До: повторное раскрытие'),@('after',"nav-$theme-toggle1-first",'После: первое раскрытие из закрытого состояния'),@('after',"nav-$theme-toggle3-first",'После: третье раскрытие'))){
  $body.Add('<article><h2>'+[Net.WebUtility]::HtmlEncode($card[2])+'</h2>'+(Image $card[0] $card[1])+'</article>')
 }
 $body.Add('</div></section>')
}
$html=@"
<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>NavigationView — первое и повторное раскрытие</title>
<style>body{font:14px 'Segoe UI Variable','Segoe UI',sans-serif;background:#f3f3f3;color:#222;margin:24px}h1{font-size:23px}h2{font-size:15px}p{max-width:1100px;line-height:1.5}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px;max-width:1450px}article{background:#fff;border:1px solid #ccc;border-radius:8px;padding:14px}img{display:block;width:100%;height:auto}section[data-theme=dark]{display:none}body.dark section[data-theme=light]{display:none}body.dark section[data-theme=dark]{display:block}select{font:inherit;padding:5px}</style>
<h1>NavigationView: первое и повторное раскрытие</h1><p>Исходный LeftCompact создавался уже раскрытым. После первого закрытия/открытия появлялись другая заливка и тень. Теперь он явно начинается свёрнутым, а первое и последующие раскрытия проходят одинаковый нативный переход. Режим CompactOverlay сохранён.</p><label>Тема <select id="theme"><option value="light">Светлая</option><option value="dark">Тёмная</option></select></label>
$($body-join"`n")
<script>document.getElementById('theme').onchange=e=>document.body.classList.toggle('dark',e.target.value==='dark');</script></html>
"@
[IO.File]::WriteAllText((Join-Path $output 'navigation-review.html'),$html,[Text.UTF8Encoding]::new($false))
Write-Output (Join-Path $output 'navigation-review.html')
