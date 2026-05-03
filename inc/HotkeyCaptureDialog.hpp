#pragma once
#include <Windows.h>


class HotkeyCaptureDialog
{
private:
    static constexpr PCWSTR kWindowClassName = L"WindowPinnerHotkeyCapture";

public:
    using ConfirmCallback = void (*)(UINT modifiers, UINT vk);

    static void RegisterWindowClass(HINSTANCE instance);
    static void Show(HINSTANCE instance, UINT currentMods, UINT currentVk, ConfirmCallback onConfirm);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    [[nodiscard]] static PCWSTR WindowClassName() noexcept { return kWindowClassName; }
};