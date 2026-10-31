#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Studio.h"
#include "ui/MixerView.h"

// The Inspector (0.9.0), inspired by Cubase's: left of the track list, the
// selected track's settings in sections (click a title to fold it; which are
// folded is kept for next time). Shown and hidden with the header's
// INSPECTOR key or Ctrl+Alt+L (the manual's Show/Hide Left Zone).
//
//   the track      its name in its colour (folds Basics)
//   BASICS         M S R W, REC (and MON on an audio track); volume and pan
//                  (Ctrl-click or double-click: 0 dB / the centre); the
//                  instrument (click: its menu, as in the track list; EDIT:
//                  its window), an FX channel's effect, or what a group is
//   ROUTING        an instrument track: MIDI IN (all MIDI inputs, or one
//                  keyboard: only it plays and records here) and CHANNEL (the
//                  one its notes go to the instrument on: a Kontakt multi's
//                  channels); an audio track: its input; OUT: the master or a
//                  group channel
//   INSERTS        its 16 slots, as the mixer's (those below the orange line
//                  are after the fader)
//   SENDS          its 8 sends, as the mixer's (not on an FX channel)
//   FADER          its mixer strip: fader, pan, M S R W, the level
class Inspector final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit Inspector (pr7s::Studio&);
    ~Inspector() override;

    std::function<void (int trackId)> onTrackMenu;        // the instrument (or input) key: the track list's menu
    std::function<void (int trackId)> onOpenInstrument;   // EDIT
    std::function<void (int busId, int slot)> onOpenEffect;
    std::function<void()> onFindPlugins;

    static constexpr int width = 210;
    enum class Section { basics, routing, inserts, sends, fader };
    static constexpr int numSections = 5;
    bool isFolded (Section) const;
    void setFolded (Section, bool);   // (kept in the settings file)

    int getTrack() const noexcept { return trackId; }   // the one shown: the selected track (0: none)

    // For tests.
    juce::ComboBox& getMidiInput() noexcept { return midiInput; }
    juce::ComboBox& getChannel() noexcept { return channel; }
    juce::ComboBox& getAudioInput() noexcept { return audioInput; }
    juce::TextButton& getOutput() noexcept { return output; }
    juce::TextButton& getInsertSlot (int slot) { return slots[(size_t) slot]; }
    juce::TextButton& getKey (const juce::String& name);   // "M", "S", "R", "W", "REC", "MON"
    juce::Slider& getVolume() noexcept { return volume; }
    juce::Slider& getPan() noexcept { return pan; }
    MixerView& getFader() noexcept { return fader; }
    juce::Component& getHeader (Section);
    juce::Component& getContent() noexcept { return content; }
    int getContentHeight() const noexcept { return content.getHeight(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Header;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void showTrack (int);
    void refresh();
    void layout();
    void followMix();
    void fillMidiInputs();
    const pr7s::Track* track() const;

    pr7s::Studio& studio;
    int trackId = 0;
    std::array<bool, numSections> folded {};
    juce::Viewport viewport;
    struct Content final : juce::Component
    {
        std::function<void (juce::Graphics&)> over;   // the line between the inserts before and after the fader
        void paintOverChildren (juce::Graphics& g) override { if (over) over (g); }
    } content;
    std::array<std::unique_ptr<Header>, numSections> headers;

    juce::TextButton mute { "M" }, solo { "S" }, read { "R" }, write { "W" }, arm { "REC" }, monitor { "MON" };
    juce::Slider volume, pan;
    juce::Label volumeLabel, panLabel, volumeReadout, panReadout;
    juce::TextButton instrument, edit { "EDIT" };
    juce::Label midiInputLabel, channelLabel, audioInputLabel, outputLabel, noSends;
    juce::ComboBox midiInput, channel, audioInput;
    juce::TextButton output;
    std::array<juce::TextButton, pr7s::Project::maxInserts> slots;
    int shownSlots = 8, preFader = pr7s::Project::maxInserts, insertLineY = -1;
    std::array<std::unique_ptr<MixerView::SendSlot>, pr7s::Project::maxSends> sendSlots;
    MixerView fader { studio, true };   // (also the insert, send and output menus' owner)
};
