#pragma once
int PrintStatus(bool profile){
    TrayAttach::Handle mapping{OpenFileMappingW(FILE_MAP_READ,FALSE,NativeTray::MappingName)};
    if(!mapping.value){TrayConsole::Write("Native tray app is not running.\n");return 1;}
    TrayAttach::View view{static_cast<NativeTray::State*>(MapViewOfFile(mapping.value,FILE_MAP_READ,0,0,sizeof(NativeTray::State)))};
    if(!view.value||!NativeTray::Valid(*view.value)){TrayConsole::Write("Incompatible native protocol.\n");return 2;}auto& s=*view.value;char text[4096]{};
    if(!profile)std::snprintf(text,sizeof(text),"mode=%ld applied=%ld thread=%ld error=0x%08lX size=%.2fx%.2f DIP app=%lu explorer=%ld attach=0x%08lX panel=%ld lifecycle=%ld revision=%ld/%ld\n",s.mode,s.applied,s.uiThread,s.lastError,s.actualWidth,s.actualHeight,s.appPid,s.explorerPid,s.attachResult,s.panelOpen,s.lifecycle,s.acknowledgedRevision,s.requestRevision);
    else std::snprintf(text,sizeof(text),"{\"session\":%lld,\"connection\":%lld,\"revision\":%ld,\"appliedRevision\":%ld,\"lifecycle\":%ld,\"pollingTimers\":%ld,\"tooltipTimer\":%ld,\"subscriptions\":%ld,\"refreshes\":%lld,\"notifications\":%lld,\"pointerEvents\":%lld,\"chromeUpdates\":%lld,\"tooltipOpens\":%lld,\"callbackErrors\":%lld,\"updateTicks\":%lld,\"maxUpdateTicks\":%lld,\"qpcFrequency\":%lld}\n",s.session,s.connection,s.requestRevision,s.acknowledgedRevision,s.lifecycle,s.pollingTimers,s.tooltipTimer,s.subscriptions,s.refreshes,s.notifications,s.pointerEvents,s.chromeUpdates,s.tooltipOpens,s.callbackErrors,s.updateTicks,s.maxUpdateTicks,s.qpcFrequency);
    TrayConsole::Write(text);if(!profile)TrayConsole::Error(TrayEnvironment::Describe(TrayEnvironment::Inspect()));return 0;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    try{
        int count=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&count);if(!argv)return 2;
        std::vector<std::wstring> args;for(int i=1;i<count;++i)args.emplace_back(argv[i]);LocalFree(argv);
        auto options=TrayCommands::Parse(args);if(!options.error.empty()){TrayConsole::Error(options.error);return 2;}
        if(options.role==TrayCommands::Role::Version){TrayConsole::Write("Yata Native Framework 16\n");return 0;}
        if(options.role==TrayCommands::Role::Diagnose){
            const auto environment=TrayEnvironment::Inspect();TrayConsole::Error(TrayEnvironment::Describe(environment));
            const auto problem=TrayEnvironment::Problem(environment);if(!problem.empty())TrayConsole::Error(problem);
            return TrayEnvironment::Classify(environment)==TrayEnvironment::Availability::Ready?0:3;
        }
        if(options.role==TrayCommands::Role::Help){TrayConsole::Write("Yata Native Framework 16\n--controls [--show]   Native control gallery\n--lang|--quick|--date [--show]  Shell panel layouts\n--standalone         Start without registering an Explorer icon or loading a patch\n--sample [--show]     Legacy visual sample\n--mode label|lines|off --width 0|32..320\n--attach --detach --status --profile --exit\nService roles are used separately. No installation is required.\n");return 0;}
        if(options.role==TrayCommands::Role::Status||options.role==TrayCommands::Role::Profile)return PrintStatus(options.role==TrayCommands::Role::Profile);
        HWND existing=FindWindowW(kWindowClass,nullptr);
        if((options.role==TrayCommands::Role::App&&!options.standalone&&!existing)||options.role==TrayCommands::Role::Attach){
            const auto environment=TrayEnvironment::Inspect();
            if(TrayEnvironment::Classify(environment)!=TrayEnvironment::Availability::Ready){
                TrayEnvironment::Log(L"startup-shell-preflight",HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
                TrayConsole::Error(TrayEnvironment::Describe(environment));TrayConsole::Error(TrayEnvironment::Problem(environment));return 3;
            }
        }
        if(options.role==TrayCommands::Role::Attach){const auto result=TrayAttach::Run();char text[80]{};std::snprintf(text,sizeof(text),"attach=0x%08lX\n",static_cast<unsigned long>(result));TrayConsole::Write(text);return FAILED(result)?1:0;}
        if(options.role==TrayCommands::Role::Exit){if(existing)PostMessageW(existing,kExitMessage,0,0);return 0;}
        if(options.role==TrayCommands::Role::Detach){options.mode=NativeTray::Off;if(!existing)return 1;}
        if(existing){
            if(options.standalone){TrayConsole::Write("--standalone is a startup option; close the running instance first.\n");return 2;}
            TrayCommands::Packet packet;packet.mode=options.mode;packet.width=options.width;packet.show=options.show||args.empty();packet.fault=options.fault;
            packet.content=options.content;if(options.content>=0)packet.show=TRUE;
            COPYDATASTRUCT data{TrayCommands::CommandTag,sizeof(packet),&packet};DWORD_PTR result=0;
            if(!SendMessageTimeoutW(existing,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result))return 1;
            return result?0:2;
        }
        TrayAttach::Handle singleton{CreateMutexW(nullptr,FALSE,kMutexName)};if(!singleton.value)return 1;if(GetLastError()==ERROR_ALREADY_EXISTS)return 1;
        winrt::init_apartment(winrt::apartment_type::single_threaded);int result=0;
        {TrayApp app;app.Initialize(instance,options);if(options.show||args.empty()||options.content>=0)app.ShowPanel();MSG message{};BOOL got=0;
            while((got=GetMessageW(&message,nullptr,0,0))>0)if(!app.Process(message)){TranslateMessage(&message);DispatchMessageW(&message);}
            result=got==-1?1:static_cast<int>(message.wParam);
        }winrt::uninit_apartment();return result;
    }catch(winrt::hresult_error const& e){char text[80]{};std::snprintf(text,sizeof(text),"Application error 0x%08lX\n",static_cast<unsigned long>(e.code().value));TrayConsole::Write(text);TrayConsole::Error(e.message());return 1;}
    catch(...){TrayConsole::Write("Application initialization failed.\n");return 1;}
}
