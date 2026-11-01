#include <catch2/catch_test_macros.hpp>

#include "engine/SessionLog.h"

#include <juce_events/juce_events.h>

using namespace pr7s;

TEST_CASE ("The session log: a readable file per session, closed normally or not, crashes and freezes written down (0.9.14)", "[log]")
{
    const auto folder = juce::File::createTempFile ("pr7s-logs");
    folder.createDirectory();

    // A session that closes normally.
    CHECK (SessionLog::start ("Paragon PR7-S test", folder, 3) == juce::File());   // nothing unfinished before
    const auto first = SessionLog::getFile();
    REQUIRE (first.existsAsFile());
    CHECK (first.getParentDirectory() == folder);
    CHECK (first.getFileName().startsWith ("PR7-S "));
    CHECK (first.getFileName().endsWith (".log"));
    SessionLog::note ("Opening a song");
    {
        const auto text = first.loadFileAsString();   // written at once (a crash next would keep it)
        CHECK (text.startsWith ("Paragon PR7-S test"));
        CHECK (text.contains ("System: "));
        CHECK (text.contains ("Processor: "));
        CHECK (text.contains ("Memory: "));
        CHECK (text.contains ("Opening a song"));
        CHECK_FALSE (SessionLog::closedNormally (first));
    }
    SessionLog::stop();
    CHECK (SessionLog::closedNormally (first));
    CHECK_FALSE (SessionLog::recordsCrash (first));

    // A session that crashes: the next start hands back its log.
    juce::Thread::sleep (1100);   // (a new second: a file name of its own)
    CHECK (SessionLog::start ("Paragon PR7-S test", folder, 3) == juce::File());
    const auto second = SessionLog::getFile();
    CHECK (second != first);
    SessionLog::note ("Loading the effect Kontakt 8");
    const auto crash = SessionLog::describeCrash ("a test crash", nullptr);
    CHECK (crash.contains ("CRASH: a test crash"));
    CHECK (crash.contains ("screen's thread"));      // (the test runs on the message thread)
    CHECK (crash.contains ("The calls that led there"));
    // As a real crash would leave it: written through the log (the crash handler
    // writes to its open file; a second writer can't open it on Windows), never closed.
    SessionLog::note (crash);
    juce::Thread::sleep (1100);
    // (the crashed session's stream is let go without its last line)
    const auto unfinished = SessionLog::start ("Paragon PR7-S test", folder, 3);
    CHECK (unfinished == second);
    CHECK (SessionLog::recordsCrash (second));
    CHECK_FALSE (SessionLog::closedNormally (second));

    // The audio thread is told apart.
    auto marked = false, said = false;
    std::thread audio ([&] { SessionLog::markAudioThread(); marked = SessionLog::isAudioThread(); said = SessionLog::describeCrash ("x", nullptr).contains ("the audio thread"); });
    audio.join();
    CHECK (marked);
    CHECK (said);
    CHECK_FALSE (SessionLog::isAudioThread());

    // A freeze: no beat for longer than the limit writes FROZEN, then the answer.
    SessionLog::startFreezeWatch (0.6);
    juce::Thread::sleep (1800);
    SessionLog::beat();
    juce::Thread::sleep (700);
    SessionLog::stopFreezeWatch();
    {
        const auto text = SessionLog::getFile().loadFileAsString();
        CHECK (text.contains ("FROZEN: the screen hasn't answered for"));
        CHECK (text.contains ("The screen answered again after"));
    }
    SessionLog::stop();

    // Only the newest `keep` (3) are kept.
    juce::Thread::sleep (1100);
    SessionLog::start ("Paragon PR7-S test", folder, 3);
    SessionLog::stop();
    CHECK (folder.findChildFiles (juce::File::findFiles, false, "PR7-S *.log").size() == 3);
    CHECK_FALSE (first.existsAsFile());

    folder.deleteRecursively();
}
