#pragma once
// Every revoker is destroyed on the XAML thread that installed it.
struct XamlConnections {
    std::vector<std::function<void()>> undo;
    ~XamlConnections(){Clear();}
    void Clear() noexcept {for(auto it=undo.rbegin();it!=undo.rend();++it){try{(*it)();}catch(...){}}undo.clear();}
    void Property(wux::DependencyObject const& object,wux::DependencyProperty const& property,std::function<void()> action){
        auto token=object.RegisterPropertyChangedCallback(property,[action=std::move(action)](auto const&,auto const&) noexcept {try{action();}catch(...){}});
        undo.emplace_back([object,property,token]{object.UnregisterPropertyChangedCallback(property,token);});
    }
    void Pointer(wux::UIElement const& element,wux::RoutedEvent const& event,std::function<void()> action){
        auto handler=winrt::box_value(wux::Input::PointerEventHandler([action=std::move(action)](auto const&,auto const&) noexcept {try{action();}catch(...){}}));
        element.AddHandler(event,handler,true);
        undo.emplace_back([element,event,handler]{element.RemoveHandler(event,handler);});
    }
};
