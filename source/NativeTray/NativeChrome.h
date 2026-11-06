#pragma once
// Included after SavedValue inside the TAP's implementation namespace.
// Keep the original native chrome untouched when the panel is closed. Normal
// notification icons do not expose the OmniButton's checked-state contract.
struct NativeChrome
{
    wux::FrameworkElement owner{nullptr};
    controls::Grid root{nullptr};
    controls::Border native{nullptr},overlay{nullptr};
    controls::ContentPresenter presenter{nullptr};
    std::unique_ptr<SavedValue> opacity,foreground;
    std::array<wux::Media::Brush,3> fills{nullptr,nullptr,nullptr},strokes{nullptr,nullptr,nullptr};
    std::array<wux::Media::Brush,3> text{nullptr,nullptr,nullptr};
    winrt::Windows::UI::ViewManagement::AccessibilitySettings accessibility{nullptr};
    RECT bounds{};
    bool active=false,light=true,contrast=false;
    int previous=-1;

    static controls::Border Background(wux::DependencyObject const& node,controls::Grid& parent,int depth=0)
    {
        if(depth>12)return nullptr;
        if(auto border=node.try_as<controls::Border>();border && border.Name()==L"BackgroundBorder")return border;
        const int count=wux::Media::VisualTreeHelper::GetChildrenCount(node);
        for(int i=0;i<count;++i){
            auto child=wux::Media::VisualTreeHelper::GetChild(node,i);
            if(auto found=Background(child,parent,depth+1)){
                if(child==found)parent=node.try_as<controls::Grid>();
                return found;
            }
        }
        return nullptr;
    }
    static wux::Media::Brush Lookup(wux::ResourceDictionary const& dictionary,winrt::Windows::Foundation::IInspectable const& key,
        wchar_t const* theme,int depth=0)
    {
        if(depth>12)return nullptr;
        try {
            if(dictionary.HasKey(key))if(auto brush=dictionary.Lookup(key).try_as<wux::Media::Brush>())return brush;
            auto themes=dictionary.ThemeDictionaries();
            auto themeKey=winrt::box_value(theme);
            if(themes.HasKey(themeKey))if(auto values=themes.Lookup(themeKey).try_as<wux::ResourceDictionary>())
                if(auto brush=Lookup(values,key,theme,depth+1))return brush;
            auto defaults=winrt::box_value(L"Default");
            if(themes.HasKey(defaults))if(auto values=themes.Lookup(defaults).try_as<wux::ResourceDictionary>())
                if(auto brush=Lookup(values,key,theme,depth+1))return brush;
            auto merged=dictionary.MergedDictionaries();
            for(uint32_t i=merged.Size();i>0;--i)if(auto brush=Lookup(merged.GetAt(i-1),key,theme,depth+1))return brush;
        }catch(...){}
        return nullptr;
    }
    wux::Media::Brush Resource(wchar_t const* name)
    {
        const auto key=winrt::box_value(name);
        const auto theme=contrast ? L"HighContrast" : light ? L"Light" : L"Dark";
        wux::DependencyObject node=owner;
        for(int depth=0;node && depth<30;++depth) {
            if(auto element=node.try_as<wux::FrameworkElement>())if(auto brush=Lookup(element.Resources(),key,theme))return brush;
            node=wux::Media::VisualTreeHelper::GetParent(node);
        }
        if(auto app=wux::Application::Current())if(auto brush=Lookup(app.Resources(),key,theme))return brush;
        return nullptr;
    }
    static winrt::Windows::UI::Color Color(uint32_t argb)
    {
        return {static_cast<uint8_t>(argb>>24),static_cast<uint8_t>(argb>>16),static_cast<uint8_t>(argb>>8),static_cast<uint8_t>(argb)};
    }
    static wux::Media::Brush Solid(uint32_t argb){return wux::Media::SolidColorBrush(Color(argb));}
    static wux::Media::Brush Gradient(uint32_t top,uint32_t bottom)
    {
        wux::Media::LinearGradientBrush brush;
        brush.StartPoint({0,0});brush.EndPoint({0,1});
        wux::Media::GradientStop first;first.Offset(0);first.Color(Color(top));
        wux::Media::GradientStop last;last.Offset(1);last.Color(Color(bottom));
        brush.GradientStops().Append(first);brush.GradientStops().Append(last);
        return brush;
    }
    void Palette()
    {
        const wchar_t* fillNames[]{L"OmniButtonBackgroundChecked",L"OmniButtonBackgroundCheckedPointerOver",L"OmniButtonBackgroundCheckedPressed"};
        const wchar_t* strokeNames[]{L"OmniButtonBorderBrushChecked",L"OmniButtonBorderBrushCheckedPointerOver",L"OmniButtonBorderBrushCheckedPressed"};
        const wchar_t* textNames[]{L"OmniButtonForegroundChecked",L"OmniButtonForegroundCheckedPointerOver",L"OmniButtonForegroundCheckedPressed"};
        // Exact fallback values from the installed SystemTrayResources.xbf:
        // checked=secondary, checked hover=primary, checked pressed=tertiary.
        const uint32_t lightFills[]{0x80FFFFFF,0xB3FFFFFF,0x4CFFFFFF};
        const uint32_t darkFills[]{0x0FFFFFFF,0x15FFFFFF,0x0AFFFFFF};
        int resolved=0;
        for(int i=0;i<3;++i){
            fills[i]=Resource(fillNames[i]);strokes[i]=Resource(strokeNames[i]);text[i]=Resource(textNames[i]);
            if(fills[i])++resolved;
            if(!fills[i])fills[i]=Solid(light ? lightFills[i] : darkFills[i]);
            if(!strokes[i])strokes[i]=i==2 ? Solid(light ? 0x05000000 : 0x0AFFFFFF) :
                Gradient(light ? (i==1 ? 0x17000000 : 0x0F000000) : (i==1 ? 0x20FFFFFF : 0x1AFFFFFF),
                    light ? (i==1 ? 0x07000000 : 0x05000000) : (i==1 ? 0x15FFFFFF : 0x0FFFFFFF));
            if(!text[i])text[i]=Solid(light ? (i==2 ? 0x9E000000 : 0xE4000000) : (i==2 ? 0xC5FFFFFF : 0xFFFFFFFF));
            if(contrast){
                auto color=[](int index){auto value=GetSysColor(index);return 0xFF000000u | (static_cast<uint32_t>(GetRValue(value))<<16) | (static_cast<uint32_t>(GetGValue(value))<<8) | GetBValue(value);};
                fills[i]=Solid(color(COLOR_HIGHLIGHT));strokes[i]=Solid(color(COLOR_WINDOWTEXT));text[i]=Solid(color(COLOR_HIGHLIGHTTEXT));
            }
        }
        Log(L"Native checked chrome: live fill resources="+std::to_wstring(resolved));
        previous=-1;
    }
    void Attach(wux::FrameworkElement const& view,controls::ContentPresenter const& content)
    {
        Close();owner=view;presenter=content;native=Background(view,root);
        if(!native){Log(L"Native background border was not found");return;}
        if(!root){Log(L"Native background parent is not a grid");native=nullptr;return;}
        overlay=controls::Border();overlay.Name(L"YataActiveChrome");overlay.IsHitTestVisible(false);
        overlay.Margin(native.Margin());overlay.CornerRadius(native.CornerRadius());overlay.BorderThickness(native.BorderThickness());
        overlay.Visibility(wux::Visibility::Collapsed);
        wux::BrushTransition transition;transition.Duration(std::chrono::milliseconds(83));overlay.BackgroundTransition(transition);
        uint32_t index=0;
        if(!root.Children().IndexOf(native,index)){Log(L"Native background is not in its parent's children");Close();return;}
        root.Children().InsertAt(index+1,overlay);
        light=view.ActualTheme()!=wux::ElementTheme::Dark;
        accessibility=winrt::Windows::UI::ViewManagement::AccessibilitySettings();
        contrast=accessibility.HighContrast();
        Palette();
    }
    void SetActive(bool value)
    {
        if(!overlay || active==value)return;
        active=value;previous=-1;
        if(value){opacity=std::make_unique<SavedValue>(native,wux::UIElement::OpacityProperty());foreground=std::make_unique<SavedValue>(presenter,controls::ContentPresenter::ForegroundProperty());native.Opacity(0);overlay.Visibility(wux::Visibility::Visible);Update();}
        else{overlay.Visibility(wux::Visibility::Collapsed);if(opacity){opacity->Restore();opacity.reset();}if(foreground){foreground->Restore();foreground.reset();}}
        // No GoToState("Normal"): the native control still owns its hover state.
    }
    void Update()
    {
        if(!active || !overlay)return;
        const bool nextLight=owner.ActualTheme()!=wux::ElementTheme::Dark;
        const bool nextContrast=accessibility.HighContrast();
        if(light!=nextLight || contrast!=nextContrast){light=nextLight;contrast=nextContrast;Palette();}
        POINT cursor{};GetPhysicalCursorPos(&cursor);
        const bool over=PtInRect(&bounds,cursor)!=FALSE;
        // An open shell panel keeps its selected chrome through a second press
        // and the complete exit animation. Pressed is only a closed-button state.
        const int state=over ? 1 : 0;
        if(state!=previous){overlay.Background(fills[state]);overlay.BorderBrush(strokes[state]);previous=state;}
        if(presenter.Foreground()!=text[state])presenter.Foreground(text[state]);
        if(native.Opacity()!=0)native.Opacity(0);
    }
    void Close() noexcept
    {
        active=false;
        try{if(overlay)overlay.Visibility(wux::Visibility::Collapsed);}catch(...){Log(L"Hide selected chrome failed");}
        try{if(opacity)opacity->Restore();}catch(...){Log(L"Restore native opacity failed");}
        try{if(foreground)foreground->Restore();}catch(...){Log(L"Restore native foreground failed");}
        try{if(root && overlay){uint32_t index=0;if(root.Children().IndexOf(overlay,index))root.Children().RemoveAt(index);}}
        catch(...){Log(L"Remove selected chrome failed");}
        opacity.reset();foreground.reset();root=nullptr;native=nullptr;overlay=nullptr;owner=nullptr;presenter=nullptr;accessibility=nullptr;active=false;previous=-1;
    }
};
