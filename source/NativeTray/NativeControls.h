#pragma once
#include <memory>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include "NativePageStrip.h"

// A sample client of NativeFlyout, not part of the shell patch. Every widget is
// a real OS control with its own input handling and AutomationPeer.
namespace NativeControls {
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
struct State {
    winrt::weak_ref<c::Grid> root;
    winrt::weak_ref<c::TextBlock> status;
    c::ContentDialog dialog{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<c::ContentDialogResult> pending{nullptr};
    unsigned actions=0;
    void Record(winrt::hstring const& text) noexcept {
        try{if(auto output=status.get())output.Text(text+L" · "+winrt::to_hstring(++actions));}catch(...){}
    }
    void Dismiss() noexcept {try{if(dialog)dialog.Hide();}catch(...){} }
    ~State(){Dismiss();}
    void ShowDialog(std::shared_ptr<State> const& self) noexcept {
        try{
            if(pending)return;
            auto owner=root.get();if(!owner || !owner.XamlRoot())return;
            dialog=c::ContentDialog();dialog.XamlRoot(owner.XamlRoot());
            dialog.Title(winrt::box_value(L"Системный диалог"));
            dialog.Content(winrt::box_value(L"Диалог поддерживает клавиатуру, фокус и UI Automation."));
            dialog.PrimaryButtonText(L"Применить");dialog.CloseButtonText(L"Отмена");
            dialog.DefaultButton(c::ContentDialogButton::Primary);
            pending=dialog.ShowAsync(c::ContentDialogPlacement::InPlace);
            std::weak_ptr<State> weak=self;
            pending.Completed([weak](auto const& operation,winrt::Windows::Foundation::AsyncStatus status) noexcept {
                try{if(auto state=weak.lock()){
                    if(status==winrt::Windows::Foundation::AsyncStatus::Completed)
                        state->Record(operation.GetResults()==c::ContentDialogResult::Primary ? L"Изменения применены" : L"Диалог закрыт");
                    state->pending=nullptr;state->dialog=nullptr;
                }}catch(...){}
            });
        }catch(...){pending=nullptr;dialog=nullptr;Record(L"Диалог недоступен");}
    }
};
inline c::Grid Create(){
    auto root=x::Markup::XamlReader::Load(LR"XAML(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" RowSpacing="12">
 <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/></Grid.RowDefinitions>
 <TextBlock Text="Элементы управления Windows" Style="{StaticResource CaptionTextBlockStyle}" TextWrapping="Wrap"/>
 <Grid Grid.Row="1" Height="440" Margin="-12,0,-12,0">
  <Grid.RowDefinitions><RowDefinition Height="48"/><RowDefinition Height="*"/></Grid.RowDefinitions>
 <Pivot x:Name="ControlPages" Height="48" IsHeaderItemsCarouselEnabled="False" AutomationProperties.Name="Каталог элементов управления">
  <PivotItem Header="Кнопки">
   <ScrollViewer VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled">
    <StackPanel Spacing="16" Margin="0,8,8,12">
     <StackPanel Orientation="Horizontal" Spacing="8">
      <Button x:Name="ActionButton" Content="Применить" AutomationProperties.AutomationId="ActionButton"/>
      <Button Content="Недоступно" IsEnabled="False"/>
      <ToggleButton x:Name="ToggleAction" Content="Закрепить"/>
     </StackPanel>
     <RepeatButton x:Name="RepeatAction" Content="Удерживайте для повтора" HorizontalAlignment="Left"/>
     <HyperlinkButton x:Name="LinkAction" Content="Действие как ссылка"/>
     <ToggleSwitch x:Name="SwitchAction" Header="Уведомления" OnContent="Включены" OffContent="Выключены"/>
     <CheckBox x:Name="CheckAction" Content="Запоминать выбор" IsThreeState="True"/>
     <StackPanel><RadioButton Content="Автоматически" GroupName="Choice" IsChecked="True"/><RadioButton Content="Вручную" GroupName="Choice"/></StackPanel>
     <Button Content="Подсказка" HorizontalAlignment="Left" ToolTipService.ToolTip="Нативная подсказка Windows"/>
    </StackPanel>
   </ScrollViewer>
  </PivotItem>
  <PivotItem Header="Ввод">
   <ScrollViewer VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled">
    <StackPanel Spacing="14" Margin="0,8,8,12">
     <TextBox x:Name="TextInput" Header="Название" PlaceholderText="Введите текст" AutomationProperties.AutomationId="TextInput"/>
     <PasswordBox Header="Пароль (пример)" PlaceholderText="Не сохраняется"/>
     <AutoSuggestBox x:Name="SearchInput" Header="Поиск" PlaceholderText="Найти элемент" QueryIcon="Find"/>
     <TextBox Header="Описание" AcceptsReturn="True" TextWrapping="Wrap" Height="72"/>
     <Slider x:Name="LevelInput" Header="Уровень" Minimum="0" Maximum="100" Value="40" StepFrequency="1" AutomationProperties.AutomationId="LevelInput"/>
     <ProgressBar x:Name="LevelProgress" Minimum="0" Maximum="100" Value="40" AutomationProperties.Name="Выбранный уровень"/>
    </StackPanel>
   </ScrollViewer>
  </PivotItem>
  <PivotItem Header="Выбор">
   <ScrollViewer VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled">
    <StackPanel Spacing="16" Margin="0,8,8,12">
     <ComboBox x:Name="ComboInput" Header="Режим" SelectedIndex="0" HorizontalAlignment="Stretch"><ComboBoxItem Content="Автоматический"/><ComboBoxItem Content="Компактный"/><ComboBoxItem Content="Расширенный"/></ComboBox>
     <Border x:Name="ListInputSurface" Background="{ThemeResource SystemControlBackgroundChromeMediumLowBrush}" BorderBrush="{ThemeResource SystemControlForegroundBaseLowBrush}" BorderThickness="1" CornerRadius="4" Padding="4">
      <ListBox x:Name="ListInput" Background="Transparent" BorderThickness="0" SelectionMode="Single" ScrollViewer.VerticalScrollMode="Disabled" ScrollViewer.VerticalScrollBarVisibility="Disabled" AutomationProperties.Name="Список вариантов">
       <ListBox.ItemsPanel><ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate></ListBox.ItemsPanel>
       <ListBox.ItemContainerStyle><Style TargetType="ListBoxItem"><Setter Property="MinHeight" Value="32"/><Setter Property="Padding" Value="12,6,12,6"/></Style></ListBox.ItemContainerStyle>
       <ListBoxItem Content="Первый"/><ListBoxItem Content="Второй"/><ListBoxItem Content="Третий"/>
      </ListBox>
     </Border>
     <ListView SelectionMode="Multiple" Height="96" AutomationProperties.Name="Множественный выбор"><ListViewItem Content="Показывать подписи"/><ListViewItem Content="Показывать значки"/></ListView>
     <GridView SelectionMode="Single" Height="72" AutomationProperties.Name="Плитки"><GridViewItem Content="А"/><GridViewItem Content="Б"/><GridViewItem Content="В"/></GridView>
    </StackPanel>
   </ScrollViewer>
  </PivotItem>
  <PivotItem Header="Дата">
   <ScrollViewer VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled">
    <StackPanel Spacing="16" Margin="0,8,8,12">
     <CalendarDatePicker Header="Дата" PlaceholderText="Выберите дату" HorizontalAlignment="Stretch"/>
     <DatePicker Header="Дата по компонентам" HorizontalAlignment="Stretch"/>
     <TimePicker Header="Время" ClockIdentifier="24HourClock"/>
     <ProgressRing IsActive="True" Width="32" Height="32" HorizontalAlignment="Left" AutomationProperties.Name="Пример индикатора ожидания"/>
     <TextBlock Text="Календарь и выбор времени используют штатные всплывающие окна." TextWrapping="Wrap"/>
    </StackPanel>
   </ScrollViewer>
  </PivotItem>
  <PivotItem Header="Меню">
   <StackPanel Spacing="16" Margin="0,8,8,12">
    <Button Content="Всплывающее меню" HorizontalAlignment="Left">
     <Button.Flyout><MenuFlyout><MenuFlyoutItem x:Name="MenuAction" Text="Применить"/><ToggleMenuFlyoutItem Text="Отображать подписи" IsChecked="True"/><MenuFlyoutSeparator/><MenuFlyoutSubItem Text="Дополнительно"><MenuFlyoutItem Text="Первый вариант"/><MenuFlyoutItem Text="Второй вариант"/></MenuFlyoutSubItem></MenuFlyout></Button.Flyout>
    </Button>
    <Button Content="Всплывающая панель" HorizontalAlignment="Left"><Button.Flyout><Flyout><StackPanel Spacing="12"><TextBlock Text="Содержимое Flyout"/><ToggleSwitch Header="Параметр"/></StackPanel></Flyout></Button.Flyout></Button>
    <Button x:Name="DialogAction" Content="Открыть диалог" HorizontalAlignment="Left" AutomationProperties.AutomationId="DialogAction"/>
    <CommandBar DefaultLabelPosition="Right" OverflowButtonVisibility="Visible"><AppBarButton x:Name="CommandAction" Icon="Accept" Label="Применить"/><AppBarSeparator/><AppBarToggleButton Icon="Pin" Label="Закрепить"/><CommandBar.SecondaryCommands><AppBarButton Icon="Setting" Label="Параметры"/></CommandBar.SecondaryCommands></CommandBar>
   </StackPanel>
  </PivotItem>
  <PivotItem Header="Навигация">
   <NavigationView x:Name="NavigationInput" PaneDisplayMode="LeftCompact" IsPaneOpen="False" IsBackButtonVisible="Collapsed" IsSettingsVisible="False" Height="340" AutomationProperties.Name="Пример навигации">
    <NavigationView.MenuItems><NavigationViewItem Content="Главная" Icon="Home"/><NavigationViewItem Content="Избранное" Icon="Favorite"/><NavigationViewItemSeparator/><NavigationViewItem Content="Параметры" Icon="Setting"/></NavigationView.MenuItems>
    <StackPanel Spacing="12" Margin="16"><TextBlock Text="Область содержимого" FontWeight="SemiBold"/><TextBlock Text="Панель навигации, клавиши и выбор обрабатывает Windows." TextWrapping="Wrap"/></StackPanel>
   </NavigationView>
  </PivotItem>
 </Pivot>
 <ScrollViewer x:Name="ControlPagesViewport" Grid.Row="1" Background="Transparent" BringIntoViewOnFocusChange="False" HorizontalScrollMode="Enabled" HorizontalScrollBarVisibility="Hidden" VerticalScrollMode="Disabled" VerticalScrollBarVisibility="Disabled" ZoomMode="Disabled" HorizontalSnapPointsType="Mandatory" HorizontalSnapPointsAlignment="Near" IsHorizontalScrollChainingEnabled="False" IsTabStop="False" AutomationProperties.Name="Страницы элементов управления">
  <StackPanel x:Name="ControlPageStrip" Orientation="Horizontal" AreScrollSnapPointsRegular="True"/>
 </ScrollViewer>
 </Grid>
 <TextBlock x:Name="ActionStatus" Grid.Row="2" Text="Готово" TextWrapping="Wrap" Style="{StaticResource CaptionTextBlockStyle}" AutomationProperties.LiveSetting="Polite" AutomationProperties.AutomationId="ActionStatus"/>
</Grid>)XAML").as<c::Grid>();
    PageStrip::Attach(root,root.FindName(L"ControlPages").as<c::Pivot>(),root.FindName(L"ControlPagesViewport").as<c::ScrollViewer>(),root.FindName(L"ControlPageStrip").as<c::StackPanel>());
    auto state=std::make_shared<State>();state->root=root;state->status=root.FindName(L"ActionStatus").as<c::TextBlock>();
    auto click=[state](auto const&,auto const&) noexcept {state->Record(L"Действие выполнено");};
    root.FindName(L"ActionButton").as<c::Button>().Click(click);
    root.FindName(L"RepeatAction").as<c::Primitives::RepeatButton>().Click(click);
    root.FindName(L"LinkAction").as<c::HyperlinkButton>().Click(click);
    root.FindName(L"CommandAction").as<c::AppBarButton>().Click(click);
    root.FindName(L"MenuAction").as<c::MenuFlyoutItem>().Click(click);
    root.FindName(L"ToggleAction").as<c::Primitives::ToggleButton>().Checked(click);
    root.FindName(L"ToggleAction").as<c::Primitives::ToggleButton>().Unchecked(click);
    root.FindName(L"SwitchAction").as<c::ToggleSwitch>().Toggled(click);
    root.FindName(L"CheckAction").as<c::CheckBox>().Checked(click);
    root.FindName(L"CheckAction").as<c::CheckBox>().Unchecked(click);
    root.FindName(L"TextInput").as<c::TextBox>().TextChanged(click);
    root.FindName(L"ComboInput").as<c::ComboBox>().SelectionChanged(click);
    root.FindName(L"ListInput").as<c::ListBox>().SelectionChanged(click);
    auto progress=winrt::make_weak(root.FindName(L"LevelProgress").as<c::ProgressBar>());
    root.FindName(L"LevelInput").as<c::Slider>().ValueChanged([state,progress](auto const&,auto const& args) noexcept {
        try{if(auto bar=progress.get())bar.Value(args.NewValue());state->Record(L"Уровень изменён");}catch(...){}
    });
    root.FindName(L"DialogAction").as<c::Button>().Click([state](auto const&,auto const&) noexcept {state->ShowDialog(state);});
    root.Unloaded([state](auto const&,auto const&) noexcept {state->Dismiss();});
    return root;
}
}
