# tray-widget

Нативный C++20 / C++/WinRT каркас для панелей и элементов управления Windows 11.
Runtime состоит из **одного EXE и одного patch DLL**. Схемы Language, Quick
Settings и Date собраны в отдельную статическую `ShellPanels.lib`; пример
`ShellBench` использует её без подключения к Explorer.

Проект находится в разработке. Совпадение с Windows проверяется на конкретном
профиле Windows 11 24H2 (26100.9711); полного соответствия всем состояниям Shell
пока нет. Открытые пункты и результаты проверок сохранены вместе с кодом.

Скачать runtime и SDK: [v16](https://github.com/make-windows-great-again/tray-widget/releases/tag/v16).

## Сборка

Нужны Windows 11 x64, Visual Studio с C++ и Windows SDK 26100.

```powershell
.\Build.ps1 -Configuration Release -SelfTest
.\tools\shell-parity\Build.ps1
.\tools\shell-parity\Package-Sdk.ps1
```

Запуск приложения: `Start-Framework.cmd`. Независимый стенд:
`artifacts\shell-panels\Release\ShellBench.exe` (клавиши 1–4, Esc, F10).
Сборка SDK содержит публичный заголовок, static lib и пример потребителя.

## Проверки и архитектура

- [Инструкция и CLI](README.txt)
- [API статической библиотеки](source/ShellPanels/README.txt)
- [История изменений](CHANGELOG.txt)
- [План исправлений](evidence/shell-parity-26100/FIX-PLAN.csv)
- [План приёмки](evidence/shell-parity-26100/TEST-PLAN.csv)
- [Приёмка DPI v16](evidence/shell-parity-26100/dpi-v16/REPORT.txt)
- [Визуальное сравнение DPI](evidence/shell-parity-26100/dpi-v16/comparison.html)

`tools/shell-parity` содержит стенды, UIA-проверки, запись DDAGrab и измерения.
DPI-стенды временно выбирают стандартный масштаб через Settings и восстанавливают
его в `finally`. Для запуска нужна интерактивная сессия Windows и открытая страница
Display Settings. Запускайте GUI-стенды последовательно.

Тестовые плитки и ползунки меняют локальную модель. Сетевые/VPN-провайдеры не
входят в визуальную работу. Меню батареи намеренно использует общий отступ от
рабочей области: привязка нативной панели к Y клика не переносится.

Сторонний ресурс шума сопровождается [лицензией Microsoft](source/ShellPanels/Noise-LICENSE.txt).
Артефакты сборки, SDK-копии, временные записи и архивы исключены из Git; они
воспроизводятся скриптами. Сохранённые материалы приёмки находятся в `evidence`.