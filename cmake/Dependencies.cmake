# Third-party dependencies, pinned to exact versions so every machine and CI
# runner builds against identical code. (The same JUCE as the rest of the PR7 family.)

include(FetchContent)

# JUCE: audio application framework (AGPLv3 for open-source projects).
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(juce)
