cmake_minimum_required(VERSION 3.21)

# Single version source for app and CI.
# First non-blank line only. Stray blank lines must not leak into the define.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt" AULAF75STUDIO_VERSION_BASE REGEX "[^ \t\r\n]" LIMIT_COUNT 1)
string(STRIP "${AULAF75STUDIO_VERSION_BASE}" AULAF75STUDIO_VERSION_BASE)

# CMake project version needs numeric x.y.z. Strip pre-release suffix.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" AULAF75STUDIO_VERSION_NUMERIC "${AULAF75STUDIO_VERSION_BASE}")

project(AulaF75Studio VERSION ${AULAF75STUDIO_VERSION_NUMERIC} LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "AULA F75 Studio talks to the keyboard through Windows HID and WASAPI.")
endif()

# Release builds have plain version.
# Dev builds have "-<build number>" suffix.
# Build number can be explicit or use git count and short hash.
set(AULAF75STUDIO_VERSION "${AULAF75STUDIO_VERSION_BASE}")
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR DEFINED AULAF75STUDIO_BUILD_NUMBER)
    if(NOT DEFINED AULAF75STUDIO_BUILD_NUMBER)
        find_package(Git QUIET)
        if(GIT_FOUND)
            execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE AULAF75STUDIO_GIT_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE AULAF75STUDIO_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        endif()
        if(AULAF75STUDIO_GIT_COUNT AND AULAF75STUDIO_GIT_SHA)
            set(AULAF75STUDIO_BUILD_NUMBER "${AULAF75STUDIO_GIT_COUNT}.${AULAF75STUDIO_GIT_SHA}")
        elseif(AULAF75STUDIO_GIT_COUNT)
            set(AULAF75STUDIO_BUILD_NUMBER "${AULAF75STUDIO_GIT_COUNT}")
        else()
            set(AULAF75STUDIO_BUILD_NUMBER "dev")
        endif()
    endif()
    set(AULAF75STUDIO_VERSION "${AULAF75STUDIO_VERSION_BASE}-${AULAF75STUDIO_BUILD_NUMBER}")
endif()
message(STATUS "AulaF75Studio version: ${AULAF75STUDIO_VERSION}")

# Expose version to C++ through Controllers/Version.h.
add_compile_definitions(AULAF75STUDIO_VERSION="${AULAF75STUDIO_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Concurrent Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.8)

# File logging uses spdlog. Prefer system package. Else fetch pinned release.
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
    )
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

# Font Awesome 6 Free (OFL fonts, MIT packaging) via the QtAwesome project.
# Used only as the fallback icon set when Segoe Fluent Icons is unavailable.
include(FetchContent)
FetchContent_Declare(QtAwesome
    GIT_REPOSITORY https://github.com/gamecreature/QtAwesome.git
    GIT_TAG        9e7d92dd5dab5ef8c976ab426f4c18370008c3cf   # font-awesome-6.7.2
    GIT_SHALLOW    TRUE)
FetchContent_Populate(QtAwesome)
qt_add_library(awesome_fonts STATIC ${qtawesome_SOURCE_DIR}/QtAwesome/QtAwesomeFree.qrc)
target_include_directories(awesome_fonts INTERFACE ${qtawesome_SOURCE_DIR}/QtAwesome)
target_link_libraries(awesome_fonts PUBLIC Qt6::Core)
set_target_properties(awesome_fonts PROPERTIES AUTORCC ON)

option(AULAF75STUDIO_BUILD_TESTS "Build the unit tests" ON)
if(AULAF75STUDIO_BUILD_TESTS)
    enable_testing()
endif()

# --- Model: pure protocol / layout / profile logic for app and tests ---
qt_add_library(aulaf75_core STATIC
    Logging.cpp
    Logging.h
    Model/GameModeRules.cpp
    Model/GameModeRules.h
    Model/KeyLayout.cpp
    Model/KeyLayout.h
    Model/LightingPreview.cpp
    Model/LightingPreview.h
    Model/ProfileStore.cpp
    Model/ProfileStore.h
    Model/Protocol.cpp
    Model/Protocol.h
)
target_link_libraries(aulaf75_core PUBLIC
    Qt6::Core
    Qt6::Gui
    spdlog::spdlog
)
# Logging on compiles TRC calls at or above AULAF75STUDIO_LOG_LEVEL. Logging off removes them all.
# Default level: DEBUG for Debug builds, INFO otherwise. INFO keeps release logs small.
option(AULAF75STUDIO_ENABLE_LOGGING "Enable spdlog file logging" ON)
set(AULAF75STUDIO_LOG_LEVEL "" CACHE STRING "Compile-time log level: DEBUG, INFO or empty for auto")
set_property(CACHE AULAF75STUDIO_LOG_LEVEL PROPERTY STRINGS "" DEBUG INFO)
if(NOT AULAF75STUDIO_ENABLE_LOGGING)
    set(AULAF75STUDIO_SPDLOG_LEVEL SPDLOG_LEVEL_OFF)
elseif(AULAF75STUDIO_LOG_LEVEL STREQUAL "DEBUG")
    set(AULAF75STUDIO_SPDLOG_LEVEL SPDLOG_LEVEL_DEBUG)
elseif(AULAF75STUDIO_LOG_LEVEL STREQUAL "INFO")
    set(AULAF75STUDIO_SPDLOG_LEVEL SPDLOG_LEVEL_INFO)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(AULAF75STUDIO_SPDLOG_LEVEL SPDLOG_LEVEL_DEBUG)
else()
    set(AULAF75STUDIO_SPDLOG_LEVEL SPDLOG_LEVEL_INFO)
endif()
message(STATUS "AulaF75Studio log level: ${AULAF75STUDIO_SPDLOG_LEVEL}")
target_compile_definitions(aulaf75_core PUBLIC SPDLOG_ACTIVE_LEVEL=${AULAF75STUDIO_SPDLOG_LEVEL})
target_compile_definitions(aulaf75_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(aulaf75_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Model
)

# --- Controllers: Windows HID / WASAPI / PDH code shared by app and tests ---
qt_add_library(aulaf75_controllers STATIC
    Controllers/CommandLine.cpp
    Controllers/CommandLine.h
    Controllers/DeviceController.cpp
    Controllers/DeviceController.h
    Controllers/DeviceWatcher.cpp
    Controllers/DeviceWatcher.h
    Controllers/GameModeController.cpp
    Controllers/GameModeController.h
    Controllers/HidTransport.cpp
    Controllers/HidTransport.h
    Controllers/HostEffectsController.cpp
    Controllers/HostEffectsController.h
    Controllers/LightingPreviewController.cpp
    Controllers/LightingPreviewController.h
    Controllers/Version.h
)
target_link_libraries(aulaf75_controllers
    PUBLIC
        aulaf75_core
        Qt6::Concurrent
    PRIVATE
        setupapi
        cfgmgr32
        hid
        ole32
        pdh
        user32
)
target_include_directories(aulaf75_controllers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Controllers)

# --- Application executable ---
qt_add_executable(AulaF75Studio
    main.cpp
    MainControllerClass.cpp
    MainControllerClass.h
)

# Emit runnable app and windeployqt runtime into <root>/bin.
set_target_properties(AulaF75Studio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

set_source_files_properties(Views/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(AulaF75Studio
    URI AulaF75Studio
    VERSION 1.0
    SOURCES
        Controllers/IconService.cpp
        Controllers/IconService.h
    QML_FILES
        Views/AccentButton.qml
        Views/Card.qml
        Views/Chip.qml
        Views/ColorField.qml
        Views/DiagnosticsPage.qml
        Views/Eyebrow.qml
        Views/HomePage.qml
        Views/Icon.qml
        Views/IconButton.qml
        Views/KeyboardView.qml
        Views/KeysPage.qml
        Views/LabeledSlider.qml
        Views/LightingPage.qml
        Views/MacrosPage.qml
        Views/Main.qml
        Views/PowerPage.qml
        Views/ProfilesPage.qml
        Views/Rail.qml
        Views/StatusBanner.qml
        Views/Theme.qml
        Views/TopBar.qml
)

target_link_libraries(AulaF75Studio PRIVATE
    aulaf75_controllers
    awesome_fonts
    Qt6::Quick
    Qt6::QuickControls2
)

if(MSVC)
    foreach(target aulaf75_core aulaf75_controllers AulaF75Studio)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    endforeach()
endif()

if(AULAF75STUDIO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
