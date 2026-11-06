#include <windows.h>
#include <dwmapi.h>
#include <ole2.h>
#include <UIAutomation.h>
#include <gdiplus.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <thread>
#include <atomic>
#include "../../source/NativeTray/NativeMenuStyle.h"
#include "ReferenceMenuData.h"

namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace media=x::Media;
HWND host=nullptr,island=nullptr;
x::Hosting::WindowsXamlManager manager{nullptr};
x::Hosting::DesktopWindowXamlSource source{nullptr};
c::Grid root{nullptr};c::MenuFlyout menu{nullptr};
std::filesystem::path folder;
int selected=0,step=0,measureStage=0;
bool themeLight=true,copied=false,opened=false,dump=false;
unsigned menuGeneration=0;
std::thread originalWorker;
std::atomic<bool> originalBusy=false;
double textScale=1;
constexpr UINT_PTR MeasureTimer=1,NextTimer=2;

std::string Utf8(std::wstring_view value){
    if(value.empty())return {};
    auto length=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(static_cast<size_t>(length),'\0');
    WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),length,nullptr,nullptr);return result;
}
std::string Json(std::wstring_view value){
    std::string result="\"";for(char ch:Utf8(value)){if(ch=='"'||ch=='\\')result+='\\';if(ch=='\n')result+="\\n";else if(ch=='\r')result+="\\r";else result+=ch;}return result+'"';
}
void Status(winrt::hstring const& value){if(root)root.FindName(L"Status").as<c::TextBlock>().Text(value);}
void Error(winrt::hresult_error const& error){
    std::ofstream log(folder/L"errors.log",std::ios::app);log<<std::hex<<static_cast<unsigned long>(error.code().value)<<' '<<Utf8(error.message())<<'\n';
    Status(L"Ошибка: "+error.message());
}
template<class T>T Find(x::DependencyObject const& node){
    if(auto value=node.try_as<T>())return value;
    for(int i=0;i<media::VisualTreeHelper::GetChildrenCount(node);++i)
        if(auto value=Find<T>(media::VisualTreeHelper::GetChild(node,i)))return value;
    return nullptr;
}
c::MenuFlyoutPresenter Presenter(){
    if(!root||!root.XamlRoot())return nullptr;
    for(auto const& popup:media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root.XamlRoot()))
        if(auto child=popup.Child())if(auto found=Find<c::MenuFlyoutPresenter>(child))return found;
    return nullptr;
}
void Set(x::Style const& style,x::DependencyProperty const& property,winrt::Windows::Foundation::IInspectable const& value){
    for(auto const& candidate:style.Setters())if(auto setter=candidate.try_as<x::Setter>();setter&&setter.Property()==property){setter.Value(value);return;}
    x::Setter setter;setter.Property(property);setter.Value(value);style.Setters().Append(setter);
}
void Resize(){
    if(!island||!root)return;RECT r{};GetClientRect(host,&r);
    SetWindowPos(island,nullptr,0,0,r.right,r.bottom,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    const auto scale=GetDpiForWindow(host)/96.0;root.Width(r.right/scale);root.Height(r.bottom/scale);
}
void Reference(){
    auto const& ref=References[static_cast<size_t>(selected)];
    auto path=(folder/L"reference"/ref.screenshot).generic_wstring();
    auto bitmap=media::Imaging::BitmapImage(winrt::Windows::Foundation::Uri(L"file:///"+path));
    auto image=root.FindName(L"ReferenceImage").as<c::Image>();image.Source(bitmap);
    image.Width(ref.imageWidth/(GetDpiForWindow(host)/96.0));
    root.FindName(L"ReferenceMetrics").as<c::TextBlock>().Text(
        L"Оригинал: "+winrt::to_hstring(static_cast<int>(std::lround(ref.width*ref.dpi/96.0)))+L" × "+
        winrt::to_hstring(static_cast<int>(std::lround(ref.height*ref.dpi/96.0)))+L" px, DPI "+winrt::to_hstring(ref.dpi));
}
void OpenSystemMenu(std::wstring const& prefix){
    winrt::com_ptr<IUIAutomation> automation;
    winrt::check_hresult(CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IUIAutomation),automation.put_void()));
    winrt::com_ptr<IUIAutomationElement> taskbar;winrt::check_hresult(automation->ElementFromHandle(FindWindowW(L"Shell_TrayWnd",nullptr),taskbar.put()));
    VARIANT value{};value.vt=VT_I4;value.lVal=UIA_ButtonControlTypeId;
    winrt::com_ptr<IUIAutomationCondition> condition;winrt::check_hresult(automation->CreatePropertyCondition(UIA_ControlTypePropertyId,value,condition.put()));
    winrt::com_ptr<IUIAutomationElementArray> buttons;winrt::check_hresult(taskbar->FindAll(TreeScope_Descendants,condition.get(),buttons.put()));
    int count=0;winrt::check_hresult(buttons->get_Length(&count));
    for(int i=0;i<count;++i){
        winrt::com_ptr<IUIAutomationElement> button;winrt::check_hresult(buttons->GetElement(i,button.put()));
        BSTR text=nullptr;winrt::check_hresult(button->get_CurrentName(&text));std::wstring name(text?text:L"");SysFreeString(text);
        if(name.rfind(prefix,0)!=0)continue;
        RECT r{};winrt::check_hresult(button->get_CurrentBoundingRectangle(&r));
        SetCursorPos((r.left+r.right)/2,(r.top+r.bottom)/2);
        INPUT input[2]{};input[0].type=INPUT_MOUSE;input[0].mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;
        input[1].type=INPUT_MOUSE;input[1].mi.dwFlags=MOUSEEVENTF_RIGHTUP;
        winrt::check_bool(SendInput(2,input,sizeof(INPUT))==2);return;
    }
    winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
}
void Original(){
    if(menu)menu.Hide();
    if(originalBusy.exchange(true))return;
    if(originalWorker.joinable())originalWorker.join();
    auto prefix=References[static_cast<size_t>(selected)].prefix;
    originalWorker=std::thread([prefix]{
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto text=new std::wstring(L"Открыто настоящее системное меню.");
        try{OpenSystemMenu(prefix);}catch(winrt::hresult_error const& error){*text=L"Не удалось открыть оригинал: "+std::wstring(error.message());}
        if(!PostMessageW(host,WM_APP+1,0,reinterpret_cast<LPARAM>(text)))delete text;
        originalBusy=false;winrt::uninit_apartment();
    });
}
CLSID PngEncoder(){
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> data(size);
    auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(data.data());Gdiplus::GetImageEncoders(count,size,codecs);
    for(UINT i=0;i<count;++i)if(!wcscmp(codecs[i].MimeType,L"image/png"))return codecs[i].Clsid;
    winrt::throw_hresult(E_FAIL);
}
BOOL CALLBACK CapturePopup(HWND window,LPARAM name){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||!IsWindowVisible(window))return TRUE;
    wchar_t type[100]{};GetClassNameW(window,type,100);if(wcscmp(type,L"Xaml_WindowedPopupClass"))return TRUE;
    RECT r{};GetWindowRect(window,&r);if(r.right-r.left<2||r.bottom-r.top<2)return TRUE;
    HDC screen=GetDC(nullptr);Gdiplus::Bitmap bitmap(r.right-r.left,r.bottom-r.top,PixelFormat24bppRGB);
    {Gdiplus::Graphics graphics(&bitmap);auto dc=graphics.GetHDC();BitBlt(dc,0,0,r.right-r.left,r.bottom-r.top,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);graphics.ReleaseHDC(dc);}
    ReleaseDC(nullptr,screen);auto codec=PngEncoder();
    winrt::check_hresult(HRESULT_FROM_WIN32(bitmap.Save(reinterpret_cast<wchar_t const*>(name),&codec,nullptr)==Gdiplus::Ok?ERROR_SUCCESS:ERROR_WRITE_FAULT));return FALSE;
}
void Show(bool useCopy){
    auto generation=++menuGeneration;
    if(menu)menu.Hide();copied=useCopy;opened=false;measureStage=0;
    auto const& ref=References[static_cast<size_t>(selected)];menu=c::MenuFlyout();
    menu.AreOpenCloseAnimationsEnabled(!dump);menu.ShouldConstrainToRootBounds(false);
    auto style=TrayMenuStyle::Presenter(themeLight);
    if(copied){
        x::Style overrides;overrides.TargetType(style.TargetType());overrides.BasedOn(style);style=overrides;
        Set(style,x::FrameworkElement::WidthProperty(),winrt::box_value(ref.width*textScale));
        media::AcrylicBrush brush;brush.BackgroundSource(media::AcrylicBackgroundSource::HostBackdrop);
        brush.TintColor(themeLight?winrt::Windows::UI::Color{255,252,252,252}:winrt::Windows::UI::Color{255,44,44,44});
        brush.TintOpacity(themeLight?0.0:0.15);brush.TintLuminosityOpacity(winrt::box_value(themeLight?0.85:0.96).as<winrt::Windows::Foundation::IReference<double>>());
        brush.FallbackColor(themeLight?winrt::Windows::UI::Color{255,249,249,249}:winrt::Windows::UI::Color{255,44,44,44});
        Set(style,c::Control::BackgroundProperty(),brush);
    }
    menu.MenuFlyoutPresenterStyle(style);
    for(auto const& row:ref.rows){
        if(row.separatorBefore)menu.Items().Append(TrayMenuStyle::Separator(themeLight));
        c::MenuFlyoutItem item;item.Text(row.text);TrayMenuStyle::Item(item);
        if(copied){item.MinHeight(0);item.Padding({10,3,10,3});item.Resources().Insert(winrt::box_value(L"MenuFlyoutItemThemePaddingNarrow"),winrt::box_value(item.Padding()));}
        item.FontSize(14*textScale);item.RequestedTheme(themeLight?x::ElementTheme::Light:x::ElementTheme::Dark);
        if(row.glyph){c::FontIcon icon;icon.FontFamily(media::FontFamily(L"Segoe Fluent Icons"));icon.FontSize(16);icon.Glyph(winrt::hstring(std::wstring(1,row.glyph)));item.Icon(icon);}
        item.Click([](auto const& sender,auto const&) noexcept {try{Status(L"Тестовая команда: "+sender.template as<c::MenuFlyoutItem>().Text());}catch(...){}});
        menu.Items().Append(item);
    }
    menu.Opened([generation](auto const&,auto const&) noexcept {try{
        if(generation!=menuGeneration)return;
        opened=true;if(auto presenter=Presenter())presenter.RequestedTheme(themeLight?x::ElementTheme::Light:x::ElementTheme::Dark);
        SetTimer(host,MeasureTimer,180,nullptr);
    }catch(winrt::hresult_error const& error){Error(error);}});
    menu.Closed([generation](auto const&,auto const&) noexcept {if(generation==menuGeneration)opened=false;});
    c::Primitives::FlyoutShowOptions options;options.Placement(c::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
    menu.ShowAt(root.FindName(L"MenuAnchor").as<x::FrameworkElement>(),options);
}
void Measure(){
    if(!opened)return;auto presenter=Presenter();if(!presenter)return;
    if(measureStage++==0){
        if(dump){
            for(auto const& item:menu.Items())if(auto control=item.try_as<c::MenuFlyoutItem>())x::VisualStateManager::GoToState(control,L"NarrowPadding",false);
            menu.Items().GetAt(0).as<c::MenuFlyoutItem>().Focus(x::FocusState::Pointer);
        }
        presenter.UpdateLayout();SetTimer(host,MeasureTimer,100,nullptr);return;
    }
    KillTimer(host,MeasureTimer);
    auto const& ref=References[static_cast<size_t>(selected)];auto scale=presenter.XamlRoot().RasterizationScale();
    const auto width=static_cast<int>(std::lround(presenter.ActualWidth()*scale)),height=static_cast<int>(std::lround(presenter.ActualHeight()*scale));
    auto variant=copied?L"copy":L"current";auto filename=ref.key+L"-"+variant+(themeLight?L"-light.png":L"-dark.png");
    auto path=folder/L"captures"/filename;std::filesystem::create_directories(path.parent_path());EnumWindows(CapturePopup,reinterpret_cast<LPARAM>(path.c_str()));
    std::ofstream out(folder/L"measurements.jsonl",std::ios::app);out<<std::setprecision(10);
    out<<"{\"category\":"<<Json(ref.key)<<",\"variant\":"<<Json(variant)<<",\"theme\":"<<Json(themeLight?L"light":L"dark")<<",\"dpi\":"<<scale*96
       <<",\"fontScale\":"<<textScale<<",\"widthPx\":"<<width<<",\"heightPx\":"<<height<<",\"referenceHeightPx\":"<<std::lround(ref.height*scale)<<",\"file\":"<<Json(filename)<<",\"rows\":[";
    bool comma=false;for(auto const& candidate:menu.Items())if(auto item=candidate.try_as<c::MenuFlyoutItem>()){
        auto point=item.TransformToVisual(presenter).TransformPoint({0,0});if(comma)out<<',';comma=true;
        out<<"{\"text\":"<<Json(item.Text())<<",\"yDip\":"<<point.Y<<",\"heightDip\":"<<item.ActualHeight()<<"}";
    }out<<"]}\n";
    Status((copied?L"Образец Windows: ":L"Текущий стиль: ")+winrt::to_hstring(width)+L" × "+winrt::to_hstring(height)+L" px; оригинал: "+
        winrt::to_hstring(static_cast<int>(std::lround(ref.width*scale)))+L" × "+winrt::to_hstring(static_cast<int>(std::lround(ref.height*scale)))+L" px.");
    if(dump){menu.Hide();SetTimer(host,NextTimer,100,nullptr);}
}
void Next(){
    KillTimer(host,NextTimer);if(step==16){PostMessageW(host,WM_CLOSE,0,0);return;}
    selected=(step/2)%4;themeLight=step<8;root.RequestedTheme(themeLight?x::ElementTheme::Light:x::ElementTheme::Dark);
    root.FindName(L"Categories").as<c::ComboBox>().SelectedIndex(selected);Reference();Show((step++%2)!=0);
}
void Build(){
    root=x::Markup::XamlReader::Load(LR"XAML(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" Background="{ThemeResource SystemControlBackgroundAltHighBrush}">
 <ScrollViewer VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled"><StackPanel Margin="24" Spacing="18">
  <TextBlock Text="Сравнение контекстных меню Windows 11" FontSize="22" FontWeight="SemiBold" TextWrapping="Wrap"/>
  <TextBlock Text="Одинаковые подписи, значки и разделители. Сравнивайте меню на одном масштабе. Команды в копиях только показывают выбранный пункт." TextWrapping="Wrap"/>
  <StackPanel Orientation="Horizontal" Spacing="16">
   <ComboBox x:Name="Categories" Header="Панель" Width="200" SelectedIndex="0" AutomationProperties.AutomationId="Categories"><ComboBoxItem Content="Wi-Fi"/><ComboBoxItem Content="Звук"/><ComboBoxItem Content="Батарея"/><ComboBoxItem Content="Дата и время"/></ComboBox>
   <ComboBox x:Name="Theme" Header="Тема копий" Width="150" SelectedIndex="0"><ComboBoxItem Content="Светлая"/><ComboBoxItem Content="Тёмная"/></ComboBox>
   <ComboBox x:Name="TextSize" Header="Текст копий" Width="150" SelectedIndex="0"><ComboBoxItem Content="100%"/><ComboBoxItem Content="150%"/><ComboBoxItem Content="200%"/></ComboBox>
  </StackPanel>
  <StackPanel Orientation="Horizontal" Spacing="12">
   <Button x:Name="SystemButton" Content="Оригинал Windows" AutomationProperties.AutomationId="OpenOriginal"/>
   <Button x:Name="CurrentButton" Content="Текущий стиль проекта" AutomationProperties.AutomationId="OpenCurrent"/>
   <Button x:Name="CopyButton" Content="По образцу Windows" AutomationProperties.AutomationId="OpenCopy"/>
  </StackPanel>
  <Grid ColumnSpacing="28"><Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
   <StackPanel Spacing="10"><TextBlock Text="Снимок настоящего системного меню" FontWeight="SemiBold"/><TextBlock x:Name="ReferenceMetrics" TextWrapping="Wrap"/><Image x:Name="ReferenceImage" HorizontalAlignment="Left" Stretch="Uniform"/></StackPanel>
   <StackPanel Grid.Column="1" Spacing="12"><TextBlock Text="Обе тестовые копии открываются отсюда" FontWeight="SemiBold" TextWrapping="Wrap"/><Button x:Name="MenuAnchor" Content="ПКМ: по образцу Windows" Height="40" HorizontalAlignment="Left" AutomationProperties.AutomationId="MenuAnchor"/><TextBlock Text="Текущий стиль использует production-заголовок. В варианте по образцу убран минимум высоты и повторены размеры и светлый материал системных меню. Меню остаются настоящими XAML-контролами." TextWrapping="Wrap"/><TextBlock Text="Оригинал Windows открывается у значка на панели задач. Светлая/тёмная тема здесь применяется только к тестовым копиям." TextWrapping="Wrap" Opacity="0.7"/></StackPanel>
  </Grid>
  <Border BorderThickness="1" BorderBrush="{ThemeResource SystemControlForegroundBaseLowBrush}" CornerRadius="4" Padding="12"><TextBlock x:Name="Status" Text="Выберите меню и вариант для сравнения." TextWrapping="Wrap" AutomationProperties.AutomationId="CompareStatus"/></Border>
 </StackPanel></ScrollViewer>
</Grid>)XAML").as<c::Grid>();root.RequestedTheme(x::ElementTheme::Light);
    source.Content(root);
    root.FindName(L"Categories").as<c::ComboBox>().SelectionChanged([](auto const& sender,auto const&) noexcept {try{selected=sender.template as<c::ComboBox>().SelectedIndex();if(menu)menu.Hide();Reference();}catch(winrt::hresult_error const& error){Error(error);}});
    root.FindName(L"Theme").as<c::ComboBox>().SelectionChanged([](auto const& sender,auto const&) noexcept {try{if(menu)menu.Hide();themeLight=sender.template as<c::ComboBox>().SelectedIndex()==0;root.RequestedTheme(themeLight?x::ElementTheme::Light:x::ElementTheme::Dark);BOOL dark=!themeLight;DwmSetWindowAttribute(host,20,&dark,sizeof(dark));}catch(winrt::hresult_error const& error){Error(error);}});
    root.FindName(L"TextSize").as<c::ComboBox>().SelectionChanged([](auto const& sender,auto const&) noexcept {try{if(menu)menu.Hide();textScale=1+0.5*sender.template as<c::ComboBox>().SelectedIndex();}catch(winrt::hresult_error const& error){Error(error);}});
    root.FindName(L"SystemButton").as<c::Button>().Click([](auto const&,auto const&) noexcept {try{Original();}catch(winrt::hresult_error const& error){Error(error);}});
    root.FindName(L"CurrentButton").as<c::Button>().Click([](auto const&,auto const&) noexcept {try{Show(false);}catch(winrt::hresult_error const& error){Error(error);}});
    root.FindName(L"CopyButton").as<c::Button>().Click([](auto const&,auto const&) noexcept {try{Show(true);}catch(winrt::hresult_error const& error){Error(error);}});
    root.FindName(L"MenuAnchor").as<c::Button>().RightTapped([](auto const&,auto const& args) noexcept {try{Show(true);args.Handled(true);}catch(winrt::hresult_error const& error){Error(error);}});
    Reference();Resize();
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
    try{
        if(message==WM_SIZE){Resize();return 0;}
        if(message==WM_DPICHANGED){auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(window,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);Resize();Reference();return 0;}
        if(message==WM_TIMER){if(wp==MeasureTimer)Measure();if(wp==NextTimer)Next();return 0;}
        if(message==WM_APP+1){auto text=reinterpret_cast<std::wstring*>(lp);Status(winrt::hstring(*text));delete text;return 0;}
        if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    }catch(winrt::hresult_error const& error){Error(error);if(dump)PostMessageW(window,WM_CLOSE,0,0);}
    return DefWindowProcW(window,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR arguments,int){
    ULONG_PTR token=0;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);int result=0;
    try{
        wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);folder=std::filesystem::path(path).parent_path();dump=std::wstring_view(arguments).find(L"--dump-all")!=std::wstring_view::npos;
        winrt::init_apartment(winrt::apartment_type::single_threaded);manager=x::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WindowProc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"Yata.MenuCompare";RegisterClassW(&wc);
        const auto scale=GetDpiForSystem()/96.0;
        host=CreateWindowExW(0,wc.lpszClassName,L"Yata — сравнение меню Windows 11 (тест)",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,80,80,static_cast<int>(960*scale),static_cast<int>(660*scale),nullptr,nullptr,instance,nullptr);winrt::check_bool(host!=nullptr);
        source=x::Hosting::DesktopWindowXamlSource();auto interop=source.as<IDesktopWindowXamlSourceNative>();winrt::check_hresult(interop->AttachToWindow(host));winrt::check_hresult(interop->get_WindowHandle(&island));
        Build();ShowWindow(host,SW_SHOW);SetForegroundWindow(host);SetFocus(island);
        if(dump){std::ofstream clear(folder/L"measurements.jsonl",std::ios::trunc);std::ofstream errors(folder/L"errors.log",std::ios::trunc);SetTimer(host,NextTimer,500,nullptr);}
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){BOOL handled=FALSE;winrt::check_hresult(source.as<IDesktopWindowXamlSourceNative2>()->PreTranslateMessage(&message,&handled));if(!handled){TranslateMessage(&message);DispatchMessageW(&message);}}
        if(originalWorker.joinable())originalWorker.join();
        if(menu)menu.Hide();menu=nullptr;root=nullptr;source.Content(nullptr);source.Close();source=nullptr;manager.Close();manager=nullptr;winrt::uninit_apartment();
    }catch(winrt::hresult_error const& error){MessageBoxW(host,error.message().c_str(),L"Menu compare",MB_OK|MB_ICONERROR);result=1;}
    catch(std::exception const& error){auto text=winrt::to_hstring(error.what());MessageBoxW(host,text.c_str(),L"Menu compare",MB_OK|MB_ICONERROR);result=1;}
    Gdiplus::GdiplusShutdown(token);return result;
}
