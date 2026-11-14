# CMakeLists.txt - build for the PAC-GAL (Al J. Jimenez, May 1982) remake.
# Remake using Claude Opus 5.5, Nov 2026
# Guided by Brett Hallen
cmake_minimum_required(VERSION 3.16)
project(pacgal VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Use an installed SDL3 if there is one (Homebrew, a distro package, vcpkg...),
# otherwise download and build it as part of this project.
option(PACGAL_VENDOR_SDL "Always download and build SDL3 instead of using an installed copy" OFF)

if(NOT PACGAL_VENDOR_SDL)
    find_package(SDL3 CONFIG QUIET)
endif()

if(NOT SDL3_FOUND)
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

set(PACGAL_CORE src/game.c src/screen.c src/sound.c)
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

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
