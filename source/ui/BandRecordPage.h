#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/BandRecord.h"
#include "engine/Studio.h"

#include <atomic>
#include <thread>

// LEARN MY BAND (0.82.0, the owner asked: the idea engine learning from what the owner's band played on
// their record). CHOOSE FOLDER... reads the record's folder into songs (engine/BandRecord.h): each song's
// drum MIDI, its DIs and who played each (guessed from the names; a menu per DI to put it right), its tempo
// (from the drum MIDI; typed when it's missing or wrong). READ THE SONGS hears every DI and writes a review
// song per song (Documents/Paragon PR7-S/Band Record/<record>/<song>): the drums lined up with the DIs, each
// DI with what was heard under it. OPEN opens one to put the notes right in the MIDI editor. What's learned
// from the corrected songs comes next (0.83.0).
class BandRecordPage final : public juce::Component,
                             private juce::Timer
{
public:
    explicit BandRecordPage (pr7s::Studio&);
    ~BandRecordPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // What the controls do (tests drive them).
    int loadFolder (const juce::File&);                      // the songs found
    const std::vector<pr7s::BandSong>& getSongs() const noexcept { return songs; }
    void setRole (int song, int di, pr7s::BandRole);
    void setTempo (int song, double bpm);
    void setIncluded (int song, bool);
    bool isIncluded (int song) const;
    void readSongs();                                         // on a thread of its own (READ THE SONGS)
    void readSongsNow();                                      // (tests) the same, waiting
    bool isReading() const noexcept { return reader.joinable() && ! readerDone; }
    void waitForReading();
    void stop();
    juce::File getReviewSong (int song) const;                // none until read
    juce::String getReport (int song) const;
    juce::String getStatusText() const;
    juce::File getReviewFolder() const;                       // where this record's review songs go
    std::function<void (const juce::File&)> onOpenSong;      // MainView: asks to save, then opens it

    static constexpr int headerHeight = 132, gap = 12;
    class Rows;

private:
    void timerCallback() override;
    void chooseFolder();
    void changed();
    void finishReading();
    struct Result { juce::File made; juce::String report, error; };
    void readInto (std::vector<Result>&, const std::vector<int>& which);

    pr7s::Studio& studio;
    juce::File folder;
    std::vector<pr7s::BandSong> songs;
    std::vector<bool> included;
    std::vector<Result> results;
    std::thread reader;
    std::atomic<bool> readerDone { true }, cancel { false };
    std::atomic<int> readingSong { -1 }, songsRead { 0 }, songsToRead { 0 };
    std::vector<Result> pending;
    juce::TextButton chooseKey { "CHOOSE FOLDER..." }, readKey { "READ THE SONGS" }, stopKey { "STOP" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;
};

class BandRecordWindow final : public juce::DocumentWindow
{
public:
    explicit BandRecordWindow (pr7s::Studio&);
    void closeButtonPressed() override;
    std::function<void()> onClose;
    BandRecordPage& getPage() { return *page; }

private:
    BandRecordPage* page = nullptr;
};
