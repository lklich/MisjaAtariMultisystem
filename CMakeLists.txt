# Misja – gra przepisana z Atari XE na C++ i SDL3 (macOS, Windows, Linux)
cmake_minimum_required(VERSION 3.20)

option(MISJA_SYSTEM_SDL "Użyj SDL3 zainstalowanego w systemie" OFF)

# macOS: minimalna wersja systemu (jak w wersji Swift) i architektury. Musi być ustawione
# przed project(). Domyślnie aplikacja uniwersalna (Apple Silicon + Intel). SDL3 z Homebrew
# jest tylko dla procesora tego Maca, więc z MISJA_SYSTEM_SDL=ON domyślna jest jedna
# architektura. Wybór ręczny: -DCMAKE_OSX_ARCHITECTURES=arm64 (x86_64, "arm64;x86_64")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimalna wersja macOS")
    if(MISJA_SYSTEM_SDL)
        set(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}" CACHE STRING "Architektury macOS")
    else()
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architektury macOS")
    endif()
endif()

project(Misja VERSION 1.0.0 LANGUAGES C CXX)

# macOS: Objective-C(++) musi być włączone w głównym zakresie przed pobraniem SDL3
# (SDL włącza OBJC we własnym podkatalogu, a NativeMenu_mac.mm wymaga OBJCXX)
if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Typ budowania" FORCE)
endif()


# Windows (MSVC): statyczne CRT dla programu i SDL – Misja.exe działa bez Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- SDL3 ---------------------------------------------------------------------------------
# Domyślnie SDL3 jest pobierane z GitHuba i linkowane statycznie (jeden plik programu,
# bez dodatkowych bibliotek). -DMISJA_SYSTEM_SDL=ON używa SDL3 zainstalowanego w systemie
# (np. brew install sdl3, vcpkg install sdl3).
set(MISJA_SDL_TAG "release-3.4.18" CACHE STRING "Wersja SDL3 pobierana z GitHuba")

if(MISJA_SYSTEM_SDL)
    find_package(SDL3 3.4 REQUIRED CONFIG)
else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${MISJA_SDL_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# --- Program ------------------------------------------------------------------------------
set(MISJA_SOURCES
    src/main.cpp
    src/App.cpp
    src/Game.cpp
    src/Machine.cpp
    src/Misja.cpp
    src/MisjaTitle.cpp
    src/MisjaSound.cpp
    src/MisjaGame.cpp
    src/MisjaPlayer.cpp
    src/MisjaShots.cpp
    src/MisjaObjects.cpp
    src/MisjaRooms.cpp
    src/OriginalTape.cpp
    src/Pokey.cpp
    src/SaveStore.cpp
    src/Sha256.cpp
    src/Storage.cpp
)

# Systemowe menu „Gra” (jak w wersji Swift)
# Kod gry działa w osobnym wątku (std::thread)
find_package(Threads REQUIRED)

if(APPLE)
    list(APPEND MISJA_SOURCES src/NativeMenu_mac.mm)
elseif(WIN32)
    list(APPEND MISJA_SOURCES src/NativeMenu_win.cpp)
else()
    list(APPEND MISJA_SOURCES src/NativeMenu_none.cpp)
endif()

if(APPLE)
    set(MISJA_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/misja.icns)
    set_source_files_properties(${MISJA_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(Misja MACOSX_BUNDLE ${MISJA_SOURCES} ${MISJA_ICON})
    set_target_properties(Misja PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/res/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER pl.lk.misja
        MACOSX_BUNDLE_BUNDLE_NAME Misja
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER pl.lk.misja
    )
elseif(WIN32)
    add_executable(Misja WIN32 ${MISJA_SOURCES} res/misja.rc)
else()
    add_executable(Misja ${MISJA_SOURCES})
endif()

#target_link_libraries(Misja PRIVATE SDL3::SDL3 Threads::Threads)

if(MISJA_SYSTEM_SDL)
    target_link_libraries(Misja PRIVATE SDL3::SDL3 Threads::Threads)
else()
    target_link_libraries(Misja PRIVATE SDL3::SDL3-static Threads::Threads)
endif()


if(APPLE)
    target_link_libraries(Misja PRIVATE "-framework Cocoa")
endif()

if(MSVC)
    # źródła w UTF-8 (polskie napisy), bez ostrzeżeń o „niebezpiecznych” funkcjach CRT
    target_compile_options(Misja PRIVATE /utf-8 /W3)
    target_compile_definitions(Misja PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Misja)
else()
    target_compile_options(Misja PRIVATE -Wall -Wextra)
endif()

if(MINGW)
    # MinGW: bez zależności od bibliotek libgcc/libstdc++ w DLL
    target_link_options(Misja PRIVATE -static -static-libgcc -static-libstdc++)
endif()
