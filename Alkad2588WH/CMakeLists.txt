cmake_minimum_required(VERSION 3.10)
project(Alkad2588WH LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим библиотеки
find_library(D3D11_LIB d3d11)
find_library(D3DCOMPILER_LIB d3dcompiler)
find_library(ADVAPI32_LIB advapi32)
find_library(WINMM_LIB winmm)

# ImGui
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_impl_dx11.cpp
    external/imgui/imgui_impl_win32.cpp
)

# Основной проект
add_library(Alkad2588WH SHARED
    src/main.cpp
    src/renderer.cpp
    src/hack_config.cpp
)

target_include_directories(Alkad2588WH PRIVATE
    external/imgui
    external/detours/include
)

target_link_libraries(Alkad2588WH PRIVATE
    ${D3D11_LIB}
    ${D3DCOMPILER_LIB}
    ${ADVAPI32_LIB}
    ${WINMM_LIB}
    imgui
    detours
)

target_compile_definitions(Alkad2588WH PRIVATE _CRT_SECURE_NO_WARNINGS)