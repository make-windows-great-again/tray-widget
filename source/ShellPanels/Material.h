#pragma once
#include "Noise.h"
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d2d1effects_2.h>
#include <dwmapi.h>
#include <optional>
#include <shcore.h>
#include <shlwapi.h>
#include <vector>
#include <windows.graphics.effects.interop.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.Storage.Streams.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace ShellPanels::detail
{
namespace fx = winrt::Windows::Graphics::Effects;
namespace abi = ABI::Windows::Graphics::Effects;
namespace foundation = winrt::Windows::Foundation;
namespace comp = winrt::Windows::UI::Composition;
namespace x = winrt::Windows::UI::Xaml;
struct Effect
    : winrt::implements<Effect, fx::IGraphicsEffect, fx::IGraphicsEffectSource, abi::IGraphicsEffectD2D1Interop>
{
    GUID id;
    winrt::hstring name;
    std::vector<foundation::IPropertyValue> properties;
    std::vector<fx::IGraphicsEffectSource> sources;
    Effect(GUID value) : id(value)
    {
    }
    winrt::hstring Name() const
    {
        return name;
    }
    void Name(winrt::hstring const &v)
    {
        name = v;
    }
    HRESULT __stdcall GetEffectId(GUID *v) noexcept override
    {
        if (!v)
            return E_POINTER;
        *v = id;
        return S_OK;
    }
    HRESULT __stdcall GetNamedPropertyMapping(LPCWSTR, UINT *,
                                              abi::GRAPHICS_EFFECT_PROPERTY_MAPPING *) noexcept override
    {
        return E_INVALIDARG;
    }
    HRESULT __stdcall GetPropertyCount(UINT *v) noexcept override
    {
        if (!v)
            return E_POINTER;
        *v = static_cast<UINT>(properties.size());
        return S_OK;
    }
    HRESULT __stdcall GetSourceCount(UINT *v) noexcept override
    {
        if (!v)
            return E_POINTER;
        *v = static_cast<UINT>(sources.size());
        return S_OK;
    }
    HRESULT __stdcall GetProperty(UINT i, ABI::Windows::Foundation::IPropertyValue **v) noexcept override
    {
        if (!v)
            return E_POINTER;
        *v = nullptr;
        if (i >= properties.size())
            return E_INVALIDARG;
        return reinterpret_cast<IUnknown *>(winrt::get_abi(properties[i]))->QueryInterface(IID_PPV_ARGS(v));
    }
    HRESULT __stdcall GetSource(UINT i, abi::IGraphicsEffectSource **v) noexcept override
    {
        if (!v)
            return E_POINTER;
        *v = nullptr;
        if (i >= sources.size())
            return E_INVALIDARG;
        return reinterpret_cast<IUnknown *>(winrt::get_abi(sources[i]))->QueryInterface(IID_PPV_ARGS(v));
    }
};
inline auto Node(GUID id, std::initializer_list<fx::IGraphicsEffectSource> sources,
                 std::initializer_list<foundation::IPropertyValue> properties)
{
    auto n = winrt::make_self<Effect>(id);
    n->sources = sources;
    n->properties = properties;
    return n;
}
inline foundation::IPropertyValue UInt(UINT v)
{
    return foundation::PropertyValue::CreateUInt32(v).as<foundation::IPropertyValue>();
}
inline foundation::IPropertyValue Float(float v)
{
    return foundation::PropertyValue::CreateSingle(v).as<foundation::IPropertyValue>();
}
inline fx::IGraphicsEffectSource Color(float rgb, float alpha)
{
    const float values[]{rgb, rgb, rgb, alpha};
    return Node(CLSID_D2D1Flood, {},
                {foundation::PropertyValue::CreateSingleArray(values).as<foundation::IPropertyValue>()})
        .as<fx::IGraphicsEffectSource>();
}
inline fx::IGraphicsEffectSource Color(winrt::Windows::UI::Color color, float alpha)
{
    const float values[]{color.R / 255.f, color.G / 255.f, color.B / 255.f, alpha};
    return Node(CLSID_D2D1Flood, {},
                {foundation::PropertyValue::CreateSingleArray(values).as<foundation::IPropertyValue>()})
        .as<fx::IGraphicsEffectSource>();
}
// Native composition material. Activation is deliberately NOT a policy input:
// a light-dismiss surface retains its material until its exit batch completes.
class Material
{
    x::Controls::Grid grid{nullptr};
    comp::SpriteVisual visual{nullptr};
    comp::CompositionRoundedRectangleGeometry clip{nullptr};
    x::Media::LoadedImageSurface noise{nullptr};
    winrt::event_token size{};

  public:
    Material() = default;
    Material(Material const &) = delete;
    Material &operator=(Material const &) = delete;
    ~Material()
    {
        try
        {
            if (grid)
                grid.SizeChanged(size);
        }
        catch (...)
        {
        }
    }
    void Attach(x::Controls::Grid const &target, float radius = 8)
    {
        grid = target;
        auto compositor = x::Hosting::ElementCompositionPreview::GetElementVisual(grid).Compositor();
        visual = compositor.CreateSpriteVisual();
        clip = compositor.CreateRoundedRectangleGeometry();
        clip.CornerRadius({radius, radius});
        visual.Clip(compositor.CreateGeometricClip(clip));
        x::Hosting::ElementCompositionPreview::SetElementChildVisual(grid, visual);
        size = grid.SizeChanged([this](auto const &, auto const &) { Resize(); });
        Resize();
        winrt::com_ptr<IStream> memory;
        memory.attach(SHCreateMemStream(ShellNoise, static_cast<UINT>(sizeof(ShellNoise))));
        winrt::check_bool(memory != nullptr);
        winrt::Windows::Storage::Streams::IRandomAccessStream stream{nullptr};
        winrt::check_hresult(CreateRandomAccessStreamOverStream(
            memory.get(), BSOS_DEFAULT, winrt::guid_of<decltype(stream)>(), winrt::put_abi(stream)));
        noise = x::Media::LoadedImageSurface::StartLoadFromStream(stream);
    }
    void Resize()
    {
        if (visual)
        {
            const winrt::Windows::Foundation::Numerics::float2 value{static_cast<float>(grid.ActualWidth()),
                                                                     static_cast<float>(grid.ActualHeight())};
            visual.Size(value);
            clip.Size(value);
        }
    }
    comp::CompositionBrush Brush() const
    {
        return visual.Brush();
    }
    void Theme(bool light, std::optional<uint32_t> tintArgb = {}, std::optional<double> luminosityOpacity = {})
    {
        auto compositor = visual.Compositor();
        const bool contrast = winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
        SYSTEM_POWER_STATUS power{};
        GetSystemPowerStatus(&power);
        if (contrast || !winrt::Windows::UI::ViewManagement::UISettings().AdvancedEffectsEnabled() ||
            power.SystemStatusFlag == 1)
        {
            const auto c = contrast ? GetSysColor(COLOR_WINDOW) : (light ? RGB(249, 249, 249) : RGB(44, 44, 44));
            visual.Brush(compositor.CreateColorBrush({255, GetRValue(c), GetGValue(c), GetBValue(c)}));
            return;
        }
        const uint32_t tintValue = tintArgb.value_or(light ? 0xFFFCFCFC : 0xFF292929);
        const winrt::Windows::UI::Color tintColor{255, static_cast<BYTE>(tintValue >> 16),
                                                  static_cast<BYTE>(tintValue >> 8), static_cast<BYTE>(tintValue)};
        auto backdrop = comp::CompositionEffectSourceParameter(L"Backdrop");
        // D2D Color/Luminosity naming is inverted in the established WinUI recipe.
        auto luminosity = Node(
            CLSID_D2D1Blend,
            {backdrop,
             Color(tintColor, static_cast<float>(std::clamp(luminosityOpacity.value_or(light ? .9 : .96), 0.0, 1.0)))},
            {UInt(D2D1_BLEND_MODE_COLOR)});
        auto tint = Node(CLSID_D2D1Blend,
                         {luminosity.as<fx::IGraphicsEffectSource>(), Color(tintColor, tintArgb ? .6f
                                                                                       : light  ? 0.f
                                                                                                : .15f)},
                         {UInt(D2D1_BLEND_MODE_LUMINOSITY)});
        auto wrap = Node(CLSID_D2D1Border, {comp::CompositionEffectSourceParameter(L"Noise")},
                         {UInt(D2D1_BORDER_EDGE_MODE_WRAP), UInt(D2D1_BORDER_EDGE_MODE_WRAP)});
        auto opacity = Node(CLSID_D2D1Opacity, {wrap.as<fx::IGraphicsEffectSource>()}, {Float(.02f)});
        auto composite =
            Node(CLSID_D2D1Composite, {tint.as<fx::IGraphicsEffectSource>(), opacity.as<fx::IGraphicsEffectSource>()},
                 {UInt(D2D1_COMPOSITE_MODE_SOURCE_OVER)});
        auto brush = compositor.CreateEffectFactory(composite.as<fx::IGraphicsEffect>()).CreateBrush();
        brush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush());
        brush.SetSourceParameter(L"Noise", compositor.CreateSurfaceBrush(noise));
        visual.Brush(brush);
    }
};
struct StableBrush : x::Media::XamlCompositionBrushBaseT<StableBrush>
{
    comp::CompositionBrush value{nullptr};
    bool connected = false;
    explicit StableBrush(comp::CompositionBrush const &brush) : value(brush)
    {
    }
    void OnConnected()
    {
        connected = true;
        CompositionBrush(value);
    }
    void OnDisconnected()
    {
        connected = false;
        CompositionBrush(nullptr);
    }
    void Set(comp::CompositionBrush const &brush)
    {
        value = brush;
        if (connected)
            CompositionBrush(value);
    }
};
} // namespace ShellPanels::detail
