Yata Native Framework 16 — Windows 11 x64
======================================

Переиспользуемый C++20 / C++/WinRT каркас: системный XAML Island,
панель у трея, Windows Composition, штатные элементы управления и отдельный
патч содержимого кнопки Explorer. VPN-провайдеры и сетевые настройки не менялись.

Готовые файлы находятся в bin\Release. Для запуска нужны ровно два файла:
  YataNativeFramework.exe
  YataNativeFramework.Patch.dll

CRT статически слинкована (/MT). Шрифты, XAML и Composition предоставляет ОС.
Ресурсы и XAML встроены в код; NuGet, WebView, установщик и дополнительные
поставляемые библиотеки для этой версии не требуются.

ЗАПУСК

  YataNativeFramework.exe
  YataNativeFramework.exe --controls --show
  YataNativeFramework.exe --standalone --controls --show

Обычный запуск показывает каталог, регистрирует собственный значок и пытается
подключить патч к Explorer. Перед запуском проверяется наличие Explorer на
рабочем столе процесса. Если панели задач нет, программа завершает запуск
с кодом 3 и точным объяснением; невидимый экземпляр не создаётся.
Start-Framework.cmd в корне проекта предназначен для запуска из Проводника.
Повторный запуск передаёт команду существующему
экземпляру. --standalone запускает тот же интерфейс без регистрации значка
и без подключения патча; этот параметр применяется при старте процесса.
При отказе Shell_NotifyIcon интерфейс также остаётся работоспособным.

  --mode label|lines|off       подпись / две строки примера / обычный значок
  --width 0|32..320            автоматическая или точная ширина в DIP
  --attach                    подключить патч; отдельная служебная команда
  --detach                    восстановить исходное содержимое значка
  --status                    режим, применённая ревизия, ошибки и жизненный цикл
  --profile                   счётчики обновлений, таймеров и подписок в JSON
  --diagnose                  рабочий стол процесса, input desktop и доступность Explorer
  --exit                      завершить приложение
  --help / --version          справка / версия без запуска интерфейса
  --sample --show             прежний визуальный пример, без реального трафика

Пример одной команды: --mode lines --width 88. Порядок параметров не влияет
на выбранный режим; другой режим сохраняет собственную ширину. Прежние
--native-vpn / --native-speed / --native-off / --native-width=88 принимаются
как совместимые имена режимов отображения. Они не управляют VPN.
Настройки интерфейса: %TEMP%\YataNativeFramework.layout.ini.
Идентификаторы окна, значка, отображения памяти и DLL отделены от v11/v12.

КАК ПОДКЛЮЧАТЬ СВОЙ ИНТЕРФЕЙС

Публичный API находится в source\ShellPanels\ShellPanels.h, реализация собирается
в ShellPanels.lib и статически включается в приложение. NativeFlyout — внутренняя
транспортная оболочка; непосредственно включать её в приложение больше не нужно.

  #include "ShellPanels.h"
  ShellPanels::Host panel(hwnd, [](ShellPanels::Action action, double value) {
      // Передайте намерение своему провайдеру; библиотека не меняет настройки ОС.
  });
  ShellPanels::Model model;
  model.light = true;
  panel.SetModel(model);
  panel.Show(ShellPanels::Kind::QuickSettings, anchorInPhysicalPixels);

Для своего дерева контролов:
  panel.SetContent(content, L"Моё приложение", 600);
  panel.Show(anchorInPhysicalPixels);

В цикле Win32 передавайте каждое MSG в panel.Process(msg) до DispatchMessage.
При WA_INACTIVE вызывайте panel.Deactivated(nextWindow, ownTrayPress).
Все вызовы и уничтожение Host выполняются на владеющем STA-потоке. HWND живёт
дольше Host. Настройте приложение на PerMonitorV2 (см. manifest тестового стенда).

Схемы: --lang --show, --quick --show, --date --show. Отдельный стенд:
  powershell -File tools\shell-parity\Build.ps1
  artifacts\shell-panels\Release\ShellBench.exe
Клавиши стенда: 1/2/3 — панели, ПКМ — меню звука, Esc — закрыть, F10 — выйти.
Параметры --dark / --custom меняют только оформление стенда.

Контракт, ограничения модели, линковка и политика меню:
source\ShellPanels\README.txt. План и фактический отчёт испытаний:
evidence\shell-parity-26100\REPORT.txt и TEST-PLAN.csv.
Индекс записей и сравнения: evidence\shell-parity-26100\comparison.html.
Эта версия является измеряемым прототипом трёх схем; полное совпадение 1:1
со всеми состояниями Shell пока не подтверждено. Отличия перечислены в отчёте.

NativeControls.h — пример клиента API с шестью разделами. В нём используются
Button, ToggleButton, RepeatButton, HyperlinkButton, CheckBox, RadioButton,
ToggleSwitch, TextBox, PasswordBox, AutoSuggestBox, Slider, ProgressBar,
ProgressRing, ComboBox, ListBox, ListView, GridView, DatePicker, TimePicker,
CalendarDatePicker, Pivot, NavigationView, CommandBar, AppBarButton,
AppBarToggleButton, ToolTip, Flyout, MenuFlyout, вложенные и переключаемые
пункты меню и ContentDialog. События кнопок, ввода, выбора и слайдера подключены.
Пароли не сохраняются. Диалог закрывается при выгрузке страницы; завершение
асинхронной операции не обращается к уничтоженной странице.

Ограничение набора: поддерживаются встроенные в ОС Windows.UI.Xaml контролы.
Это не поставка всего WinUI 3. Компоненты только из Microsoft.UI.Xaml
(например InfoBar, NumberBox и TeachingTip) в данный комплект не включены.
Windows.UI.Xaml.Controls: https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls

АРХИТЕКТУРА

EXE содержит приложение, CLI, диагностику и временную роль --attach. Отдельный
Host.exe не нужен. DLL загружается в Explorer через XAML Diagnostics и временные
хуки только на этапе подключения. Штатные шаблоны Explorer не являются публичным
контрактом; несовместимость должна приводить к восстановлению обычного значка.

Обновления идут по событиям XAML и уведомлениям протокола v13. Запрос режима
и размеров читается как одна согласованная ревизия. Нет постоянных таймеров
опроса: один таймер используется для задержки подсказки, ещё один — в приложении
только для ограничения времени подключения. Таймеры снимаются при завершении.
Ожидание процесса владельца сообщает о выходе через сообщение на UI-поток.
Отключение восстанавливает содержимое, bindings, оформление и подсказку,
снимает подписки и освобождает отображение памяти. Неактивная DLL и пустое
broker-окно могут оставаться в Explorer до его завершения — диагностика держит
модуль загруженным. Перезапуск Explorer для обычного выхода не требуется.

СБОРКА И ПРОВЕРКИ

Нужны Visual Studio с Desktop development with C++ x64 и Windows SDK с C++/WinRT.
Каталог Visual Studio и версия SDK определяются автоматически; vswhere необязателен.

  powershell -NoProfile -ExecutionPolicy Bypass -File Build.ps1 -Configuration Release -SelfTest
  powershell -NoProfile -ExecutionPolicy Bypass -File Build.ps1 -Configuration Debug -SelfTest
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Client.ps1
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\Test-Live.ps1 -CrashTest
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\Package.ps1 -NoBuild

Build.ps1 — каноническая сборка. В source\YataNativeFramework.vcxproj есть
Makefile-проект для запуска той же сборки из Visual Studio.
Тестовые EXE, PDB, LIB и OBJ находятся в artifacts, отдельно от двух runtime-файлов.

SelfTest проверяет настоящий XAML-контроллер, отказ и восстановление, 20 циклов
подключения/отключения, согласованность CLI/протокола, 3125 расчётов размещения,
настоящие контролы каталога, UI Automation, фокус, темы, диалог, замену страницы
и анимации. Explorer эти тесты не модифицируют.
Test-Client проверяет IPC и интерфейс готового EXE без патча оболочки.
Test-Live подключает DLL к реальному Explorer и проверяет жизненный цикл.
-CrashTest завершает только экземпляр, запущенный самим тестом.

Фактические результаты и оставшаяся матрица приёмки: evidence\VERIFICATION.txt.
Причины ошибок подключения и названия этапов также сохраняются в UTF-8:
%TEMP%\YataNativeFrameworkConnection.log. Рабочий стол не переключается автоматически.
Исторические скрипты и результаты v12 вынесены в evidence\v12-legacy;
они не являются подтверждением текущей версии и не входят в архив исходников.

ОБНОВЛЕНИЕ v15: DATE И КОНТЕЙНЕР КАРТОЧЕК

Date использует тот же публичный SetSections, что и отдельное приложение.
Header / Body / Footer каждой карточки имеют собственную рамку и материал.
Гибкая секция забирает оставшуюся высоту рабочей области; фиксированные тела
сворачиваются через ExpandSection. Состояние сохраняется при закрытии окна.
В ShellBench нажмите 4 для примера из трёх карточек; 1/2/3 открывают схемы Shell.
Подробности и записи: evidence\shell-parity-26100\DATE-v15.txt и comparison.html.
Это очередная итерация визуального профиля, а не сертификат полного совпадения.
ОБНОВЛЕНИЕ v16: DPI
Реальные 100–250%, штатный CalendarView, компактные панели на короткой рабочей области.
Результаты и ограничения: evidence\shell-parity-26100\dpi-v16\REPORT.txt.
