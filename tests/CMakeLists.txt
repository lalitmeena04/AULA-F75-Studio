# --- GoogleTest / GMock: fetched, used for pure-logic tests ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
# Match Qt dynamically-linked CRT on MSVC.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# One GoogleTest executable aggregating every per-class test file.
add_executable(aulaf75_gtest
    gtest_main.cpp
    Controllers/GameModeControllerTest.cpp
    Controllers/LightingPreviewControllerTest.cpp
    Model/GameModeRulesTest.cpp
    Model/KeyLayoutTest.cpp
    Model/LightingPreviewTest.cpp
    Model/ProfileStoreTest.cpp
    Model/ProtocolTest.cpp
    Views/QmlContractTest.cpp
)
target_link_libraries(aulaf75_gtest PRIVATE
    aulaf75_controllers
    Qt6::Test
    GTest::gtest
    GTest::gmock
)
target_compile_definitions(aulaf75_gtest PRIVATE AULAF75STUDIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(MSVC)
    target_compile_options(aulaf75_gtest PRIVATE /utf-8)
endif()
add_test(NAME gtest_all COMMAND aulaf75_gtest)

# Headless render of every page through the real QML module.
add_test(NAME qml_smoke COMMAND AulaF75Studio --smoke)
set_tests_properties(qml_smoke PROPERTIES TIMEOUT 40)

# Qt DLLs live in the Qt bin dir. Prepend it to PATH for test executables.
set_tests_properties(gtest_all qml_smoke PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
