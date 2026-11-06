[CmdletBinding()]
param([string]$Directory=(Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)) 'Download\YataMenuCompare'))
$ErrorActionPreference='Stop'
$Directory=[IO.Path]::GetFullPath($Directory)
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$comparison=Get-Content -LiteralPath (Join-Path $Directory 'comparison.json') -Raw -Encoding UTF8|ConvertFrom-Json
if(@($comparison|Where-Object{$_.CopyHeightDeltaPx-ne0}).Count){throw 'The comparison prototype does not match all reference heights'}
$exportedSource=Join-Path $Directory 'source';[IO.Directory]::CreateDirectory($exportedSource)|Out-Null
$cpp=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'MenuCompare.cpp')).Replace('../../source/NativeTray/NativeMenuStyle.h','NativeMenuStyle.h')
[IO.File]::WriteAllText((Join-Path $exportedSource 'MenuCompare.cpp'),$cpp,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $projectRoot 'source\NativeTray\NativeMenuStyle.h'),(Join-Path $projectRoot 'artifacts\menu-compare\include\ReferenceMenuData.h'),(Join-Path $PSScriptRoot 'MenuCompare.manifest') -Destination $exportedSource -Force
$build=[IO.File]::ReadAllText((Join-Path $projectRoot 'Build.ps1'))
$setup=$build.Substring(0,$build.IndexOf('Push-Location $objects'))
[IO.File]::WriteAllText((Join-Path $exportedSource 'Toolchain.ps1'),$setup,[Text.UTF8Encoding]::new($true))
$standalone=@'
[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..'))
$ErrorActionPreference='Stop'
$comparisonSource=$PSScriptRoot
. (Join-Path $comparisonSource 'Toolchain.ps1') -Configuration Release -OutputDirectory ([IO.Path]::GetFullPath($OutputDirectory))
$flags+="/I$comparisonSource"
Push-Location $objects
try{Compile (Join-Path $comparisonSource 'MenuCompare.cpp') (Join-Path $output 'YataMenuCompare.exe') ($xamlLibs+@('/SUBSYSTEM:WINDOWS','/MANIFEST:EMBED',"/MANIFESTINPUT:$comparisonSource\MenuCompare.manifest",'dwmapi.lib','gdiplus.lib','gdi32.lib','UIAutomationCore.lib','uuid.lib'))}finally{Pop-Location}
'@
[IO.File]::WriteAllText((Join-Path $exportedSource 'Build.ps1'),$standalone,[Text.UTF8Encoding]::new($true))
$readme=@'
Yata Menu Compare — отдельная тестовая сборка

Запуск: YataMenuCompare.exe. Установка не нужна.

1. Выберите Wi-Fi, звук, батарею или дату/время.
2. «Оригинал Windows» открывает настоящее меню у значка панели задач.
3. «Текущий стиль проекта» и «По образцу Windows» открывают тестовые меню
   в одном месте, с одинаковыми подписями, значками и разделителями.
4. ПКМ по области «ПКМ: по образцу Windows» также показывает тестовую копию.
5. Можно переключить светлую/тёмную тему и размер текста копий.
Команды внутри копий показывают выбранный пункт в строке состояния.

comparison.html — самостоятельное сравнение трёх вариантов рядом,
с линией высоты эталона и переключением темы копий. Изображения встроены.
comparison.json — сводные размеры; measurements.jsonl — измерения XAML.
reference/ — исходные системные меню; captures/ — 16 кадров тестовых меню.

На текущем DPI 144 / 150%:
                Windows    Текущий стиль    Тестовая копия
Wi-Fi           161 px       164 px            161 px
Звук            156 px       159 px            156 px
Батарея          57 px        58 px             57 px
Дата/время      104 px       106 px            104 px

По ширине у копий остаётся разница 1 px из-за компоновки; она показана
в сравнении. Высота, число строк и разделителей совпадают на данном DPI.
Системные эталоны сняты в светлой теме. Dark — предварительный просмотр
тестовых копий; настоящий системный Dark в этом прогоне не снимался.
Прозрачный материал зависит от фона под меню. Снимки не ретушировались.

Тестовый вариант: высота пунктов по содержимому без MinHeight=28,
отступы 10,3,10,3 DIP, согласованное состояние NarrowPadding.
Для материала использованы параметры AcrylicBackgroundFillColorDefaultBrush
из открытых ресурсов Microsoft WinUI (TintLuminosityOpacity 0.85/0.96).
Это воспроизведение по живым замерам и открытым ресурсам.

Исходники находятся в source/. Для самостоятельной сборки нужны Visual
Studio C++ x64 и Windows SDK с C++/WinRT:
  powershell -NoProfile -ExecutionPolicy Bypass -File source\Build.ps1
Перед пересборкой закройте тестовое окно. Эталоны и изображения должны
оставаться рядом с EXE в каталоге reference/.

Средства пересъёмки и генерации отчёта добавлены также в основной проект:
  YataNativeFramework\tools\menu-compare\

Тестовый EXE не подключает DLL к Explorer. Основные EXE/DLL сохранены;
проверка хешей проведена отдельно. VPN и сетевые настройки не менялись.

Первичные источники для параметров материала и кодов значков:
https://github.com/microsoft/microsoft-ui-xaml/blob/main/controls/dev/Materials/Acrylic/AcrylicBrush_themeresources.xaml
https://learn.microsoft.com/en-us/windows/apps/design/iconography/segoe-fluent-icons-font
'@
[IO.File]::WriteAllText((Join-Path $Directory 'README.txt'),$readme,[Text.UTF8Encoding]::new($true))
$records=Get-ChildItem -LiteralPath $Directory -Recurse -File|Where-Object{$_.Name-ne'SHA256.json'}|ForEach-Object{[pscustomobject]@{File=$_.FullName.Substring($Directory.Length+1);Bytes=$_.Length;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}}
$records|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Directory 'SHA256.json') -Encoding UTF8
$archive=Join-Path ([IO.Path]::GetDirectoryName($Directory)) 'YataMenuCompare-test.zip'
Compress-Archive -LiteralPath $Directory -DestinationPath $archive -Force
Write-Output $archive
