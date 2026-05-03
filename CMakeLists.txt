cmake_minimum_required(VERSION 3.25)
project(WindowPinner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WindowPinner WIN32
    src/Hotkey.cpp
    src/HotkeyCaptureDialog.cpp
    src/Overlay.cpp
    src/Tray.cpp
    src/Main.cpp
    res/WindowPinner.rc
)

target_include_directories(WindowPinner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_definitions(WindowPinner PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(WindowPinner PRIVATE
    dwmapi
    user32
    gdi32
    shell32
    advapi32
)