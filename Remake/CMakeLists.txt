# CMakeLists.txt - build for the PAC-GAL (Al J. Jimenez, May 1982) remake.
# Remake using Claude Opus 5.5, Nov 2026
# Guided by Brett Hallen
cmake_minimum_required(VERSION 3.16)
project(pacgal VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Two programs:
#   PAC-GAL       the SDL3 window version
#   pacgal-text   the terminal version: needs nothing but a C compiler, and
#                 uses SDL3 for sound only if SDL3 is available
option(PACGAL_GUI "Build the SDL3 window version (PAC-GAL)" ON)
option(PACGAL_TEXT_AUDIO "Give the terminal version sound through SDL3 when SDL3 is available" ON)
# Use an installed SDL3 if there is one (Homebrew, a distro package, vcpkg...),
# otherwise (for the window version) download and build it.
option(PACGAL_VENDOR_SDL "Always download and build SDL3 instead of using an installed copy" OFF)

if(PACGAL_GUI OR PACGAL_TEXT_AUDIO)
    if(NOT PACGAL_VENDOR_SDL)
        find_package(SDL3 CONFIG QUIET)
    endif()
    if(NOT SDL3_FOUND AND PACGAL_GUI)
        message(STATUS "SDL3 not found - fetching it from GitHub (this takes a few minutes the first time)")
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.x
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL3)
    endif()
endif()

set(PACGAL_CORE src/game.c src/screen.c src/sound.c)
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if(PACGAL_GUI)
    add_executable(pacgal WIN32 MACOSX_BUNDLE src/main.c ${PACGAL_CORE})
    target_link_libraries(pacgal PRIVATE SDL3::SDL3)
    set_target_properties(pacgal PROPERTIES
        OUTPUT_NAME "PAC-GAL"
        MACOSX_BUNDLE_BUNDLE_NAME "PAC-GAL"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.pacgal"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    if(NOT MSVC)
        target_link_libraries(pacgal PRIVATE m)
        target_compile_options(pacgal PRIVATE -Wall -Wextra)
    endif()
endif()

# The terminal version.
add_executable(pacgal_text src/term_main.c ${PACGAL_CORE})
set_target_properties(pacgal_text PROPERTIES OUTPUT_NAME "pacgal-text")
if(PACGAL_TEXT_AUDIO AND TARGET SDL3::SDL3)
    target_compile_definitions(pacgal_text PRIVATE PACGAL_TEXT_SDL_AUDIO)
    target_link_libraries(pacgal_text PRIVATE SDL3::SDL3)
    message(STATUS "pacgal-text: sound through SDL3")
else()
    message(STATUS "pacgal-text: no SDL3, the terminal version will be silent")
endif()
if(NOT MSVC)
    target_link_libraries(pacgal_text PRIVATE m)
    target_compile_options(pacgal_text PRIVATE -Wall -Wextra)
endif()

# Logic tests: no SDL needed.  Run with `ctest` from the build folder.
include(CTest)
if(BUILD_TESTING)
    add_executable(pacgal_tests tests/test_game.c ${PACGAL_CORE})
    target_include_directories(pacgal_tests PRIVATE src)
    if(NOT MSVC)
        target_link_libraries(pacgal_tests PRIVATE m)
    endif()
    add_test(NAME pacgal_tests COMMAND pacgal_tests)
    # hours of simulated real-time play: checks nothing ever stalls
    add_executable(pacgal_soak tests/soak.c ${PACGAL_CORE})
    target_include_directories(pacgal_soak PRIVATE src)
    if(NOT MSVC)
        target_link_libraries(pacgal_soak PRIVATE m)
    endif()
    add_test(NAME pacgal_soak COMMAND pacgal_soak 500000)
    # headless state trace, used to check the port against the original EXE
    add_executable(pacgal_trace tests/trace.c ${PACGAL_CORE})
    target_include_directories(pacgal_trace PRIVATE src)
    if(NOT MSVC)
        target_link_libraries(pacgal_trace PRIVATE m)
    endif()
endif()
