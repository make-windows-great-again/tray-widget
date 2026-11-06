[CmdletBinding()]
param([string]$AuditDirectory=(Join-Path $PSScriptRoot '..\..\evidence\visual-audit-20261105'))
$ErrorActionPreference='Stop'
$AuditDirectory=[IO.Path]::GetFullPath($AuditDirectory)
$rows=Get-Content (Join-Path $AuditDirectory 'probe\probe.jsonl') -Encoding UTF8|ForEach-Object{$_|ConvertFrom-Json}
function Picture([string]$case,[bool]$popup=$false){
 $window=$rows|Where-Object{$_.window-and $_.case-eq $case -and $(if($popup){$_.class-eq 'Xaml_WindowedPopupClass'}else{$_.class-eq 'YataNativeFramework.VisualAudit'})}|Select-Object -First 1
 if(!$window){throw "Missing screenshot: $case popup=$popup"}
 return ('probe/'+$window.file)
}
$figures=@(
 @('Белый текст на светлом меню после Light → Dark',(Picture 'light-open-menu-theme-flip' $true),'P1: фон presenter остался Light, пункты уже Dark.'),
 @('Чёрный текст на тёмном меню после Dark → Light',(Picture 'dark-open-menu-theme-flip' $true),'P1: воспроизведение в обратную сторону.'),
 @('Светлый диалог внутри тёмного интерфейса',(Picture 'dark-dialog'),'P1: ContentDialog.ActualTheme=Light при Dark владельце.'),
 @('Три строки в ListBox высотой 108 DIP',(Picture 'light-page2'),'P2: 3 × 40 DIP не помещаются в viewport 108 DIP.'),
 @('Обычное меню галереи',(Picture 'light-gallery-menu' $true),'Радиус 0 DIP, стандартный шаблон Windows.UI.Xaml.'),
 @('Отдельно стилизованное меню трея',(Picture 'light-tray-menu' $true),'Радиус 8 DIP. Системная focus-рамка не считается дефектом.'),
 @('Локальная пользовательская палитра',(Picture 'custom-page2'),'Строки приняли #E8DBFF, заголовок и поверхность остались прежними.'),
 @('Ширина 320 DIP / 480 px',(Picture 'width-320'),'Заголовки Pivot 24 DIP уходят за пределы viewport.'),
 @('Искусственное увеличение текста ×1.5',(Picture 'synthetic-text150'),'Это stress-тест layout, не изменение системного DPI/размера текста.'),
 @('Контроль: ComboBox корректно меняет тему',(Picture 'light-open-combo-theme-flip' $true),'Фон и текст согласованы; нельзя обобщать дефект MenuFlyout на все popup.')
)
$reference=Get-Content (Join-Path $AuditDirectory 'native-reference.json') -Raw -Encoding UTF8|ConvertFrom-Json
foreach($item in @($reference|Where-Object Target -eq network)){$figures+=,@('Живое меню сети Windows',$item.Screenshot,'Тот же DPI 144. Segoe UI Variable, UIA FontSizeRaw=14. Используется для типографики/геометрии, не сравнения RGB.')}
function Encode([string]$value){[Net.WebUtility]::HtmlEncode($value)}
$cards=foreach($figure in $figures){
 $path=[IO.Path]::GetFullPath((Join-Path $AuditDirectory $figure[1]))
 if(!$path.StartsWith($AuditDirectory+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected evidence path'}
 $base64=[Convert]::ToBase64String([IO.File]::ReadAllBytes($path))
 '<figure><h3>'+ (Encode $figure[0])+'</h3><a href="'+(Encode $figure[1])+'"><img src="data:image/png;base64,'+$base64+'" alt="'+(Encode $figure[0])+'"></a><figcaption>'+(Encode $figure[2])+'</figcaption></figure>'
}
$html=@'
<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Повторный визуальный аудит — Yata Native Framework v13</title>
<style>body{font:16px/1.55 system-ui,"Segoe UI",sans-serif;margin:0;background:#f5f6f8;color:#18202c}main{max-width:1180px;margin:auto;padding:36px 24px}h1{font-size:30px;line-height:1.2}h2{margin-top:34px}h3{font-size:17px;margin:0 0 14px}.meta{color:#526071}.lead{font-size:19px;max-width:930px}.notice{padding:16px 20px;background:#fff3d7;border-left:4px solid #ad7000}table{border-collapse:collapse;width:100%;background:white}td,th{text-align:left;padding:12px;border-bottom:1px solid #dce1e8;vertical-align:top}th{background:#edf0f4}code{font-family:Consolas,monospace;font-size:14px}.gallery{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:20px;align-items:start}figure{margin:0;padding:20px;background:white;border:1px solid #dce1e8;border-radius:10px;overflow:hidden}img{max-width:100%;height:auto;display:block;margin:auto}figcaption{color:#526071;margin-top:14px;font-size:14px}a{color:#064ec1}li{margin:6px 0}footer{margin-top:36px;color:#526071}@media print{body{background:white}.gallery{grid-template-columns:1fr 1fr}figure{break-inside:avoid}main{padding:0}}</style>
<main><p class="meta">05.11.2026 · Windows 11 24H2 / 26100.9711 · DPI 144 / 150% · text scale 100%</p>
<h1>Повторный визуальный аудит Yata Native Framework v13</h1>
<p class="lead">Главные проблемы — несогласованные темы popup и диалога, усечение списка и смешение разных стилей. На текущем DPI ошибки двойного масштабирования не найдено.</p>
<p>46 измеренных состояний, 64 актуальных кадра. Проверены Light, Dark и локальная пользовательская палитра, шесть страниц, меню и подменю, ComboBox, Flyout и ContentDialog. Контролы получены из тех же production-заголовков в отдельном измерительном EXE; production EXE/DLL не менялись.</p>
<table><thead><tr><th>Приоритет</th><th>Наблюдение</th><th>Основание</th></tr></thead><tbody>
<tr><td>P1 / V01</td><td>Открытое MenuFlyout становится нечитаемым при смене локальной темы.</td><td>Presenter остаётся в старой теме, пункты получают новую; обе стороны воспроизведены.</td></tr>
<tr><td>P1 / V02</td><td>Новый ContentDialog светлый внутри тёмной панели.</td><td>XamlRoot назначен, тема владельца не перенесена.</td></tr>
<tr><td>P2 / V03</td><td>ListBox со скриншота обрезает третью строку.</td><td>Viewport 108 DIP, три строки по 40 DIP; на 150% видимая высота последней — 42 вместо 60 px. Доступна прокрутка.</td></tr>
<tr><td>P2 / V04</td><td>Нет единого оформления контролов и меню.</td><td>Меню галереи 0 DIP, трея 8 DIP; обычные кнопки 0 DIP, закрытие 4 DIP.</td></tr>
<tr><td>P2 / V05</td><td>Кастомная палитра применяется частично.</td><td>Новые строки меняют кисть, существующая оболочка и popup остаются с прежними ресурсами.</td></tr>
<tr><td>P2 / V06</td><td>Галерея слишком жёстко задаёт размеры и навигацию.</td><td>Pivot 440 DIP; поверхность галереи 600 × 582.667 DIP; вкладки 24 DIP при заголовке панели 14 DIP.</td></tr>
</tbody></table>
<h2>Шрифты и масштаб</h2><p>Основной текст и меню используют <b>Segoe UI Variable, 14 DIP</b>, подписи — 12 DIP. На 150% это 21 и 18 физических px для em. Высота глифа, строки и hit rectangle — разные метрики. Системное меню сети на том же мониторе также даёт UIA FontSizeRaw=14; шаг его строк со значками — 52 px, тогда как у меню приложения — 48 px.</p>
<p>600 DIP дают 900 физических px, 320 DIP — 480 px. Уменьшать шрифт вручную для компенсации DPI оснований нет: сначала нужно исправить viewport, layout и стили. Размер HWND с тенью не равен видимой поверхности панели.</p>
<div class="notice"><b>Пределы:</b> настоящий DPI проверен только на 150%, монитор один. High Contrast, другие физические DPI, системный text scale и прозрачность глобально не переключались. Light/Dark проверены через API темы приложения; тест локальной палитры не является проверкой всех сторонних тем Windows. В измерительном хосте отключены анимации и автоскрытие при деактивации.</div>
<h2>Доказательства</h2><div class="gallery">
'@
$html+=($cards-join "`n")
$html+=@'
</div><h2>Следующие исправления</h2><ol><li>Согласовать тему всех слоёв popup и ContentDialog, включая уже открытые окна.</li><li>Определить единый контракт ресурсов и стилей, обновляемых при смене темы.</li><li>Заменить жёсткие высоты списков и страниц на измерение контента; добавить компактную навигацию.</li><li>Пройти оставшуюся матрицу настоящих DPI, text scale, High Contrast и transparency-off.</li></ol>
<p><a href="VISUAL-AUDIT.txt">Полный технический отчёт с привязкой к исходникам</a> · <a href="summary.json">Сводка измерений</a> · <a href="probe/probe.jsonl">Сырые XAML-метрики</a></p>
<footer>Ресурсы Microsoft: <a href="https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography">типографика</a>, <a href="https://learn.microsoft.com/en-us/windows/apps/develop/input/text-scaling">увеличение текста</a>, <a href="https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.frameworkelement.requestedtheme">RequestedTheme</a>. Production-код и VPN-часть в этом проходе не изменялись.</footer></main></html>
'@
[IO.File]::WriteAllText((Join-Path $AuditDirectory 'VISUAL-AUDIT.html'),$html,[Text.UTF8Encoding]::new($false))
'Wrote visual report'
