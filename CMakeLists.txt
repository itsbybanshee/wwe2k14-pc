# wwe2k14 - WWE 2K14 (Xbox 360) static recompilation with the ReXGlue SDK.
# SDK boilerplate lives in generated/rexglue.cmake (rendered by tools/codegen.ps1 via `rexglue init`).

cmake_minimum_required(VERSION 3.25)
project(wwe2k14 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

set(WWE2K14_SOURCES
    src/main.cpp
    src/frame_rate.cpp
    src/resolve_coherency.cpp
    src/launcher_settings.cpp
    src/cp_profiler.cpp
    src/display_mode.cpp
    src/screenshot.cpp
    src/achievement_toast.cpp
    src/achievement_sound.cpp
    src/exit_menu.cpp
    src/disc_image.cpp
    src/mouse_input.cpp
    src/re_tools.cpp
    src/test_input.cpp
    src/shadows.cpp
    src/fps_overlay.cpp
)

if(WIN32)
    add_executable(wwe2k14 WIN32 ${WWE2K14_SOURCES})
else()
    add_executable(wwe2k14 ${WWE2K14_SOURCES})
endif()

rexglue_setup_target(wwe2k14)

# Optional title update build: wwe2k14_tu.exe from generated_tu\ (tools/build-tu.ps1). Same app code; the
# WWE2K14_TU define selects the title update's function addresses where the app hooks the game.
set(WWE2K14_TARGETS wwe2k14)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated_tu/sources.cmake")
    set(WWE2K14_VANILLA_SOURCES ${GENERATED_SOURCES})
    include(generated_tu/sources.cmake)
    set(WWE2K14_TU_SOURCES ${GENERATED_SOURCES})
    set(GENERATED_SOURCES ${WWE2K14_VANILLA_SOURCES})
    if(WIN32)
        add_executable(wwe2k14_tu WIN32 ${WWE2K14_SOURCES} ${WWE2K14_TU_SOURCES})
    else()
        add_executable(wwe2k14_tu ${WWE2K14_SOURCES} ${WWE2K14_TU_SOURCES})
    endif()
    target_include_directories(wwe2k14_tu PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/generated_tu)
    target_link_libraries(wwe2k14_tu PRIVATE rex::core rex::system rex::kernel rex::graphics rex::ui)
    rexglue_configure_target(wwe2k14_tu)
    target_compile_definitions(wwe2k14_tu PRIVATE WWE2K14_TU=1)
    list(APPEND WWE2K14_TARGETS wwe2k14_tu)
endif()

foreach(target IN LISTS WWE2K14_TARGETS)
if(WIN32)
    # Exe / window icon (res/wwe2k14.rc -> MAINICON).
    enable_language(RC)
    target_sources(${target} PRIVATE res/wwe2k14.rc)
    # screenshot.cpp: PNG encoding through the Windows Imaging Component, Documents folder lookup.
    target_link_libraries(${target} PRIVATE windowscodecs ole32 shell32 winmm)
    # Achievement pop-up sound, loaded from next to the exe.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/res/achievement.ogg"
                "$<TARGET_FILE_DIR:${target}>/achievement.ogg")
    target_link_options(${target} PRIVATE "LINKER:/map:$<TARGET_FILE_DIR:${target}>/${target}.map")
endif()
endforeach()
