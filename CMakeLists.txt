cmake_minimum_required(VERSION 3.16)
project(MagicII C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MAGICII_USE_FETCHCONTENT_RAYLIB "Fetch raylib with CMake if not installed" ON)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

if(ANDROID)
    set(PLATFORM Android CACHE STRING "" FORCE)
else()
    set(PLATFORM Desktop CACHE STRING "" FORCE)
endif()

if(MAGICII_USE_FETCHCONTENT_RAYLIB)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    FetchContent_MakeAvailable(raylib)
else()
    find_package(raylib REQUIRED)
endif()

set(MAGICII_SOURCES
    src/main.c
    src/ui.c
    src/util.c
    src/npc.c
)

if(ANDROID)
    add_library(metsys_engine SHARED ${MAGICII_SOURCES})
    set_target_properties(metsys_engine PROPERTIES OUTPUT_NAME main)
    target_compile_definitions(metsys_engine PRIVATE PLATFORM_ANDROID)

    if(DEFINED ANDROID_NDK)
        target_sources(metsys_engine PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
        target_include_directories(metsys_engine PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
    endif()
else()
    add_executable(metsys_engine ${MAGICII_SOURCES})
endif()

target_include_directories(metsys_engine PRIVATE src /usr/local/include /opt/homebrew/include)
target_link_libraries(metsys_engine PRIVATE raylib)

if(ANDROID)
    target_link_libraries(metsys_engine PRIVATE log android m)
endif()
