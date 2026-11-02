#include "ChordPadsView.h"

#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"

using ChordUi::chordFont;
using ChordUi::InterKey;
using ChordUi::spokenKey;
using ChordUi::signatureText;

using pr7s::Chord;
using pr7s::ChordPads;

namespace
{
    juce::Font font (float height, int style = juce::Font::bold) { return juce::Font (ParagonLookAndFeel::textFont (height, style)); }

    const juce::Colour pearl { 0xfff4eeea };
    const juce::Colour padBody { 0xff1b171c }, padEmpty { 0xff121013 };

    void styleKey (juce::TextButton& key, bool menu = false)
    {
        key.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        key.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        key.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        key.setWantsKeyboardFocus (false);
        if (menu)
            key.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    }

    bool isBlack (int note) { const auto p = ((note % 12) + 12) % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }

    // A MIDI note's name as the studio writes them (middle C = C3).
    juce::String keyName (int note, bool flats) { return Chord::noteName (note, flats) + juce::String (note / 12 - 2); }

    // A small padlock, for a locked pad.
    void drawLock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
    {
        g.setColour (colour);
        const auto body = r.removeFromBottom (r.getHeight() * 0.58f);
        g.fillRoundedRectangle (body, 1.5f);
        juce::Path shackle;
        shackle.addCentredArc (body.getCentreX(), body.getY(), body.getWidth() * 0.32f, r.getHeight() * 0.85f, 0.0f,
                               -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.6f));
    }
}

//==============================================================================
// The chord editor (the manual's: a root note, a chord type, a tension, a bass note).
class ChordPadsView::ChordEditor final : public juce::Component
{
public:
    explicit ChordEditor (ChordPadsView& v) : view (v)
    {
        for (int i = 0; i < 12; ++i)
        {
            auto* r = roots.add (new InterKey (Chord::noteName (i, false)));
            r->onClick = [this, i] { view.editRoot (i); };
            auto* b = basses.add (new InterKey (Chord::noteName (i, false)));
            b->onClick = [this, i] { view.editBass (i); };
        }
        auto* none = basses.add (new juce::TextButton ("-"));
        none->onClick = [this] { view.editBass (-1); };
        for (const auto type : Chord::allTypes())
        {
            auto* t = types.add (new juce::TextButton (Chord::typeName (type)));
            t->onClick = [this, type] { view.editType (type); };
        }
        for (const auto tension : Chord::allTensions())
        {
            auto* t = tensions.add (new InterKey (Chord::tensionName (tension, Chord::Type::major)));
            t->onClick = [this, tension] { view.editTension (tension); };
        }
        for (auto* list : { &roots, &types, &tensions, &basses })
            for (auto* key : *list)
            {
                styleKey (*key);
                addAndMakeVisible (key);
            }
    }

    void refresh (const std::optional<Chord>& chord, const pr7s::Key& key, bool enabled)
    {
        for (int i = 0; i < 12; ++i)
        {
            // (Each note as the key spells it: Gb in Eb minor, F# in D major.)
            roots[i]->setButtonText (key.spell (i).text());
            basses[i]->setButtonText (key.spell (i).text());
            roots[i]->setToggleState (chord && chord->root == i, juce::dontSendNotification);
            basses[i]->setToggleState (chord && chord->bass == i, juce::dontSendNotification);
        }
        basses[12]->setToggleState (chord && chord->bass < 0, juce::dontSendNotification);
        for (int i = 0; i < types.size(); ++i)
            types[i]->setToggleState (chord && chord->type == Chord::allTypes()[(size_t) i], juce::dontSendNotification);
        for (int i = 0; i < tensions.size(); ++i)
            tensions[i]->setToggleState (chord && chord->tension == Chord::allTensions()[(size_t) i], juce::dontSendNotification);
        for (auto* list : { &roots, &types, &tensions, &basses })
            for (auto* key : *list)
                key->setEnabled (enabled);
        title = ! enabled ? "Select a pad"
              : chord ? chord->name (key) + "   (" + chord->noteNames (key).joinIntoString (" ") + ")" : "A new chord: its root";
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff100c11));
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (0, 0, 1, getHeight());
        auto area = getLocalBounds().reduced (10, 6);
        auto head = area.removeFromTop (18).toFloat();
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("CHORD EDITOR", head.removeFromLeft (110.0f), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::text);
        g.setFont (chordFont (14.0f));
        g.drawText (title, head, juce::Justification::centredLeft, true);
        g.setColour (ParagonColours::textDim);
        g.setFont (font (10.0f));
        for (const auto& [label, y] : labels)
            g.drawText (label, juce::Rectangle<float> ((float) area.getX(), (float) y, 54.0f, rowHeight), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 6);
        area.removeFromTop (22);
        labels.clear();
        const auto rows = 7.0f, gaps = 3.0f;
        rowHeight = juce::jlimit (14.0f, 26.0f, ((float) area.getHeight() - gaps * 6.0f) / rows);
        auto y = (float) area.getY();
        const auto x = area.getX() + 56;
        const auto width = (float) (area.getRight() - x);
        const auto row = [&] (juce::OwnedArray<juce::TextButton>& keys, int from, int count, int across)
        {
            const auto w = width / (float) across;
            for (int i = 0; i < count; ++i)
                keys[from + i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) i, y, w - 2.0f, rowHeight).toNearestInt());
            y += rowHeight + gaps;
        };
        labels.push_back ({ "ROOT", (int) y });
        row (roots, 0, 6, 6);
        row (roots, 6, 6, 6);
        labels.push_back ({ "TYPE", (int) y });
        row (types, 0, 6, 6);
        labels.push_back ({ "TENSION", (int) y });
        row (tensions, 0, 7, 7);
        row (tensions, 7, 7, 7);
        labels.push_back ({ "BASS", (int) y });
        // "-" (none) first, then the twelve, over two rows of seven.
        const auto w = width / 7.0f;
        basses[12]->setBounds (juce::Rectangle<float> ((float) x, y, w - 2.0f, rowHeight).toNearestInt());
        for (int i = 0; i < 6; ++i)
            basses[i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) (i + 1), y, w - 2.0f, rowHeight).toNearestInt());
        y += rowHeight + gaps;
        for (int i = 6; i < 12; ++i)
            basses[i]->setBounds (juce::Rectangle<float> ((float) x + w * (float) (i - 5), y, w - 2.0f, rowHeight).toNearestInt());
    }

private:
    ChordPadsView& view;
    juce::OwnedArray<juce::TextButton> roots, types, tensions, basses;
    std::vector<std::pair<juce::String, int>> labels;
    float rowHeight = 20.0f;
    juce::String title;
};

//==============================================================================
// The circle of fifths (0.15.0, the owner asked; the manual's Circle of Fifths
// assistant): the twelve major chords round the outside in fifths, their relative
// minors inside, the key's tonic at the top; the key's own chords marked with their
// numerals. Beside it, what could come next after the chord clicked (or the
// selected pad's), each scored (suggestNext): best first, strong to adventurous.
class ChordPadsView::CircleAssistant final : public juce::Component,
                                              public juce::SettableTooltipClient,
                                              private juce::Timer
{
public:
    explicit CircleAssistant (ChordPadsView& v) : view (v) {}

    struct Spot { Chord chord; juce::Point<float> centre; float radius; };

    std::vector<Spot> spots() const
    {
        std::vector<Spot> list;
        const auto key = view.studio.getChordPads().key();
        // The major chord at the top: the key's tonic (a minor key: its relative major, its i inside).
        const auto topMajor = key.minor ? (key.pitchClass() + 3) % 12 : key.pitchClass();
        const auto c = circleArea().getCentre();
        const auto r = circleArea().getWidth() * 0.5f;
        for (int i = 0; i < 12; ++i)
        {
            const auto angle = juce::MathConstants<float>::twoPi * (float) i / 12.0f;
            const auto at = [&c, angle] (float radius) { return c + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius; };
            const auto major = (topMajor + 7 * i) % 12;
            list.push_back ({ { major, Chord::Type::major, Chord::Tension::none, -1 }, at (r * 0.80f), r * 0.17f });
            list.push_back ({ { (major + 9) % 12, Chord::Type::minor, Chord::Tension::none, -1 }, at (r * 0.48f), r * 0.14f });
        }
        return list;
    }

    juce::Rectangle<float> circleArea() const
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f);
        const auto side = juce::jmin (area.getHeight(), area.getWidth() * 0.36f);
        return area.removeFromLeft (side).withSizeKeepingCentre (side, side);
    }

    juce::Rectangle<float> listArea() const
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f);
        area.removeFromLeft (circleArea().getWidth() + 14.0f);
        if (const auto popular = popularArea(); ! popular.isEmpty())
            area.setRight (popular.getX() - 18.0f * k());
        return area;
    }

    // The key's ten most popular progressions (0.17.2, the owner asked), at the right when there's room.
    juce::Rectangle<float> popularArea() const
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f);
        area.removeFromLeft (circleArea().getWidth() + 14.0f);
        const auto room = area.getWidth() - suggestionsLeast * k();
        if (room < popularLeast)
            return {};
        return area.removeFromRight (juce::jmin (room, popularMost * k()));
    }
    bool showsPopular() const { return ! popularArea().isEmpty(); }
    // All ten in the room there is (smaller rows than the suggestions' when it's short).
    float popularRowHeight() const
    {
        return juce::jmin (rowHeight(), ((float) getHeight() - 12.0f - headerHeight()) / 10.0f);
    }
    static constexpr float popularLeast = 420.0f, popularMost = 760.0f, suggestionsLeast = 560.0f;

    int popularRowAt (juce::Point<float> p) const
    {
        const auto area = popularArea();
        if (area.isEmpty() || ! area.contains (p) || p.y < area.getY() + headerHeight())
            return -1;
        const auto row = (int) ((p.y - area.getY() - headerHeight()) / popularRowHeight());
        return row < (int) popular.size() ? row : -1;
    }
    const std::vector<pr7s::Progression>& getPopular() const noexcept { return popular; }
    int getPlayingProgression() const noexcept { return playingRow; }

    // A progression played: a chord every half bar at the song's tempo (each lit as it sounds).
    void playProgression (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) popular.size()))
            return;
        playingRow = index;
        playingStep = -1;
        const auto bpm = view.studio.getProject().bpmAt (view.studio.getEngine().getPosition());
        const auto beats = view.studio.getProject().beatsPerBar() * 0.5;
        startTimer (juce::jlimit (350, 1400, juce::roundToInt (60000.0 / juce::jmax (20.0, bpm) * beats)));
        timerCallback();
    }
    void stopProgression()
    {
        stopTimer();
        playingRow = playingStep = -1;
        repaint();
    }
    void toChordTrack (int index)
    {
        if (juce::isPositiveAndBelow (index, (int) popular.size()))
            view.studio.addProgressionToChordTrack (popular[(size_t) index].chords, view.studio.getEngine().getPosition());
    }
    // A progression onto the chord track, then BASS & MELODY for its chords (0.18.0).
    void linesFor (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) popular.size()))
            return;
        auto& studio = view.studio;
        const auto bar = studio.getProject().beatsPerBar();
        const auto start = std::floor (studio.getEngine().getPosition() / bar + 1.0e-9) * bar;
        const auto end = start + bar * (double) popular[(size_t) index].chords.size();
        studio.addProgressionToChordTrack (popular[(size_t) index].chords, start);
        std::vector<int> events;
        const auto& all = studio.getChordTrack().events;
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].beat >= start - 1.0e-6 && all[(size_t) i].beat < end - 1.0e-6)
                events.push_back (i);
        if (view.onLineIdeas)
            view.onLineIdeas (events);
    }
    void toPads (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) popular.size()))
            return;
        const auto first = view.selected.empty() ? 0 : view.selected.front();
        view.studio.assignProgressionToPads (popular[(size_t) index].chords, first);
    }

    // Larger with room (0.17.0: the right screen's whole height and width).
    float k() const { return juce::jlimit (1.0f, 2.2f, juce::jmin ((float) getHeight() / 230.0f, (float) getWidth() / 640.0f)); }
    float rowHeight() const { return 22.0f * k(); }
    float headerHeight() const { return 20.0f * k(); }

    int rowAt (juce::Point<float> p) const
    {
        const auto list = listArea();
        if (! list.contains (p) || p.y < list.getY() + headerHeight())
            return -1;
        const auto row = (int) ((p.y - list.getY() - headerHeight()) / rowHeight());
        return row < (int) shown.size() ? row : -1;
    }

    std::optional<Chord> chordAt (juce::Point<float> p) const
    {
        for (const auto& spot : spots())
            if (spot.centre.getDistanceFrom (p) <= spot.radius)
                return spot.chord;
        if (const auto row = rowAt (p); row >= 0)
            return shown[(size_t) row].chord;
        return std::nullopt;
    }

    void refresh()
    {
        const auto from = view.getSuggestionsFrom();
        shown = from ? view.getSuggestions() : std::vector<pr7s::Suggestion> {};
        popular = pr7s::popularProgressions (view.studio.getChordPads().key());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff100c11));
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (0, 0, 1, getHeight());
        const auto key = view.studio.getChordPads().key();
        const auto from = view.getSuggestionsFrom();

        // The circle: the rings, then each chord; the key's own lit, with its numeral.
        const auto area = circleArea();
        g.setColour (juce::Colour (0xff1b171c));
        g.fillEllipse (area);
        g.setColour (ParagonColours::panelEdge);
        g.drawEllipse (area, 1.0f);
        g.drawEllipse (area.reduced (area.getWidth() * 0.18f), 1.0f);
        for (const auto& spot : spots())
        {
            const auto inKey = spot.chord.isInKey (key);
            const auto isTonic = inKey && spot.chord.root == key.pitchClass() && (spot.chord.type == Chord::Type::minor) == key.minor;
            const auto isFrom = from && from->root == spot.chord.root && from->type == spot.chord.type;
            const auto dot = juce::Rectangle<float> (spot.radius * 2.0f, spot.radius * 2.0f).withCentre (spot.centre);
            g.setColour (isTonic ? ParagonColours::accent : inKey ? juce::Colour (0xff4a1a24) : juce::Colour (0xff141015));
            g.fillEllipse (dot);
            if (isFrom)
            {
                g.setColour (juce::Colour (0xfff4eeea));
                g.drawEllipse (dot.reduced (1.0f), 2.0f);
            }
            g.setColour (isTonic ? juce::Colour (0xff120d10) : inKey ? ParagonColours::text : ParagonColours::textDim);
            g.setFont (chordFont (juce::jlimit (9.0f, 15.0f * k(), spot.radius * 0.62f)));
            auto label = dot;
            g.drawText (spot.chord.name (key), inKey ? label.removeFromTop (label.getHeight() * 0.62f) : label, juce::Justification::centredBottom, false);
            if (inKey)
            {
                g.setFont (chordFont (juce::jlimit (7.0f, 10.0f * k(), spot.radius * 0.42f)));
                g.drawText (spot.chord.romanName (key), label, juce::Justification::centredTop, false);
            }
        }
        g.setColour (ParagonColours::textDim);
        g.setFont (chordFont (10.0f * k()));
        g.drawText (spokenKey (key), area.withSizeKeepingCentre (area.getWidth() * 0.5f, 14.0f * k()), juce::Justification::centred, false);

        // The suggestions.
        auto list = listArea();
        auto head = list.removeFromTop (headerHeight());
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (12.0f * k()));
        g.drawText ("WHAT COULD COME NEXT", head.removeFromLeft (150.0f * k()), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::text);
        g.setFont (chordFont (13.0f * k()));
        g.drawText (from ? "after " + from->name (key) + "  (" + from->romanName (key) + ")" : juce::String ("click a chord on the circle, or select a pad"),
                    head, juce::Justification::centredLeft, true);
        for (size_t i = 0; i < shown.size(); ++i)
        {
            if (list.getHeight() < rowHeight())
                break;   // (as many as fit; the best first)
            const auto row = list.removeFromTop (rowHeight());
            const auto& s = shown[i];
            auto r = row.reduced (0.0f, 2.0f);
            if ((int) i == hoverRow)
            {
                g.setColour (juce::Colour (0xff1e181f));
                g.fillRect (row);
            }
            g.setColour (s.inKey ? ParagonColours::text : ParagonColours::textDim);
            g.setFont (chordFont (13.0f * k()));
            g.drawText (s.chord.name (key), r.removeFromLeft (64.0f * k()), juce::Justification::centredLeft, false);
            g.setFont (chordFont (11.0f * k()));
            g.drawText (s.chord.romanName (key), r.removeFromLeft (52.0f * k()), juce::Justification::centredLeft, false);
            // The score: a bar lit along the family's gradient, and its number.
            auto bar = r.removeFromLeft (90.0f * k()).reduced (0.0f, 4.0f);
            g.setColour (juce::Colour (0xff1b171c));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (ParagonColours::meterColour ((float) s.score / 100.0f).withAlpha (s.score >= 80 ? 1.0f : s.score >= 60 ? 0.75f : 0.45f));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) s.score / 100.0f), 2.0f);
            g.setColour (ParagonColours::text);
            g.setFont (chordFont (11.0f * k()));
            g.drawText (juce::String (s.score), r.removeFromLeft (30.0f * k()), juce::Justification::centred, false);
            g.setColour (ParagonColours::textDim);
            g.setFont (chordFont (10.5f * k()));
            g.drawText (s.reason, r, juce::Justification::centredLeft, true);
        }

        // The key's most popular progressions, the best known first: each chord spelled in the
        // key with its numeral under it, lit while it sounds.
        if (auto area = popularArea(); ! area.isEmpty())
        {
            g.setColour (ParagonColours::panelEdge);
            g.fillRect (area.getX() - 9.0f * k(), area.getY(), 1.0f, area.getHeight());
            auto top = area.removeFromTop (headerHeight());
            g.setColour (ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::titleFont (12.0f * k()));
            g.drawText ("POPULAR PROGRESSIONS IN " + spokenKey (key).toUpperCase(), top, juce::Justification::centredLeft, true);
            const auto rowH = popularRowHeight();
            const auto nameWidth = juce::jmin (area.getWidth() * 0.3f, 170.0f * k());
            const auto cellWidth = (area.getWidth() - nameWidth - 4.0f) / 8.0f;   // (the longest has eight)
            const auto chordSize = juce::jmin (13.0f * k(), rowH * 0.48f, cellWidth * 0.36f);
            for (size_t i = 0; i < popular.size(); ++i)
            {
                if (area.getHeight() < rowH - 0.5f)
                    break;
                const auto row = area.removeFromTop (rowH);
                const auto& p = popular[i];
                if ((int) i == hoverPopular || (int) i == playingRow)
                {
                    g.setColour (juce::Colour (0xff1e181f));
                    g.fillRect (row);
                }
                auto r = row.reduced (0.0f, 1.0f);
                auto name = r.removeFromLeft (nameWidth);
                g.setColour (ParagonColours::text);
                g.setFont (chordFont (juce::jmin (12.0f * k(), rowH * 0.42f)));
                g.drawText (juce::String ((int) i + 1) + "  " + p.name, name.removeFromTop (name.getHeight() * 0.58f), juce::Justification::bottomLeft, true);
                g.setColour (ParagonColours::textDim);
                g.setFont (chordFont (juce::jmin (10.0f * k(), rowH * 0.32f)));
                g.drawText (p.style, name, juce::Justification::topLeft, true);
                r.removeFromLeft (4.0f);
                for (size_t c = 0; c < p.chords.size(); ++c)
                {
                    const auto cell = r.removeFromLeft (cellWidth).reduced (1.5f, 0.0f);
                    const auto& chord = p.chords[c];
                    const auto lit = (int) i == playingRow && (int) c == playingStep;
                    g.setColour (lit ? ParagonColours::accent : juce::Colour (0xff171318));
                    g.fillRoundedRectangle (cell, 3.0f);
                    const auto inKey = chord.isInKey (key);
                    g.setColour (lit ? juce::Colour (0xff120d10) : inKey ? ParagonColours::text : ParagonColours::accent);
                    g.setFont (chordFont (chordSize));
                    auto label = cell;
                    g.drawFittedText (chord.name (key), label.removeFromTop (label.getHeight() * 0.58f).toNearestInt(), juce::Justification::centredBottom, 1, 0.7f);
                    g.setFont (chordFont (chordSize * 0.72f));
                    g.drawFittedText (chord.romanName (key), label.toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
                }
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto row = rowAt (e.position);
        const auto popularRow = popularRowAt (e.position);
        if (row != hoverRow || popularRow != hoverPopular)
        {
            hoverRow = row;
            hoverPopular = popularRow;
            repaint();
        }
        if (popularRow >= 0)
            setTooltip (popular[(size_t) popularRow].name + " (" + popular[(size_t) popularRow].style + "). Click to hear it; "
                        "double-click: onto the chord track from the playhead's bar, a chord a bar; right-click: onto the pads too");
        else if (row >= 0)
        {
            const auto& s = shown[(size_t) row];
            setTooltip (juce::String (s.score >= 80 ? "Strong" : s.score >= 60 ? "Good" : "Adventurous") + " (" + juce::String (s.score) + "): "
                        + s.reason + ". Double-click: onto the selected pad (or the first without a chord); or drag it onto a pad.");
        }
        else
            setTooltip ("A chord on the circle: click to hear it and see what could come next; double-click or drag it onto a pad");
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverRow = -1;
        hoverPopular = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = false;
        // A progression: heard (again: stopped); its menu.
        if (const auto row = popularRowAt (e.position); row >= 0)
        {
            downChord.reset();
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu menu;
                menu.addSectionHeader (popular[(size_t) row].name.toUpperCase());
                menu.addItem (1, "Play It");
                menu.addItem (2, "Onto the Chord Track (From the Playhead's Bar, a Chord a Bar)");
                menu.addItem (3, "Onto the Pads (From the Selected Pad, Else the First)");
                menu.addItem (4, "Bass and Melody Ideas for It (Onto the Chord Track First)");
                menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<CircleAssistant> (this), row] (int chosen)
                {
                    if (safe == nullptr)
                        return;
                    if (chosen == 1) safe->playProgression (row);
                    if (chosen == 2) safe->toChordTrack (row);
                    if (chosen == 3) safe->toPads (row);
                    if (chosen == 4) safe->linesFor (row);
                });
                return;
            }
            if (playingRow == row && isTimerRunning())
                stopProgression();
            else
                playProgression (row);
            return;
        }
        downChord = chordAt (e.position);
        if (! downChord)
            return;
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Assign to the Selected Pad (or the First Unassigned)");
            menu.addItem (2, "Suggest What Comes After It");
            menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<CircleAssistant> (this), chord = *downChord] (int chosen)
            {
                if (safe == nullptr)
                    return;
                if (chosen == 1)
                    safe->view.assignToNextPad (chord);
                if (chosen == 2)
                    safe->view.circleChordClicked (chord);
            });
            return;
        }
        // A chord on the circle or in the most used: played, and the suggestions follow it; a suggestion: played.
        const auto onCircle = rowAt (e.position) < 0;
        if (onCircle)
            view.circleChordClicked (*downChord);
        else
            view.studio.auditionChord (*downChord);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! downChord || dragged || e.getDistanceFromDragStart() < 6)
            return;
        dragged = true;
        view.startDragging (juce::String (ChordPadsView::chordDragPrefix) + downChord->toText(), this, juce::ScaledImage(), true);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (const auto row = popularRowAt (e.position); row >= 0)
        {
            stopProgression();
            toChordTrack (row);
            return;
        }
        if (const auto chord = chordAt (e.position))
            view.assignToNextPad (*chord);
    }

private:
    ChordPadsView& view;
    std::vector<pr7s::Suggestion> shown;
    std::optional<Chord> downChord;
    bool dragged = false;
    int hoverRow = -1, hoverPopular = -1, playingRow = -1, playingStep = -1;
    std::vector<pr7s::Progression> popular;

    void timerCallback() override
    {
        if (! juce::isPositiveAndBelow (playingRow, (int) popular.size())
            || ++playingStep >= (int) popular[(size_t) playingRow].chords.size())
        {
            stopProgression();
            return;
        }
        view.studio.auditionChord (popular[(size_t) playingRow].chords[(size_t) playingStep]);
        repaint();
    }
};

//==============================================================================
// Every key, on the screen left of the pads (0.17.0, the owner: "all major and minor
// scales displayed for easy selecting", large). A row per key signature: the major,
// its signature, its relative minor (the same notes); C first, then the sharps, then
// the flats, as the KEY menu. Click one to choose it (as the KEY menu does).
class ChordPadsView::KeyList final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    explicit KeyList (ChordPadsView& v) : view (v)
    {
        setTooltip ("Click a key: the pads take it (as the KEY menu's choice says); its chords and numerals follow");
    }

    // As many columns as the height needs for rows of at least minimumRow.
    int columns() const
    {
        const auto h = (float) getHeight() - headerHeight;
        for (int c = 1; c < 4; ++c)
            if (h / (float) ((rows + c - 1) / c) >= minimumRow)
                return c;
        return 4;
    }
    int rowsPerColumn() const { return (rows + columns() - 1) / columns(); }
    float rowHeight() const { return juce::jmin (maximumRow, ((float) getHeight() - headerHeight) / (float) rowsPerColumn()); }

    juce::Rectangle<float> cellBounds (int row, bool minor) const
    {
        const auto area = getLocalBounds().toFloat().reduced (10.0f, 0.0f).withTrimmedTop (headerHeight);
        const auto columnWidth = area.getWidth() / (float) columns();
        const auto column = row / rowsPerColumn();
        const auto line = juce::Rectangle<float> (area.getX() + columnWidth * (float) column, area.getY() + rowHeight() * (float) (row % rowsPerColumn()),
                                                  columnWidth, rowHeight()).reduced (4.0f, 2.0f);
        // [major | signature | minor]
        const auto side = line.getWidth() * 0.38f;
        return minor ? line.withTrimmedLeft (line.getWidth() - side) : line.withWidth (side);
    }

    // The key under a point (tests and clicks).
    std::optional<pr7s::Key> keyAt (juce::Point<float> p) const
    {
        for (int row = 0; row < rows; ++row)
            for (const auto minor : { false, true })
                if (cellBounds (row, minor).contains (p))
                    return pr7s::Key::all()[(size_t) (row + (minor ? rows : 0))];
        return std::nullopt;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff100c11));
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (getWidth() - 1, 0, 1, getHeight());
        const auto now = view.studio.getChordPads().key();
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText ("KEYS  -  MAJOR, ITS SIGNATURE, ITS RELATIVE MINOR", getLocalBounds().toFloat().reduced (14.0f, 0.0f).withHeight (headerHeight),
                    juce::Justification::centredLeft, false);
        const auto font = juce::jlimit (13.0f, 30.0f, rowHeight() * 0.5f);
        for (int row = 0; row < rows; ++row)
        {
            for (const auto minor : { false, true })
            {
                const auto& key = pr7s::Key::all()[(size_t) (row + (minor ? rows : 0))];
                const auto cell = cellBounds (row, minor);
                const auto lit = key == now, over = hover && *hover == key;
                g.setColour (lit ? ParagonColours::accent : over ? juce::Colour (0xff2a2129) : juce::Colour (0xff171318));
                g.fillRoundedRectangle (cell, 4.0f);
                g.setColour (lit ? juce::Colour (0xff120d10) : minor ? ParagonColours::text.withAlpha (0.85f) : ParagonColours::text);
                g.setFont (chordFont (font));
                g.drawText (key.name(), cell.reduced (font * 0.5f, 0.0f), juce::Justification::centredLeft, false);
            }
            const auto left = cellBounds (row, false), right = cellBounds (row, true);
            g.setColour (ParagonColours::textDim);
            g.setFont (chordFont (font * 0.8f));
            g.drawText (signatureText (pr7s::Key::all()[(size_t) row]), left.withLeft (left.getRight()).withRight (right.getX()),
                        juce::Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto now = keyAt (e.position);
        if (now != hover)
        {
            hover = now;
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        hover.reset();
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const auto key = keyAt (e.position))
            view.chooseKey (*key);
    }

    static constexpr int rows = 15;
    static constexpr float headerHeight = 30.0f, minimumRow = 34.0f, maximumRow = 64.0f;

private:
    ChordPadsView& view;
    std::optional<pr7s::Key> hover;
};

//==============================================================================
ChordPadsView::ChordPadsView (pr7s::Studio& s) : studio (s)
{
    setWantsKeyboardFocus (true);
    editor = std::make_unique<ChordEditor> (*this);
    addChildComponent (*editor);
    circle = std::make_unique<CircleAssistant> (*this);
    addChildComponent (*circle);
    keyList = std::make_unique<KeyList> (*this);
    addChildComponent (*keyList);
    for (auto* key : { &rootKey, &presetsKey, &playerKey, &modeKey, &displayKey, &remoteKey })
        styleKey (*key, true);
    for (auto* key : { &previousPreset, &nextPreset, &saveKey, &editorKey, &circleKey, &windowKey, &closeKey, &selectAllKey, &editKey, &fromMidiKey,
                       &removeKey, &transposeDown, &transposeUp, &tensionsDown, &tensionsUp, &voicingPrevious, &voicingNext, &lockKey,
                       &adaptiveKey, &referenceKey, &fromChordTrackKey })
        styleKey (*key);
    for (auto* key : { &rootKey, &presetsKey, &previousPreset, &nextPreset, &saveKey, &playerKey, &modeKey, &displayKey, &remoteKey, &editorKey, &circleKey,
                       &windowKey, &closeKey, &selectAllKey, &editKey, &fromMidiKey, &removeKey, &transposeDown, &transposeUp, &tensionsDown,
                       &tensionsUp, &voicingPrevious, &voicingNext, &lockKey, &adaptiveKey, &referenceKey, &fromChordTrackKey })
        addAndMakeVisible (*key);

    rootKey.setTooltip ("The key: every major and minor key. Choosing one fills the pads with its chords, spelled as the key spells them "
                        "(or transposes your pads to it, or only names them in it)");
    rootKey.onClick = [this] { showRootMenu(); };
    presetsKey.setTooltip ("Chord pads presets: the scales (in the root key) and your own; what to load (chords, the player, or both)");
    presetsKey.onClick = [this] { showPresetsMenu(); };
    previousPreset.setTooltip ("The previous preset");
    previousPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.setTooltip ("The next preset");
    nextPreset.onClick = [this] { stepPreset (1); };
    saveKey.setTooltip ("Save the pads as a preset (their chords and the player)");
    saveKey.onClick = [this] { savePreset(); };
    playerKey.setTooltip ("The player: how chords are voiced (the Basic player: close chords, its octave)");
    playerKey.onClick = [this] { showPlayerMenu(); };
    modeKey.setTooltip ("The player mode: plain chords, and what happens to a chord held when the next is played (Hold First, Legato, Stop First)");
    modeKey.onClick = [this] { showModeMenu(); };
    displayKey.setTooltip ("How the pads show their chords (note names, roman numerals, Nashville numbers) and their layout (keyboard or grid)");
    displayKey.onClick = [this] { showDisplayMenu(); };
    remoteKey.setTooltip ("The keys on your MIDI keyboard that play the pads (C1 to B1 at first), and Latch Chords");
    remoteKey.onClick = [this] { showRemoteMenu(); };
    editorKey.setTooltip ("The chord editor beside the pads: the selected pad's root, type, tension and bass");
    editorKey.onClick = [this] { setEditorShown (! editorShown); };
    circleKey.setTooltip ("The circle of fifths: the key's chords marked; click one to hear it and see what could come next, scored "
                          "(double-click or drag a chord onto a pad)");
    circleKey.onClick = [this] { setCircleShown (! circleShown); };
    windowKey.onClick = [this] { if (onWindowToggle) onWindowToggle(); };
    closeKey.onClick = [this] { if (onClose) onClose(); };

    selectAllKey.setTooltip ("Select all the pads (Ctrl+A)");
    selectAllKey.onClick = [this] { selectAll(); };
    editKey.setTooltip ("Edit the selected pad's chord (double-click it, or Return)");
    editKey.onClick = [this] { setEditorShown (true); };
    fromMidiKey.setTooltip ("Assign from MIDI Input: play a chord on your MIDI keyboard, and the selected pad takes it, voicing and all (then the next selected pad)");
    fromMidiKey.onClick = [this]
    {
        studio.setAssignFromInput (studio.getAssignFromInput().empty() ? selected : std::vector<int> {});
    };
    removeKey.setTooltip ("Remove the selected pads' chords (Delete). This can't be undone");
    removeKey.onClick = [this] { removeSelected (true); };
    transposeDown.setTooltip ("Transpose the selected pads down a semitone (Shift + the wheel over a pad)");
    transposeDown.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.transposePads (pads, -1); }); };
    transposeUp.setTooltip ("Transpose the selected pads up a semitone");
    transposeUp.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.transposePads (pads, 1); }); };
    tensionsDown.setTooltip ("Fewer tensions on the selected pads (Alt + the wheel over a pad)");
    tensionsDown.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadTensions (pads, -1); }); };
    tensionsUp.setTooltip ("More tensions on the selected pads");
    tensionsUp.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadTensions (pads, 1); }); };
    voicingPrevious.setTooltip ("The previous voicing (the wheel over a pad). A voicing chosen by hand turns the pad's adaptive voicing off");
    voicingPrevious.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadVoicings (pads, -1); }); };
    voicingNext.setTooltip ("The next voicing");
    voicingNext.onClick = [this] { applyToSelection ([this] (auto& pads) { studio.changePadVoicings (pads, 1); }); };
    lockKey.setTooltip ("Lock the selected pads: no editing, no changes from the keys or the wheel (Ctrl+Shift+L)");
    lockKey.onClick = [this]
    {
        if (selected.empty())
            return;
        studio.setPadsLocked (selected, ! studio.getChordPads().pads[(size_t) selected.front()].locked);
    };
    adaptiveKey.setTooltip ("Adaptive voicing: the pad's voicing chosen as it's played, so the voices move as little as they can from the chord before");
    adaptiveKey.onClick = [this]
    {
        if (selected.empty())
            return;
        studio.setPadsAdaptive (selected, ! studio.getChordPads().pads[(size_t) selected.front()].adaptive);
    };
    fromChordTrackKey.setTooltip ("Assign from Chord Track: the chord track's chords, each once, onto the pads in order (they replace the pads' chords)");
    fromChordTrackKey.onClick = [this]
    {
        if (studio.getChordTrack().events.empty())
            return;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Assign from Chord Track",
                                            "The chord track's chords onto the pads, each once, in order. The pads' chords are replaced (locked ones kept).",
                                            "Assign", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ChordPadsView> (this)] (int result)
                                            {
                                                if (safe != nullptr && result == 1)
                                                    safe->studio.assignPadsFromChordTrack();
                                            }));
    };
    referenceKey.setTooltip ("Adaptive voicing reference: the other pads' voicings stay near this pad's (one pad at a time)");
    referenceKey.onClick = [this]
    {
        if (selected.empty())
            return;
        studio.setAdaptiveReference (studio.getChordPads().adaptiveReference == selected.front() ? -1 : selected.front());
    };

    studio.addChangeListener (this);
    refresh();
    startTimerHz (30);
}

ChordPadsView::~ChordPadsView()
{
    studio.removeChangeListener (this);
    if (pressedPad >= 0)
        studio.getPadPlayer().release (pressedPad);
}

void ChordPadsView::setInWindow (bool shouldBeInWindow)
{
    inWindow = shouldBeInWindow;
    windowKey.setButtonText (inWindow ? "DOCK" : "WINDOW");
    windowKey.setTooltip (inWindow ? "Back into the studio's lower zone" : "The chord pads in a window of their own");
}

void ChordPadsView::selectPads (std::vector<int> pads)
{
    const auto count = (int) studio.getChordPads().pads.size();
    pads.erase (std::remove_if (pads.begin(), pads.end(), [count] (int p) { return ! juce::isPositiveAndBelow (p, count); }), pads.end());
    selected = std::move (pads);
    refresh();
}

void ChordPadsView::selectAll()
{
    std::vector<int> all;
    for (int i = 0; i < (int) studio.getChordPads().pads.size(); ++i)
        all.push_back (i);
    selectPads (all);
}

bool ChordPadsView::isPopularShown() const { return circle != nullptr && circle->isVisible() && circle->showsPopular(); }
const std::vector<pr7s::Progression>& ChordPadsView::getProgressions() const { return circle->getPopular(); }
void ChordPadsView::playProgression (int index) { circle->playProgression (index); }
int ChordPadsView::getPlayingProgression() const { return circle->getPlayingProgression(); }
void ChordPadsView::progressionToChordTrack (int index) { circle->toChordTrack (index); }
void ChordPadsView::progressionToPads (int index) { circle->toPads (index); }
bool ChordPadsView::isKeyListShown() const { return keyList != nullptr && keyList->isVisible(); }
juce::Component* ChordPadsView::getKeyList() const noexcept { return keyList.get(); }
juce::Component* ChordPadsView::getCircle() const noexcept { return circle.get(); }
std::optional<pr7s::Key> ChordPadsView::keyListKeyAt (juce::Point<float> p) const { return keyList->keyAt (p); }

juce::Rectangle<float> ChordPadsView::keyListCell (const pr7s::Key& key) const
{
    const auto& all = pr7s::Key::all();
    const auto i = (int) (std::find (all.begin(), all.end(), key) - all.begin());
    return keyList->cellBounds (i % KeyList::rows, i >= KeyList::rows);
}

void ChordPadsView::setEditorShown (bool shown)
{
    editorShown = shown;
    if (shown)
        circleShown = false;   // (one panel at the right at a time)
    editor->setVisible (shown);
    resized();
    refresh();
}

void ChordPadsView::setCircleShown (bool shown)
{
    circleShown = shown;
    if (shown)
        editorShown = false;
    editor->setVisible (editorShown);
    resized();
    refresh();
}

void ChordPadsView::chooseKey (const pr7s::Key& key)
{
    studio.setPadsKey (key, keyChange);
    circleFrom.reset();   // (the suggestions start again from the pads)
    refresh();
}

int ChordPadsView::assignToNextPad (const Chord& chord)
{
    // The selected pad, else the first without a chord (the manual's Assign to First Unassigned Pad).
    const auto& pads = studio.getChordPads().pads;
    auto target = -1;
    if (! selected.empty() && ! pads[(size_t) selected.front()].locked)
        target = selected.front();
    for (int i = 0; target < 0 && i < (int) pads.size(); ++i)
        if (! pads[(size_t) i].chord && ! pads[(size_t) i].locked)
            target = i;
    if (target < 0)
        return -1;
    studio.assignChord (target, chord);
    audition (target);
    return target;
}

std::optional<Chord> ChordPadsView::getSuggestionsFrom() const
{
    if (circleFrom)
        return circleFrom;
    if (! selected.empty())
        if (const auto& chord = studio.getChordPads().pads[(size_t) selected.front()].chord)
            return chord;
    return std::nullopt;
}

std::vector<pr7s::Suggestion> ChordPadsView::getSuggestions() const
{
    const auto from = getSuggestionsFrom();
    return from ? pr7s::suggestNext (*from, studio.getChordPads().key()) : std::vector<pr7s::Suggestion> {};
}

void ChordPadsView::circleChordClicked (const Chord& chord)
{
    circleFrom = chord;
    studio.auditionChord (chord);
    refresh();
}

void ChordPadsView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    selectPads (selected);   // (pads may have gone: fewer of them)
}

void ChordPadsView::refresh()
{
    const auto& pads = studio.getChordPads();
    rootKey.setButtonText ("KEY: " + spokenKey (pads.key()));
    playerKey.setButtonText (pads.octaveOffset == 0 ? juce::String ("PLAYER: BASIC")
                                                    : "PLAYER: BASIC " + juce::String (pads.octaveOffset > 0 ? "+" : "") + juce::String (pads.octaveOffset));
    modeKey.setButtonText (pads.overlap == ChordPads::Overlap::holdFirst ? "PLAIN: HOLD FIRST"
                         : pads.overlap == ChordPads::Overlap::stopFirst ? "PLAIN: STOP FIRST" : "PLAIN: LEGATO");
    editorKey.setToggleState (editorShown, juce::dontSendNotification);
    circleKey.setToggleState (circleShown, juce::dontSendNotification);
    if (circle != nullptr)
        circle->refresh();
    if (keyList != nullptr)
        keyList->repaint();
    fromMidiKey.setToggleState (! studio.getAssignFromInput().empty(), juce::dontSendNotification);
    const auto* first = selected.empty() ? nullptr : &pads.pads[(size_t) selected.front()];
    lockKey.setToggleState (first != nullptr && first->locked, juce::dontSendNotification);
    adaptiveKey.setToggleState (first != nullptr && first->adaptive, juce::dontSendNotification);
    referenceKey.setToggleState (first != nullptr && pads.adaptiveReference == selected.front(), juce::dontSendNotification);
    for (auto* key : { &editKey, &fromMidiKey, &removeKey, &transposeDown, &transposeUp, &tensionsDown, &tensionsUp, &voicingPrevious,
                       &voicingNext, &lockKey, &adaptiveKey, &referenceKey })
        key->setEnabled (! selected.empty() || key == &fromMidiKey);
    fromChordTrackKey.setEnabled (! studio.getChordTrack().events.empty());
    if (editor != nullptr)
        editor->refresh (first != nullptr ? first->chord : std::nullopt, pads.key(), first != nullptr && ! first->locked);
    repaint();
}

void ChordPadsView::audition (int pad)
{
    if (juce::isPositiveAndBelow (pad, (int) studio.getChordPads().pads.size()) && studio.getChordPads().pads[(size_t) pad].chord)
        studio.auditionPad (pad);
}

void ChordPadsView::applyToSelection (std::function<void (const std::vector<int>&)> action, bool listen)
{
    if (selected.empty())
        return;
    action (selected);
    if (listen)
        audition (selected.front());   // (the manual: the first selected pad is played)
}

//==============================================================================
void ChordPadsView::editRoot (int pitchClass)
{
    if (selected.empty())
        return;
    const auto& pad = studio.getChordPads().pads[(size_t) selected.front()];
    auto chord = pad.chord.value_or (Chord {});
    chord.root = pitchClass;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::editType (Chord::Type type)
{
    if (selected.empty())
        return;
    auto chord = studio.getChordPads().pads[(size_t) selected.front()].chord.value_or (Chord {});
    chord.type = type;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::editTension (Chord::Tension tension)
{
    if (selected.empty())
        return;
    auto chord = studio.getChordPads().pads[(size_t) selected.front()].chord.value_or (Chord {});
    chord.tension = tension;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::editBass (int pitchClass)
{
    if (selected.empty())
        return;
    auto chord = studio.getChordPads().pads[(size_t) selected.front()].chord.value_or (Chord {});
    chord.bass = pitchClass;
    studio.assignChord (selected.front(), chord);
    audition (selected.front());
}

void ChordPadsView::removeSelected (bool ask)
{
    if (selected.empty())
        return;
    if (! ask)
    {
        studio.removeChords (selected);
        return;
    }
    // (The manual: this can't be undone, so it asks.)
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Remove chords",
                                        "Remove the chords from the selected pads? This can't be undone.", "Remove", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ChordPadsView> (this)] (int result)
                                        {
                                            if (safe != nullptr && result == 1)
                                                safe->studio.removeChords (safe->selected);
                                        }));
}

//==============================================================================
void ChordPadsView::showRootMenu()
{
    // The KEY menu (0.15.0): every major and minor key, round the circle of fifths;
    // then what choosing one does; and the enharmonic spelling.
    const auto now = studio.getChordPads().key();
    juce::PopupMenu menu, majors, minors;
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        (keys[(size_t) i].minor ? minors : majors).addItem (1 + i, spokenKey (keys[(size_t) i]) + "  (" + signatureText (keys[(size_t) i]) + ")",
                                                          true, keys[(size_t) i] == now);
    menu.addSectionHeader ("KEY: " + spokenKey (now).toUpperCase());
    menu.addSubMenu ("Major Keys", majors);
    menu.addSubMenu ("Minor Keys", minors);
    menu.addSeparator();
    menu.addSectionHeader ("CHOOSING A KEY");
    using C = pr7s::Studio::PadsKeyChange;
    menu.addItem (100, "Fills the pads with its chords", true, keyChange == C::fillWithChords);
    menu.addItem (101, "Transposes my pads to it", true, keyChange == C::transposePads);
    menu.addItem (102, "Only names my chords in it (numerals, spelling)", true, keyChange == C::renameOnly);
    menu.addSeparator();
    menu.addItem (110, "Fill the Pads with This Key's Chords Now");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rootKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        using C = pr7s::Studio::PadsKeyChange;
        if (chosen >= 100 && chosen <= 102)
            safe->keyChange = chosen == 100 ? C::fillWithChords : chosen == 101 ? C::transposePads : C::renameOnly;
        else if (chosen == 110)
            safe->studio.setPadsKey (safe->studio.getChordPads().key(), C::fillWithChords);
        else if (chosen >= 1 && chosen <= (int) pr7s::Key::all().size())
            safe->chooseKey (pr7s::Key::all()[(size_t) (chosen - 1)]);
        safe->refresh();
    });
}

juce::Array<juce::File> ChordPadsView::userPresets() const
{
    auto files = pr7s::Studio::chordPadsPresetFolder().findChildFiles (juce::File::findFiles, false, "*.pr7pads");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

void ChordPadsView::showPresetsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("SCALES, IN THE ROOT KEY");
    const auto scales = ChordPads::scaleNames();
    for (int i = 0; i < scales.size(); ++i)
        menu.addItem (1 + i, scales[i], true, presetIndex == i);
    const auto mine = userPresets();
    if (! mine.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader ("YOUR PRESETS");
        for (int i = 0; i < mine.size(); ++i)
            menu.addItem (100 + i, mine[i].getFileNameWithoutExtension(), true, presetIndex == scales.size() + i);
    }
    menu.addSeparator();
    menu.addItem (201, "Load Chords & Players", true, loadWhat == pr7s::Studio::PadsPresetLoad::chordsAndPlayers);
    menu.addItem (202, "Load Chords Only", true, loadWhat == pr7s::Studio::PadsPresetLoad::chordsOnly);
    menu.addItem (203, "Load Players Only", true, loadWhat == pr7s::Studio::PadsPresetLoad::playersOnly);
    menu.addSeparator();
    menu.addItem (210, "Save Preset...");
    menu.addItem (211, "Show the Presets Folder");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this), count = scales.size()] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        using L = pr7s::Studio::PadsPresetLoad;
        if (chosen >= 201 && chosen <= 203)
            safe->loadWhat = chosen == 201 ? L::chordsAndPlayers : chosen == 202 ? L::chordsOnly : L::playersOnly;
        else if (chosen == 210)
            safe->savePreset();
        else if (chosen == 211)
        {
            pr7s::Studio::chordPadsPresetFolder().createDirectory();
            pr7s::Studio::chordPadsPresetFolder().revealToUser();
        }
        else
        {
            safe->presetIndex = chosen >= 100 ? count + chosen - 100 : chosen - 1;
            safe->stepPreset (0);
        }
    });
}

void ChordPadsView::stepPreset (int direction)
{
    // The scales, then your presets, round and round.
    const auto scales = ChordPads::scaleNames().size();
    const auto mine = userPresets();
    const auto total = scales + mine.size();
    presetIndex = ((presetIndex + direction) % total + total) % total;
    if (presetIndex < scales)
        studio.loadScalePreset ((ChordPads::Scale) presetIndex, studio.getChordPads().rootKey, loadWhat);
    else
        studio.loadChordPadsPreset (mine[presetIndex - scales], loadWhat);
    presetsKey.setButtonText (presetIndex < scales ? ChordPads::scaleNames()[presetIndex].toUpperCase()
                                                   : mine[presetIndex - scales].getFileNameWithoutExtension().toUpperCase());
}

void ChordPadsView::savePreset()
{
    auto* box = new juce::AlertWindow ("Save chord pads preset", "Its name:", juce::MessageBoxIconType::NoIcon);
    box->addTextEditor ("name", "My chords");
    box->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<ChordPadsView> (this)] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = juce::File::createLegalFileName (box->getTextEditorContents ("name").trim());
        if (name.isEmpty())
            return;
        safe->studio.saveChordPadsPreset (pr7s::Studio::chordPadsPresetFolder().getChildFile (name + ".pr7pads"));
    }), true);
}

void ChordPadsView::showPlayerMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("BASIC PLAYER: CLOSE CHORDS");
    menu.addItem (99, "(Piano and guitar players: a later release)", false, false);
    menu.addSeparator();
    menu.addSectionHeader ("OCTAVE");
    for (int o = -2; o <= 2; ++o)
        menu.addItem (10 + o, o == 0 ? juce::String ("0") : (o > 0 ? "+" : "") + juce::String (o), true, pads.octaveOffset == o);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&playerKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen < 8 || chosen > 12)
            return;
        auto p = safe->studio.getChordPads();
        p.octaveOffset = chosen - 10;
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::showModeMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("PLAIN CHORDS: A CHORD PLAYED WHILE ANOTHER IS HELD");
    menu.addItem (1, "Hold First (the first goes on; common notes aren't played again)", true, pads.overlap == ChordPads::Overlap::holdFirst);
    menu.addItem (2, "Legato (the first ends, but its common notes go on)", true, pads.overlap == ChordPads::Overlap::legato);
    menu.addItem (3, "Stop First (the first ends, common notes too)", true, pads.overlap == ChordPads::Overlap::stopFirst);
    menu.addSeparator();
    menu.addItem (99, "(Pattern and Sections modes: a later release)", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modeKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen < 1 || chosen > 3)
            return;
        auto p = safe->studio.getChordPads();
        p.overlap = chosen == 1 ? ChordPads::Overlap::holdFirst : chosen == 2 ? ChordPads::Overlap::legato : ChordPads::Overlap::stopFirst;
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::setPadCount (int count)
{
    auto p = studio.getChordPads();
    count = juce::jlimit (1, 64, count);
    p.pads.resize ((size_t) count);
    p.triggerEnd = juce::jmin (127, p.triggerStart + count - 1);
    if (p.adaptiveReference >= count)
        p.adaptiveReference = -1;
    studio.setChordPads (p);
}

void ChordPadsView::showDisplayMenu()
{
    const auto& pads = studio.getChordPads();
    using D = ChordPads::Display;
    juce::PopupMenu primary, secondary, layout, start, grid;
    const std::pair<D, const char*> names[] = { { D::noteName, "Note Name" }, { D::roman, "Roman Numerals" }, { D::nashville, "Nashville Number System" } };
    for (const auto& [d, name] : names)
    {
        primary.addItem (10 + (int) d, name, true, pads.primary == d);
        secondary.addItem (20 + (int) d, name, true, pads.secondary == d);
    }
    secondary.addItem (20 + (int) D::none, "None", true, pads.secondary == D::none);
    layout.addItem (30, "Keyboard: 12 pads", true, pads.layout == ChordPads::Layout::keyboard && pads.pads.size() == 12);
    layout.addItem (31, "Keyboard: 24 pads", true, pads.layout == ChordPads::Layout::keyboard && pads.pads.size() == 24);
    const std::pair<int, int> grids[] = { { 2, 4 }, { 2, 6 }, { 2, 8 }, { 3, 4 }, { 4, 4 } };
    for (int i = 0; i < 5; ++i)
        layout.addItem (40 + i, "Grid: " + juce::String (grids[i].first) + " x " + juce::String (grids[i].second), true,
                        pads.layout == ChordPads::Layout::grid && pads.rows == grids[i].first && pads.columns == grids[i].second);
    for (int i = 0; i < 12; ++i)
        start.addItem (50 + i, Chord::noteName (i, pads.flats), true, pads.startNote == i);
    juce::PopupMenu menu;
    menu.addSectionHeader ("CHORD SYMBOLS");
    menu.addSubMenu ("Primary Type", primary);
    menu.addSubMenu ("Secondary Type", secondary);
    menu.addSeparator();
    menu.addSectionHeader ("PAD LAYOUT");
    menu.addSubMenu ("Type and Number of Pads", layout);
    menu.addSubMenu ("Start Note (keyboard)", start, pads.layout == ChordPads::Layout::keyboard);
    menu.addSeparator();
    menu.addItem (60, "Show Voicing Indicators", true, pads.showVoicing);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&displayKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto p = safe->studio.getChordPads();
        const std::pair<int, int> gridSizes[] = { { 2, 4 }, { 2, 6 }, { 2, 8 }, { 3, 4 }, { 4, 4 } };
        if (chosen >= 10 && chosen < 20)      p.primary = (D) (chosen - 10);
        else if (chosen >= 20 && chosen < 30) p.secondary = (D) (chosen - 20);
        else if (chosen == 30 || chosen == 31)
        {
            p.layout = ChordPads::Layout::keyboard;
            safe->studio.setChordPads (p);
            safe->setPadCount (chosen == 30 ? 12 : 24);
            return;
        }
        else if (chosen >= 40 && chosen < 45)
        {
            p.layout = ChordPads::Layout::grid;
            p.rows = gridSizes[chosen - 40].first;
            p.columns = gridSizes[chosen - 40].second;
            safe->studio.setChordPads (p);
            safe->setPadCount (p.rows * p.columns);
            return;
        }
        else if (chosen >= 50 && chosen < 62) p.startNote = chosen - 50;
        else if (chosen == 60)                p.showVoicing = ! p.showVoicing;
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::showRemoteMenu()
{
    const auto& pads = studio.getChordPads();
    juce::PopupMenu menu;
    menu.addSectionHeader ("THE KEYS THAT PLAY THE PADS");
    for (int octave = -1; octave <= 4; ++octave)
    {
        const auto first = (octave + 2) * 12;
        menu.addItem (10 + octave + 1, keyName (first, pads.flats) + " to " + keyName (first + (int) pads.pads.size() - 1, pads.flats), true,
                      pads.triggerStart == first);
    }
    menu.addSeparator();
    menu.addItem (30, "Latch Chords (a pad plays until it's played again)", true, pads.latch);
    menu.addItem (99, "(They work while the chord pads are open. MIDI Learn and the modifiers: a later release)", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&remoteKey),
                        [safe = juce::Component::SafePointer<ChordPadsView> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        auto p = safe->studio.getChordPads();
        if (chosen == 30)
            p.latch = ! p.latch;
        else if (chosen >= 10 && chosen <= 15)
        {
            p.triggerStart = (chosen - 11 + 2) * 12;
            p.triggerEnd = juce::jmin (127, p.triggerStart + (int) p.pads.size() - 1);
        }
        safe->studio.setChordPads (p);
    });
}

void ChordPadsView::showPadMenu (int pad)
{
    const auto& p = studio.getChordPads().pads[(size_t) pad];
    juce::PopupMenu menu;
    menu.addItem (1, "Edit Chord...", ! p.locked);
    menu.addItem (2, "Assign from MIDI Input", ! p.locked);
    menu.addItem (3, "Remove Chord", p.chord.has_value() && ! p.locked);
    menu.addSeparator();
    menu.addItem (4, "Lock", true, p.locked);
    menu.addItem (5, "Adaptive Voicing", ! p.locked, p.adaptive);
    menu.addItem (6, "Adaptive Voicing Reference", p.chord.has_value(), studio.getChordPads().adaptiveReference == pad);
    menu.addSeparator();
    menu.addItem (7, "Transpose Up", p.chord.has_value() && ! p.locked);
    menu.addItem (8, "Transpose Down", p.chord.has_value() && ! p.locked);
    // Its voicings by name, with their notes as the key spells them (0.15.0).
    if (p.chord)
    {
        juce::PopupMenu voicings;
        const auto key = studio.getChordPads().key();
        const auto named = pr7s::namedVoicings (*p.chord, 48 + 12 * studio.getChordPads().octaveOffset);
        const auto now = studio.getPadVoicing (pad);
        for (int i = 0; i < (int) named.size(); ++i)
            voicings.addItem (1000 + i, named[(size_t) i].kind + ":  " + pr7s::notesText (named[(size_t) i].notes, *p.chord, key),
                              ! p.locked, ! p.adaptive && now == i);
        voicings.addSeparator();
        voicings.addItem (999, "Adaptive (chosen as it's played)", ! p.locked, p.adaptive);
        menu.addSeparator();
        menu.addSubMenu ("Voicing", voicings, ! p.locked);
    }
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ChordPadsView> (this), pad] (int chosen)
    {
        if (safe == nullptr)
            return;
        auto& s = safe->studio;
        if (chosen >= 1000)
        {
            s.setPadVoicing (pad, chosen - 1000);
            safe->audition (pad);
            return;
        }
        if (chosen == 999)
        {
            s.setPadsAdaptive ({ pad }, true);
            return;
        }
        switch (chosen)
        {
            case 1: safe->setEditorShown (true); break;
            case 2: s.setAssignFromInput ({ pad }); break;
            case 3: safe->removeSelected (true); break;
            case 4: s.setPadsLocked ({ pad }, ! s.getChordPads().pads[(size_t) pad].locked); break;
            case 5: s.setPadsAdaptive ({ pad }, ! s.getChordPads().pads[(size_t) pad].adaptive); break;
            case 6: s.setAdaptiveReference (s.getChordPads().adaptiveReference == pad ? -1 : pad); break;
            case 7: s.transposePads ({ pad }, 1); safe->audition (pad); break;
            case 8: s.transposePads ({ pad }, -1); safe->audition (pad); break;
            default: break;
        }
    });
}

//==============================================================================
juce::Rectangle<float> ChordPadsView::getPadBounds (int pad) const
{
    const auto& pads = studio.getChordPads();
    const auto count = (int) pads.pads.size();
    if (! juce::isPositiveAndBelow (pad, count) || padsArea.isEmpty())
        return {};
    const auto area = padsArea.toFloat();
    if (pads.layout == ChordPads::Layout::grid)
    {
        // As a pad controller's: the first at the bottom left.
        const auto cols = juce::jmax (1, pads.columns), rows = juce::jmax (1, (count + cols - 1) / cols);
        const auto w = area.getWidth() / (float) cols, h = area.getHeight() / (float) rows;
        const auto row = rows - 1 - pad / cols, col = pad % cols;
        return { area.getX() + w * (float) col, area.getY() + h * (float) row, w, h };
    }
    // Keyboard: white keys' pads along the bottom, black keys' above, between them.
    auto whites = 0;
    for (int i = 0; i < count; ++i)
        whites += isBlack (pads.startNote + i) ? 0 : 1;
    const auto w = area.getWidth() / (float) juce::jmax (1, whites), h = area.getHeight() * 0.5f;
    auto white = 0;
    for (int i = 0; i < pad; ++i)
        white += isBlack (pads.startNote + i) ? 0 : 1;
    if (isBlack (pads.startNote + pad))
        return { juce::jlimit (area.getX(), area.getRight() - w, area.getX() + w * (float) white - w * 0.5f), area.getY(), w, h };
    return { area.getX() + w * (float) white, area.getY() + h, w, h };
}

int ChordPadsView::padAt (juce::Point<float> at) const
{
    for (int i = 0; i < (int) studio.getChordPads().pads.size(); ++i)
        if (getPadBounds (i).reduced (2.0f).contains (at))
            return i;
    return -1;
}

void ChordPadsView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0a0e));
    if (content != getLocalBounds())
    {
        // (Beyond the middle screen: the zone's dark floor, its edges marked.)
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (content.getX() - 1, content.getY(), 1, content.getHeight());
        g.fillRect (content.getRight(), content.getY(), 1, content.getHeight());
    }
    for (const auto& strip : { bar, extendedBar })
    {
        g.setColour (juce::Colour (0xff100c11));
        g.fillRect (strip);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (strip.withY (strip.getBottom() - 1).withHeight (1));
    }
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (9.5f));
    g.drawText ("CHORD PADS", bar.withTrimmedLeft (14).toFloat(), juce::Justification::centredLeft, false);

    const auto count = (int) studio.getChordPads().pads.size();
    for (int i = 0; i < count; ++i)
        paintPad (g, i, getPadBounds (i));
    paintKeyboard (g, keysArea.toFloat());
}

void ChordPadsView::paintPad (juce::Graphics& g, int index, juce::Rectangle<float> r)
{
    const auto& pads = studio.getChordPads();
    const auto& pad = pads.pads[(size_t) index];
    r = r.reduced (3.0f);
    const auto pressed = index < (int) shownPressed.size() && shownPressed[(size_t) index];
    if (pressed)
        g.setGradientFill (juce::ColourGradient (ParagonColours::meterColour (0.0f), r.getX(), r.getBottom(),
                                                 ParagonColours::meterColour (1.0f), r.getX(), r.getY(), false));
    else
        g.setColour (pad.chord ? padBody : padEmpty);
    g.fillRoundedRectangle (r, 5.0f);

    const auto isSelected = std::find (selected.begin(), selected.end(), index) != selected.end();
    const auto& targets = studio.getAssignFromInput();
    const auto isInputTarget = ! targets.empty() && targets.front() == index;
    if (index == dropPad)                      { g.setColour (ParagonColours::accent); g.drawRoundedRectangle (r, 5.0f, 2.5f); }
    else if (isInputTarget)                    { g.setColour (juce::Colour (0xffd42e2a)); g.drawRoundedRectangle (r, 5.0f, 2.5f); }
    else if (isSelected)                       { g.setColour (pearl); g.drawRoundedRectangle (r, 5.0f, 2.0f); }
    else if (pads.adaptiveReference == index)  { g.setColour (ParagonColours::accent); g.drawRoundedRectangle (r, 5.0f, 2.0f); }
    else                                       { g.setColour (ParagonColours::panelEdge); g.drawRoundedRectangle (r, 5.0f, 1.0f); }

    const auto ink = pressed ? juce::Colour (0xff120d10) : ParagonColours::text;
    const auto dim = pressed ? juce::Colour (0xff3a2a26) : ParagonColours::textDim;
    auto inside = r.reduced (6.0f, 4.0f);
    auto top = inside.removeFromTop (14.0f);
    auto bottom = inside.removeFromBottom (14.0f);
    g.setFont (font (10.0f));
    // The top corners: the reference, and adaptive voicing (left); the lock (right).
    if (pads.adaptiveReference == index)
    {
        g.setColour (pressed ? ink : ParagonColours::accent);
        g.drawText ("REF", top, juce::Justification::centredLeft, false);
    }
    if (pad.locked)
        drawLock (g, top.removeFromRight (10.0f).withSizeKeepingCentre (9.0f, 12.0f), dim);
    else if (pad.chord && pad.adaptive)
    {
        g.setColour (dim);
        g.drawText ("A", top, juce::Justification::centredRight, false);
    }
    // The trigger key at the bottom right; the voicing at the bottom left.
    g.setColour (dim);
    const auto key = pads.triggerStart + index;
    if (key <= pads.triggerEnd)
        g.drawText (keyName (key, pads.flats), bottom, juce::Justification::centredRight, false);
    if (pad.chord && pads.showVoicing && r.getHeight() > 56.0f)
    {
        const auto at = studio.getPadVoicing (index), of = studio.getPadVoicingCount (index);
        g.drawText (at >= 0 ? "V " + juce::String (at + 1) + "/" + juce::String (of) : juce::String ("V PLAYED"), bottom, juce::Justification::centredLeft, false);
    }
    // The chord: large, and the second way of naming it under it.
    if (! pad.chord)
    {
        g.setColour (dim.withAlpha (0.6f));
        g.setFont (ParagonLookAndFeel::titleFont (18.0f));
        g.drawText ("+", inside, juce::Justification::centred, false);
        return;
    }
    const auto primary = pads.displayName (*pad.chord, pads.primary);
    const auto secondary = pads.secondary == pads.primary ? juce::String() : pads.displayName (*pad.chord, pads.secondary);
    const auto big = juce::jlimit (12.0f, 30.0f, inside.getHeight() * 0.42f);
    auto nameArea = secondary.isNotEmpty() && inside.getHeight() > 40.0f ? inside.removeFromTop (inside.getHeight() * 0.66f) : inside;
    g.setColour (ink);
    g.setFont (chordFont (big * 0.85f));
    g.drawFittedText (primary, nameArea.toNearestInt(), juce::Justification::centredBottom, 1, 0.6f);
    if (secondary.isNotEmpty() && inside.getHeight() > 8.0f && nameArea != inside)
    {
        g.setColour (dim);
        g.setFont (chordFont (juce::jmin (12.0f, big * 0.45f)));
        g.drawFittedText (secondary, inside.toNearestInt(), juce::Justification::centredTop, 1, 0.6f);
    }
}

void ChordPadsView::paintKeyboard (juce::Graphics& g, juce::Rectangle<float> area)
{
    // From C0 to C6: what's sounding lit, the pads' trigger keys tinted, the key
    // of the pad under the mouse outlined (the manual's chord pads zone keyboard).
    const auto& pads = studio.getChordPads();
    constexpr int low = 24, high = 96;
    g.setColour (juce::Colour (0xff0a080b));
    g.fillRect (area);
    area = area.reduced (4.0f, 3.0f);
    auto whites = 0;
    for (int n = low; n <= high; ++n)
        whites += isBlack (n) ? 0 : 1;
    const auto w = area.getWidth() / (float) whites;
    const auto sounding = [this] (int n) { return std::find (shownSounding.begin(), shownSounding.end(), n) != shownSounding.end(); };
    const auto trigger = [&pads] (int n) { return n >= pads.triggerStart && n <= pads.triggerEnd && n - pads.triggerStart < (int) pads.pads.size(); };
    const auto hoverKey = hoverPad >= 0 ? pads.triggerStart + hoverPad : -1;
    std::vector<std::pair<int, juce::Rectangle<float>>> blacks;
    auto x = area.getX();
    for (int n = low; n <= high; ++n)
    {
        if (isBlack (n))
        {
            blacks.push_back ({ n, { x - w * 0.32f, area.getY(), w * 0.64f, area.getHeight() * 0.6f } });
            continue;
        }
        const juce::Rectangle<float> key (x, area.getY(), w - 1.0f, area.getHeight());
        g.setColour (sounding (n) ? ParagonColours::accent : trigger (n) ? juce::Colour (0xffe8b0b8) : juce::Colour (0xffd8d2d4));
        g.fillRect (key);
        if (n == hoverKey)
        {
            g.setColour (juce::Colour (0xffde0639));
            g.drawRect (key, 2.0f);
        }
        if (n % 12 == 0)
        {
            g.setColour (ParagonColours::ink);
            g.setFont (font (8.0f));
            g.drawText (keyName (n, false), key.withTop (key.getBottom() - 11.0f), juce::Justification::centred, false);
        }
        x += w;
    }
    for (const auto& [n, key] : blacks)
    {
        g.setColour (sounding (n) ? ParagonColours::accent : trigger (n) ? juce::Colour (0xff8a2a44) : juce::Colour (0xff1a1619));
        g.fillRect (key);
        if (n == hoverKey)
        {
            g.setColour (juce::Colour (0xffde0639));
            g.drawRect (key, 2.0f);
        }
    }
}

void ChordPadsView::resized()
{
    // On one screen (0.15.0, the owner: across three screens the pads were stretched):
    // the part of this view on the screen under its centre, and never wider than a
    // screen's worth (1920) when we can't tell (the tests, one wide display).
    content = getLocalBounds();
    if (isShowing())
    {
        const auto onScreen = getScreenBounds();
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForPoint (onScreen.getCentre()))
        {
            const auto here = display->totalArea.getIntersection (onScreen) - onScreen.getPosition();
            if (! here.isEmpty())
                content = here;
        }
    }
    if (content.getWidth() > 2400)
        content = content.withSizeKeepingCentre (1920, content.getHeight());

    // The screens either side (0.17.0, the owner): the keys on the left, the circle and its
    // suggestions on the right, whenever there's a screen's worth of room (docked or a window).
    const auto leftSide = getLocalBounds().withRight (content.getX());
    const auto rightSide = getLocalBounds().withLeft (content.getRight());
    keyList->setVisible (leftSide.getWidth() >= sideMinimum);
    if (keyList->isVisible())
        keyList->setBounds (leftSide.withTrimmedLeft (juce::jmax (0, leftSide.getWidth() - sideMaximum)));
    circleOnSide = rightSide.getWidth() >= sideMinimum;
    circleKey.setEnabled (! circleOnSide);
    circleKey.setTooltip (circleOnSide ? "The circle of fifths is on the screen to the right" : "The circle of fifths and what could come next, beside the pads");

    auto area = content;
    bar = area.removeFromTop (toolbarHeight);
    extendedBar = area.removeFromTop (toolbarHeight);
    keysArea = area.removeFromBottom (keyboardHeight);
    if (editorShown)
        editor->setBounds (area.removeFromRight (juce::jmin (editorWidth, area.getWidth() / 2)));
    circle->setVisible (circleOnSide || circleShown);
    if (circleOnSide)
        circle->setBounds (rightSide.withWidth (juce::jmin (rightSide.getWidth(), sideMaximum)));
    else if (circleShown)
        circle->setBounds (area.removeFromRight (juce::jmin (circleWidth, area.getWidth() * 3 / 5)));
    padsArea = area.reduced (6, 4);

    auto b = bar.reduced (6, 4);
    b.removeFromLeft (70);
    closeKey.setBounds (b.removeFromRight (64));
    b.removeFromRight (6);
    windowKey.setBounds (b.removeFromRight (74));
    b.removeFromRight (12);
    const auto place = [&b] (juce::TextButton& key, int width, int gap = 4)
    {
        key.setBounds (b.removeFromLeft (width));
        b.removeFromLeft (gap);
    };
    place (rootKey, 150, 10);
    place (presetsKey, 120, 2);
    place (previousPreset, 26, 2);
    place (nextPreset, 26);
    place (saveKey, 56, 10);
    place (playerKey, 120);
    place (modeKey, 140, 10);
    place (displayKey, 84);
    place (remoteKey, 80, 10);
    place (editorKey, 70);
    place (circleKey, 116);

    auto e = extendedBar.reduced (6, 4);
    const auto placeE = [&e] (juce::TextButton& key, int width, int gap = 4)
    {
        key.setBounds (e.removeFromLeft (width));
        e.removeFromLeft (gap);
    };
    placeE (selectAllKey, 84, 10);
    placeE (editKey, 86);
    placeE (fromMidiKey, 84);
    placeE (removeKey, 74, 10);
    placeE (transposeDown, 96, 2);
    placeE (transposeUp, 28, 8);
    placeE (tensionsDown, 90, 2);
    placeE (tensionsUp, 28, 8);
    placeE (voicingPrevious, 84, 2);
    placeE (voicingNext, 28, 10);
    placeE (lockKey, 56);
    placeE (adaptiveKey, 84);
    placeE (referenceKey, 90, 10);
    placeE (fromChordTrackKey, 132);
}

//==============================================================================
void ChordPadsView::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto pad = padAt (event.position);
    dragStarted = false;
    if (pad < 0)
    {
        if (! event.mods.isCommandDown() && ! event.mods.isShiftDown() && padsArea.contains (event.getPosition()))
            selectPads ({});
        return;
    }
    if (event.mods.isPopupMenu())
    {
        if (std::find (selected.begin(), selected.end(), pad) == selected.end())
            selectPads ({ pad });
        showPadMenu (pad);
        return;
    }
    if (event.mods.isCommandDown())
    {
        auto now = selected;
        if (const auto it = std::find (now.begin(), now.end(), pad); it != now.end())
            now.erase (it);
        else
            now.insert (now.begin(), pad);   // (the newest first: the modifiers start from it)
        selectPads (now);
    }
    else if (event.mods.isShiftDown() && ! selected.empty())
    {
        std::vector<int> run;
        const auto from = selected.front();
        for (int i = juce::jmin (from, pad); i <= juce::jmax (from, pad); ++i)
            run.push_back (i);
        std::stable_partition (run.begin(), run.end(), [from] (int i) { return i == from; });
        selectPads (run);
    }
    else
        selectPads ({ pad });
    // Clicked: played, as long as it's held (and the suggestions follow it).
    circleFrom.reset();
    pressedPad = pad;
    studio.getPadPlayer().press (pad, 100);
}

void ChordPadsView::mouseDrag (const juce::MouseEvent& event)
{
    if (pressedPad < 0 || dragStarted || event.getDistanceFromDragStart() < 6
        || ! studio.getChordPads().pads[(size_t) pressedPad].chord)
        return;
    // Dragged: onto another pad (swap; Alt: copy) or onto a track (a MIDI part).
    dragStarted = true;
    studio.getPadPlayer().release (pressedPad);
    const auto r = getPadBounds (pressedPad).toNearestInt();
    juce::ScaledImage image (createComponentSnapshot (r, true).convertedToFormat (juce::Image::ARGB));
    image.getImage().multiplyAllAlphas (0.75f);
    startDragging (juce::String (dragPrefix) + juce::String (pressedPad) + (event.mods.isAltDown() ? ":copy" : ""), this, image, true);
}

void ChordPadsView::mouseUp (const juce::MouseEvent&)
{
    if (pressedPad >= 0 && ! dragStarted)
        studio.getPadPlayer().release (pressedPad);
    pressedPad = -1;
    dragStarted = false;
}

void ChordPadsView::mouseMove (const juce::MouseEvent& event)
{
    const auto pad = padAt (event.position);
    if (pad != hoverPad)
    {
        hoverPad = pad;
        repaint (keysArea);
    }
    if (pad >= 0)
        if (const auto& chord = studio.getChordPads().pads[(size_t) pad].chord)
            setTooltip (chord->name (studio.getChordPads().flats));
}

void ChordPadsView::mouseExit (const juce::MouseEvent&)
{
    hoverPad = -1;
    repaint (keysArea);
}

void ChordPadsView::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (const auto pad = padAt (event.position); pad >= 0)
    {
        selectPads ({ pad });
        setEditorShown (true);
    }
}

void ChordPadsView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto pad = padAt (event.position);
    if (pad < 0 || ! studio.getChordPads().pads[(size_t) pad].chord)
        return;
    const auto delta = std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX;
    if (std::abs (delta) < 1.0e-4f)
        return;
    const auto up = delta > 0.0f;
    // (The manual: the wheel over a pad: Shift transposes, Alt changes tensions, alone: up is the previous voicing.)
    if (event.mods.isShiftDown())
        studio.transposePads ({ pad }, up ? 1 : -1);
    else if (event.mods.isAltDown())
        studio.changePadTensions ({ pad }, up ? 1 : -1);
    else
        studio.changePadVoicings ({ pad }, up ? -1 : 1);
    audition (pad);
}

bool ChordPadsView::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        removeSelected (true);
        return true;
    }
    if (mods.isCommandDown() && ! mods.isShiftDown() && (key.getKeyCode() == 'A' || key.getKeyCode() == 'a'))
    {
        selectAll();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        setEditorShown (true);
        return true;
    }
    if (mods.isCommandDown() && mods.isShiftDown() && (key.getKeyCode() == 'L' || key.getKeyCode() == 'l'))
    {
        lockKey.onClick();
        return true;
    }
    return false;   // (the studio's keys: Space and the rest)
}

//==============================================================================
bool ChordPadsView::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith (dragPrefix) || details.description.toString().startsWith (chordDragPrefix);
}

void ChordPadsView::itemDragEnter (const SourceDetails& details) { itemDragMove (details); }

void ChordPadsView::itemDragMove (const SourceDetails& details)
{
    const auto pad = padAt (details.localPosition.toFloat());
    if (pad != dropPad)
    {
        dropPad = pad;
        repaint();
    }
}

void ChordPadsView::itemDragExit (const SourceDetails&)
{
    dropPad = -1;
    repaint();
}

void ChordPadsView::itemDropped (const SourceDetails& details)
{
    // A chord from the circle or the suggestions: onto the pad dropped on.
    if (details.description.toString().startsWith (chordDragPrefix))
    {
        const auto to = padAt (details.localPosition.toFloat());
        dropPad = -1;
        if (const auto chord = Chord::fromText (details.description.toString().fromFirstOccurrenceOf (chordDragPrefix, false, false));
            chord && to >= 0 && ! studio.getChordPads().pads[(size_t) to].locked)
        {
            studio.assignChord (to, *chord);
            selectPads ({ to });
            audition (to);
        }
        repaint();
        return;
    }
    const auto text = details.description.toString().fromFirstOccurrenceOf (dragPrefix, false, false);
    const auto from = text.getIntValue();
    const auto copy = text.endsWith (":copy");
    const auto to = padAt (details.localPosition.toFloat());
    dropPad = -1;
    if (to >= 0 && to != from)
    {
        copy ? studio.copyPad (from, to) : studio.swapPads (from, to);
        selectPads ({ to });
    }
    repaint();
}

//==============================================================================
void ChordPadsView::timerCallback()
{
    // What's sounding and which pads are down: drawn only when it changes.
    const auto sounding = studio.getPadPlayer().getSounding();
    std::vector<bool> pressed (studio.getChordPads().pads.size());
    for (size_t i = 0; i < pressed.size(); ++i)
        pressed[i] = studio.getPadPlayer().isPressed ((int) i);
    if (sounding != shownSounding)
    {
        shownSounding = sounding;
        repaint (keysArea);
    }
    if (pressed != shownPressed)
    {
        shownPressed = pressed;
        repaint (padsArea.expanded (4));
    }
}
