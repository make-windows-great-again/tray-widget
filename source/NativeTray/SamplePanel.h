#pragma once
namespace TraySample { inline winrt::Windows::UI::Xaml::UIElement Create(){ return winrt::Windows::UI::Xaml::Markup::XamlReader::Load(LR"XAML(<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"> <Grid.Resources>
  <ResourceDictionary>
   <ResourceDictionary.ThemeDictionaries>
    <ResourceDictionary x:Key="Light">
     <SolidColorBrush x:Key="MetricCardFill" Color="#B3FFFFFF"/>
     <SolidColorBrush x:Key="MetricCardStroke" Color="#0F000000"/>
    </ResourceDictionary>
    <ResourceDictionary x:Key="Default">
     <SolidColorBrush x:Key="MetricCardFill" Color="#0DFFFFFF"/>
     <SolidColorBrush x:Key="MetricCardStroke" Color="#12FFFFFF"/>
    </ResourceDictionary>
    <ResourceDictionary x:Key="HighContrast">
     <SolidColorBrush x:Key="MetricCardFill" Color="{ThemeResource SystemColorWindowColor}"/>
     <SolidColorBrush x:Key="MetricCardStroke" Color="{ThemeResource SystemColorWindowTextColor}"/>
    </ResourceDictionary>
   </ResourceDictionary.ThemeDictionaries>
  </ResourceDictionary>
 </Grid.Resources><Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/><RowDefinition Height="Auto"/></Grid.RowDefinitions> <TextBlock Grid.Row="0" Text="Исходящий и входящий трафик" Margin="0,4,0,20"
            Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource SystemControlForegroundBaseMediumBrush}"/>
 <Border Grid.Row="1" CornerRadius="4" BorderThickness="1" Padding="16,12" Margin="0,0,0,8"
         BorderBrush="{ThemeResource MetricCardStroke}" Background="{ThemeResource MetricCardFill}">
  <Grid ColumnSpacing="16">
   <Grid.ColumnDefinitions><ColumnDefinition Width="20"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
   <FontIcon FontFamily="Segoe Fluent Icons" Glyph="&#xE74A;" FontSize="20" VerticalAlignment="Center" AutomationProperties.AccessibilityView="Raw"/>
   <StackPanel Grid.Column="1">
    <TextBlock Text="Отправка" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource SystemControlForegroundBaseMediumBrush}"/>
    <Grid ColumnSpacing="8" Margin="0,2,0,0">
     <Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
     <TextBlock Text="100" FontSize="28" FontWeight="SemiBold" Typography.NumeralAlignment="Tabular"
                AutomationProperties.Name="Отправка: 100 мегабит в секунду" AutomationProperties.AutomationId="UploadValue"/>
     <TextBlock Grid.Column="1" Text="Мбит/с" FontSize="14" VerticalAlignment="Bottom" Margin="0,0,0,4" Foreground="{ThemeResource SystemControlForegroundBaseMediumBrush}"/>
    </Grid>
   </StackPanel>
  </Grid>
 </Border>
 <Border Grid.Row="2" CornerRadius="4" BorderThickness="1" Padding="16,12"
         BorderBrush="{ThemeResource MetricCardStroke}" Background="{ThemeResource MetricCardFill}">
  <Grid ColumnSpacing="16">
   <Grid.ColumnDefinitions><ColumnDefinition Width="20"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
   <FontIcon FontFamily="Segoe Fluent Icons" Glyph="&#xE74B;" FontSize="20" VerticalAlignment="Center" AutomationProperties.AccessibilityView="Raw"/>
   <StackPanel Grid.Column="1">
    <TextBlock Text="Получение" Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource SystemControlForegroundBaseMediumBrush}"/>
    <Grid ColumnSpacing="8" Margin="0,2,0,0">
     <Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
     <TextBlock Text="100" FontSize="28" FontWeight="SemiBold" Typography.NumeralAlignment="Tabular"
                AutomationProperties.Name="Получение: 100 килобит в секунду" AutomationProperties.AutomationId="DownloadValue"/>
     <TextBlock Grid.Column="1" Text="Кбит/с" FontSize="14" VerticalAlignment="Bottom" Margin="0,0,0,4" Foreground="{ThemeResource SystemControlForegroundBaseMediumBrush}"/>
    </Grid>
   </StackPanel>
  </Grid>
 </Border>
 <TextBlock Grid.Row="3" Text="Демонстрационные значения" Margin="0,16,0,0"
            Style="{StaticResource CaptionTextBlockStyle}" Foreground="{ThemeResource SystemControlForegroundBaseMediumBrush}"/>
</Grid>)XAML").as<winrt::Windows::UI::Xaml::UIElement>();} }