#pragma once
#include "UniqueHandle.hpp"

#include <Windows.h>

#include <span>
#include <unordered_map>
#include <vector>


class PinnedWindow
{
private:
    HWND mTarget = nullptr;
    UniqueHwnd mOverlay;
    RECT mCachedRect = {};
    int mCachedWidth = 0;
    int mCachedHeight = 0;
    bool mMinimized = false;
    bool mWasOriginallyTopmost = false;

    void ApplyClip(const std::unordered_map<HWND, int>& zMap, std::span<const HWND> pinnedTargets);

    void MoveFrom(PinnedWindow&& other) noexcept;

public:
    explicit PinnedWindow(HWND target);

    PinnedWindow(const PinnedWindow&) = delete;
    PinnedWindow& operator=(const PinnedWindow&) = delete;

    PinnedWindow(PinnedWindow&&) noexcept;
    PinnedWindow& operator=(PinnedWindow&&) noexcept;

    ~PinnedWindow() noexcept;

    void Update(const std::unordered_map<HWND, int>& zMap, std::span<const HWND> pinnedTargets);
    void SetMinimized(bool minimized);

    [[nodiscard]] HWND Target() const noexcept { return mTarget; }
    [[nodiscard]] HWND OverlayHwnd() const noexcept { return mOverlay.Get(); }
    [[nodiscard]] bool IsMinimized() const noexcept { return mMinimized; }
    [[nodiscard]] bool IsValid() const noexcept { return static_cast<bool>(mOverlay); }
    [[nodiscard]] bool WasOriginallyTopmost() const noexcept { return mWasOriginallyTopmost; }
};


class OverlayManager
{
private:
    static constexpr PCWSTR kWindowClassName = L"WindowPinnerOverlay";

    std::vector<PinnedWindow> mPinned;

    PinnedWindow* Find(HWND target);
    bool IsOwnOverlay(HWND hwnd) const;
    std::vector<HWND> CollectTargets() const;

public:
    OverlayManager() = default;
    ~OverlayManager() noexcept = default;

    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;

    static void RegisterWindowClass(HINSTANCE instance);
    [[nodiscard]] static PCWSTR WindowClassName() noexcept { return kWindowClassName; }

    void Toggle(HWND target);
    void UpdateAll();
    void OnWinEvent(DWORD event, HWND hwnd);
    void RemoveTopmostFromAll();
};