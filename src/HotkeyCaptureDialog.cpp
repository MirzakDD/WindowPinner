#include "HotkeyCaptureDialog.hpp"
#include "Hotkey.hpp"
#include "UniqueHandle.hpp"
#include "UniqueWindowClass.hpp"
#include "..\res\Resource.h"

#include <string>


namespace
{
    UniqueWindowClass sWindowClass;

    constexpr UINT kWmHotkeyCaptured = WM_APP + 100;
    constexpr UINT kWmHotkeyConfirm = WM_APP + 101;

    // single-instance state - only one capture session at a time
    UniqueHwnd sWindow;
    UniqueHhook sKeyboardHook;
    UINT sCaptureModState = 0;

    UINT sCurrentModifiers = 0;
    UINT sCurrentVk = 0;
    UINT sCapturedModifiers = 0;
    UINT sCapturedVk = 0;

    HotkeyCaptureDialog::ConfirmCallback sOnConfirm = nullptr;


    UINT VkToMod(UINT vk)
    {
        switch (vk)
        {
            case VK_LSHIFT:
            case VK_RSHIFT:
                return MOD_SHIFT;

            case VK_LCONTROL:
            case VK_RCONTROL:
                return MOD_CONTROL;

            case VK_LMENU:
            case VK_RMENU:
                return MOD_ALT;

            case VK_LWIN:
            case VK_RWIN:
                return MOD_WIN;

            default:
                return 0;
        }
    }

    LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wparam, LPARAM lparam)
    {
        if (code != HC_ACTION || !sWindow)
            return CallNextHookEx(sKeyboardHook.Get(), code, wparam, lparam);

        auto* kbs = reinterpret_cast<PKBDLLHOOKSTRUCT>(lparam);
        UINT vk = kbs->vkCode;

        if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)
        {
            UINT mod = VkToMod(vk);
            if (mod != 0)
            {
                sCaptureModState |= mod;
                return 1;
            }

            if (vk == VK_ESCAPE && sCaptureModState == 0)
                PostMessageW(sWindow.Get(), WM_CLOSE, 0, 0);

            else if (vk == VK_RETURN && sCaptureModState == 0)
                PostMessageW(sWindow.Get(), kWmHotkeyConfirm, 0, 0);

            else if (sCaptureModState != 0)
                PostMessageW(sWindow.Get(), kWmHotkeyCaptured, sCaptureModState, vk);

            return 1;
        }

        if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP)
        {
            UINT mod = VkToMod(vk);
            if (mod != 0)
                sCaptureModState &= ~mod;
            return 1;
        }

        return CallNextHookEx(sKeyboardHook.Get(), code, wparam, lparam);
    }
}  // anonymous namespace


void HotkeyCaptureDialog::RegisterWindowClass(HINSTANCE instance)
{
    sWindowClass = UniqueWindowClass(
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .lpfnWndProc = WndProc,
            .hInstance = instance,
            .hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON)),
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
            .lpszClassName = kWindowClassName,
            .hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON))
        }
    );
}

void HotkeyCaptureDialog::Show(HINSTANCE instance, UINT currentMods, UINT currentVk, ConfirmCallback onConfirm)
{
    if (sWindow)
    {
        SetForegroundWindow(sWindow.Get());
        return;
    }

    sCurrentModifiers = currentMods;
    sCurrentVk = currentVk;
    sCapturedModifiers = 0;
    sCapturedVk = 0;
    sCaptureModState = 0;
    sOnConfirm = onConfirm;

    const int w = 380;
    const int h = 170;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    sWindow.Reset(CreateWindowExW(
        WS_EX_TOPMOST,
        kWindowClassName, L"Set Hotkey",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, w, h,
        nullptr, nullptr, instance, nullptr
    ));

    if (!sWindow)
    {
        sOnConfirm = nullptr;
        return;
    }

    sKeyboardHook.Reset(SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, instance, 0));

    ShowWindow(sWindow.Get(), SW_SHOW);
    SetForegroundWindow(sWindow.Get());
}

LRESULT CALLBACK HotkeyCaptureDialog::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            SetBkMode(hdc, TRANSPARENT);

            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            // line 1 (current hotkey)
            std::wstring buffer = L"Current hotkey:   ";
            buffer += Hotkey::FormatHotkey(sCurrentModifiers, sCurrentVk);

            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            RECT r1 = { 20, 18, clientRect.right - 20, 44 };
            DrawTextW(hdc, buffer.c_str(), -1, &r1, DT_LEFT | DT_SINGLELINE);

            // line 2 (new hotkey)
            buffer = L"New hotkey:   ";
            if (sCapturedVk != 0)
                buffer += Hotkey::FormatHotkey(sCapturedModifiers, sCapturedVk);
            else
                buffer += L"(none)";

            RECT r2 = { 20, 50, clientRect.right - 20, 76 };
            DrawTextW(hdc, buffer.c_str(), -1, &r2, DT_LEFT | DT_SINGLELINE);

            // line 3 (hints)
            SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
            RECT r3 = { 20, 92, clientRect.right - 20, 118 };
            DrawTextW(hdc, L"Enter - confirm      Esc - cancel", -1, &r3, DT_LEFT | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case kWmHotkeyCaptured:
        {
            sCapturedModifiers = static_cast<UINT>(wparam);
            sCapturedVk = static_cast<UINT>(lparam);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case kWmHotkeyConfirm:
        {
            if (sCapturedVk != 0)
            {
                if (sOnConfirm)
                    sOnConfirm(sCapturedModifiers, sCapturedVk);

                sCurrentModifiers = sCapturedModifiers;
                sCurrentVk = sCapturedVk;
                sCapturedModifiers = 0;
                sCapturedVk = 0;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else
                DestroyWindow(hwnd);

            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            sKeyboardHook.Reset();
            sCaptureModState = 0;
            sCapturedModifiers = 0;
            sCapturedVk = 0;
            sCurrentModifiers = 0;
            sCurrentVk = 0;
            sOnConfirm = nullptr;
            (void)sWindow.Release();
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}