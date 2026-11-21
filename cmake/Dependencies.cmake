# Third-party dependencies, pinned to exact versions so every machine and CI
# runner builds against identical code. (The same JUCE as the rest of the PR7 family.)

include(FetchContent)

# JUCE: audio application framework (AGPLv3 for open-source projects).
# (0.76.1) Patched after fetching: cmake/PatchJuce.cmake (DRAW WITHOUT THE GRAPHICS CARD: no Direct2D at all).
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/PatchJuce.cmake)

FetchContent_MakeAvailable(juce)

# Signalsmith Stretch: time-stretching and pitch-shifting (MIT licence; it fetches its own
# signalsmith-linear 0.6.4, also MIT). 0.23.0, the owner's choice: the MediaBay's previews
# stretched to the song's tempo and transposed.
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        1.4.0
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(signalsmith-stretch)

# Rubber Band Library: time-stretching (GPL v2 or later, used under GPL v3 with PR7-S's AGPLv3).
# 0.188.0: a fifth Time Stretch Algorithm, RUBBER BAND (its R3 "finer" engine), to compare with
# CLEAN on the same clip. Built from its single-file source (single/RubberBandSingle.cpp: the
# built-in FFT and resampler, no other libraries) straight into PR7SCore: CMakeLists.txt.
# It has no CMakeLists of its own (it builds with Meson), so only its files are fetched.
FetchContent_Declare(rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        v4.0.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  no-cmake-here)

FetchContent_MakeAvailable(rubberband)

# LAME (LGPL v2 or later): the MP3 encoder. 0.200.0, the owner asked ("build LAME in"): EXPORT
# writes MP3 files. Only libmp3lame's C sources are compiled (CMakeLists.txt: the pr7s_lame
# library, with our own config.h in cmake/lame/: no assembly, no decoder); the tarball has no
# CMakeLists of its own, so only its files are fetched. (github's mirror isn't reachable from
# every machine; SourceForge's is LAME's own download.)
FetchContent_Declare(lame
    URL            https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
    URL_HASH       SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR  no-cmake-here)

FetchContent_MakeAvailable(lame)
