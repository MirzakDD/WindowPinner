#include "Globals.hpp"
#include "Hotkey.hpp"
#include "HotkeyCaptureDialog.hpp"
#include "Overlay.hpp"
#include "Tray.hpp"
#include "UniqueHandle.hpp"
#include "UniqueWindowClass.hpp"
#include "../res/Resource.h"

#include <windows.h>


App gApp;

namespace
{
    constexpr UINT kFallbackTimerMs = 250;
    constexpr UINT kFallbackTimerId = 1;
    constexpr UINT kWmTrayCallback = WM_APP + 1;
    constexpr PCWSTR kMutexName = L"WindowPinnerInstance";
    constexpr PCWSTR kMsgWindowClassName = L"WindowPinnerMsg";

    UniqueWindowClass sMsgWc;
    const UINT sTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    PCWSTR FormatTrayTooltip()
    {
        constexpr size_t kTooltipMaxLength = std::extent_v<decltype(NOTIFYICONDATAW::szTip)>;
        static wchar_t tip[kTooltipMaxLength];
        swprintf_s(tip, L"Window Pinner (%ls)", Hotkey::FormatHotkey(gApp.hotkey.GetModifiers(), gApp.hotkey.GetVk()).c_str());
        return tip;
    }

    constexpr TrayMenuItem kTrayMenu[] =
    {
        { L"Set Hotkey...",
            []
            {
                HotkeyCaptureDialog::Show(
                    gApp.instance,
                    gApp.hotkey.GetModifiers(),
                    gApp.hotkey.GetVk(),
                    [] (UINT mods, UINT vk)
                    {
                        gApp.hotkey.SetCombination(mods, vk);
                        gApp.tray.SetTooltip(FormatTrayTooltip());
                    }
                );
            }
        },
        { nullptr, nullptr },
        { L"Exit", [] { PostQuitMessage(0); }},
    };

    void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
    {
        if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
            return;

        gApp.overlay.OnWinEvent(event, hwnd);
    }

    LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if (msg == sTaskbarCreatedMsg && sTaskbarCreatedMsg != 0)
        {
            gApp.tray.Reinstall();
            return 0;
        }

        switch (msg)
        {
            case WM_HOTKEY:
                if (wparam == static_cast<WPARAM>(Hotkey::GetId()))
                {
                    HWND fg = GetForegroundWindow();
                    if (fg)
                        gApp.overlay.Toggle(fg);
                }
                return 0;

            case kWmTrayCallback:
                gApp.tray.HandleNotify(wparam, lparam);
                return 0;

            case WM_TIMER:
                if (wparam == kFallbackTimerId)
                    gApp.overlay.UpdateAll();
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            default:
                break;
        }

        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
} // anonymous namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*prev*/, LPWSTR /*cmdLine*/, int /*showCmd*/)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    UniqueKernelHandle mutex(CreateMutexW(nullptr, FALSE, kMutexName));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    gApp.instance = instance;

    OverlayManager::RegisterWindowClass(instance);
    HotkeyCaptureDialog::RegisterWindowClass(instance);

    sMsgWc = UniqueWindowClass(
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .lpfnWndProc = MsgWndProc,
            .hInstance = instance,
            .lpszClassName = kMsgWindowClassName
        }
    );

    gApp.msgHwnd.Reset(CreateWindowExW(WS_EX_TOOLWINDOW, kMsgWindowClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr));

    if (sTaskbarCreatedMsg != 0)
        ChangeWindowMessageFilterEx(gApp.msgHwnd.Get(), sTaskbarCreatedMsg, MSGFLT_ALLOW, nullptr);

    gApp.hotkey = Hotkey(gApp.msgHwnd.Get());
    gApp.tray = Tray(
        {
            .owner = gApp.msgHwnd.Get(),
            .icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON)),
            .callbackMessage = kWmTrayCallback,
            .initialTooltip = FormatTrayTooltip(),
            .menu = kTrayMenu
        }
    );

    gApp.hookLocationChange.Reset(SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));

    gApp.hookDestroy.Reset(SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));

    gApp.hookMinimize.Reset(SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));

    gApp.hookForeground.Reset(SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));

    SetTimer(gApp.msgHwnd.Get(), kFallbackTimerId, kFallbackTimerMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(gApp.msgHwnd.Get(), kFallbackTimerId);
    gApp.overlay.RemoveTopmostFromAll();
}