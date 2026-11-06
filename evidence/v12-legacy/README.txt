Yata VPN — нативный прототип трея, визуальная версия приложения v11 (модуль Explorer v10)

Запуск: YataVpnTrayStub.exe --native-speed
Применить модуль к Explorer: Apply-VisualFix.cmd
Проверить: Check-Status.cmd (для двух строк должно быть mode=2 applied=2).
Скрипт загружает YataVpnNativeTray.v10.dll без перезапуска Explorer.
Прежние модули остаются неактивными до выхода из Explorer.

V11: фон, текст и рамка подсказки берутся из живых ресурсов оболочки:
ToolTipBackgroundBrush, ToolTipForegroundBrush, ToolTipBorderBrush.
Палитра учитывает тему владельца и High Contrast; сбой чтения палитры
не сбрасывает оформление кнопки. В Explorer подтверждены все три кисти.
На одинаковом фоне наша подсказка совпала с Wi-Fi/аудио/батареей и часами
с максимальным расхождением менее 0.5 уровня RGB по измеренной полосе фона.
Повторно пройдены 12 наведений, проверки панели и серий кликов.
Материал при закрытии проверен на текущей светлой теме: через повторный клик
и через внешний клик потемнение не воспроизведено. Код материала не изменялся.
Подробности и пределы проверки: MATERIAL-V11-VERIFICATION.txt.

V10: подсказка привязана к кнопке и располагается выше панели задач.
Проверены 12 точек входа: сверху/снизу, слева/по центру/справа в режимах
44 и 88 DIP. Координаты не зависят от указателя, ширина кнопки сохраняется.
Ошибка подсказки изолирована от оформления трея и не вызывает его сброс.
Изолированный тест открывает настоящее XAML-окно подсказки при 44/88/160 DIP
и проверяет его экранные границы; одного сравнения настроек недостаточно.
Результаты: HOVER-V10-VERIFICATION.txt, hover-matrix-v10-results.txt.
Текущий статус цветов и материала приведён выше в разделе V11.

Левый клик открывает панель, повторный закрывает. Открытая панель удерживает
выбранный фон кнопки. После закрытия исходные фон и цвет текста восстанавливаются
без принудительного GoToState("Normal"). Цвета selected/hover/pressed берутся
из ресурсов Explorer; резервные значения извлечены из установленного XBF.

Панель анимируется Windows Composition внутри неподвижного прозрачного XAML Island.
Окно полностью выше трея; содержимое и тень обрезаны его границами.
Вход 250 мс, выход 167 мс. Учитывается системное отключение анимаций.
Кнопка закрытия: мышь, Space, Esc и UI Automation.
Контекстное меню использует системный MenuFlyout.

Типографика берётся у живых часов. Здесь это Segoe UI Variable, 12 DIP.
Цифры используют Typography.NumeralAlignment=Tabular; полный моноширинный
шрифт не нужен. Направление, число и единица имеют отдельные колонки.

Правый клик: режим и ширина. Точное значение: --native-speed --native-width=88
Допустимо 32..320 DIP, 0 — автоматически. Настройки VPN и скорости раздельные:
%TEMP%\YataVpnNativeTray.layout.ini. При обновлении сохранены 88 DIP для скорости.
Скорость 100 Мбит/с и 100 Кбит/с — демонстрационные значения.

Сборка: powershell -ExecutionPolicy Bypass -File source\NativeTray\Build.ps1 -SelfTest
Требуются Visual Studio C++ x64 и Windows SDK 10.0.26100.0.
Проверки: selftest-results.txt, visual-v5-results.txt, VISUAL-FIX-PLAN.txt.
Изолированные проверки не подтверждают загрузку в Explorer: нужны applied=2
в Check-Status.cmd и отдельная проверка реальных кликов.

Документация прозрачности XAML Island:
https://learn.microsoft.com/en-us/windows/apps/api-reference/interface-members/ixamlsourcetransparency-isbackgroundtransparent

V6: Quick Settings curves, transition lock, selected chrome through closing.
See MOTION-V6.txt and VISUAL-FIX-PLAN.txt. Context menu and panel placement updated in v7.

V7: monitor-aware panel width, centered placement, 12 DIP margins, vertical scrolling, rounded native context menus and neutral item states.
V11 status: tooltip palette fixed; closing material stable in the tested light-theme scenarios.
