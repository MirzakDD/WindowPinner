#include "Overlay.hpp"
#include "Globals.hpp"

#include <Windows.h>
#include <dwmapi.h>

#include <algorithm>
#include <cstdint>
#include <utility>


namespace
{
    constexpr uint32_t kBorderColorRgb = 0x0078D4; // blue
    constexpr int kBorderThickness = 4;
    constexpr int kCornerRadius = 8;
    constexpr int kBorderOffset = 0;
    constexpr BYTE kBorderAlpha = 255; // opacity (0 - fully transparent, 255 - fully opaque)

    UniqueWindowClass sWindowClass;
    HBRUSH sBorderBrush = nullptr;

    LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    RECT GetExtendedFrameBounds(HWND hwnd)
    {
        RECT rect;
        HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
        if (FAILED(hr))
            GetWindowRect(hwnd, &rect);

        return rect;
    }

    RECT ApplyOffset(const RECT& rect)
    {
        const int expand = kBorderOffset + kBorderThickness;
        return RECT{ rect.left - expand, rect.top - expand, rect.right + expand, rect.bottom + expand };
    }

    UniqueHwnd CreateOverlayWindow(const RECT& r)
    {
        UniqueHwnd hwnd(CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            OverlayManager::WindowClassName(), L"", WS_POPUP,
            r.left, r.top, r.right - r.left, r.bottom - r.top,
            nullptr, nullptr, gApp.instance, nullptr
        ));

        if (hwnd)
            SetLayeredWindowAttributes(hwnd.Get(), 0, kBorderAlpha, LWA_ALPHA);

        return hwnd;
    }

    UniqueHrgn BuildRingRegion(int width, int height)
    {
        UniqueHrgn outer(CreateRoundRectRgn(0, 0, width, height, 2 * kCornerRadius, 2 * kCornerRadius));
        if (!outer)
            return {};

        // skip the hole if window is too small to contain a meaningful inner rect
        if (width > 2 * kBorderThickness && height > 2 * kBorderThickness)
        {
            const int innerRadius = std::max(0, kCornerRadius - kBorderThickness);
            UniqueHrgn inner(CreateRoundRectRgn(
                kBorderThickness, kBorderThickness,
                width - kBorderThickness, height - kBorderThickness,
                2 * innerRadius, 2 * innerRadius
            ));

            if (inner)
                CombineRgn(outer.Get(), outer.Get(), inner.Get(), RGN_DIFF);
        }

        return outer;
    }

    std::unordered_map<HWND, int> BuildZOrderMap()
    {
        std::unordered_map<HWND, int> map;
        int index = 0;
        for (HWND h = GetTopWindow(nullptr); h; h = GetWindow(h, GW_HWNDNEXT))
            map.emplace(h, index++);
        return map;
    }
}  // anonymous namespace


PinnedWindow::PinnedWindow(HWND target) : mTarget(target)
{
    if (!target)
        return;

    LONG_PTR exStyle = GetWindowLongPtrW(target, GWL_EXSTYLE);
    mWasOriginallyTopmost = (exStyle & WS_EX_TOPMOST) != 0;
    mMinimized = IsIconic(target) != FALSE;

    if (!mWasOriginallyTopmost)
    {
        if (!SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE))
        {
            mTarget = nullptr; // prevent dtor from attempting NOTOPMOST restore on a window that never was modified
            return;
        }
    }

    RECT targetRect = GetExtendedFrameBounds(target);
    RECT overlayRect = ApplyOffset(targetRect);
    int w = overlayRect.right - overlayRect.left;
    int h = overlayRect.bottom - overlayRect.top;

    mOverlay = CreateOverlayWindow(overlayRect);
    if (!mOverlay)
    {
        if (!mWasOriginallyTopmost)
            SetWindowPos(target, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return;
    }

    // initial ring; clip against other pinned overlays will be applied on first update
    UniqueHrgn ring = BuildRingRegion(w, h);
    if (ring && SetWindowRgn(mOverlay.Get(), ring.Get(), TRUE))
        (void)ring.Release();

    mCachedRect = targetRect;
    mCachedWidth = w;
    mCachedHeight = h;

    if (!mMinimized)
        ShowWindow(mOverlay.Get(), SW_SHOWNA);
}

PinnedWindow::PinnedWindow(PinnedWindow&& other) noexcept
{
    MoveFrom(std::move(other));
}

PinnedWindow& PinnedWindow::operator=(PinnedWindow&& other) noexcept
{
    if (this != &other)
    {
        if (mTarget && !mWasOriginallyTopmost && IsWindow(mTarget))
            SetWindowPos(mTarget, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        MoveFrom(std::move(other));
    }

    return *this;
}

PinnedWindow::~PinnedWindow() noexcept
{
    if (mTarget && !mWasOriginallyTopmost && IsWindow(mTarget))
        SetWindowPos(mTarget, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void PinnedWindow::MoveFrom(PinnedWindow&& other) noexcept
{
    mTarget = std::exchange(other.mTarget, nullptr);
    mOverlay = std::move(other.mOverlay);
    mCachedRect = other.mCachedRect;
    mCachedWidth = std::exchange(other.mCachedWidth, 0);
    mCachedHeight = std::exchange(other.mCachedHeight, 0);
    mMinimized = std::exchange(other.mMinimized, false);
    mWasOriginallyTopmost = std::exchange(other.mWasOriginallyTopmost, false);
}

void PinnedWindow::ApplyClip(const std::unordered_map<HWND, int>& zMap, std::span<const HWND> pinnedTargets)
{
    UniqueHrgn region = BuildRingRegion(mCachedWidth, mCachedHeight);
    if (!region)
        return;

    auto myZ = zMap.find(mTarget);
    if (myZ != zMap.end())
    {
        // overlay rect in screen coordinates - needed to translate other targets rects into local space
        const RECT overlayScreen = ApplyOffset(mCachedRect);

        for (const auto& [otherHwnd, otherZ] : zMap)
        {
            if (otherHwnd == mTarget || otherZ >= myZ->second)
                continue;

            if (std::find(pinnedTargets.begin(), pinnedTargets.end(), otherHwnd) == pinnedTargets.end())
                continue;

            const RECT otherRect = GetExtendedFrameBounds(otherHwnd);
            UniqueHrgn sub(CreateRectRgn(
                otherRect.left - overlayScreen.left,
                otherRect.top - overlayScreen.top,
                otherRect.right - overlayScreen.left,
                otherRect.bottom - overlayScreen.top
            ));

            if (sub)
                CombineRgn(region.Get(), region.Get(), sub.Get(), RGN_DIFF);
        }
    }

    if (SetWindowRgn(mOverlay.Get(), region.Get(), TRUE))
        (void)region.Release();
}

void PinnedWindow::Update(const std::unordered_map<HWND, int>& zMap, std::span<const HWND> pinnedTargets)
{
    if (!IsWindow(mTarget) || !mOverlay)
        return;

    RECT targetRect = GetExtendedFrameBounds(mTarget);
    RECT overlayRect = ApplyOffset(targetRect);
    int newW = overlayRect.right - overlayRect.left;
    int newH = overlayRect.bottom - overlayRect.top;

    if (newW < 1 || newH < 1)
        return;

    mCachedWidth = newW;
    mCachedHeight = newH;

    SetWindowPos(mOverlay.Get(), HWND_TOPMOST, overlayRect.left, overlayRect.top, newW, newH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    mCachedRect = targetRect;

    ApplyClip(zMap, pinnedTargets);
}

void PinnedWindow::SetMinimized(bool minimized)
{
    mMinimized = minimized;
    if (mOverlay)
        ShowWindow(mOverlay.Get(), minimized ? SW_HIDE : SW_SHOWNA);
}


void OverlayManager::RegisterWindowClass(HINSTANCE instance)
{
    if (!sBorderBrush)
    {
        const uint8_t r = static_cast<uint8_t>((kBorderColorRgb >> 16) & 0xFF);
        const uint8_t g = static_cast<uint8_t>((kBorderColorRgb >> 8) & 0xFF);
        const uint8_t b = static_cast<uint8_t>(kBorderColorRgb & 0xFF);
        sBorderBrush = CreateSolidBrush(RGB(r, g, b));
    }

    sWindowClass = UniqueWindowClass(
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .lpfnWndProc = OverlayWndProc,
            .hInstance = instance,
            .hbrBackground = sBorderBrush,
            .lpszClassName = kWindowClassName
        }
    );
}

PinnedWindow* OverlayManager::Find(HWND target)
{
    auto it = std::find_if(mPinned.begin(), mPinned.end(), [target](const PinnedWindow& pw) { return pw.Target() == target; });
    return it != mPinned.end() ? &*it : nullptr;
}

bool OverlayManager::IsOwnOverlay(HWND hwnd) const
{
    return std::any_of(mPinned.begin(), mPinned.end(), [hwnd](const PinnedWindow& pw) { return pw.OverlayHwnd() == hwnd; });
}

std::vector<HWND> OverlayManager::CollectTargets() const
{
    std::vector<HWND> targets;
    targets.reserve(mPinned.size());

    for (const auto& pw : mPinned)
        targets.push_back(pw.Target());
    return targets;
}

void OverlayManager::Toggle(HWND target)
{
    if (!target || target == GetDesktopWindow() || target == GetShellWindow())
        return;

    if (target == gApp.msgHwnd.Get())
        return;

    if (IsOwnOverlay(target))
        return;

    auto it = std::find_if(mPinned.begin(), mPinned.end(), [target](const PinnedWindow& pw) { return pw.Target() == target; });
    if (it != mPinned.end())
    {
        mPinned.erase(it);
        return;
    }

    PinnedWindow pw(target);
    if (pw.IsValid())
        mPinned.push_back(std::move(pw));
    else
        MessageBeep(MB_ICONWARNING);
}

void OverlayManager::UpdateAll()
{
    auto zMap = BuildZOrderMap();

    for (size_t i = mPinned.size(); i > 0; --i)
    {
        if (!IsWindow(mPinned[i - 1].Target()))
            mPinned.erase(mPinned.begin() + (i - 1));
    }

    auto targets = CollectTargets();

    for (auto& pw : mPinned)
        if (!pw.IsMinimized())
            pw.Update(zMap, targets);
}

void OverlayManager::OnWinEvent(DWORD event, HWND hwnd)
{
    if (event == EVENT_SYSTEM_FOREGROUND)
    {
        UpdateAll();
        return;
    }

    PinnedWindow* pw = Find(hwnd);
    if (!pw)
        return;

    switch (event)
    {
        case EVENT_OBJECT_LOCATIONCHANGE:
            if (!pw->IsMinimized())
            {
                auto zMap = BuildZOrderMap();
                auto targets = CollectTargets();
                pw->Update(zMap, targets);
            }
            break;

        case EVENT_OBJECT_DESTROY:
        {
            auto it = std::find_if(mPinned.begin(), mPinned.end(), [hwnd](const PinnedWindow& p) { return p.Target() == hwnd; });
            if (it != mPinned.end())
                mPinned.erase(it);
            break;
        }

        case EVENT_SYSTEM_MINIMIZESTART:
            pw->SetMinimized(true);
            break;

        case EVENT_SYSTEM_MINIMIZEEND:
        {
            pw->SetMinimized(false);
            auto zMap = BuildZOrderMap();
            auto targets = CollectTargets();
            pw->Update(zMap, targets);
            break;
        }

        default:
            break;
    }
}

void OverlayManager::RemoveTopmostFromAll()
{
    mPinned.clear();
}