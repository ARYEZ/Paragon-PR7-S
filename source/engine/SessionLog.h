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
// If the screen stops answering for 10 seconds, a FROZEN entry: written at once
// with the steps under way (0.52.1), then what every thread is doing (Windows:
// the module each is in and the modules on its stack, read without anything a
// stuck plug-in could be holding; then, last, the calls by name), and how long
// until it answered again. A render that stops moving: STUCK, the same report.
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

    // (0.52.1) A step under way: "Starting: <what>" written at once, "  done: <what> (n ms)" when it
    // ends. While it runs it's listed in a FROZEN or STUCK entry, with its thread and how long it's
    // been going: a hang shows what had begun, not only what had finished. Any thread but the
    // audio callback's. Nested steps are listed inside out.
    class Step
    {
    public:
        explicit Step (const juce::String& what);
        ~Step();
        Step (const Step&) = delete;
        Step& operator= (const Step&) = delete;
    private:
        int slot = -1;
        double started = 0.0;
        juce::String text;
    };
    // What's under way now (each step's thread, how long, what): the lines a FROZEN entry gives.
    juce::String stepsUnderWay();

    // (0.52.1) A long job on a thread of its own (a render): `jobBeat` as it moves (any thread, cheap:
    // an atomic). No beat for `jobSeconds` (startFreezeWatch) while one runs writes a STUCK entry
    // (the screen still answers then, so FROZEN never would).
    class Job
    {
    public:
        explicit Job (const juce::String& what);
        ~Job();
        Job (const Job&) = delete;
        Job& operator= (const Job&) = delete;
    };
    void jobBeat() noexcept;

    // (0.52.1) Closing: the watch carries on (with `seconds` as its limit), so a hang while
    // plug-ins unload is written down too; stop() ends it.
    void closing (double seconds = 30.0);

    // Catches crashes (and std::terminate, abort) into the log. Once, after start.
    void installCrashHandler();
    // Watches the screen's thread: call `beat` from it (a Timer) a few times
    // a second; a gap of `seconds` writes a FROZEN entry.
    void startFreezeWatch (double seconds = 10.0, double jobSeconds = 30.0);
    void beat();
    void stopFreezeWatch();

    // The audio device's callback marks its thread (a thread_local flag:
    // no allocation), so a crash can say it happened there.
    void markAudioThread (bool on = true) noexcept;
    bool isAudioThread() noexcept;

    // What a crash writes (also for tests: `platformInfo` as JUCE's crash handler gets it, or nullptr).
    juce::String describeCrash (const juce::String& what, void* platformInfo);
}
