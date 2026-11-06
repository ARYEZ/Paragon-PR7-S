#include "ui/DrumLearnPage.h"

#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"
#include "engine/SessionLog.h"

#include <thread>

namespace
{
    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }
    const juce::Colour page { 0xff0c0a0d }, rowDark { 0xff151217 }, rowLit { 0xff1e181f };

    juce::String plural (int n, const juce::String& word) { return juce::String (n) + " " + word + (n == 1 ? "" : "s"); }
}

juce::Colour DrumLearnPage::colourOf (pr7s::DrumSection s)
{
    using S = pr7s::DrumSection;
    switch (s)
    {
        case S::intro: return juce::Colour (0xffe9e1dc);                       // pearl white
        case S::verse: return pr7s::FamilyColours::orange;
        case S::chorus: return pr7s::FamilyColours::pink;
        case S::bridge: return pr7s::FamilyColours::paletteColour (6, 5);       // a pale orange-red
        case S::breakdown: return pr7s::FamilyColours::red;
        case S::ending: return juce::Colour (0xff8d8389);                      // pearl onyx, lighter
        case S::solo: return pr7s::FamilyColours::paletteColour (2, 3);        // (0.65.0) a deep pink-red
    }
    return pr7s::FamilyColours::orange;
}

//==============================================================================
// The songs, a row each, painted (only those in sight).
class DrumLearnPage::Rows final : public juce::Component,
                                  public juce::SettableTooltipClient,
                                  private juce::Timer
{
public:
    explicit Rows (DrumLearnPage& p) : owner (p)
    {
        setTooltip ("Click a section: what it is, leave it out, split or join it. Drag its left edge to move its start. Double-click: hear it.");
    }

    juce::Rectangle<int> rowBounds (int song) const { return { 0, song * rowHeight, getWidth(), rowHeight }; }
    juce::Rectangle<int> useBox (int song) const { return rowBounds (song).removeFromLeft (useWidth).withSizeKeepingCentre (18, 18).translated (6, 0); }
    juce::Rectangle<int> strip (int song) const
    {
        auto r = rowBounds (song).reduced (0, 10);
        r.removeFromLeft (useWidth + nameWidth);
        return r.withTrimmedRight (8);
    }
    float barWidth (int song) const
    {
        const auto bars = (int) owner.songs[(size_t) song].bars.size();
        return (float) strip (song).getWidth() / (float) juce::jmax (1, bars);
    }
    juce::Rectangle<int> sectionBounds (int song, int section) const
    {
        const auto& s = owner.songs[(size_t) song].sections[(size_t) section];
        const auto r = strip (song);
        const auto w = barWidth (song);
        const auto x0 = r.getX() + (int) std::round (s.firstBar * w), x1 = r.getX() + (int) std::round ((s.firstBar + s.bars) * w);
        return { x0, r.getY(), juce::jmax (1, x1 - x0 - 1), r.getHeight() };
    }

    struct Hit { int song = -1, section = -1, bar = -1; bool edge = false, use = false; };
    Hit hit (juce::Point<int> p) const
    {
        Hit h;
        const auto song = p.y / rowHeight;
        if (p.y < 0 || song >= (int) owner.songs.size())
            return h;
        h.song = song;
        if (useBox (song).expanded (4).contains (p))
        {
            h.use = true;
            return h;
        }
        const auto& sections = owner.songs[(size_t) song].sections;
        for (int i = 0; i < (int) sections.size(); ++i)
        {
            const auto b = sectionBounds (song, i);
            if (i > 0 && std::abs (p.x - b.getX()) <= 5 && p.y >= b.getY() && p.y < b.getBottom())
            {
                h.section = i;
                h.edge = true;
                return h;
            }
            if (b.withWidth (b.getWidth() + 1).contains (p))
            {
                h.section = i;
                h.bar = juce::jlimit (sections[(size_t) i].firstBar, sections[(size_t) i].firstBar + sections[(size_t) i].bars - 1,
                                      (int) ((p.x - strip (song).getX()) / barWidth (song)));
                return h;
            }
        }
        return h;
    }

    void paint (juce::Graphics& g) override
    {
        const auto clip = g.getClipBounds();
        if (owner.songs.empty())
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (14.0f));
            g.drawFittedText ("CHOOSE FOLDER... and pick a folder of drum MIDI: whole songs, one song a file (the folders under it are read too).\n"
                              "Each song's sections are found from the drums and named; put right what's wrong here, then name the drummer and SAVE PROFILE.",
                              getLocalBounds().withHeight (120).reduced (20, 0), juce::Justification::centred, 4, 1.0f);
            return;
        }
        for (int song = juce::jmax (0, clip.getY() / rowHeight); song < (int) owner.songs.size() && song * rowHeight < clip.getBottom(); ++song)
        {
            const auto& s = owner.songs[(size_t) song];
            const auto bounds = rowBounds (song);
            g.setColour (song == hoverSong ? rowLit : (song % 2 == 0 ? rowDark : page));
            g.fillRect (bounds);

            // The tick: learn from it or not.
            const auto box = useBox (song).toFloat();
            g.setColour (ParagonColours::panelEdge);
            g.drawRoundedRectangle (box, 3.0f, 1.2f);
            if (s.use)
            {
                g.setColour (ParagonColours::accent);
                g.fillRoundedRectangle (box.reduced (4.0f), 2.0f);
            }
            auto name = bounds.withTrimmedLeft (useWidth + 8).withWidth (nameWidth - 16).reduced (0, 8);
            g.setColour (s.use ? ParagonColours::text : ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::titleFont (17.0f));
            g.drawText (s.name, name.removeFromTop (name.getHeight() / 2), juce::Justification::bottomLeft, true);
            g.setColour (ParagonColours::textDim);
            g.setFont (interFont (11.5f));
            g.drawText (plural ((int) s.bars.size(), "bar") + ", " + juce::String (juce::roundToInt (s.bpm)) + " BPM, " + plural ((int) s.sections.size(), "section"),
                        name, juce::Justification::topLeft, true);

            for (int i = 0; i < (int) s.sections.size(); ++i)
            {
                const auto& sec = s.sections[(size_t) i];
                const auto r = sectionBounds (song, i).toFloat();
                const auto colour = colourOf (sec.kind);
                const auto on = s.use && sec.used;
                g.setColour (colour.withAlpha (on ? 0.85f : 0.18f));
                g.fillRoundedRectangle (r, 3.0f);
                if (! on)
                {
                    // Left out: hatched.
                    g.saveState();
                    g.reduceClipRegion (r.toNearestInt());
                    g.setColour (colour.withAlpha (0.35f));
                    for (auto x = r.getX() - r.getHeight(); x < r.getRight(); x += 7.0f)
                        g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
                    g.restoreState();
                }
                if (owner.hearing == std::make_pair (song, i))
                {
                    g.setColour (juce::Colour (0xfff4eeea));
                    g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 2.0f);
                }
                const auto dark = on && sec.kind != pr7s::DrumSection::ending;
                g.setColour (dark ? juce::Colour (0xff0c0a0d) : ParagonColours::text);
                const auto label = pr7s::drumSectionName (sec.kind);
                if (r.getWidth() > 40.0f)
                {
                    g.setFont (ParagonLookAndFeel::titleFont (r.getWidth() > 90.0f ? 15.0f : 12.0f));
                    g.drawText (r.getWidth() > 70.0f ? label : label.substring (0, 1), r.reduced (5.0f, 4.0f).withTrimmedBottom (r.getHeight() * 0.4f),
                                juce::Justification::topLeft, true);
                    g.setFont (interFont (10.5f));
                    g.drawText (juce::String (sec.bars), r.reduced (5.0f, 4.0f), juce::Justification::bottomLeft, true);
                }
                else if (r.getWidth() > 9.0f)
                {
                    g.setFont (ParagonLookAndFeel::titleFont (11.0f));
                    g.drawText (label.substring (0, 1), r, juce::Justification::centred, false);
                }
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto h = hit (e.getPosition());
        setMouseCursor (h.edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        if (h.song != hoverSong)
        {
            if (hoverSong >= 0)
                repaint (rowBounds (hoverSong));
            hoverSong = h.song;
            if (hoverSong >= 0)
                repaint (rowBounds (hoverSong));
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverSong >= 0)
            repaint (rowBounds (hoverSong));
        hoverSong = -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        down = hit (e.getPosition());
        dragged = false;
        if (down.use && ! e.mods.isPopupMenu())
        {
            owner.setSongUsed (down.song, ! owner.songs[(size_t) down.song].use);
            down = {};
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! down.edge || e.getDistanceFromDragStart() < 2)
            return;
        dragged = true;
        const auto bar = (int) std::round ((e.x - strip (down.song).getX()) / barWidth (down.song));
        owner.moveStart (down.song, down.section, bar);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragged || down.edge || down.section < 0 || e.getNumberOfClicks() > 1 || e.mouseWasDraggedSinceMouseDown())
            return;
        // The menu waits a double-click's time (a double-click hears the section instead); right-click: at once.
        pending = down;
        if (e.mods.isPopupMenu())
            showMenu (pending);
        else
            startTimer (juce::MouseEvent::getDoubleClickTimeout());
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        stopTimer();
        const auto h = hit (e.getPosition());
        if (h.section >= 0 && ! h.edge)
            owner.hear (h.song, h.section);
    }

private:
    void timerCallback() override
    {
        stopTimer();
        if (juce::isPositiveAndBelow (pending.song, (int) owner.songs.size())
            && juce::isPositiveAndBelow (pending.section, (int) owner.songs[(size_t) pending.song].sections.size()))
            showMenu (pending);
    }

    void showMenu (Hit h)
    {
        const auto& s = owner.songs[(size_t) h.song];
        const auto& sec = s.sections[(size_t) h.section];
        juce::PopupMenu menu;
        menu.addSectionHeader (s.name.toUpperCase() + ": BARS " + juce::String (sec.firstBar + 1) + " TO " + juce::String (sec.firstBar + sec.bars));
        for (const auto kind : pr7s::drumSections())
            menu.addItem (1 + (int) kind, pr7s::drumSectionName (kind).substring (0, 1) + pr7s::drumSectionName (kind).substring (1).toLowerCase(), true, sec.kind == kind);
        menu.addSeparator();
        menu.addItem (20, sec.used ? "Leave Out of Learning" : "Learn From It");
        menu.addItem (21, "Split Here (Bar " + juce::String (h.bar + 1) + ")", h.bar > sec.firstBar);
        menu.addItem (22, "Join With the Next", h.section + 1 < (int) s.sections.size());
        menu.addSeparator();
        menu.addItem (23, owner.hearing == std::make_pair (h.song, h.section) ? "Stop" : "Hear It");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [safe = juce::Component::SafePointer<Rows> (this), h] (int chosen)
                            {
                                if (safe == nullptr || chosen == 0)
                                    return;
                                auto& o = safe->owner;
                                if (chosen >= 1 && chosen <= 6)
                                    o.setKind (h.song, h.section, (pr7s::DrumSection) (chosen - 1));
                                if (chosen == 20)
                                    o.setUsed (h.song, h.section, ! o.getSongs()[(size_t) h.song].sections[(size_t) h.section].used);
                                if (chosen == 21)
                                    o.split (h.song, h.section, h.bar);
                                if (chosen == 22)
                                    o.join (h.song, h.section);
                                if (chosen == 23)
                                {
                                    if (o.getHearing() == std::make_pair (h.song, h.section))
                                        o.stop();
                                    else
                                        o.hear (h.song, h.section);
                                }
                            });
    }

    DrumLearnPage& owner;
    Hit down, pending;
    bool dragged = false;
    int hoverSong = -1;
};

//==============================================================================
DrumLearnPage::DrumLearnPage (pr7s::Studio& s) : studio (s)
{
    for (auto* k : { &chooseKey, &saveKey, &stopKey })
    {
        ChordUi::styleKey (*k);
        addAndMakeVisible (*k);
    }
    chooseKey.setTooltip ("A folder of drum MIDI: whole songs, one song a file (the folders under it too)");
    chooseKey.onClick = [this] { chooseFolder(); };
    saveKey.setTooltip ("Learn from the songs ticked and the sections not left out, and save it as a drummer profile for IDEAS' DRUMS tab");
    saveKey.onClick = [this]
    {
        juce::String error;
        if (! save (error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Learn a Drummer", error);
    };
    stopKey.onClick = [this] { stop(); };
    nameEditor.setFont (interFont (15.0f));
    nameEditor.setTextToShowWhenEmpty ("THE DRUMMER'S NAME", ParagonColours::textDim);
    nameEditor.setTooltip ("What the profile is called on the DRUMS tab");
    addAndMakeVisible (nameEditor);

    rows = std::make_unique<Rows> (*this);
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);
    setSize (1500, 860);
}

DrumLearnPage::~DrumLearnPage()
{
    stop();
    cancelReading();
}

struct DrumLearnPage::Reading
{
    std::thread thread;
    std::atomic<int> done { 0 };
    int total = 0;
    std::atomic<bool> cancel { false }, finished { false };
    std::vector<pr7s::DrumSong> songs;   // (the thread's own until finished)
    juce::StringArray unread;
};

void DrumLearnPage::readInto (std::vector<pr7s::DrumSong>& out, juce::StringArray& unread, const std::vector<ToRead>& files,
                              std::atomic<int>* done, std::atomic<bool>* cancel)
{
    for (const auto& item : files)
    {
        if (cancel != nullptr && cancel->load())
            return;
        juce::String error;
        if (auto song = pr7s::readDrumSong (item.file, error))
        {
            if (item.fromProfile)
            {
                if (const auto sections = pr7s::sectionsFromText (item.sections, (int) song->bars.size()))
                    song->sections = *sections;
                song->use = item.use;
            }
            out.push_back (std::move (*song));
        }
        else
            unread.add (item.file.getFileName());
        if (done != nullptr)
            ++*done;
    }
}

namespace
{
    std::vector<juce::File> songsIn (const juce::File& folder) { return pr7s::findDrumSongs (folder); }
}

int DrumLearnPage::loadFolder (const juce::File& f)
{
    stop();
    cancelReading();
    folder = f;
    songs.clear();
    unread.clear();
    std::vector<ToRead> files;
    for (const auto& file : songsIn (f))
        files.push_back ({ file });
    readInto (songs, unread, files);
    if (nameEditor.isEmpty())
        nameEditor.setText (f.getFileName(), false);
    pr7s::SessionLog::note ("Learn a Drummer: " + juce::String ((int) songs.size()) + " songs read from " + f.getFullPathName()
                            + (unread.isEmpty() ? juce::String() : ", " + juce::String (unread.size()) + " not"));
    changed();
    return (int) songs.size();
}

void DrumLearnPage::openProfile (const pr7s::DrumProfile& profile)
{
    stop();
    cancelReading();
    songs.clear();
    unread.clear();
    folder = juce::File();
    std::vector<ToRead> files;
    for (const auto& saved : profile.songs)
        files.push_back ({ juce::File (saved.path), saved.sections, saved.use, true });
    readInto (songs, unread, files);
    nameEditor.setText (profile.name, false);
    changed();
}

void DrumLearnPage::loadFolderInBackground (const juce::File& f)
{
    stop();
    folder = f;
    if (nameEditor.isEmpty())
        nameEditor.setText (f.getFileName(), false);
    std::vector<ToRead> files;
    for (const auto& file : songsIn (f))
        files.push_back ({ file });
    startReading (std::move (files));
}

void DrumLearnPage::openProfileInBackground (const pr7s::DrumProfile& profile)
{
    stop();
    folder = juce::File();
    nameEditor.setText (profile.name, false);
    std::vector<ToRead> files;
    for (const auto& saved : profile.songs)
        files.push_back ({ juce::File (saved.path), saved.sections, saved.use, true });
    startReading (std::move (files));
}

void DrumLearnPage::cancelReading()
{
    if (reading == nullptr)
        return;
    reading->cancel = true;
    if (reading->thread.joinable())
        reading->thread.join();
    reading.reset();
}

void DrumLearnPage::startReading (std::vector<ToRead> files)
{
    cancelReading();
    songs.clear();
    unread.clear();
    reading = std::make_unique<Reading>();
    reading->total = (int) files.size();
    auto* r = reading.get();
    r->thread = std::thread ([r, list = std::move (files)]
    {
        readInto (r->songs, r->unread, list, &r->done, &r->cancel);
        r->finished = true;
    });
    startTimerHz (10);
    changed();
}

void DrumLearnPage::finishReading()
{
    if (reading == nullptr)
        return;
    if (reading->thread.joinable())
        reading->thread.join();
    songs = std::move (reading->songs);
    unread = reading->unread;
    reading.reset();
    pr7s::SessionLog::note ("Learn a Drummer: " + juce::String ((int) songs.size()) + " songs read"
                            + (folder.isDirectory() ? " from " + folder.getFullPathName() : juce::String())
                            + (unread.isEmpty() ? juce::String() : ", " + juce::String (unread.size()) + " not"));
    changed();
}

void DrumLearnPage::waitForReading()
{
    if (reading != nullptr)
    {
        if (reading->thread.joinable())
            reading->thread.join();
        finishReading();
    }
}

void DrumLearnPage::setName (const juce::String& name) { nameEditor.setText (name, false); }
juce::String DrumLearnPage::getName() const { return nameEditor.getText().trim(); }

bool DrumLearnPage::save (juce::String& error)
{
    const auto name = getName();
    if (name.isEmpty())
    {
        error = "Give the drummer a name first.";
        return false;
    }
    const auto profile = pr7s::learnProfile (name, songs);
    if (profile.barsLearned() == 0)
    {
        error = songs.empty() ? "No songs yet: CHOOSE FOLDER... first." : "Nothing to learn: every song or section is left out.";
        return false;
    }
    if (! studio.saveDrummer (profile, error))
        return false;
    if (onSaved)
        onSaved (name);
    return true;
}

void DrumLearnPage::setKind (int song, int section, pr7s::DrumSection kind)
{
    if (! juce::isPositiveAndBelow (song, (int) songs.size()))
        return;
    pr7s::setSectionKind (songs[(size_t) song], section, kind);
    changed();
}

void DrumLearnPage::setUsed (int song, int section, bool used)
{
    if (! juce::isPositiveAndBelow (song, (int) songs.size()))
        return;
    pr7s::setSectionUsed (songs[(size_t) song], section, used);
    changed();
}

void DrumLearnPage::setSongUsed (int song, bool used)
{
    if (! juce::isPositiveAndBelow (song, (int) songs.size()))
        return;
    songs[(size_t) song].use = used;
    changed();
}

bool DrumLearnPage::split (int song, int section, int atBar)
{
    if (! juce::isPositiveAndBelow (song, (int) songs.size()) || ! pr7s::splitSection (songs[(size_t) song], section, atBar))
        return false;
    stop();
    changed();
    return true;
}

bool DrumLearnPage::join (int song, int section)
{
    if (! juce::isPositiveAndBelow (song, (int) songs.size()) || ! pr7s::joinSections (songs[(size_t) song], section))
        return false;
    stop();
    changed();
    return true;
}

bool DrumLearnPage::moveStart (int song, int section, int firstBar)
{
    if (! juce::isPositiveAndBelow (song, (int) songs.size()) || ! pr7s::moveSectionStart (songs[(size_t) song], section, firstBar))
        return false;
    changed();
    return true;
}

void DrumLearnPage::hear (int song, int section)
{
    stop();
    if (! juce::isPositiveAndBelow (song, (int) songs.size()) || ! juce::isPositiveAndBelow (section, (int) songs[(size_t) song].sections.size()))
        return;
    const auto& s = songs[(size_t) song];
    const auto idea = pr7s::sectionIdea (s, section);
    const auto& sec = s.sections[(size_t) section];
    const auto last = juce::jmin ((int) s.bars.size(), sec.firstBar + sec.bars) - 1;
    const auto length = s.bars[(size_t) last].start + s.bars[(size_t) last].length - s.bars[(size_t) sec.firstBar].start;
    const std::vector<pr7s::ChordSpan> spans { { 0.0, length, pr7s::Chord { 0, pr7s::Chord::Type::major, pr7s::Chord::Tension::none, -1 }, pr7s::Key { 0, 0, false } } };
    studio.previewLine (&idea, spans, false);
    hearing = { song, section };
    startTimerHz (10);
    repaint();
}

void DrumLearnPage::stop()
{
    if (hearing.first >= 0)
        studio.stopPreview();
    hearing = { -1, -1 };
    if (reading == nullptr)
        stopTimer();
    if (rows != nullptr)
        rows->repaint();
}

void DrumLearnPage::timerCallback()
{
    if (reading != nullptr)
    {
        if (reading->finished)
            finishReading();
        else
            repaint (0, 0, getWidth(), headerHeight);
    }
    if (hearing.first >= 0 && ! studio.isPreviewing())
        stop();
    if (reading == nullptr && hearing.first < 0)
        stopTimer();
}

juce::String DrumLearnPage::getStatusText() const
{
    if (reading != nullptr)
        return "Reading the songs: " + juce::String (reading->done.load()) + " of " + juce::String (reading->total) + "...";
    if (songs.empty())
        return unread.isEmpty() ? juce::String ("No songs yet.") : "None of the " + plural (unread.size(), "file") + " held drums.";
    std::array<int, 6> sections {};
    std::array<int, 6> bars {};
    auto used = 0, total = 0;
    for (const auto& s : songs)
    {
        total += (int) s.bars.size();
        if (! s.use)
            continue;
        ++used;
        for (const auto& sec : s.sections)
            if (sec.used)
            {
                ++sections[(size_t) sec.kind];
                bars[(size_t) sec.kind] += sec.bars;
            }
    }
    juce::StringArray kinds;
    for (const auto kind : pr7s::drumSections())
        if (sections[(size_t) kind] > 0)
            kinds.add (juce::String (sections[(size_t) kind]) + " " + pr7s::drumSectionName (kind).toLowerCase()
                       + (sections[(size_t) kind] == 1 ? "" : kind == pr7s::DrumSection::chorus ? "es" : "s")
                       + " (" + juce::String (bars[(size_t) kind]) + " bars)");
    auto text = plural ((int) songs.size(), "song") + ", " + plural (total, "bar") + "; learning from " + juce::String (used) + ": "
              + (kinds.isEmpty() ? juce::String ("nothing") : kinds.joinIntoString (", ")) + ".";
    if (! unread.isEmpty())
        text << "  " << plural (unread.size(), "file") << " couldn't be read (" << unread.joinIntoString (", ").substring (0, 120) << ").";
    return text;
}

juce::Rectangle<int> DrumLearnPage::getSectionBounds (int song, int section) const
{
    return rows->sectionBounds (song, section) + rowsView.getBounds().getPosition() - rowsView.getViewPosition();
}

juce::Rectangle<int> DrumLearnPage::getUseBox (int song) const
{
    return rows->useBox (song) + rowsView.getBounds().getPosition() - rowsView.getViewPosition();
}

void DrumLearnPage::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("A folder of drum MIDI songs", folder.isDirectory() ? folder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<DrumLearnPage> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe != nullptr && fc.getResult().isDirectory())
                                  safe->loadFolderInBackground (fc.getResult());
                          });
}

void DrumLearnPage::changed()
{
    rows->setSize (juce::jmax (100, rowsView.getMaximumVisibleWidth()), juce::jmax (rowsView.getHeight(), (int) songs.size() * rowHeight));
    rows->repaint();
    repaint (0, 0, getWidth(), headerHeight);
}

void DrumLearnPage::paint (juce::Graphics& g)
{
    g.fillAll (page);
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    auto title = head.removeFromTop (34);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("LEARN A DRUMMER", title.removeFromLeft (230), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (12.5f));
    g.drawText (folder.isDirectory() ? folder.getFullPathName() : juce::String ("Sections are found from the drums alone: a guess. Put right what's wrong before saving."),
                title.withTrimmedRight (560), juce::Justification::centredLeft, true);
    head.removeFromTop (44);
    g.setColour (ParagonColours::text);
    g.setFont (interFont (13.0f));
    g.drawText (getStatusText(), head.removeFromTop (20), juce::Justification::centredLeft, true);
    // The key to the colours.
    auto legend = head.removeFromTop (20).withTrimmedTop (2);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    for (const auto kind : pr7s::drumSections())
    {
        auto r = legend.removeFromLeft (120);
        g.setColour (colourOf (kind));
        g.fillRoundedRectangle (r.removeFromLeft (14).withSizeKeepingCentre (14, 12).toFloat(), 2.0f);
        g.setColour (ParagonColours::textDim);
        g.drawText (pr7s::drumSectionName (kind), r.withTrimmedLeft (6), juce::Justification::centredLeft, false);
    }
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (11.5f));
    g.drawText ("Click a section to change it; drag its left edge; double-click to hear it. The tick: learn from that song.", legend, juce::Justification::centredRight, true);
}

void DrumLearnPage::resized()
{
    auto area = getLocalBounds().reduced (gap, 0);
    auto head = area.removeFromTop (headerHeight).reduced (0, 8);
    auto top = head.removeFromTop (34);
    stopKey.setBounds (top.removeFromRight (80).reduced (0, 3));
    top.removeFromRight (8);
    saveKey.setBounds (top.removeFromRight (140).reduced (0, 3));
    top.removeFromRight (8);
    nameEditor.setBounds (top.removeFromRight (260).reduced (0, 3));
    head.removeFromTop (8);
    chooseKey.setBounds (head.removeFromTop (32).removeFromLeft (180));
    area.removeFromBottom (gap);
    rowsView.setBounds (area);
    changed();
}

//==============================================================================
DrumLearnWindow::DrumLearnWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Learn a Drummer", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new DrumLearnPage (studio);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (1000, 520, 3840, 2160);
}

void DrumLearnWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
