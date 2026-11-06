param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100'
$recordings=Join-Path $out 'recordings';$frames=Join-Path $out 'frames';[IO.Directory]::CreateDirectory($frames)|Out-Null
$ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
Add-Type -AssemblyName System.Drawing
function Read-Spans([string]$file,[int]$x,[bool]$dark){
 $bitmap=[Drawing.Bitmap]::new($file);$spans=[Collections.Generic.List[string]]::new();$start=-1
 try{for($y=80;$y-lt1520;$y++){$color=$bitmap.GetPixel($x,$y);$surface=if($dark){$color.R-lt70}else{$color.R-gt160};if($surface){if($start-lt0){$start=$y}}else{if($start-ge0-and$y-$start-ge60){$spans.Add("$start..$($y-1)")};$start=-1}};return ($spans-join';')}finally{$bitmap.Dispose()}
}
function Read-Mean([string]$file,[int]$x,[int]$y,[int]$width,[int]$height){
 $bitmap=[Drawing.Bitmap]::new($file);$red=0.0;$green=0.0;$blue=0.0
 try{for($j=$y;$j-lt$y+$height;$j++){for($i=$x;$i-lt$x+$width;$i++){$color=$bitmap.GetPixel($i,$j);$red+=$color.R;$green+=$color.G;$blue+=$color.B}};$n=$width*$height;return @(($red/$n),($green/$n),($blue/$n))}finally{$bitmap.Dispose()}
}
$tags=@('native-light','cpp-light','native-dark','cpp-dark','cpp-custom')
$rows=[Collections.Generic.List[object]]::new();$timelines=@{}
foreach($tag in $tags){
 $events=Get-Content (Join-Path $recordings ($tag+'-events.json')) -Raw|ConvertFrom-Json;$timelines[$tag]=$events
 foreach($scene in @('lang','quick','date','network-context','battery-context','quick-context','date-context')){
  $eventName=if($scene.Contains('context')){$scene}else{$scene+'-open'}
  $event=$events|Where-Object Event -eq $eventName|Select-Object -First 1
  $time=([double]$event.Ms/1000+.55).ToString('F6',[Globalization.CultureInfo]::InvariantCulture)
  $file=Join-Path $frames ($tag+'-'+$scene+'.png')
  & $ff -hide_banner -loglevel error -y -ss $time -i (Join-Path $recordings ($tag+'.mp4')) -frames:v 1 -update 1 $file
  if($LASTEXITCODE){throw "Frame extraction failed: $tag / $scene"}
  $lossless=Join-Path $recordings ($tag+'-'+$scene+'-lossless.png');if(Test-Path -LiteralPath $lossless){Copy-Item -LiteralPath $lossless -Destination $file -Force}
  if(!$scene.Contains('context')){
   $x=@{lang=490;quick=575;date=610}[$scene];$dark=$tag.Contains('dark')-or$tag.Contains('custom')
   $spans=(Read-Spans $file $x $dark)
   $mean=if($scene-eq'quick'){(Read-Mean $file 575 1270 10 100)}else{@(0,0,0)}
   $rows.Add([pscustomobject]@{Tag=$tag;Scene=$scene;ImageSource=$(if(Test-Path -LiteralPath $lossless){'lossless screenshot'}else{'decoded H264'});FrameTime=$(if(Test-Path -LiteralPath $lossless){''}else{$time});SampleX=$x;VerticalSpans=$spans;MaterialR=$mean[0];MaterialG=$mean[1];MaterialB=$mean[2]})
  }
 }
}
$rows|Export-Csv (Join-Path $out 'pixel-measurements.csv') -NoTypeInformation
$stats=foreach($tag in $tags){Get-Content (Join-Path $recordings ($tag+'-metrics.json')) -Raw|ConvertFrom-Json}
$stats|ConvertTo-Json|Set-Content (Join-Path $out 'capture-metrics.json')
$json=$timelines|ConvertTo-Json -Depth 5 -Compress
$html=@'
<!doctype html><html lang="ru"><meta charset="utf-8"><title>ShellPanels — натурные испытания</title>
<style>body{font:16px system-ui;background:#202020;color:#eee;margin:24px;max-width:1400px}h1{font-size:25px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:16px}video{width:100%;max-height:75vh;background:#606060}button,select{font:inherit;padding:8px;margin:4px}a{color:#8dcaff}.note{color:#ccc;max-width:1000px}img{width:100%}details{margin:20px 0}.fig{display:grid;grid-template-columns:1fr 1fr;gap:16px}</style>
<h1>Windows 11 26100.9711 / C++ ShellPanels</h1>
<p class="note">2560×1600, 150%, 240 Гц. Нативные панели открыты физическими кликами; C++ — командой стенду. Закрытие вне панели и интерактивный тест используют физические клики. Запись VFR: 240 — запрошенная частота, фактические интервалы находятся в capture-metrics.json. Записи не доказывают полное совпадение 1:1.</p>
<p><a href="REPORT.txt">Отчёт</a> · <a href="TEST-PLAN.csv">Матрица тестов</a> · <a href="FIX-PLAN.csv">План исправлений</a> · <a href="pixel-measurements.csv">Пиксельные измерения</a> · <a href="capture-metrics.json">Частота записи</a></p>
<select id="theme"><option value="light">Светлая</option><option value="dark">Тёмная</option></select>
<select id="scene"><option value="lang-open">Язык</option><option value="quick-open">Быстрые настройки</option><option value="date-open">Дата</option><option value="quick-context">ПКМ: звук</option><option value="network-context">ПКМ: сеть</option><option value="battery-context">ПКМ: батарея</option><option value="date-context">ПКМ: часы</option></select>
<button id="seek">К сцене</button><button id="play">Воспроизвести обе</button><button id="pause">Пауза</button><button id="slow">¼ скорости</button><button id="normal">Обычная скорость</button>
<div class="pair"><div><p>Windows</p><video id="native" controls preload="metadata"></video></div><div><p>C++</p><video id="cpp" controls preload="metadata"></video></div></div>
<p class="note">Синхронизация приблизительная, по журналу входных событий. Для оценки траекторий используйте покадровые *-motion.csv: запуск FFmpeg и временная шкала ввода имеют смещение. Меню батареи C++ намеренно выровнено над taskbar, а не по Y курсора, по требованию пользователя.</p>
<details><summary>Нажатия календаря и наведения: отдельные записи</summary><div class="pair"><video controls src="recordings/native-light-interactions.mp4"></video><video controls src="recordings/cpp-light-interactions.mp4"></video></div></details>
<details><summary>Пользовательская палитра C++</summary><video controls src="recordings/cpp-custom.mp4"></video></details>
<details><summary>Реальный tray-widget: три схемы, каталог и ПКМ</summary><p>Реальные клики по виджету; состояние подтверждается IPC приложения. Команды CLI только выбирают схему перед циклом.</p><video controls src="recordings/tray-light.mp4"></video></details>
<details><summary>Готовые кадры</summary><p><a href="frames/native-light-lang.png">Язык Windows</a> / <a href="frames/cpp-light-lang.png">C++</a> · <a href="frames/native-light-quick.png">Quick Windows</a> / <a href="frames/cpp-light-quick.png">C++</a> · <a href="frames/native-light-date.png">Дата Windows</a> / <a href="frames/cpp-light-date.png">C++</a></p><div class="fig"><img src="frames/native-light-quick.png"><img src="frames/cpp-light-quick.png"></div></details>
<script>
const timelines=TIMELINES;
const videos=['native','cpp'].map(id=>document.getElementById(id));
function load(){videos.forEach(v=>v.src='recordings/'+v.id+'-'+theme.value+'.mp4');}load();theme.onchange=load;
seek.onclick=()=>videos.forEach(v=>{const e=timelines[v.id+'-'+theme.value].find(e=>e.Event===scene.value);v.currentTime=Math.max(0,e.Ms/1000-.25)});
play.onclick=()=>videos.forEach(v=>v.play());pause.onclick=()=>videos.forEach(v=>v.pause());slow.onclick=()=>videos.forEach(v=>v.playbackRate=.25);normal.onclick=()=>videos.forEach(v=>v.playbackRate=1);
</script></html>
'@
[IO.File]::WriteAllText((Join-Path $out 'comparison.html'),$html.Replace('TIMELINES',$json),[Text.UTF8Encoding]::new($false))
$rows|Format-Table -AutoSize
