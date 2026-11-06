WinUI 3 VPN tray prototype
=========================

Это отдельное тестовое приложение C++20 / C++/WinRT с новым patch DLL.
VPN здесь — подпись виджета. Сетевого подключения и изменений VPN нет.
Старый YataNativeFramework не запускается и не заменяется.

Что работает
------------
ЛКМ по VPN открывает/закрывает Hello World, ПКМ открывает MenuFlyout WinUI 3.
В панели есть Slider ширины 32–320 DIP, автоматическая ширина, пресеты,
закрытие, системная/светлая/тёмная темы. Esc и потеря фокуса закрывают панель.
Панель движется вверх при открытии и вниз при закрытии через Composition.
SystemBackdropElement и DesktopAcrylicController сохраняют один материал
на всё время закрытия; системные ограничения прозрачности остаются в силе.

Ширина меняется и в реальном трее Explorer. Новая DLL обновляет существующий
элемент без пересоздания его содержимого, hover/pressed оформления и tooltip.
Отдельной интерполяции ширины с заданной частотой пока нет: Slider передаёт
значения, а Explorer выполняет layout. Это не подтверждение работы на 240 fps.

Граница движков
---------------
YataWinUI3Tray.exe: Microsoft.UI.Xaml (WinUI 3), HWND и DesktopWindowXamlSource,
Microsoft.UI.Composition, современный Windows App SDK. Всё содержимое панели
и контекстного меню принадлежит этому процессу.

YataWinUI3Tray.Patch.dll: нативный мост внутри Explorer, Windows.UI.Xaml
того Explorer, который уже запущен в Windows. Загрузить WinUI 3 вместо XAML
оболочки нельзя простым переносом кода DLL. Между процессами передаются числа,
состояния и HWND, а не XAML-объекты.

TrayProtocol.h задаёт отдельные GUID, shared memory, broker и registered messages
версии v1. TrayPatch.cpp реализует новую декорацию и изменение ширины на месте.
Проверенные NativeEventBackend, NativeChrome и NativeTooltip из старого проекта
используются как общие исходники. Старый клиент с новым протоколом несовместим.
Сессии, восстановление локальных значений/bindings и удаление декорации при
исчезновении процесса обеспечиваются общим backend.

Сборка и запуск
---------------
Нужны Windows 11 x64, PowerShell 7, Visual Studio 18 с MSVC v145, Windows SDK
10.0.26100.0 и доступ к NuGet для первого восстановления. Версии закреплены:
Windows App SDK 2.6.5, C++/WinRT 3.0.260818.1. Restore-Packages.ps1 проверяет
SHA512 архивов по каталогу NuGet и сохраняет resolved.json.

Из корня репозитория:
  pwsh -File tools\winui3-tray\Build.ps1 -Restore -SelfTest
  Start-WinUI3-Tray.cmd

Готовая папка: bin\WinUI3Tray\Release.
Собственных runtime-бинарников два: EXE и patch DLL. Дополнительно нужны DLL,
PRI, XBF и ресурсы Microsoft из этой папки. Выбран self-contained Windows App
SDK без глобальной установки Windows App Runtime. Нельзя перенести только
два наших файла. На машине также нужен Microsoft Visual C++ x64 runtime.
Файлы Microsoft распространяются на условиях их пакетов NuGet; лицензии
доступны в artifacts\winui3-packages. Это не включение их под лицензию проекта.

Команды (EXE = bin\WinUI3Tray\Release\YataWinUI3Tray.exe):
  EXE --standalone --show     панель без значка и без DLL в Explorer
  EXE --show                 открыть панель / запустить приложение
  EXE --width 144             установить ширину (0 = автоматически)
  EXE --theme dark            dark / light / system
  EXE --menu                 открыть меню
  EXE --hide                 скрыть панель, оставить значок
  EXE --status               JSON: requested и actual width, applied, ошибки
  EXE --exit                 закрыть приложение и удалить декорацию/значок

Если Windows спрятала новый значок в overflow, его можно переместить в видимую
часть трея. Приложение не меняет настройки видимости значков за пользователя.
При ошибке подключения остаётся обычный значок; подробности доступны в --status.
Лог DLL: %TEMP%\YataWinUI3TrayPatch.log (UTF-16).
Общий helper подключения пока пишет в %TEMP%\YataNativeFrameworkConnection.log.
Автозапуска, службы, установки или автоматического перезапуска Explorer нет.

XAML Diagnostics удерживает DLL до завершения Explorer. После --exit декорация
восстановлена, но неактивный модуль может оставаться загруженным. Для изменений
только клиента используйте Build.ps1 -ClientOnly. Замена загруженного patch DLL
требует следующего штатного завершения Explorer/выхода из сеанса; сборка не
перезапускает оболочку. Старый patch DLL не изменён.

Проверки
--------
Build.ps1 -SelfTest запускает изолированные XAML-тесты нового patch, без Explorer.
Test.ps1 проверяет UIA, CLI, меню, темы и цикл открытия/закрытия.
Test.ps1 -Live также проверяет фактическую ширину в Explorer, ЛКМ/ПКМ и detach.
Test-Dpi.ps1 проверяет 100/150/200/250% через открытую страницу Display Settings
и восстанавливает исходный масштаб в finally. GUI-тесты запускаются последовательно
в интерактивной сессии. Они кратковременно используют мышь/клавиатуру и показывают
собственный нейтральный фон, чтобы не записывать содержимое других приложений.
Test-Dpi.ps1 -Record записывает панель при 150% через локальный FFmpeg/DDAGrab;
FFmpeg должен находиться по пути, заданному в Test.ps1. Без -Record он не нужен.

Результаты: evidence\winui3-tray-20261106\REPORT.txt.

Границы прототипа
----------------
Это Hello World и проверка новой архитектуры, а не полный перенос старой
галереи, Language/Quick Settings/Date или работающий VPN-клиент.
Ширина/тема не сохраняются между запусками. Проверено на одном мониторе
Windows 11 24H2 build 26100. Полная приёмка нескольких мониторов, увеличенного
размера текста, high contrast и иных сборок Explorer ещё не выполнена.
Подключение к дереву Explorer зависит от реализации оболочки и не является
публичным API расширения ширины notification icon.

Документация Microsoft
---------------------
https://learn.microsoft.com/windows/apps/winui/winui3/
https://learn.microsoft.com/windows/apps/windows-app-sdk/system-backdrop-controller
https://learn.microsoft.com/windows/apps/windows-app-sdk/deploy-self-contained-apps
