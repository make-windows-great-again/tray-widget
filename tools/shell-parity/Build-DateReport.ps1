$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100';$recordings=Join-Path $out 'recordings'
$tags=@('native-light','cpp-light-v15','native-dark','cpp-dark-v15','cpp-custom-v15')
$timelines=@{}
foreach($tag in $tags){
 $timelines[$tag]=Get-Content (Join-Path $recordings ($tag+'-events.json')) -Raw|ConvertFrom-Json
 foreach($scene in @('lang','quick','date')){
  Copy-Item -LiteralPath (Join-Path $recordings ($tag+'-'+$scene+'-lossless.png')) -Destination (Join-Path $out ('frames\'+$tag+'-'+$scene+'.png')) -Force
 }
}
$json=$timelines|ConvertTo-Json -Depth 5 -Compress
$page=@'
<!doctype html><html lang="ru"><meta charset="utf-8"><title>ShellPanels v15 — Date и карточки</title>
<style>body{font:16px system-ui;background:#202020;color:#eee;margin:24px;max-width:1500px}h1{font-size:26px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:16px}video,img{width:100%;max-height:78vh;object-fit:contain;background:#606060}button,select{font:inherit;padding:8px;margin:4px}a{color:#8dcaff}.note{color:#ccc;max-width:1100px}details{margin:24px 0}summary{cursor:pointer;padding:8px}code{color:#a9dec9}</style>
<h1>ShellPanels v15 — Date и независимые карточки</h1>
<p class="note">Windows 11 26100.9711, 2560×1600, 150%, монитор 240 Гц. Отдельная статическая библиотека C++/WinRT. Runtime приложения: 1 EXE + 1 patch DLL. Полное совпадение 1:1 пока не подтверждено.</p>
<p><a href="DATE-v15.txt">Изменения и ограничения</a> · <a href="date-v15-motion.csv">Вход/выход</a> · <a href="date-v15-card-motion.csv">Сворачивание</a> · <a href="TEST-PLAN.csv">Тесты</a> · <a href="FIX-PLAN.csv">Открытые пункты</a> · <a href="comparison-v14.html">Предыдущая итерация</a></p>
<select id="theme"><option value="light">Светлая</option><option value="dark">Тёмная</option><option value="custom">Свой цвет C++ / тёмная Windows</option></select>
<select id="scene"><option value="date-open">Дата</option><option value="quick-open">Быстрые настройки</option><option value="lang-open">Язык</option><option value="date-context">ПКМ: часы</option><option value="quick-context">ПКМ: звук</option><option value="network-context">ПКМ: сеть</option><option value="battery-context">ПКМ: батарея</option></select>
<button id="seek">К сцене</button><button id="play">Воспроизвести обе</button><button id="pause">Пауза</button><button id="slow">¼ скорости</button><button id="normal">Обычная скорость</button>
<div class="pair"><div><p>Windows — исходная запись</p><video id="native" controls preload="metadata"></video></div><div><p>C++ — v15</p><video id="cpp" controls preload="metadata"></video></div></div>
<p class="note">Синхронизация по событиям приблизительная: FFmpeg и журнал ввода имеют смещение. Частота захвата VFR не равна частоте рендера. Числа движения получены по границам кадров. Меню батареи намеренно сохраняет общий отступ над taskbar.</p>
<details open><summary>Сворачивание календаря: Windows / C++ v15</summary><div class="pair"><video controls src="recordings/native-light-interactions.mp4"></video><video controls src="recordings/cpp-light-interactions-v15.mp4"></video></div></details>
<details><summary>Три карточки в отдельном приложении</summary><p><code>Host::SetSections</code> принимает Header / Body / Footer. Гибкая карточка занимает остаток высоты, фиксированная сворачивает только Body. Клавиша 4 в ShellBench открывает этот пример.</p><video controls src="recordings/cpp-light-cards-v15.mp4"></video></details>
<details><summary>Сворачивание в тёмной теме</summary><video controls src="recordings/cpp-dark-interactions-v15.mp4"></video></details>
<details><summary>Новый EXE через настоящий tray-widget</summary><video controls src="recordings/tray-light-v15.mp4"></video></details>
<details><summary>Date — снимки без видеосжатия</summary><div class="pair"><img src="frames/native-light-date.png" alt="Windows Date"><img src="frames/cpp-light-v15-date.png" alt="C++ Date v15"></div></details>
<script>
const timelines=TIMELINES, videos=['native','cpp'].map(id=>document.getElementById(id));
function keys(){return ['native-'+(theme.value==='custom'?'dark':theme.value),'cpp-'+theme.value+'-v15'];}
function load(){keys().forEach((key,i)=>videos[i].src='recordings/'+key+'.mp4');}load();theme.onchange=load;
seek.onclick=()=>keys().forEach((key,i)=>{const e=timelines[key].find(e=>e.Event===scene.value);if(e)videos[i].currentTime=Math.max(0,e.Ms/1000-.25)});
play.onclick=()=>videos.forEach(v=>v.play());pause.onclick=()=>videos.forEach(v=>v.pause());slow.onclick=()=>videos.forEach(v=>v.playbackRate=.25);normal.onclick=()=>videos.forEach(v=>v.playbackRate=1);
</script></html>
'@
$prior=Join-Path $out 'comparison-v14.html'
if(!(Test-Path -LiteralPath $prior)){Copy-Item -LiteralPath (Join-Path $out 'comparison.html') -Destination $prior}
[IO.File]::WriteAllText((Join-Path $out 'comparison.html'),$page.Replace('TIMELINES',$json),[Text.UTF8Encoding]::new($false))
