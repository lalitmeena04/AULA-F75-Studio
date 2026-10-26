cmake_minimum_required(VERSION 3.24)
project(AulaF75Studio VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "AULA F75 Studio talks to the keyboard through Windows HID and WASAPI.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Concurrent Test)
qt_standard_project_setup(REQUIRES 6.8)

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

qt_add_library(f75_core STATIC
    src/Protocol.cpp src/Protocol.h
    src/KeyLayout.cpp src/KeyLayout.h
    src/HidTransport.cpp src/HidTransport.h
    src/ProfileStore.cpp src/ProfileStore.h
    src/HostEffects.cpp src/HostEffects.h)
target_include_directories(f75_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(f75_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent PRIVATE setupapi cfgmgr32 hid ole32 pdh)
target_compile_definitions(f75_core PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

qt_add_executable(AulaF75Studio src/main.cpp)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(AulaF75Studio
    URI AulaF75Studio
    VERSION 1.0
    SOURCES
        src/Device.cpp src/Device.h
        src/IconService.cpp src/IconService.h
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/Card.qml
        qml/Chip.qml
        qml/Eyebrow.qml
        qml/AccentButton.qml
        qml/LabeledSlider.qml
        qml/ColorField.qml
        qml/KeyboardView.qml
        qml/Rail.qml
        qml/TopBar.qml
        qml/StatusBanner.qml
        qml/HomePage.qml
        qml/LightingPage.qml
        qml/KeysPage.qml
        qml/MacrosPage.qml
        qml/PowerPage.qml
        qml/ProfilesPage.qml
        qml/DiagnosticsPage.qml
)
target_link_libraries(AulaF75Studio PRIVATE f75_core awesome_fonts Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(AulaF75Studio PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(f75_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(AulaF75Studio PRIVATE /W4 /permissive- /utf-8)
endif()

enable_testing()
qt_add_executable(protocol_tests tests/ProtocolTest.cpp)
target_link_libraries(protocol_tests PRIVATE f75_core Qt6::Test)
add_test(NAME protocol COMMAND protocol_tests)
add_test(NAME qml_smoke COMMAND AulaF75Studio --smoke)
set_tests_properties(qml_smoke PROPERTIES TIMEOUT 40)
