#pragma once
#include <Windows.h>

#include <concepts>
#include <type_traits>
#include <utility>


template <typename THandle>
struct BaseHandleTraits
{
    using HandleType = THandle;
};

template <typename THandleTraits>
concept HandleTraits = std::derived_from<THandleTraits, BaseHandleTraits<typename THandleTraits::HandleType>>
&& requires(typename THandleTraits::HandleType h) { { THandleTraits::Close(h) } -> std::same_as<void>; };

struct KernelHandleTraits : BaseHandleTraits<HANDLE>
{
    inline static void Close(HANDLE h) noexcept { CloseHandle(h); }
};

struct HwndTraits : BaseHandleTraits<HWND>
{
    inline static void Close(HWND h) noexcept { DestroyWindow(h); }
};

struct HbitmapTraits : BaseHandleTraits<HBITMAP>
{
    inline static void Close(HBITMAP h) noexcept { DeleteObject(h); }
};

struct HhookTraits : BaseHandleTraits<HHOOK>
{
    inline static void Close(HHOOK h) noexcept { UnhookWindowsHookEx(h); }
};

struct HwineventhookTraits : BaseHandleTraits<HWINEVENTHOOK>
{
    inline static void Close(HWINEVENTHOOK h) noexcept { UnhookWinEvent(h); }
};

struct HmenuTraits : BaseHandleTraits<HMENU>
{
    inline static void Close(HMENU h) noexcept { DestroyMenu(h); }
};

struct HkeyTraits : BaseHandleTraits<HKEY>
{
    inline static void Close(HKEY h) noexcept { RegCloseKey(h); }
};

struct HrgnTraits : BaseHandleTraits<HRGN>
{
    inline static void Close(HRGN h) noexcept { DeleteObject(h); }
};


template <HandleTraits TTraits>
class UniqueHandle
{
public:
    using HandleType = typename TTraits::HandleType;

private:
    HandleType mHandle = nullptr;

public:
    constexpr UniqueHandle() noexcept = default;
    constexpr explicit UniqueHandle(HandleType handle) noexcept : mHandle(handle) {}

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : mHandle(std::exchange(other.mHandle, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (mHandle)
                Reset();

            mHandle = std::exchange(other.mHandle, nullptr);
        }

        return *this;
    }

    ~UniqueHandle() noexcept
    {
        if (mHandle)
            TTraits::Close(mHandle);
    }

    void Reset(HandleType handle = nullptr) noexcept
    {
        HandleType old = std::exchange(mHandle, handle);
        if (old != nullptr)
            TTraits::Close(old);
    }

    [[nodiscard]] inline constexpr HandleType Get() const noexcept { return mHandle; }
    [[nodiscard]] inline HandleType Release() noexcept { return std::exchange(mHandle, nullptr); }
    [[nodiscard]] inline HandleType* Put() noexcept { Reset(); return &mHandle; }

    [[nodiscard]] inline constexpr explicit operator bool() const noexcept { return mHandle != nullptr; }
};

using UniqueKernelHandle = UniqueHandle<KernelHandleTraits>;
using UniqueHwnd = UniqueHandle<HwndTraits>;
using UniqueHbitmap = UniqueHandle<HbitmapTraits>;
using UniqueHhook = UniqueHandle<HhookTraits>;
using UniqueHwineventhook = UniqueHandle<HwineventhookTraits>;
using UniqueHmenu = UniqueHandle<HmenuTraits>;
using UniqueHkey = UniqueHandle<HkeyTraits>;
using UniqueHrgn = UniqueHandle<HrgnTraits>;