FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(catch2)

add_executable(ParagonPR7STests
    TestMain.cpp
    ProjectTests.cpp
    EngineTests.cpp
    MixerTests.cpp
    AudioTests.cpp
    ExportTests.cpp
    AutomationTests.cpp
    SendTests.cpp
    MidiImportTests.cpp
    EditTests.cpp
    ImportAudioTests.cpp
    DeviceTests.cpp
    UiTests.cpp
    Snapshot.cpp)

# The studio's own code (PR7SCore), so the tests exercise exactly what ships.
target_link_libraries(ParagonPR7STests PRIVATE PR7SCore Catch2::Catch2)
# Files the tests read (tests/data: a short MP3, made with ffmpeg: a 440 Hz tone).
target_compile_definitions(ParagonPR7STests PRIVATE PR7S_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(ParagonPR7STests)

# Linux: the tests talk to the X display directly once (see TestMain.cpp).
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(ParagonPR7STests PRIVATE X11::X11)
endif()
