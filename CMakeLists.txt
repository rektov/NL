cmake_minimum_required(VERSION 3.20)
project(NL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NL_BUILD_APP "Build the loader executable (Windows only)" ON)
option(NL_BUILD_TESTS "Build the unit tests (any OS)" ON)
option(NL_USE_MUSEO "Use local Museo Sans embed (src/Fonts.cpp)" OFF)

if(NL_BUILD_APP AND NOT WIN32)
  message(STATUS "NL: the loader app targets Windows only; building tests only")
  set(NL_BUILD_APP OFF)
endif()

set(NL_VENDOR "${CMAKE_SOURCE_DIR}/vendor")

if(NL_BUILD_APP)
  set(NL_VENDOR_SOURCES
    ${NL_VENDOR}/imgui/imgui.cpp
    ${NL_VENDOR}/imgui/imgui_draw.cpp
    ${NL_VENDOR}/imgui/imgui_tables.cpp
    ${NL_VENDOR}/imgui/imgui_widgets.cpp
    ${NL_VENDOR}/imgui/imgui_impl_dx11.cpp
    ${NL_VENDOR}/imgui/imgui_impl_win32.cpp
  )

  set(NL_FONT_SOURCES src/FontsFree.cpp)
  if(NL_USE_MUSEO)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/Fonts.cpp")
      message(FATAL_ERROR "NL_USE_MUSEO=ON but src/Fonts.cpp is missing")
    endif()
    set(NL_FONT_SOURCES src/Fonts.cpp)
    message(STATUS "NL: Museo Sans (NL_USE_MUSEO)")
  else()
    message(STATUS "NL: Inter (default)")
  endif()

  add_executable(NL WIN32
    src/main.cpp
    src/App.cpp
    src/AppWindow.cpp
    src/AppDevice.cpp
    src/AppFrame.cpp
    src/Log.cpp
    src/Paths.cpp
    src/Cheats.cpp
    src/Texture.cpp
    ${NL_FONT_SOURCES}
    src/Embedded.cpp
    src/ui/Dashboard.cpp
    src/ui/GameModal.cpp
    src/ui/Textures.cpp
    src/ui/Widgets.cpp
    src/ui/Scroll.cpp
    src/ui/BlurImGui.cpp
    src/gfx/Blur.cpp
    ${NL_VENDOR_SOURCES}
  )

  target_compile_features(NL PRIVATE cxx_std_20)
  target_include_directories(NL PRIVATE src ${NL_VENDOR}/imgui ${NL_VENDOR}/stb)
  target_include_directories(NL SYSTEM PRIVATE ${NL_VENDOR})
  target_compile_definitions(NL PRIVATE
    WIN32 _WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601 WINVER=0x0601
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<BOOL:${NL_USE_MUSEO}>:NL_USE_MUSEO=1>
  )
  target_link_libraries(NL PRIVATE d3d11 dxgi dwmapi)

  enable_language(RC)
  target_sources(NL PRIVATE res/NL.rc)

  if(MSVC)
    target_compile_options(NL PRIVATE /permissive- /W4 /WX /FS /utf-8 /MP)
    set_source_files_properties(${NL_VENDOR_SOURCES} PROPERTIES COMPILE_OPTIONS "/W3;/WX-")
    set_property(TARGET NL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET NL PROPERTY VS_DPI_AWARE "PerMonitor")
    target_sources(NL PRIVATE app.manifest)
  elseif(MINGW)
    target_compile_options(NL PRIVATE -Wall -Wextra)
    target_link_options(NL PRIVATE -municode -static)
  endif()
endif()

if(NL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
