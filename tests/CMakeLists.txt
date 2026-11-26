FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(catch2)

add_executable(ParagonPR7STests
    TestMain.cpp
    PowerChordKeyTests.cpp
    ProjectTests.cpp
    EngineTests.cpp
    MixerTests.cpp
    MultiCoreTests.cpp
    AudioTests.cpp
    ExportTests.cpp
    AutomationTests.cpp
    SendTests.cpp
    MidiImportTests.cpp
    DawProjectTests.cpp
    SessionLogTests.cpp
    TempoTests.cpp
    KeyCommandTests.cpp
    WorkflowTests.cpp
    HubTests.cpp
    EditTests.cpp
    ImportAudioTests.cpp
    DeviceTests.cpp
    ReviewTests.cpp
    ChordPadTests.cpp
    ChordTrackTests.cpp
    LinkTests.cpp
    LineTests.cpp
    RiffTests.cpp
    RiffGeneratorTests.cpp
    DragOutTests.cpp
    DrumIdeaTests.cpp
    DrumGeneratorTests.cpp
    DrumLearningTests.cpp
    DrumLearnUiTests.cpp
    BuildSectionTests.cpp
    ComposerTests.cpp
    ComposerTracksTests.cpp
    ArchitectLevelsTests.cpp
    ArchitectSongTests.cpp
    BarLockTests.cpp
    ComposerSpeedTests.cpp
    InstrumentTests.cpp
    GrooveTests.cpp
    SoloTests.cpp
    SoloStyleTests.cpp
    DelayCompensationTests.cpp
    GroupRoutingTests.cpp
    NewSongTests.cpp
    SpeedTests.cpp
    MediaBayTests.cpp
    ControllerTests.cpp
    DrumTests.cpp
    AutomationEditTests.cpp
    AutomationPanelTests.cpp
    TakeTests.cpp
    AutoFadeTests.cpp
    RenderKeyTests.cpp
    WarpTests.cpp
    SizingTests.cpp
    RubberBandTests.cpp
    AlignTests.cpp
    VariAudioTests.cpp
    VariAudio2Tests.cpp
    FollowChordTests.cpp
    PatternTests.cpp
    ProgressionTests.cpp
    MelodyTests.cpp
    ChordDetectTests.cpp
    KeyCheckTests.cpp
    EqTests.cpp
    ChordTool2Tests.cpp
    SignatureTests.cpp
    InfoLinePartsTests.cpp
    MarkerTests.cpp
    HistoryTests.cpp
    Speed083Tests.cpp
    Speed0852Tests.cpp
    Speed0123Tests.cpp
    Speed0164Tests.cpp
    Speed0165Tests.cpp
    Speed0166Tests.cpp
    AutomationDotTests.cpp
    Speed0174Tests.cpp
    Speed0175Tests.cpp
    Speed0176Tests.cpp
    LibraryOptionsTests.cpp
    FavouriteFoldersTests.cpp
    Speed0179Tests.cpp
    AutoHearTests.cpp
    ExpressionControllerTests.cpp
    Speed0173Tests.cpp
    MediaBaySpeedTests.cpp
    Speed0113Tests.cpp
    CleanGuitarTests.cpp
    AcousticLaneTests.cpp
    GuitarToneTests.cpp
    SectionToneTests.cpp
    MyRiffLaneTests.cpp
    BlockRiffTests.cpp
    RiffFinderTests.cpp
    SongKeyTests.cpp
    BlueprintTests.cpp
    Tweaks0853Tests.cpp
    SyncTests.cpp
    DroningBandTests.cpp
    SongSectionTests.cpp
    ChordRollTests.cpp
    InsertSilenceTests.cpp
    ComposerOctaveTests.cpp
    LaneWorldTests.cpp
    SynthPatchTests.cpp
    OptionBrowserTests.cpp
    ChordGuideTests.cpp
    LibraryDrumsTests.cpp
    GroovePoolTests.cpp
    Pr7dGroovesTests.cpp
    Pr7dLinkTests.cpp
    FolderTests.cpp
    StemExportTests.cpp
    ExportFormatTests.cpp
    PerformanceTests.cpp
    UiTests.cpp
    Tweaks071Tests.cpp
    Tweaks073Tests.cpp
    Tweaks077Tests.cpp
    Tweaks078Tests.cpp
    AutoColourTests.cpp
    Tweaks080Tests.cpp
    Tweaks081Tests.cpp
    BandRecordTests.cpp
    FastPickingTests.cpp
    BandSectionTests.cpp
    AutoSaveTests.cpp
    Speed0190Tests.cpp
    Speed0191Tests.cpp
    PerfTests.cpp
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
