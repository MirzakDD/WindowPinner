#pragma once
#include "Hotkey.hpp"
#include "Overlay.hpp"
#include "Tray.hpp"
#include "UniqueHandle.hpp"
#include "UniqueWindowClass.hpp"

#include <windows.h>


struct App
{
    HINSTANCE instance = nullptr;

    UniqueHwnd msgHwnd;

    UniqueHwineventhook hookLocationChange;
    UniqueHwineventhook hookDestroy;
    UniqueHwineventhook hookMinimize;
    UniqueHwineventhook hookForeground;

    OverlayManager overlay;
    Hotkey hotkey;
    Tray tray;
};

extern App gApp;