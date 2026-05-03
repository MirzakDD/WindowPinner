#pragma once
#include <Windows.h>

#include <string>


class Hotkey
{
private:
    static constexpr int kId = 1;

    static constexpr UINT kDefaultModifiers = MOD_WIN | MOD_CONTROL;
    static constexpr UINT kDefaultVk = 'T';

    static constexpr PCWSTR kRegistryKeyPath = L"Software\\WindowPinner";
    static constexpr PCWSTR kRegistryValueName = L"Hotkey";

    HWND mOwner = nullptr;
    UINT mModifiers = 0;
    UINT mVk = 0;

    bool mRegistered = false;

    bool Register();
    void Unregister() noexcept;
    bool LoadFromRegistry();
    void SaveToRegistry() const;

public:
    Hotkey() = default;
    explicit Hotkey(HWND owner);
    ~Hotkey() noexcept;

    Hotkey(const Hotkey&) = delete;
    Hotkey& operator=(const Hotkey&) = delete;

    Hotkey(Hotkey&& other) noexcept;
    Hotkey& operator=(Hotkey&& other) noexcept;

    bool SetCombination(UINT modifiers, UINT vk);

    [[nodiscard]] UINT GetModifiers() const noexcept { return mModifiers; }
    [[nodiscard]] UINT GetVk() const noexcept { return mVk; }
    [[nodiscard]] static constexpr int GetId() noexcept { return kId; }

    [[nodiscard]] static std::wstring FormatHotkey(UINT modifiers, UINT vk);
};