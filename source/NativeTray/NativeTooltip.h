#pragma once
// Included inside the TAP namespace after SavedValue and the XAML aliases.
struct NativeTooltip
{
    wux::FrameworkElement owner{nullptr};
    controls::ToolTip tip{nullptr};
    std::unique_ptr<SavedValue> original;
    std::unique_ptr<SavedValue> placement;
    bool hovered=false,ownedOpen=false;
    winrt::event_token opened{};
    UINT delay=400;
    bool reportedError=false;
    wux::ElementTheme paletteTheme=wux::ElementTheme::Default;
    bool paletteContrast=false,paletteValid=false,paletteError=false;

    void Palette() noexcept
    {
        try{
        HIGHCONTRASTW settings{sizeof(settings)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(settings),&settings,0);
        const bool contrast=(settings.dwFlags&HCF_HIGHCONTRASTON)!=0;
        const auto theme=owner.ActualTheme();
        if(paletteValid && paletteTheme==theme && paletteContrast==contrast)return;
        NativeChrome resources;resources.owner=owner;resources.light=theme!=wux::ElementTheme::Dark;resources.contrast=contrast;
        int resolved=0;
        if(auto brush=resources.Resource(L"ToolTipBackgroundBrush")){tip.Background(brush);++resolved;}
        if(auto brush=resources.Resource(L"ToolTipForegroundBrush")){tip.Foreground(brush);++resolved;}
        if(auto brush=resources.Resource(L"ToolTipBorderBrush")){tip.BorderBrush(brush);++resolved;}
        Log(L"Native tooltip palette: live brushes="+std::to_wstring(resolved));
        paletteTheme=theme;paletteContrast=contrast;paletteValid=true;
        }catch(...){if(!paletteError)Log(L"Native tooltip palette unavailable; keeping platform defaults");paletteError=true;}
    }

    void Attach(wux::FrameworkElement const& element)
    {
        Close();owner=element;
        original=std::make_unique<SavedValue>(owner,controls::ToolTipService::ToolTipProperty());
        placement=std::make_unique<SavedValue>(owner,controls::ToolTipService::PlacementProperty());
        const auto previous=controls::ToolTipService::GetToolTip(owner).try_as<controls::ToolTip>();
        tip=controls::ToolTip();
        if(previous && previous.Style())tip.Style(previous.Style());
        tip.Content(winrt::box_value(NativeTray::Tooltip));
        tip.FontFamily(wux::Media::FontFamily(L"Segoe UI Variable"));tip.FontSize(12);
        tip.Padding({8,5,8,7});tip.CornerRadius({4,4,4,4});
        tip.Placement(controls::Primitives::PlacementMode::Top);
        // Directional ToolTip offsets inflate the exclusion rect: positive moves Top upward.
        tip.PlacementTarget(owner);tip.VerticalOffset(12);
        tip.IsHitTestVisible(false);
        Palette();
        wux::Automation::AutomationProperties::SetAutomationId(tip,L"YataTrayTooltip");
        controls::ToolTipService::SetPlacement(owner,controls::Primitives::PlacementMode::Top);
        controls::ToolTipService::SetToolTip(owner,tip);
        opened=tip.Opened([this](auto const&,auto const&) noexcept {
            try{if(!ownedOpen && tip)tip.IsOpen(false);}catch(...){Log(L"Tooltip automatic-open suppression failed");}
        });
        SystemParametersInfoW(SPI_GETMOUSEHOVERTIME,0,&delay,0);
    }
    void Cancel() noexcept
    {
        hovered=false;ownedOpen=false;
        try{if(tip && tip.IsOpen())tip.IsOpen(false);}catch(...){}
    }
    bool Update(RECT const& bounds,bool allowed) noexcept
    {
        try{
        if(!owner || !tip)return false;
        Palette();
        tip.IsEnabled(allowed);
        POINT cursor{};
        const bool pointerValid=GetPhysicalCursorPos(&cursor)!=FALSE;
        const bool button=(GetAsyncKeyState(VK_LBUTTON)&0x8000) || (GetAsyncKeyState(VK_RBUTTON)&0x8000);
        if(!allowed || !pointerValid || button || !PtInRect(&bounds,cursor) || !owner.IsLoaded()){
            Cancel();return false;
        }
        const bool entered=!hovered;hovered=true;return entered;
        }catch(winrt::hresult_error const& error){
            if(!reportedError)Log(L"Tooltip open failed HRESULT="+std::to_wstring(static_cast<unsigned long>(error.code().value))+L" "+std::wstring(error.message()));
            reportedError=true;
            try{if(tip)tip.IsOpen(false);}catch(...){}
            // A popup failure must never remove or reapply the tray decoration.
        }catch(...){Log(L"Tooltip update failed");}
        Cancel();return false;
    }
    bool Open(RECT const& bounds,bool allowed) noexcept
    {
        Update(bounds,allowed);
        if(!hovered)return false;
        return ShowOwned();
    }
    bool ShowOwned() noexcept
    {
        try{
            ownedOpen=true;
            tip.PlacementTarget(owner);
            tip.IsOpen(true);return true;
        }catch(...){Cancel();Log(L"Tooltip open failed");return false;}
    }
    void Close() noexcept
    {
        try{if(tip)tip.Opened(opened);}catch(...){Log(L"Revoke tooltip callback failed");}
        try{if(tip){tip.IsOpen(false);tip.PlacementTarget(nullptr);}}catch(...){Log(L"Close tooltip failed");}
        try{if(original)original->Restore();}catch(...){Log(L"Restore original tooltip failed");}
        try{if(placement)placement->Restore();}catch(...){Log(L"Restore tooltip placement failed");}
        original.reset();placement.reset();tip=nullptr;owner=nullptr;hovered=false;ownedOpen=false;reportedError=false;paletteValid=false;paletteError=false;opened={};
    }
};
