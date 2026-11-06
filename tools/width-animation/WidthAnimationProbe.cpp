#define NATIVE_FLYOUT_SELFTEST
#include "../../source/NativeTray/NativeFlyout.h"
#include <dwmapi.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#include <iomanip>

namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
NativeFlyout* active=nullptr;
LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b) noexcept {
    if(m==WM_PAINT){PAINTSTRUCT p{};BeginPaint(w,&p);EndPaint(w,&p);return 0;}
    if(m==WM_ERASEBKGND)return 1;return DefWindowProcW(w,m,a,b);
}
void Pump(DWORD duration){auto end=GetTickCount64()+duration;do{MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE))if(!active||!active->Process(m)){TranslateMessage(&m);DispatchMessageW(&m);}auto now=GetTickCount64();if(now<end)MsgWaitForMultipleObjectsEx(0,nullptr,static_cast<DWORD>(std::min<ULONGLONG>(16,end-now)),QS_ALLINPUT,MWMO_INPUTAVAILABLE);}while(GetTickCount64()<end);}
struct Frame{double ms,wall,width,neighbor;};
struct Result{std::string phase;double callbackHz=0,maxGapMs=0,width=0,neighbor=0,hitWidth=0;unsigned callbacks=0,sizeChanges=0;};
int wmain(int argc,wchar_t** argv){
    if(argc<2)return 2;
    HWND host=nullptr;
    try{
        std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=Proc;wc.lpszClassName=L"Yata.WidthAnimationProbe";RegisterClassW(&wc);
        host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"Width animation research",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);winrt::check_bool(host!=nullptr);
        NativeFlyout flyout;active=&flyout;flyout.Initialize(host,[](bool){},[](UINT){});flyout.AnimationsForTest(false);
        auto grid=x::Markup::XamlReader::Load(LR"(<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><StackPanel Spacing="16"><TextBlock Text="Проверка реальной ширины и соседнего элемента" TextWrapping="Wrap"/><StackPanel x:Name="Row" Orientation="Horizontal" Spacing="8"><Button x:Name="Target" Content="UI" Width="88" MinWidth="0" Height="48"/><Button x:Name="Neighbor" Content="Соседний значок" Width="160" Height="48"/></StackPanel></StackPanel></Grid>)").as<c::Grid>();
        flyout.SetContent(grid,L"Анимация ширины — изолированный стенд",600);flyout.Theme(true);
        MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);winrt::check_bool(GetMonitorInfoW(MonitorFromWindow(host,MONITOR_DEFAULTTOPRIMARY),&monitor));
        DEVMODEW mode{};mode.dmSize=sizeof(mode);winrt::check_bool(EnumDisplaySettingsExW(monitor.szDevice,ENUM_CURRENT_SETTINGS,&mode,0));
        DWM_TIMING_INFO timing{};timing.cbSize=sizeof(timing);const auto dwmResult=DwmGetCompositionTimingInfo(nullptr,&timing);
        flyout.Show({monitor.rcWork.left+900,monitor.rcWork.bottom-180});Pump(350);
        auto target=grid.FindName(L"Target").as<c::Button>();auto neighbor=grid.FindName(L"Neighbor").as<c::Button>();auto row=grid.FindName(L"Row").as<c::StackPanel>();
        const double scale=grid.XamlRoot().RasterizationScale();
        std::vector<Result> results;
        for(bool layout:{true,false}){
            target.Width(88);Pump(80);
            std::vector<Frame> frames;frames.reserve(1000);unsigned sizes=0;
            const auto started=std::chrono::steady_clock::now();
            auto changed=target.SizeChanged([&](auto const&,auto const&){++sizes;});
            auto rendered=x::Media::CompositionTarget::Rendering([&](auto const&,auto const& args){
                const auto time=args.template as<x::Media::RenderingEventArgs>().RenderingTime();
                frames.push_back({time.count()/10000.0,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),target.ActualWidth(),neighbor.TransformToVisual(row).TransformPoint({0,0}).X});
            });
            x::Media::Animation::Storyboard story;auto visual=x::Hosting::ElementCompositionPreview::GetElementVisual(target);
            if(layout){
                x::Media::Animation::DoubleAnimation animation;animation.EnableDependentAnimation(true);
                animation.From(winrt::box_value(88.0).as<winrt::Windows::Foundation::IReference<double>>());animation.To(winrt::box_value(224.0).as<winrt::Windows::Foundation::IReference<double>>());
                x::Duration duration{};duration.Type=x::DurationType::TimeSpan;duration.TimeSpan=std::chrono::milliseconds(1200);animation.Duration(duration);
                x::Media::Animation::Storyboard::SetTarget(animation,target);x::Media::Animation::Storyboard::SetTargetProperty(animation,L"Width");story.Children().Append(animation);story.Begin();
            }else{
                auto animation=visual.Compositor().CreateScalarKeyFrameAnimation();animation.Duration(std::chrono::milliseconds(1200));animation.InsertKeyFrame(0,1);animation.InsertKeyFrame(1,224.0f/88);
                visual.StartAnimation(L"Scale.X",animation);
            }
            Pump(1300);
            x::Media::CompositionTarget::Rendering(rendered);target.SizeChanged(changed);
            Result result;result.phase=layout?"layout-width":"composition-scale";result.callbacks=static_cast<unsigned>(frames.size());result.sizeChanges=sizes;
            result.width=target.ActualWidth();result.neighbor=neighbor.TransformToVisual(row).TransformPoint({0,0}).X;
            auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(target);result.hitWidth=peer.GetBoundingRectangle().Width;
            if(frames.size()>1){result.callbackHz=1000.0*(frames.size()-1)/(frames.back().wall-frames.front().wall);for(size_t i=1;i<frames.size();++i)result.maxGapMs=std::max(result.maxGapMs,frames[i].wall-frames[i-1].wall);}
            std::ofstream csv(output/(result.phase+".csv"));csv<<"render_ms,wall_ms,width_dip,neighbor_x_dip\n"<<std::setprecision(12);for(auto const& frame:frames)csv<<frame.ms<<','<<frame.wall<<','<<frame.width<<','<<frame.neighbor<<'\n';
            results.push_back(result);
            if(layout){if(std::abs(result.width-224)>1||std::abs(result.neighbor-232)>1)throw std::runtime_error("Layout animation did not move the adjacent control");target.Width(224);story.Stop();}
            else{visual.StopAnimation(L"Scale.X");visual.Scale({1,1,1});if(std::abs(result.width-88)>1||std::abs(result.neighbor-96)>1)throw std::runtime_error("Scale unexpectedly changed layout");}
        }
        std::ofstream json(output/L"summary.json");json<<std::setprecision(10)<<"{\"monitorHz\":"<<mode.dmDisplayFrequency<<",\"dwmHz\":"<<(SUCCEEDED(dwmResult)&&timing.rateRefresh.uiDenominator?static_cast<double>(timing.rateRefresh.uiNumerator)/timing.rateRefresh.uiDenominator:0)
            <<",\"dpi\":"<<scale*96<<",\"durationMs\":1200,\"isolated\":true,\"runs\":[";
        bool comma=false;for(auto const& r:results){if(comma)json<<',';comma=true;json<<"{\"phase\":\""<<r.phase<<"\",\"uiRenderingCallbacks\":"<<r.callbacks<<",\"uiCallbackHz\":"<<r.callbackHz<<",\"maxGapMs\":"<<r.maxGapMs
            <<",\"sizeChanges\":"<<r.sizeChanges<<",\"finalWidthDip\":"<<r.width<<",\"neighborXDip\":"<<r.neighbor<<",\"uiaWidthPx\":"<<r.hitWidth<<"}";}json<<"]}\n";
        flyout.Close();active=nullptr;DestroyWindow(host);host=nullptr;winrt::uninit_apartment();std::puts("WIDTH ANIMATION RESEARCH PASSED (isolated; Explorer was not modified)");return 0;
    }catch(winrt::hresult_error const& e){std::printf("FAIL 0x%08lX %ls\n",static_cast<unsigned long>(e.code().value),e.message().c_str());}
    catch(std::exception const& e){std::printf("FAIL %s\n",e.what());}
    active=nullptr;if(host)DestroyWindow(host);return 1;
}
