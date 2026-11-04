#include "ui/ProgressionsPage.h"

#include "ui/ChordUi.h"

namespace
{
    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }
    const juce::Colour page { 0xff0c0a0d }, rowDark { 0xff151217 }, rowLit { 0xff1e181f }, cell { 0xff171318 };

    // A star (the favourite mark), filled or outlined.
    void drawStar (juce::Graphics& g, juce::Rectangle<float> r, bool filled)
    {
        juce::Path star;
        star.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
        if (filled)
        {
            g.setColour (ParagonColours::accent);
            g.fillPath (star);
        }
        else
        {
            g.setColour (ParagonColours::textDim.withAlpha (0.6f));
            g.strokePath (star, juce::PathStrokeType (1.2f));
        }
    }
}

//==============================================================================
// The rows, in a viewport: painted (no components per row), only those in sight.
class ProgressionsPage::Rows final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    explicit Rows (ProgressionsPage& p) : owner (p) {}

    enum class Part { none, star, chords, hear, track, pads, ideas };
    static constexpr int starWidth = 34, nameWidth = 250, keyWidth = 82, keyGap = 6;

    juce::Rectangle<int> rowBounds (int row) const { return { 0, row * rowHeight, getWidth(), rowHeight }; }
    juce::Rectangle<int> keyBounds (int row, int key) const   // 0 HEAR, 1 TO TRACK, 2 TO PADS, 3 IDEAS
    {
        auto r = rowBounds (row).reduced (8, 14);
        r.removeFromRight ((3 - key) * (keyWidth + keyGap));
        return r.removeFromRight (keyWidth);
    }
    juce::Rectangle<int> chordsBounds (int row) const
    {
        auto r = rowBounds (row).reduced (8, 6);
        r.removeFromLeft (starWidth + nameWidth);
        r.removeFromRight (4 * (keyWidth + keyGap) + 10);
        return r;
    }

    std::pair<int, Part> hit (juce::Point<int> p) const
    {
        const auto row = p.y / rowHeight;
        if (p.y < 0 || row >= (int) owner.shown.size())
            return { -1, Part::none };
        for (int k = 0; k < 4; ++k)
            if (keyBounds (row, k).contains (p))
                return { row, (Part) ((int) Part::hear + k) };
        if (p.x < 8 + starWidth)
            return { row, Part::star };
        return { row, Part::chords };
    }

    void paint (juce::Graphics& g) override
    {
        const auto clip = g.getClipBounds();
        const auto favorites = owner.studio.getProgressionFavorites();
        const auto& library = pr7s::progressionLibrary();
        if (owner.shown.empty())
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (14.0f));
            g.drawText ("Nothing here: try another genre or mood, or clear the search.", getLocalBounds().withHeight (80), juce::Justification::centred, false);
            return;
        }
        for (int row = juce::jmax (0, clip.getY() / rowHeight); row < (int) owner.shown.size() && row * rowHeight < clip.getBottom(); ++row)
        {
            const auto index = owner.shown[(size_t) row];
            const auto& p = library[(size_t) index];
            const auto bounds = rowBounds (row);
            const auto playing = index == owner.hearing;
            g.setColour (playing || row == hoverRow ? rowLit : (row % 2 == 0 ? rowDark : page));
            g.fillRect (bounds);
            if (playing)
            {
                g.setColour (ParagonColours::accent);
                g.fillRect (bounds.withWidth (3));
            }
            auto r = bounds.reduced (8, 6);
            drawStar (g, r.removeFromLeft (starWidth).withSizeKeepingCentre (18, 18).toFloat(), favorites.contains (p.name));
            auto name = r.removeFromLeft (nameWidth).reduced (4, 0);
            g.setColour (ParagonColours::text);
            g.setFont (ChordUi::chordFont (15.0f));
            g.drawText (p.name, name.removeFromTop (name.getHeight() / 2), juce::Justification::bottomLeft, true);
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (11.5f));
            const auto mode = p.minor == owner.key.minor ? juce::String() : "  (" + juce::String (p.minor ? "minor, on the relative minor" : "major, on the relative major") + ")";
            g.drawText (p.genre + "  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  " + p.moods.joinIntoString (", ") + mode,
                        name, juce::Justification::topLeft, true);

            // The chords, each as wide as its bars (at least eight bars' room).
            const auto area = chordsBounds (row).toFloat();
            const auto steps = owner.stepsOf (index);
            const auto perBar = area.getWidth() / (float) juce::jmax (8.0, p.totalBars());
            auto x = area.getX();
            for (size_t s = 0; s < steps.size(); ++s)
            {
                const auto w = perBar * (float) steps[s].bars;
                const auto c = juce::Rectangle<float> (x, area.getY(), w, area.getHeight()).reduced (1.5f, 0.0f);
                x += w;
                const auto lit = playing && (int) s == owner.litStep;
                g.setColour (lit ? ParagonColours::accent : cell);
                g.fillRoundedRectangle (c, 3.0f);
                g.setColour (lit ? juce::Colour (0xff120d10) : ParagonColours::text);
                g.setFont (ChordUi::chordFont (juce::jmin (15.0f, c.getHeight() * 0.36f)));
                auto label = c;
                g.drawFittedText (steps[s].name (owner.key), label.removeFromTop (label.getHeight() * 0.56f).toNearestInt(), juce::Justification::centredBottom, 1, 0.6f);
                g.setColour (lit ? juce::Colour (0xff120d10) : ParagonColours::textDim);
                g.setFont (ChordUi::chordFont (juce::jmin (11.0f, c.getHeight() * 0.26f)));
                g.drawFittedText (steps[s].numeralIn (owner.key), label.toNearestInt(), juce::Justification::centredTop, 1, 0.6f);
            }

            static const char* const names[] { "HEAR", "TO TRACK", "TO PADS", "IDEAS" };
            for (int k = 0; k < 4; ++k)
            {
                const auto kb = keyBounds (row, k).toFloat();
                const auto on = k == 0 && playing;
                const auto over = row == hoverRow && k == hoverKey;
                g.setColour (on ? ParagonColours::accent : over ? juce::Colour (0xff2a2229) : juce::Colour (0xff1b171c));
                g.fillRoundedRectangle (kb, 4.0f);
                g.setColour (ParagonColours::panelEdge);
                g.drawRoundedRectangle (kb.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (on ? juce::Colour (0xff120d10) : ParagonColours::text);
                g.setFont (ParagonLookAndFeel::textFont (13.0f));
                g.drawText (on ? "STOP" : names[k], kb, juce::Justification::centred, false);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto [row, part] = hit (e.getPosition());
        const auto key = (int) part - (int) Part::hear;
        if (row != hoverRow || key != hoverKey)
        {
            repaintRow (hoverRow);
            hoverRow = row;
            hoverKey = key;
            repaintRow (hoverRow);
            switch (part)
            {
                case Part::star:   setTooltip ("A favourite (FAVORITES shows only those)"); break;
                case Part::chords: setTooltip ("Click: hear it. Double-click: onto the chord track at the playhead. Drag: onto the chord track's lane."); break;
                case Part::hear:   setTooltip ("Hear it at the song's tempo"); break;
                case Part::track:  setTooltip ("Onto the chord track from the playhead's bar"); break;
                case Part::pads:   setTooltip ("Onto the chord pads from the first (locked pads skipped)"); break;
                case Part::ideas:  setTooltip ("Onto the chord track, then piano, guitar, bass and pattern ideas for it"); break;
                case Part::none:   setTooltip ({}); break;
            }
        }
    }
    void mouseExit (const juce::MouseEvent&) override { repaintRow (hoverRow); hoverRow = -1; hoverKey = -1; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = false;
        const auto [row, part] = hit (e.getPosition());
        if (row < 0)
            return;
        const auto index = owner.shown[(size_t) row];
        if (e.mods.isPopupMenu())
        {
            showMenu (index);
            return;
        }
        switch (part)
        {
            case Part::star:   owner.toggleFavorite (index); break;
            case Part::hear:   owner.hear (index); break;
            case Part::track:  owner.toChordTrack (index); break;
            case Part::pads:   owner.toPads (index); break;
            case Part::ideas:  owner.toIdeas (index); break;
            case Part::chords: break;   // (on release: it may be a drag)
            case Part::none:   break;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto [row, part] = hit (e.getMouseDownPosition());
        if (dragging || part != Part::chords || e.getDistanceFromDragStart() < 6)
            return;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            const auto index = owner.shown[(size_t) row];
            juce::Image image (juce::Image::ARGB, 200, 30, true);
            {
                juce::Graphics g (image);
                g.setColour (ParagonColours::accent.withAlpha (0.85f));
                g.fillRoundedRectangle (image.getBounds().toFloat(), 5.0f);
                g.setColour (juce::Colour (0xff120d10));
                g.setFont (ChordUi::chordFont (14.0f));
                g.drawText (pr7s::progressionLibrary()[(size_t) index].name, image.getBounds(), juce::Justification::centred, true);
            }
            container->startDragging ("progression:" + pr7s::progressionToText (owner.stepsOf (index)), this,
                                      juce::ScaledImage (image), true);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto [row, part] = hit (e.getMouseDownPosition());
        if (! dragging && part == Part::chords && ! e.mods.isPopupMenu() && e.getNumberOfClicks() == 1 && ! e.mouseWasDraggedSinceMouseDown())
            owner.hear (owner.shown[(size_t) row]);
        dragging = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto [row, part] = hit (e.getPosition());
        if (part == Part::chords)
        {
            owner.stop();
            owner.toChordTrack (owner.shown[(size_t) row]);
        }
    }

    void repaintRow (int row)
    {
        if (row >= 0)
            repaint (rowBounds (row));
    }

private:
    void showMenu (int index)
    {
        const auto& p = pr7s::progressionLibrary()[(size_t) index];
        const auto favorite = owner.studio.getProgressionFavorites().contains (p.name);
        juce::PopupMenu menu;
        menu.addSectionHeader (p.name.toUpperCase());
        menu.addItem (1, owner.hearing == index ? "Stop" : "Hear It");
        menu.addItem (2, "Onto the Chord Track at the Playhead");
        menu.addItem (3, "Onto the Chord Pads");
        menu.addItem (4, "Ideas for It...");
        menu.addSeparator();
        menu.addItem (5, favorite ? "Remove from Favorites" : "Add to Favorites");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [safe = juce::Component::SafePointer<Rows> (this), index] (int chosen)
                            {
                                if (safe == nullptr)
                                    return;
                                auto& o = safe->owner;
                                if (chosen == 1) o.hear (index);
                                if (chosen == 2) o.toChordTrack (index);
                                if (chosen == 3) o.toPads (index);
                                if (chosen == 4) o.toIdeas (index);
                                if (chosen == 5) o.toggleFavorite (index);
                            });
    }

    ProgressionsPage& owner;
    int hoverRow = -1, hoverKey = -1;
    bool dragging = false;
};

//==============================================================================
// The genres and moods at the left, each with how many it holds.
class ProgressionsPage::Filters final : public juce::Component
{
public:
    explicit Filters (ProgressionsPage& p) : owner (p) {}
    static constexpr int itemHeight = 26, headingHeight = 30;

    struct Item { juce::String value; bool mood = false; juce::Rectangle<int> bounds; };
    std::vector<Item> items() const
    {
        std::vector<Item> out;
        auto y = 0;
        for (const auto mood : { false, true })
        {
            y += headingHeight;
            for (const auto& value : mood ? owner.getMoodItems() : owner.getGenreItems())
            {
                out.push_back ({ value, mood, { 0, y, getWidth(), itemHeight } });
                y += itemHeight;
            }
            y += 10;
        }
        return out;
    }
    int contentHeight() const
    {
        return 2 * (headingHeight + 10) + itemHeight * (owner.getMoodItems().size() + owner.getGenreItems().size());
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        g.setColour (ParagonColours::textDim);
        g.drawText ("GENRE", 10, 0, getWidth() - 10, headingHeight, juce::Justification::centredLeft, false);
        g.drawText ("MOOD", 10, headingHeight + 10 + itemHeight * owner.getGenreItems().size(), getWidth() - 10, headingHeight,
                    juce::Justification::centredLeft, false);
        for (const auto& item : items())
        {
            const auto chosen = (item.mood ? owner.mood : owner.genre) == item.value;
            const auto count = item.mood ? owner.countFor (owner.genre, item.value) : owner.countFor (item.value, owner.mood);
            auto r = item.bounds.reduced (4, 1);
            if (chosen)
            {
                g.setColour (rowLit);
                g.fillRoundedRectangle (r.toFloat(), 3.0f);
                g.setColour (ParagonColours::accent);
                g.fillRect (r.removeFromLeft (3));
            }
            g.setColour (count == 0 && ! chosen ? ParagonColours::textDim.withAlpha (0.5f) : chosen ? ParagonColours::accent : ParagonColours::text);
            g.setFont (ParagonLookAndFeel::textFont (14.0f));
            auto text = r.reduced (8, 0);
            g.drawText (juce::String (count), text.removeFromRight (34), juce::Justification::centredRight, false);
            g.drawText (item.value.isEmpty() ? "ALL" : item.value, text, juce::Justification::centredLeft, true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (const auto& item : items())
            if (item.bounds.contains (e.getPosition()))
            {
                if (item.mood)
                    owner.setMood (item.value);
                else
                    owner.setGenre (item.value);
                return;
            }
    }

private:
    ProgressionsPage& owner;
};

//==============================================================================
ProgressionsPage::ProgressionsPage (pr7s::Studio& s) : studio (s)
{
    // The key the song is in at the playhead (its chord track), else the pads'.
    const auto songKey = [this]
    {
        return studio.getChordTrack().events.empty() ? studio.getChordPads().key()
                                                     : studio.chordTrackKeyAt (studio.getEngine().getPosition());
    };
    key = songKey();

    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        keyBox.addItem (keys[(size_t) i].name(), i + 1);
    keyBox.setTooltip ("The key the progressions are spelled and played in");
    keyBox.onChange = [this]
    {
        const auto i = keyBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (i, (int) pr7s::Key::all().size()))
            setKey (pr7s::Key::all()[(size_t) i]);
    };
    addAndMakeVisible (keyBox);

    ChordUi::styleKey (followKey);
    followKey.setTooltip ("Back to the song's key (the chord track's at the playhead, else the pads')");
    followKey.onClick = [this, songKey] { setKey (songKey()); };
    addAndMakeVisible (followKey);

    search.setTextToShowWhenEmpty ("SEARCH: a name, a genre, a mood, a chord (\"Am\"), a numeral (\"bVII\")", ParagonColours::textDim);
    search.setFont (interFont (14.0f));
    search.onTextChange = [this] { setSearch (search.getText()); };
    search.onEscapeKey = [this] { search.setText ({}); };
    addAndMakeVisible (search);

    for (auto* k : { &ownModeKey, &favoritesKey })
    {
        ChordUi::styleKey (*k);
        k->setClickingTogglesState (true);
        addAndMakeVisible (*k);
    }
    ownModeKey.setTooltip ("Only the progressions written in the key's own mode (major or minor). Off: the other mode's too, played on the relative key.");
    ownModeKey.onClick = [this] { setOwnModeOnly (ownModeKey.getToggleState()); };
    favoritesKey.setTooltip ("Only your favourites (click a row's star)");
    favoritesKey.onClick = [this] { setFavoritesOnly (favoritesKey.getToggleState()); };
    ChordUi::styleKey (stopKey);
    stopKey.onClick = [this] { stop(); };
    addAndMakeVisible (stopKey);

    filters = std::make_unique<Filters> (*this);
    filtersView.setViewedComponent (filters.get(), false);
    filtersView.setScrollBarsShown (true, false);
    addAndMakeVisible (filtersView);
    rows = std::make_unique<Rows> (*this);
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    rowsView.setSingleStepSizes (rowHeight, rowHeight);
    addAndMakeVisible (rowsView);

    setKey (key);
    setSize (1500, 860);
}

ProgressionsPage::~ProgressionsPage()
{
    if (hearing >= 0)
        studio.stopPreview();
}

pr7s::ProgressionFilter ProgressionsPage::filterFor (const juce::String& g, const juce::String& m) const
{
    pr7s::ProgressionFilter f;
    f.genre = g;
    f.mood = m;
    f.text = search.getText();
    f.ownModeOnly = ownModeOnly;
    f.favoritesOnly = favoritesOnly;
    if (favoritesOnly)
        f.favorites = studio.getProgressionFavorites();
    return f;
}

juce::StringArray ProgressionsPage::getGenreItems() const
{
    juce::StringArray items { "" };
    items.addArray (pr7s::progressionGenres());
    return items;
}

juce::StringArray ProgressionsPage::getMoodItems() const
{
    juce::StringArray items { "" };
    items.addArray (pr7s::progressionMoods());
    return items;
}

int ProgressionsPage::countFor (const juce::String& g, const juce::String& m) const
{
    return (int) pr7s::findProgressions (key, filterFor (g, m)).size();
}

void ProgressionsPage::refresh()
{
    shown = pr7s::findProgressions (key, filterFor (genre, mood));
    rows->setSize (juce::jmax (100, rowsView.getMaximumVisibleWidth()), juce::jmax (1, (int) shown.size()) * rowHeight);
    rows->repaint();
    filters->setSize (juce::jmax (100, filtersView.getMaximumVisibleWidth()), filters->contentHeight());
    filters->repaint();
    repaint (0, 0, getWidth(), headerHeight);
}

void ProgressionsPage::setKey (const pr7s::Key& k)
{
    key = k;
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        if (keys[(size_t) i] == k)
            keyBox.setSelectedId (i + 1, juce::dontSendNotification);
    stop();
    refresh();
}

void ProgressionsPage::setGenre (const juce::String& g)  { genre = g; refresh(); rowsView.setViewPosition (0, 0); }
void ProgressionsPage::setMood (const juce::String& m)   { mood = m; refresh(); rowsView.setViewPosition (0, 0); }

void ProgressionsPage::setSearch (const juce::String& text)
{
    if (search.getText() != text)
        search.setText (text, false);
    refresh();
    rowsView.setViewPosition (0, 0);
}

void ProgressionsPage::setOwnModeOnly (bool on)
{
    ownModeOnly = on;
    ownModeKey.setToggleState (on, juce::dontSendNotification);
    refresh();
}

void ProgressionsPage::setFavoritesOnly (bool on)
{
    favoritesOnly = on;
    favoritesKey.setToggleState (on, juce::dontSendNotification);
    refresh();
}

void ProgressionsPage::toggleFavorite (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) pr7s::progressionLibrary().size()))
        return;
    const auto& name = pr7s::progressionLibrary()[(size_t) index].name;
    studio.setProgressionFavorite (name, ! studio.getProgressionFavorites().contains (name));
    refresh();
}

std::vector<pr7s::ProgressionStep> ProgressionsPage::stepsOf (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) pr7s::progressionLibrary().size()))
        return {};
    return pr7s::progressionLibrary()[(size_t) index].in (key);
}

void ProgressionsPage::hear (int index)
{
    if (index == hearing)
    {
        stop();
        return;
    }
    const auto steps = stepsOf (index);
    if (steps.empty())
        return;
    studio.previewLine (nullptr, studio.progressionSpans (steps, 0.0, key), true);
    hearing = index;
    litStep = 0;
    rows->repaint();
    startTimerHz (30);
}

void ProgressionsPage::stop()
{
    if (hearing >= 0)
        studio.stopPreview();
    hearing = litStep = -1;
    stopTimer();
    if (rows != nullptr)
        rows->repaint();
}

void ProgressionsPage::timerCallback()
{
    if (hearing < 0 || ! studio.isPreviewing())
    {
        stop();
        return;
    }
    const auto steps = stepsOf (hearing);
    const auto bar = studio.getProject().beatsPerBar();
    const auto beat = studio.getPreviewBeat();
    auto step = -1;
    auto at = 0.0;
    for (int i = 0; i < (int) steps.size(); ++i)
    {
        if (beat >= at - 1.0e-6)
            step = i;
        at += steps[(size_t) i].bars * bar;
    }
    if (step != litStep)
    {
        litStep = step;
        const auto row = (int) (std::find (shown.begin(), shown.end(), hearing) - shown.begin());
        rows->repaintRow (row);
    }
}

void ProgressionsPage::toChordTrack (int index)
{
    const auto steps = stepsOf (index);
    if (! steps.empty())
        studio.addProgressionToChordTrack (steps, studio.getEngine().getPosition());
}

int ProgressionsPage::toPads (int index)
{
    std::vector<pr7s::Chord> chords;
    for (const auto& s : stepsOf (index))
        chords.push_back (s.chord);
    return studio.assignProgressionToPads (chords, 0);
}

void ProgressionsPage::toIdeas (int index)
{
    const auto steps = stepsOf (index);
    if (steps.empty())
        return;
    stop();
    const auto bar = studio.getProject().beatsPerBar();
    const auto start = std::floor (juce::jmax (0.0, studio.getEngine().getPosition()) / bar + 1.0e-9) * bar;
    auto end = start;
    for (const auto& s : steps)
        end += s.bars * bar;
    studio.addProgressionToChordTrack (steps, start);
    std::vector<int> events;
    const auto& track = studio.getChordTrack();
    for (int i = 0; i < (int) track.events.size(); ++i)
        if (track.events[(size_t) i].beat >= start - 1.0e-6 && track.events[(size_t) i].beat < end - 1.0e-6)
            events.push_back (i);
    if (onIdeas)
        onIdeas (events);
}

void ProgressionsPage::paint (juce::Graphics& g)
{
    g.fillAll (page);
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    auto title = head.removeFromTop (30);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText ("PROGRESSIONS", title.removeFromLeft (190), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (12.5f));
    g.drawText (juce::String ((int) shown.size()) + " of " + juce::String ((int) pr7s::progressionLibrary().size())
                    + " in " + ChordUi::spokenKey (key) + (genre.isEmpty() ? juce::String() : ", " + genre.toLowerCase())
                    + (mood.isEmpty() ? juce::String() : ", " + mood.toLowerCase())
                    + ".  Click to hear, double-click or drag onto the chord track.",
                title.withTrimmedRight (440), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (filtersWidth + gap, headerHeight, 1, getHeight() - headerHeight - gap);
}

void ProgressionsPage::resized()
{
    auto area = getLocalBounds().reduced (gap, 0);
    auto head = area.removeFromTop (headerHeight).reduced (0, 8);
    auto top = head.removeFromTop (30);
    stopKey.setBounds (top.removeFromRight (90).reduced (0, 2));
    top.removeFromRight (8);
    followKey.setBounds (top.removeFromRight (130).reduced (0, 2));
    top.removeFromRight (8);
    keyBox.setBounds (top.removeFromRight (180).reduced (0, 2));
    head.removeFromTop (10);
    auto bar = head.removeFromTop (32);
    favoritesKey.setBounds (bar.removeFromRight (120));
    bar.removeFromRight (8);
    ownModeKey.setBounds (bar.removeFromRight (150));
    bar.removeFromRight (8);
    search.setBounds (bar);
    area.removeFromBottom (gap);
    filtersView.setBounds (area.removeFromLeft (filtersWidth));
    area.removeFromLeft (gap + 1);
    rowsView.setBounds (area);
    refresh();
}

//==============================================================================
ProgressionsWindow::ProgressionsWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Progressions", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new ProgressionsPage (studio);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (1100, 560, 3840, 2160);
}

void ProgressionsWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
