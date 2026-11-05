cmake_minimum_required(VERSION 3.24)

# macOS: build one universal binary for Apple Silicon and Intel Macs.
# These must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

project(ParagonPR7S VERSION 0.57.0 LANGUAGES C CXX)

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
    source/engine/TempoMap.cpp
    source/engine/Signature.cpp
    source/engine/Markers.cpp
    source/engine/MidiExport.cpp
    source/engine/TempoDetect.cpp
    source/engine/Chords.cpp
    source/engine/FollowChords.cpp
    source/engine/Patterns.cpp
    source/engine/Progressions.cpp
    source/engine/Melody.cpp
    source/engine/ChordDetect.cpp
    source/engine/Eq.cpp
    source/engine/Lines.cpp
    source/engine/Riffs.cpp
    source/engine/DragOut.cpp
    source/engine/DrumIdeas.cpp
    source/engine/DrumGenerator.cpp
    source/engine/DrumLearning.cpp
    source/engine/SectionPlan.cpp
    source/engine/MediaBay.cpp
    source/engine/ParagonSynth.cpp
    source/engine/PluginHost.cpp
    source/engine/AudioWorkers.cpp
    source/engine/Engine.cpp
    source/engine/Studio.cpp
    source/engine/MidiImport.cpp
    source/engine/DawProject.cpp
    source/engine/AudioAdvice.cpp
    source/engine/SessionLog.cpp
    source/engine/ProjectLibrary.cpp
    source/engine/Stretch.cpp
    source/engine/Warp.cpp
    source/engine/Align.cpp
    source/engine/VariAudio.cpp
    source/engine/Controllers.cpp
    source/engine/DrumMaps.cpp
    source/ui/ParagonLookAndFeel.cpp
    source/ui/Finish.cpp
    source/ui/LedDisplay.cpp
    source/ui/Logo.cpp
    source/ui/LevelMeter.cpp
    source/ui/TransportBar.cpp
    source/ui/TrackList.cpp
    source/ui/ArrangeView.cpp
    source/ui/ArrangeAutomation.cpp
    source/ui/ArrangeSignature.cpp
    source/ui/ArrangeMarkers.cpp
    source/ui/MarkersPage.cpp
    source/ui/ExportPages.cpp
    source/ui/TempoDetectionPanel.cpp
    source/ui/KeyEditor.cpp
    source/ui/KeyEditorLanes.cpp
    source/ui/KeyEditorDrums.cpp
    source/ui/DrumMapPage.cpp
    source/ui/AutomationPanel.cpp
    source/ui/AudioAlignmentPanel.cpp
    source/ui/VariAudioEditor.cpp
    source/ui/MixerView.cpp
    source/ui/Inspector.cpp
    source/ui/FollowSetupPage.cpp
    source/ui/InfoLine.cpp
    source/ui/TrackActions.cpp
    source/ui/MainView.cpp
    source/ui/MainViewCommands.cpp
    source/ui/KeyCommands.cpp
    source/ui/KeyCommandsPage.cpp
    source/ui/ProgressBox.cpp
    source/ui/Hub.cpp
    source/ui/ChordPadsView.cpp
    source/ui/ChordEventEditor.cpp
    source/ui/LinkGroupPage.cpp
    source/ui/AutoFadesPage.cpp
    source/ui/PluginsPage.cpp
    source/ui/TrackColours.cpp
    source/ui/LinesPage.cpp
    source/ui/ProgressionsPage.cpp
    source/ui/DrumLearnPage.cpp
    source/ui/EqEditor.cpp
    source/ui/PadsRemotePage.cpp
    source/ui/MediaBayPage.cpp
    source/ui/MediaUi.cpp
    source/ui/MediaRack.cpp
    source/ui/TempoEditor.cpp
    source/ui/Windows.cpp)

target_include_directories(PR7SCore PUBLIC source)

target_compile_definitions(PR7SCore
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1      # it loads VST3 instruments and effects
        JUCE_PLUGINHOST_AU=0
        JUCE_MODAL_LOOPS_PERMITTED=1   # (0.12.1: the screen's thread carries on while Render in Place / Freeze render on a thread of their own)
        PR7S_VERSION="${PROJECT_VERSION}")   # the studio's version, for the About text and the synth

target_link_libraries(PR7SCore
    PRIVATE
        signalsmith-stretch
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

# Windows: the app's .pdb (its functions' names and source lines) beside it in
# the zip, so the session log's crash entries name the studio's own code
# (0.9.14). /Z7 keeps each object's debug information in itself (no shared
# .pdb for parallel compiles to fight over); /OPT:REF,ICF keep the .exe as small.
if(MSVC)
    target_compile_options(PR7SCore PUBLIC $<$<CONFIG:Release>:/Z7>)
endif()

# The stretcher's own code is theirs: our strict warnings stay off for its one file.
if(MSVC)
    set_source_files_properties(source/engine/Stretch.cpp PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(source/engine/Stretch.cpp PROPERTIES COMPILE_OPTIONS "-w")
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
    # (0.52.3) The app's icon: the taskbar, the title bar, the .exe (Windows makes its .ico from these).
    # Drawn by Logo::drawAppIcon; written again with: PARAGON_ICON_DIR=<folder> ParagonPR7STests "[make-icon]".
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/icon-big.png"
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/icon-small.png"
    MICROPHONE_PERMISSION_ENABLED TRUE)

target_sources(ParagonPR7S PRIVATE source/Main.cpp)
target_link_libraries(ParagonPR7S PRIVATE PR7SCore)
if(MSVC)
    target_link_options(ParagonPR7S PRIVATE $<$<CONFIG:Release>:/DEBUG:FULL /OPT:REF /OPT:ICF>)
endif()

if(PARAGON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
