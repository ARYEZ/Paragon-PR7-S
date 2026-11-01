#pragma once

#include <juce_core/juce_core.h>

// The session log (0.9.14, the owner asked): a plain text file the studio
// writes as it runs, so when it ever shuts down on its own there's something
// anyone can read to find out why (unlike a .dmp, which needs a debugger).
//
// One file per session, "PR7-S <date> <time>.log" in the Logs folder beside
// the settings (Windows: %APPDATA%\Paragon PR7-S\Logs), the newest 30 kept.
// It holds: the version, the computer (system, processor, memory), the audio
// device, then a line for each thing the studio did that could go wrong (a
// song opened or saved, a plug-in loaded or its window opened, a render, the
// device changed...), each written to the disk at once. Its last line, when
// the studio is closed, says so.
//
// If it crashes: what went wrong (an access violation at an address, an
// abort...), on which thread (the screen's, the audio's, another), in which
// file (the studio itself, or a plug-in's .vst3: the culprit's name) and the
// chain of calls that led there, with function names (and, for the studio,
// source files and lines: the Windows zip has the app's .pdb for that).
// If the screen stops answering for 10 seconds, a FROZEN entry with what the
// screen's thread was doing (Windows), and how long until it answered again.
//
// The next start, a log that didn't end "closed normally" is offered to be
// shown (the studio crashed, froze and was ended, or the computer went off).
namespace pr7s::SessionLog
{
    // Starts this session's file in `folder` (the default: Logs beside the
    // settings), keeping the newest `keep`. `header`: the first lines.
    // Returns the previous session's log if it didn't close normally.
    juce::File start (const juce::String& header, juce::File folder = {}, int keep = 30);
    juce::File getFolder();   // the default Logs folder
    juce::File getFile();     // this session's (none before start)

    // A line, with the time, written to the disk at once (any thread, but
    // never the audio thread's callback: it allocates).
    void note (const juce::String&);

    // The last line ("closed normally") and the file closed.
    void stop();

    // Whether a log's session ended as it should; and whether it holds a CRASH entry.
    bool closedNormally (const juce::File&);
    bool recordsCrash (const juce::File&);

    // Catches crashes (and std::terminate, abort) into the log. Once, after start.
    void installCrashHandler();
    // Watches the screen's thread: call `beat` from it (a Timer) a few times
    // a second; a gap of `seconds` writes a FROZEN entry.
    void startFreezeWatch (double seconds = 10.0);
    void beat();
    void stopFreezeWatch();

    // The audio device's callback marks its thread (a thread_local flag:
    // no allocation), so a crash can say it happened there.
    void markAudioThread (bool on = true) noexcept;
    bool isAudioThread() noexcept;

    // What a crash writes (also for tests: `platformInfo` as JUCE's crash handler gets it, or nullptr).
    juce::String describeCrash (const juce::String& what, void* platformInfo);
}
