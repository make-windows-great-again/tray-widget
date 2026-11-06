#define NATIVE_FLYOUT_SELFTEST
#include "../../source/NativeTray/NativeFlyout.h"
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>

namespace x=winrt::Windows::UI::Xaml;
NativeFlyout* current=nullptr;
bool dismissOnDeactivate=false;
bool ownTrayPress=false;
LRESULT CALLBACK Host(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
    if(message==WM_PAINT){PAINTSTRUCT p{};BeginPaint(window,&p);EndPaint(window,&p);return 0;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_ACTIVATE&&LOWORD(wp)==WA_INACTIVE&&dismissOnDeactivate&&current)
        try{current->OwnerDeactivated(reinterpret_cast<HWND>(lp),ownTrayPress);}catch(...){}
    return DefWindowProcW(window,message,wp,lp);
}
LRESULT CALLBACK Background(HWND window,UINT message,WPARAM wp,LPARAM lp) noexcept {
    if(message==WM_PAINT){
        PAINTSTRUCT p{};const auto dc=BeginPaint(window,&p);RECT r{};GetClientRect(window,&r);
        for(LONG left=0;left<r.right;left+=100){RECT stripe{left,0,left+100,r.bottom};
            const auto brush=CreateSolidBrush((left/100)%2?RGB(20,70,120):RGB(210,110,40));
            FillRect(dc,&stripe,brush);DeleteObject(brush);}
        EndPaint(window,&p);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
void Pump(DWORD ms){
    const auto end=GetTickCount64()+ms;
    do{MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){
        if(!current||!current->Process(m)){TranslateMessage(&m);DispatchMessageW(&m);}
    }Sleep(1);}while(GetTickCount64()<end);
}
void Click(POINT point){
    SetCursorPos(point.x,point.y);INPUT input[2]{};input[0].type=INPUT_MOUSE;input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    input[1].type=INPUT_MOUSE;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(2,input,sizeof(INPUT));
}
CLSID Encoder(){
    UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<BYTE> bytes(size);
    auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());Gdiplus::GetImageEncoders(count,size,codecs);
    for(UINT i=0;i<count;++i)if(!wcscmp(codecs[i].MimeType,L"image/png"))return codecs[i].Clsid;
    winrt::throw_hresult(E_FAIL);
}
int wmain(int argc,wchar_t** argv){
    if(argc<2)return 2;
    POINT originalCursor{};GetPhysicalCursorPos(&originalCursor);
    ULONG_PTR gdip=0;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&gdip,&input,nullptr);
    try{
        const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
        std::ofstream log(output/L"frames.csv");log<<"theme,action,frame,elapsed_ms,phase,foreground_pid\n";
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=Host;wc.lpszClassName=L"Yata.ExitMaterial.Host";RegisterClassW(&wc);
        HWND host=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"Exit material verification",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);
        wc.lpfnWndProc=Background;wc.lpszClassName=L"Yata.ExitMaterial.Background";RegisterClassW(&wc);
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(host,MONITOR_DEFAULTTOPRIMARY),&monitor);
        HWND background=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"Exit material test pattern",WS_POPUP,monitor.rcWork.left+150,monitor.rcWork.top+150,1300,900,nullptr,nullptr,wc.hInstance,nullptr);
        SetWindowPos(background,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        {
            NativeFlyout flyout;current=&flyout;flyout.Initialize(host,[](bool){},[](UINT){});
            auto content=x::Controls::Grid();content.Height(180);
            flyout.SetContent(content,L"Acrylic exit test",420);
            POINT anchor{monitor.rcWork.left+900,monitor.rcWork.top+900};
            const auto encoder=Encoder();
            for(bool light:{true,false})for(int action=0;action<6;++action){
                dismissOnDeactivate=false;ownTrayPress=false;flyout.Theme(light);flyout.Show(anchor);Pump(350);
                auto initial=flyout.SurfaceForTest();Click({initial.left+100,initial.top+100});Pump(350);
                const auto rect=flyout.SurfaceForTest();const int width=rect.right-rect.left,height=rect.bottom-rect.top;
                std::vector<std::unique_ptr<Gdiplus::Bitmap>> frames;
                auto capture=[&]{
                    auto bitmap=std::make_unique<Gdiplus::Bitmap>(width,height,PixelFormat24bppRGB);
                    auto screen=GetDC(nullptr);{Gdiplus::Graphics graphics(bitmap.get());auto target=graphics.GetHDC();
                    BitBlt(target,0,0,width,height,screen,rect.left,rect.top,SRCCOPY|CAPTUREBLT);graphics.ReleaseHDC(target);}
                    ReleaseDC(nullptr,screen);frames.push_back(std::move(bitmap));
                };
                capture();
                const auto start=GetTickCount64();
                if(action==0)flyout.Hide();
                else {
                    dismissOnDeactivate=action!=2;ownTrayPress=action==3;
                    if(action==4){flyout.Hide();Pump(30);}
                    winrt::check_bool(SetForegroundWindow(FindWindowW(L"Shell_TrayWnd",nullptr))!=FALSE);
                    if(action==3||action==5){Pump(20);flyout.Toggle(anchor);}
                }
                for(int frame=0;frame<16;++frame){
                    Pump(8);capture();DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
                    log<<(light?"light":"dark")<<','<<action<<','<<frame<<','<<GetTickCount64()-start<<','<<static_cast<int>(flyout.CurrentPhase())<<','<<pid<<'\n';
                }
                const int thumbWidth=315,thumbHeight=static_cast<int>(height*315.0/width);
                Gdiplus::Bitmap sheet(thumbWidth*4,thumbHeight*5,PixelFormat24bppRGB);Gdiplus::Graphics graphics(&sheet);
                graphics.Clear(Gdiplus::Color(255,100,100,100));
                for(size_t i=0;i<frames.size();++i)graphics.DrawImage(frames[i].get(),static_cast<int>(i%4)*thumbWidth,static_cast<int>(i/4)*thumbHeight,thumbWidth,thumbHeight);
                const auto prefix=std::wstring(light?L"light":L"dark")+L"-action"+std::to_wstring(action);
                sheet.Save((output/(prefix+L"-sheet.png")).c_str(),&encoder,nullptr);
                frames.front()->Save((output/(prefix+L"-open.png")).c_str(),&encoder,nullptr);
                if(action==1||action==3||action==4||action==5)winrt::check_bool(!flyout.Visible()&&flyout.CurrentPhase()==NativeFlyout::Phase::Closed);
                dismissOnDeactivate=false;flyout.Hide();Pump(320);
            }
            flyout.Close();current=nullptr;
        }
        DestroyWindow(host);DestroyWindow(background);winrt::uninit_apartment();
        SetCursorPos(originalCursor.x,originalCursor.y);Gdiplus::GdiplusShutdown(gdip);return 0;
    }catch(winrt::hresult_error const& e){std::printf("FAIL 0x%08lX %ls\n",static_cast<unsigned long>(e.code().value),e.message().c_str());}
    current=nullptr;SetCursorPos(originalCursor.x,originalCursor.y);Gdiplus::GdiplusShutdown(gdip);return 1;
}
