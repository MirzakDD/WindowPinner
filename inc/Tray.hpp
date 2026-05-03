#pragma once
#include <windows.h>
#include <shellapi.h>

#include <span>
#include <vector>


struct TrayMenuItem
{
    PCWSTR text; // nullptr = separator
    void (*onClick)();
};

struct TrayConfig
{
    HWND owner = nullptr;
    HICON icon = nullptr;
    UINT callbackMessage = 0; // WM_APP + N, unique per owner
    UINT iconId = 1;
    PCWSTR initialTooltip = nullptr;
    std::span<const TrayMenuItem> menu;
};

class Tray
{
private:
    NOTIFYICONDATAW mNid = {};
    std::vector<TrayMenuItem> mItems;
    bool mActive = false;

    void ShowMenu();
    void Reset() noexcept;

public:
    Tray() = default;
    explicit Tray(const TrayConfig& config);

    Tray(const Tray&) = delete;
    Tray& operator=(const Tray&) = delete;

    Tray(Tray&& other) noexcept;
    Tray& operator=(Tray&& other) noexcept;

    ~Tray() noexcept;

    void SetTooltip(PCWSTR text);
    void HandleNotify(WPARAM /*wparam*/, LPARAM lparam);
};