cmake_minimum_required(VERSION 3.24)

# macOS: build one universal binary for Apple Silicon and Intel Macs.
# These must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

project(ParagonPR7S VERSION 0.7.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PARAGON_BUILD_TESTS "Build the unit tests" ON)

# Windows: the C++ runtime built into the app, so it runs without installing
# Microsoft's runtime first.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(cmake/Dependencies.cmake)

# Inter and Bebas Neue (SIL Open Font License, see resources/fonts/OFL-LICENSE.txt
# and BebasNeue-OFL-LICENSE.txt), built
# in so the studio looks the same on every computer.
juce_add_binary_data(ParagonPR7SFonts
    HEADER_NAME ParagonFonts.h
    NAMESPACE   ParagonFonts
    SOURCES
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-SemiBold.ttf
        resources/fonts/InterDisplay-BlackItalic.ttf
        resources/fonts/BebasNeue-Regular.ttf)

# Everything but main(): the engine, the project, the UI. A static library,
# so the app and the tests run exactly the same code. (JUCE's pattern for
# code shared between targets: the modules are compiled here once, and
# whoever links this gets its include paths and settings.)
add_library(PR7SCore STATIC
    source/engine/Project.cpp
    source/engine/ParagonSynth.cpp
    source/engine/PluginHost.cpp
    source/engine/Engine.cpp
    source/engine/Studio.cpp
    source/engine/MidiImport.cpp
    source/ui/ParagonLookAndFeel.cpp
    source/ui/Finish.cpp
    source/ui/LedDisplay.cpp
    source/ui/Logo.cpp
    source/ui/LevelMeter.cpp
    source/ui/TransportBar.cpp
    source/ui/TrackList.cpp
    source/ui/ArrangeView.cpp
    source/ui/KeyEditor.cpp
    source/ui/MixerView.cpp
    source/ui/MainView.cpp
    source/ui/Windows.cpp)

target_include_directories(PR7SCore PUBLIC source)

target_compile_definitions(PR7SCore
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1      # it loads VST3 instruments and effects
        JUCE_PLUGINHOST_AU=0
        PR7S_VERSION="${PROJECT_VERSION}")   # the studio's version, for the About text and the synth

target_link_libraries(PR7SCore
    PRIVATE
        ParagonPR7SFonts
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_warning_flags   # strict warnings for our code only
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ASIO on Windows (the lowest delay, through the interface's own ASIO driver):
# needs Steinberg's ASIO SDK, used under its GPLv3 option (PR7-S is AGPLv3).
# CI downloads it and passes its folder here; without it, Windows uses its own
# audio (WASAPI). ASIO is a trademark and software of Steinberg Media
# Technologies GmbH.
set(PARAGON_ASIO_SDK "" CACHE PATH "The ASIO SDK's folder (the one with common/iasiodrv.h), for ASIO on Windows")
if(WIN32 AND PARAGON_ASIO_SDK AND EXISTS "${PARAGON_ASIO_SDK}/common/iasiodrv.h")
    message(STATUS "ASIO: on, with the SDK in ${PARAGON_ASIO_SDK}")
    target_compile_definitions(PR7SCore PUBLIC JUCE_ASIO=1)
    target_include_directories(PR7SCore PUBLIC "${PARAGON_ASIO_SDK}/common")
elseif(WIN32)
    message(STATUS "ASIO: off (no ASIO SDK given: PARAGON_ASIO_SDK)")
endif()

# Whoever links PR7SCore sees the JUCE modules' headers and settings too.
target_include_directories(PR7SCore INTERFACE $<TARGET_PROPERTY:PR7SCore,INCLUDE_DIRECTORIES>)
target_compile_definitions(PR7SCore INTERFACE $<TARGET_PROPERTY:PR7SCore,COMPILE_DEFINITIONS>)
set_target_properties(PR7SCore PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    VISIBILITY_INLINES_HIDDEN TRUE
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

# The studio itself: a standalone app.
juce_add_gui_app(ParagonPR7S
    PRODUCT_NAME                "Paragon PR7-S"
    COMPANY_NAME                "Paragon"
    BUNDLE_ID                   "com.paragon.paragonpr7s"
    MICROPHONE_PERMISSION_ENABLED TRUE)

target_sources(ParagonPR7S PRIVATE source/Main.cpp)
target_link_libraries(ParagonPR7S PRIVATE PR7SCore)

if(PARAGON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
