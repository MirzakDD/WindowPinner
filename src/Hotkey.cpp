#include "Hotkey.hpp"
#include "UniqueHandle.hpp"

#include <cstdint>
#include <utility>


#pragma pack(push, 1)
struct PackedHotkeyData
{
    UINT modifiers;
    uint8_t vk;
};
#pragma pack(pop)
static_assert(sizeof(PackedHotkeyData) == 5);

Hotkey::Hotkey(HWND owner) : mOwner(owner)
{
    if (!LoadFromRegistry())
    {
        mModifiers = kDefaultModifiers;
        mVk = kDefaultVk;
    }

    Register();
}

Hotkey::Hotkey(Hotkey&& other) noexcept : 
    mOwner(std::exchange(other.mOwner, nullptr)), 
    mModifiers(std::exchange(other.mModifiers, 0)), 
    mVk(std::exchange(other.mVk, 0)), 
    mRegistered(std::exchange(other.mRegistered, false)) {}

Hotkey& Hotkey::operator=(Hotkey&& other) noexcept
{
    if (this != &other)
    {
        Unregister();

        mOwner = std::exchange(other.mOwner, nullptr);
        mModifiers = std::exchange(other.mModifiers, 0);
        mVk = std::exchange(other.mVk, 0);
        mRegistered = std::exchange(other.mRegistered, false);
    }

    return *this;
}

Hotkey::~Hotkey() noexcept 
{ 
    Unregister(); 
}

bool Hotkey::SetCombination(UINT modifiers, UINT vk)
{
    if (modifiers == 0 || vk == 0)
        return false;

    Unregister();

    mModifiers = modifiers;
    mVk = vk;

    if (!Register())
        return false;

    SaveToRegistry();
    return true;
}

bool Hotkey::Register()
{
    if (!mOwner || mModifiers == 0 || mVk == 0)
        return false;

    if (RegisterHotKey(mOwner, kId, mModifiers | MOD_NOREPEAT, mVk))
    {
        mRegistered = true;
        return true;
    }

    return false;
}

void Hotkey::Unregister() noexcept
{
    if (mRegistered)
    {
        UnregisterHotKey(mOwner, kId);
        mRegistered = false;
    }
}

bool Hotkey::LoadFromRegistry()
{
    UniqueHkey key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryKeyPath, 0, KEY_READ, key.Put()) != ERROR_SUCCESS)
        return false;

    PackedHotkeyData data;
    DWORD size = sizeof(data);
    DWORD type;
    LSTATUS status = RegQueryValueExW(key.Get(), kRegistryValueName, nullptr, &type, reinterpret_cast<PBYTE>(&data), &size);
    if (status == ERROR_SUCCESS && type == REG_BINARY && size == sizeof(data))
    {
        UINT mods = data.modifiers & (MOD_WIN | MOD_CONTROL | MOD_ALT | MOD_SHIFT);
        if (mods != 0 && data.vk != 0)
        {
            mModifiers = mods;
            mVk = data.vk;
            return true;
        }
    }

    return false;
}

void Hotkey::SaveToRegistry() const
{
    UniqueHkey key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryKeyPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, key.Put(), nullptr) != ERROR_SUCCESS)
        return;

    PackedHotkeyData data = { .modifiers = mModifiers, .vk = static_cast<uint8_t>(mVk) };
    RegSetValueExW(key.Get(), kRegistryValueName, 0, REG_BINARY, reinterpret_cast<PBYTE>(&data), sizeof(data));
}

std::wstring Hotkey::FormatHotkey(UINT modifiers, UINT vk)
{
    std::wstring result;

    if (modifiers & MOD_WIN)     result += L"Win + ";
    if (modifiers & MOD_CONTROL) result += L"Ctrl + ";
    if (modifiers & MOD_ALT)     result += L"Alt + ";
    if (modifiers & MOD_SHIFT)   result += L"Shift + ";

    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG lparam = static_cast<LONG>(scanCode << 16);

    switch (vk)
    {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
        case VK_DIVIDE:
        case VK_NUMLOCK:
            lparam |= (1 << 24);
            break;
    }

    wchar_t keyName[64];
    int length = GetKeyNameTextW(lparam, keyName, static_cast<int>(std::size(keyName)));

    if (length > 0)
    {
        result.append(keyName, static_cast<size_t>(length));
        return result;
    }

    UINT charCode = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR);
    if (charCode != 0)
    {
        result.push_back(static_cast<wchar_t>(charCode & 0x7FFFFFFF));
        return result;
    }

    wchar_t fallbackBuffer[16];
    swprintf_s(fallbackBuffer, L"VK_%02X", vk);
    result += fallbackBuffer;
    return result;
}