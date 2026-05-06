#include "Tray.hpp"
#include "UniqueHandle.hpp"

#include <cstring>
#include <utility>


static constexpr UINT kMenuBaseId = 1;

Tray::Tray(const TrayConfig& config) : mItems(config.menu.begin(), config.menu.end())
{
    mNid =
    {
        .cbSize = sizeof(NOTIFYICONDATAW),
        .hWnd = config.owner,
        .uID = config.iconId,
        .uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP,
        .uCallbackMessage = config.callbackMessage,
        .hIcon = config.icon,
    };

    if (config.initialTooltip)
        wcsncpy_s(mNid.szTip, config.initialTooltip, _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &mNid))
        return;

    mNid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &mNid);
    mActive = true;
}

Tray::Tray(Tray&& other) noexcept : mNid(other.mNid), mItems(std::move(other.mItems)), mActive(std::exchange(other.mActive, false))
{
    other.mNid = {};
}

Tray& Tray::operator=(Tray&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        mNid = other.mNid;
        mItems = std::move(other.mItems);
        mActive = std::exchange(other.mActive, false);
        other.mNid = {};
    }

    return *this;
}

Tray::~Tray() noexcept
{
    Reset();
}

void Tray::Reset() noexcept
{
    if (mActive)
    {
        Shell_NotifyIconW(NIM_DELETE, &mNid);
        mActive = false;
    }
}

void Tray::SetTooltip(PCWSTR text)
{
    if (!mActive || !text)
        return;

    wcsncpy_s(mNid.szTip, text, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &mNid);
}

void Tray::Reinstall()
{
    if (!mActive)
        return;

    Shell_NotifyIconW(NIM_DELETE, &mNid);

    if (!Shell_NotifyIconW(NIM_ADD, &mNid))
        return;

    mNid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &mNid);
}

void Tray::HandleNotify(WPARAM /*wparam*/, LPARAM lparam)
{
    switch (LOWORD(lparam))
    {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowMenu();
            break;
    }
}

void Tray::ShowMenu()
{
    UniqueHmenu menu(CreatePopupMenu());
    if (!menu)
        return;

    for (size_t i = 0; i < mItems.size(); ++i)
    {
        if (mItems[i].text == nullptr)
            AppendMenuW(menu.Get(), MF_SEPARATOR, 0, nullptr);

        else
            AppendMenuW(menu.Get(), MF_STRING, kMenuBaseId + static_cast<UINT>(i), mItems[i].text);
    }

    SetForegroundWindow(mNid.hWnd);

    POINT pt;
    GetCursorPos(&pt);

    UINT cmd = static_cast<UINT>(TrackPopupMenu(
        menu.Get(),
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, 0, mNid.hWnd, nullptr
    ));

    PostMessageW(mNid.hWnd, WM_NULL, 0, 0);

    if (cmd >= kMenuBaseId && cmd < kMenuBaseId + mItems.size())
    {
        const auto& item = mItems[cmd - kMenuBaseId];
        if (item.onClick)
            item.onClick();
    }
}