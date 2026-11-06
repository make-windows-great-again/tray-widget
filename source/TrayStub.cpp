#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "NativeTray/NativeTrayProtocol.h"
#include "ShellPanels/ShellPanels.h"
#include "NativeTray/TrayCommands.h"
#include "NativeTray/TrayConsole.h"
#include "NativeTray/TrayAttach.h"
#include "NativeTray/NativeControls.h"
#include "NativeTray/SamplePanel.h"
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace
{
constexpr auto kWindowClass=NativeTray::WindowClass;
constexpr auto kMutexName=NativeTray::MutexName;
const UINT kTrayMessage=NativeTray::TrayMessage();
const UINT kShowMessage=NativeTray::ShowMessage();
const UINT kExitMessage=NativeTray::ExitMessage();
const UINT kCommandMessage=NativeTray::CommandMessage();

constexpr GUID kTrayGuid = NativeTray::IconGuid;

bool UsesLightTheme(const wchar_t* valueName)
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        valueName, RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value != 0;
}

HICON CreateTrayIcon(ID2D1Factory* factory, IDWriteFactory* textFactory, UINT dpi, bool light)
{
    const int width = std::max(16, GetSystemMetricsForDpi(SM_CXSMICON, dpi));
    const int height = std::max(16, GetSystemMetricsForDpi(SM_CYSMICON, dpi));
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return nullptr;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* storage = nullptr;
    HBITMAP color = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &storage, nullptr, 0);
    if (!color) { DeleteDC(dc); return nullptr; }
    std::fill_n(static_cast<std::uint32_t*>(storage), static_cast<size_t>(width) * height, 0u);
    const HGDIOBJ previous = SelectObject(dc, color);
    bool rendered = false;
    {
        ComPtr<ID2D1DCRenderTarget> target;
        ComPtr<ID2D1SolidColorBrush> brush;
        ComPtr<IDWriteTextFormat> format;
        ComPtr<IDWriteTextLayout> layout;
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        const RECT bounds{0, 0, width, height};
        const float fontPixels = 11.0f * static_cast<float>(dpi) / 96.0f;
        if (SUCCEEDED(factory->CreateDCRenderTarget(&properties, &target)) &&
            SUCCEEDED(target->BindDC(dc, &bounds)) &&
            SUCCEEDED(textFactory->CreateTextFormat(L"Segoe UI Variable Small", nullptr,
                DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, fontPixels, L"en-US", &format)) &&
            SUCCEEDED(textFactory->CreateTextLayout(L"UI", 2, format.Get(), 256, 256, &layout)) &&
            SUCCEEDED(target->CreateSolidColorBrush(
                light ? D2D1::ColorF(0, 0.895f) : D2D1::ColorF(1, 1, 1, 1), &brush)))
        {
            DWRITE_TEXT_METRICS metrics{};
            if (SUCCEEDED(layout->GetMetrics(&metrics)) && metrics.width > 0)
            {
                // Shell owns the icon slot. Fit one text row into its actual
                // pixel width, without creating an additional desktop window.
                const float scaleX = std::min(1.0f, (static_cast<float>(width) - 1.0f) / metrics.width);
                const float x = (static_cast<float>(width) - metrics.width * scaleX) / 2.0f;
                const float y = (static_cast<float>(height) - metrics.height) / 2.0f;
                target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                target->BeginDraw();
                target->Clear(D2D1::ColorF(0, 0.0f));
                target->SetTransform(D2D1::Matrix3x2F::Scale(scaleX, 1) *
                    D2D1::Matrix3x2F::Translation(x, y));
                target->DrawTextLayout(D2D1::Point2F(0, 0), layout.Get(), brush.Get());
                rendered = SUCCEEDED(target->EndDraw());
            }
        }
    }
    SelectObject(dc, previous);
    const size_t maskStride = static_cast<size_t>((width + 15) / 16) * 2;
    std::vector<BYTE> maskPixels(maskStride * height, 0);
    HBITMAP mask = rendered ? CreateBitmap(width, height, 1, 1, maskPixels.data()) : nullptr;
    ICONINFO iconInfo{};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = color;
    iconInfo.hbmMask = mask;
    HICON icon = mask ? CreateIconIndirect(&iconInfo) : nullptr;
    if (mask) DeleteObject(mask);
    DeleteObject(color);
    DeleteDC(dc);
    return icon;
}

#include "TrayApp.h"
}
#include "TrayMain.h"
