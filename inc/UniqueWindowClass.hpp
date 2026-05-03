#pragma once
#include <Windows.h>


class UniqueWindowClass
{
public:
    UniqueWindowClass() = default;
    explicit UniqueWindowClass(const WNDCLASSEXW& wc) : mAtom(RegisterClassExW(&wc)), mInstance(wc.hInstance) {}

    UniqueWindowClass(const UniqueWindowClass&) = delete;
    UniqueWindowClass& operator=(const UniqueWindowClass&) = delete;

    UniqueWindowClass(UniqueWindowClass&& other) noexcept : mAtom(std::exchange(other.mAtom, 0)), mInstance(std::exchange(other.mInstance, nullptr)) {}
    UniqueWindowClass& operator=(UniqueWindowClass&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            mAtom = std::exchange(other.mAtom, 0);
            mInstance = std::exchange(other.mInstance, nullptr);
        }

        return *this;
    }

    ~UniqueWindowClass() noexcept { Reset(); }

    void Reset() noexcept
    {
        if (mAtom)
        {
            UnregisterClassW(MAKEINTATOM(mAtom), mInstance);
            mAtom = 0;
            mInstance = nullptr;
        }
    }

    [[nodiscard]] inline ATOM Get() const { return mAtom; }
    [[nodiscard]] inline explicit operator bool() const { return mAtom != 0; }

private:
    ATOM mAtom = 0;
    HINSTANCE mInstance = nullptr;
};