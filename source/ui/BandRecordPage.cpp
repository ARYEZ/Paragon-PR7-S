#include "BandRecordPage.h"

#include "ui/ParagonLookAndFeel.h"

using namespace pr7s;

namespace
{
    const juce::Colour pageColour (0xff0c0a0d), rowColour (0xff151217);
    juce::Font textFont (float h) { return ParagonLookAndFeel::textFont (h); }

    void styleKey (juce::TextButton& b)
    {
        b.setWantsKeyboardFocus (false);
    }
}

//==============================================================================
// A song: its tick, name, tempo (typed), signature; a row per DI with its player's menu; when read, OPEN and
// what the reader said.
class BandRecordPage::Rows final : public juce::Component
{
public:
    explicit Rows (BandRecordPage& p) : page (p) {}

    struct SongRow
    {
        juce::ToggleButton use;
        juce::Label name, tempo;
        juce::TextButton open { "OPEN" };
        std::vector<std::unique_ptr<juce::ComboBox>> roles;
        std::vector<std::unique_ptr<juce::Label>> files;
        int top = 0, height = 0;
    };

    void rebuild()
    {
        removeAllChildren();
        rows.clear();
        auto y = 0;
        for (int s = 0; s < (int) page.songs.size(); ++s)
        {
            const auto& song = page.songs[(size_t) s];
            auto row = std::make_unique<SongRow>();
            auto* r = row.get();
            r->top = y;
            r->use.setToggleState (page.isIncluded (s), juce::dontSendNotification);
            r->use.setWantsKeyboardFocus (false);
            r->use.setTooltip ("Read this song or leave it out");
            r->use.onClick = [this, s, r] { page.setIncluded (s, r->use.getToggleState()); };
            addAndMakeVisible (r->use);
            r->name.setText (song.name.toUpperCase(), juce::dontSendNotification);
            r->name.setFont (ParagonLookAndFeel::titleFont (20.0f));
            r->name.setColour (juce::Label::textColourId, ParagonColours::text);
            addAndMakeVisible (r->name);
            r->tempo.setText (juce::String (song.bpm, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."), juce::dontSendNotification);
            r->tempo.setEditable (true);
            r->tempo.setFont (textFont (16.0f));
            r->tempo.setColour (juce::Label::textColourId, song.tempoFromFile ? ParagonColours::text : ParagonColours::accent);
            r->tempo.setColour (juce::Label::outlineColourId, ParagonColours::panelEdge);
            r->tempo.setJustificationType (juce::Justification::centred);
            r->tempo.setTooltip (song.tempoFromFile ? "The song's tempo, from the drum MIDI (double-click to type another)"
                                                    : "No tempo in the drum MIDI: 120 assumed. Double-click to type the song's tempo");
            r->tempo.onTextChange = [this, s, r] { page.setTempo (s, r->tempo.getText().getDoubleValue()); };
            addAndMakeVisible (r->tempo);
            r->open.setWantsKeyboardFocus (false);
            r->open.setTooltip ("Open the review song: the DIs with what was heard under them, to put right in the MIDI editor");
            r->open.onClick = [this, s]
            {
                if (const auto f = page.getReviewSong (s); f.existsAsFile() && page.onOpenSong)
                    page.onOpenSong (f);
            };
            addChildComponent (r->open);
            auto line = 0;
            for (int d = 0; d < (int) song.dis.size(); ++d, ++line)
            {
                auto file = std::make_unique<juce::Label>();
                file->setText (song.dis[(size_t) d].first.getFileName(), juce::dontSendNotification);
                file->setFont (textFont (14.0f));
                file->setColour (juce::Label::textColourId, ParagonColours::textDim);
                addAndMakeVisible (*file);
                auto box = std::make_unique<juce::ComboBox>();
                // (0.82.4) A MIDI file is the keyboards or not used; a DI a guitar, the bass, or not used.
                const auto midi = song.dis[(size_t) d].first.hasFileExtension ("mid;midi");
                for (const auto role : midi ? std::vector<BandRole> { BandRole::keys, BandRole::unknown }
                                            : std::vector<BandRole> { BandRole::rhythm, BandRole::lead, BandRole::bass, BandRole::unknown })
                    box->addItem (bandRoleName (role), (int) role + 1);
                box->setSelectedId ((int) song.dis[(size_t) d].second + 1, juce::dontSendNotification);
                box->setWantsKeyboardFocus (false);
                box->onChange = [this, s, d, b = box.get()] { page.setRole (s, d, (BandRole) (b->getSelectedId() - 1)); };
                addAndMakeVisible (*box);
                r->files.push_back (std::move (file));
                r->roles.push_back (std::move (box));
            }
            r->height = 40 + line * 28 + 30;
            y += r->height + gap;
            rows.push_back (std::move (row));
        }
        setSize (juce::jmax (600, getWidth()), juce::jmax (10, y));
        refresh();
        resized();
    }

    void refresh()
    {
        for (int s = 0; s < (int) rows.size(); ++s)
            rows[(size_t) s]->open.setVisible (page.getReviewSong (s).existsAsFile());
        repaint();
    }

    void resized() override
    {
        for (int s = 0; s < (int) rows.size(); ++s)
        {
            auto& r = *rows[(size_t) s];
            auto area = juce::Rectangle<int> (0, r.top, getWidth(), r.height).reduced (10, 6);
            auto head = area.removeFromTop (30);
            r.use.setBounds (head.removeFromLeft (28));
            r.open.setBounds (head.removeFromRight (90).reduced (0, 2));
            head.removeFromRight (14);
            r.tempo.setBounds (head.removeFromRight (90).reduced (0, 2));
            head.removeFromRight (150);   // (TEMPO / the signature, painted)
            r.name.setBounds (head);
            area.removeFromTop (4);
            for (size_t d = 0; d < r.files.size(); ++d)
            {
                auto line = area.removeFromTop (28).withTrimmedLeft (28);
                r.roles[d]->setBounds (line.removeFromLeft (190).reduced (0, 3));
                line.removeFromLeft (12);
                r.files[d]->setBounds (line);
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        for (int s = 0; s < (int) rows.size(); ++s)
        {
            const auto& r = *rows[(size_t) s];
            const auto& song = page.songs[(size_t) s];
            auto box = juce::Rectangle<int> (0, r.top, getWidth(), r.height).toFloat();
            g.setColour (rowColour);
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (page.readingSong == s ? ParagonColours::accent : ParagonColours::panelEdge);
            g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, page.readingSong == s ? 2.0f : 1.0f);
            // TEMPO and the signature, left of the tempo box.
            auto head = juce::Rectangle<int> (0, r.top, getWidth(), 42).reduced (10, 6);
            head.removeFromRight (90 + 14 + 90);
            auto label = head.removeFromRight (150);
            g.setColour (ParagonColours::textDim);
            g.setFont (textFont (14.0f));
            g.drawText (juce::String (song.numerator) + "/" + juce::String (song.denominator) + "   TEMPO", label.withTrimmedRight (8),
                        juce::Justification::centredRight, false);
            // What happened (or what's missing), along the foot.
            auto foot = juce::Rectangle<int> (0, r.top + r.height - 30, getWidth(), 26).reduced (38, 2);
            juce::String text;
            if (song.drums == juce::File())
                text = "NO DRUM MIDI FOUND: the DIs will be read on their own. ";
            else
                text = "DRUMS: " + song.drums.getFileName() + ". ";
            if (s < (int) page.results.size())
            {
                const auto& result = page.results[(size_t) s];
                if (result.error.isNotEmpty())
                    text << result.error.replace ("\n", " ").toUpperCase();
                if (result.report.isNotEmpty())
                    text << result.report.replace ("\n", " ");
            }
            g.setColour (ParagonColours::textDim);
            g.setFont (textFont (13.0f));
            g.drawText (text, foot, juce::Justification::centredLeft, true);
        }
    }

private:
    BandRecordPage& page;
    std::vector<std::unique_ptr<SongRow>> rows;
};

//==============================================================================
BandRecordPage::BandRecordPage (Studio& s) : studio (s)
{
    rows = std::make_unique<Rows> (*this);
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);
    chooseKey.setTooltip ("The record's folder: a folder per song, or every song's files together (their names tell them apart)");
    chooseKey.onClick = [this] { chooseFolder(); };
    readKey.setTooltip ("Hear every DI of the ticked songs and write a review song for each (it takes a minute or two a song)");
    readKey.onClick = [this] { readSongs(); };
    stopKey.setTooltip ("Stop reading (the songs already done stay)");
    stopKey.onClick = [this] { stop(); };
    for (auto* b : { &chooseKey, &readKey, &stopKey })
    {
        styleKey (*b);
        addAndMakeVisible (*b);
    }
    changed();
    startTimerHz (5);
}

BandRecordPage::~BandRecordPage()
{
    cancel = true;
    if (reader.joinable())
        reader.join();
}

juce::File BandRecordPage::getReviewFolder() const
{
    return defaultBandReviewFolder().getChildFile (juce::File::createLegalFileName (folder.isDirectory() ? folder.getFileName() : juce::String ("Record")));
}

int BandRecordPage::loadFolder (const juce::File& f)
{
    stop();
    folder = f;
    songs = scanBandFolder (f);
    included.assign (songs.size(), true);
    results.assign (songs.size(), {});
    // Review songs read before (in this record's folder) are offered again.
    for (size_t s = 0; s < songs.size(); ++s)
    {
        const auto name = juce::File::createLegalFileName (songs[s].name);
        const auto made = getReviewFolder().getChildFile (name).getChildFile (name + Project::fileExtension);
        if (made.existsAsFile())
            results[s].made = made;
    }
    rows->rebuild();
    changed();
    return (int) songs.size();
}

void BandRecordPage::setRole (int song, int di, BandRole role)
{
    if (juce::isPositiveAndBelow (song, (int) songs.size()) && juce::isPositiveAndBelow (di, (int) songs[(size_t) song].dis.size()))
        songs[(size_t) song].dis[(size_t) di].second = role;
}

void BandRecordPage::setTempo (int song, double bpm)
{
    if (juce::isPositiveAndBelow (song, (int) songs.size()) && bpm >= 20.0 && bpm <= 400.0)
    {
        songs[(size_t) song].bpm = bpm;
        songs[(size_t) song].tempoFromFile = true;   // (yours now)
    }
}

void BandRecordPage::setIncluded (int song, bool on)
{
    if (juce::isPositiveAndBelow (song, (int) included.size()))
        included[(size_t) song] = on;
    changed();
}

bool BandRecordPage::isIncluded (int song) const
{
    return juce::isPositiveAndBelow (song, (int) included.size()) && included[(size_t) song];
}

juce::File BandRecordPage::getReviewSong (int song) const
{
    return juce::isPositiveAndBelow (song, (int) results.size()) ? results[(size_t) song].made : juce::File();
}

juce::String BandRecordPage::getReport (int song) const
{
    return juce::isPositiveAndBelow (song, (int) results.size()) ? results[(size_t) song].report : juce::String();
}

void BandRecordPage::readInto (std::vector<Result>& into, const std::vector<int>& which)
{
    BandReviewOptions options;
    options.velocities = studio.getGuitarVelocities();
    if (const auto p = studio.profileFor (Studio::ProfileUse::guitar)) options.guitarBendRange = p->bendRange;
    if (const auto p = studio.profileFor (Studio::ProfileUse::lead)) options.leadBendRange = p->bendRange;
    else options.leadBendRange = options.guitarBendRange;
    if (const auto p = studio.profileFor (Studio::ProfileUse::bass)) options.bassBendRange = p->bendRange;
    const auto into_ = getReviewFolder();
    const auto copies = songs;
    into.assign (songs.size(), {});
    songsToRead = (int) which.size();
    songsRead = 0;
    readerDone = false;
    cancel = false;
    reader = std::thread ([this, &into, which, copies, options, into_]
    {
        for (const auto s : which)
        {
            if (cancel)
                break;
            readingSong = s;
            Result result;
            if (makeBandReviewSong (copies[(size_t) s], into_, options, result.made, result.error, [this] (double) { return ! cancel.load(); }, &result.report))
                result.error = result.error.trim();
            else
                result.made = juce::File();
            into[(size_t) s] = result;
            ++songsRead;
        }
        readingSong = -1;
        readerDone = true;
    });
}

void BandRecordPage::readSongs()
{
    if (isReading() || songs.empty())
        return;
    if (reader.joinable())
        reader.join();
    std::vector<int> which;
    for (int s = 0; s < (int) songs.size(); ++s)
        if (isIncluded (s))
            which.push_back (s);
    if (which.empty())
        return;
    readInto (pending, which);
    changed();
}

void BandRecordPage::readSongsNow()
{
    readSongs();
    waitForReading();
}

void BandRecordPage::waitForReading()
{
    if (reader.joinable())
        reader.join();
    finishReading();
}

void BandRecordPage::finishReading()
{
    if (reader.joinable() && readerDone)
        reader.join();
    for (size_t s = 0; s < pending.size() && s < results.size(); ++s)
        if (pending[s].made != juce::File() || pending[s].error.isNotEmpty())
            results[s] = pending[s];
    pending.clear();
    rows->refresh();
    changed();
}

void BandRecordPage::stop()
{
    cancel = true;
    if (reader.joinable())
        reader.join();
    if (! pending.empty())
        finishReading();
}

void BandRecordPage::timerCallback()
{
    if (! pending.empty() && readerDone)
        finishReading();
    else if (isReading())
    {
        rows->repaint();
        repaint (0, 0, getWidth(), headerHeight);
    }
}

juce::String BandRecordPage::getStatusText() const
{
    if (isReading())
    {
        const auto s = readingSong.load();
        return "READING " + juce::String (songsRead.load() + 1) + " OF " + juce::String (songsToRead.load())
               + (juce::isPositiveAndBelow (s, (int) songs.size()) ? ": " + songs[(size_t) s].name.toUpperCase() : juce::String())
               + " (HEARING EVERY NOTE: A MINUTE OR TWO A SONG)";
    }
    if (songs.empty())
        return folder.isDirectory() ? "NO SONGS FOUND IN THIS FOLDER (MIDI AND WAV FILES, A FOLDER A SONG OR NAMED BY SONG)"
                                    : "CHOOSE THE RECORD'S FOLDER: EACH SONG'S DRUM MIDI AND ONE DI PER PLAYER, ALL STARTING ON THE SAME BEAT";
    auto made = 0;
    for (const auto& r : results)
        if (r.made.existsAsFile())
            ++made;
    return juce::String ((int) songs.size()) + (songs.size() == 1 ? " SONG" : " SONGS") + ", " + juce::String (made) + " READ. CHECK EACH DI'S PLAYER, THEN READ THE SONGS.";
}

void BandRecordPage::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("The record's folder", folder.isDirectory() ? folder : juce::File::getSpecialLocation (juce::File::userHomeDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<BandRecordPage> (this)] (const juce::FileChooser& c)
                          {
                              if (safe != nullptr && c.getResult().isDirectory())
                                  safe->loadFolder (c.getResult());
                          });
}

void BandRecordPage::changed()
{
    readKey.setEnabled (! songs.empty() && ! isReading());
    stopKey.setEnabled (isReading());
    chooseKey.setEnabled (! isReading());
    repaint();
}

void BandRecordPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    auto title = head.removeFromTop (34);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("LEARN MY BAND", title.removeFromLeft (220), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (textFont (13.0f));
    g.drawText (folder.isDirectory() ? folder.getFullPathName() : juce::String ("READ YOUR RECORD: WHAT EACH PLAYER PLAYED, FOR THE IDEAS TO LEARN FROM"),
                title.withTrimmedRight (440), juce::Justification::centredLeft, true);
    head.removeFromTop (44);
    g.setColour (ParagonColours::text);
    g.setFont (textFont (15.0f));
    g.drawText (getStatusText(), head.removeFromTop (22), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (textFont (12.5f));
    g.drawText ("REVIEW SONGS GO TO " + getReviewFolder().getFullPathName().toUpperCase() + ". OPEN ONE, PUT THE NOTES RIGHT IN THE MIDI EDITOR, SAVE.",
                head.removeFromTop (20), juce::Justification::centredLeft, true);
}

void BandRecordPage::resized()
{
    auto area = getLocalBounds().reduced (gap, 0);
    auto head = area.removeFromTop (headerHeight).reduced (0, 8);
    auto top = head.removeFromTop (34);
    stopKey.setBounds (top.removeFromRight (90).reduced (0, 3));
    top.removeFromRight (8);
    readKey.setBounds (top.removeFromRight (170).reduced (0, 3));
    head.removeFromTop (8);
    chooseKey.setBounds (head.removeFromTop (32).removeFromLeft (190));
    area.removeFromBottom (gap);
    rowsView.setBounds (area);
    rows->setSize (area.getWidth() - rowsView.getScrollBarThickness(), rows->getHeight());
    rows->resized();
}

//==============================================================================
BandRecordWindow::BandRecordWindow (Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Learn My Band", pageColour, juce::DocumentWindow::closeButton)
{
    page = new BandRecordPage (studio);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (900, 480, 3840, 2160);
}

void BandRecordWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
