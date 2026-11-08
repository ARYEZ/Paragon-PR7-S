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
