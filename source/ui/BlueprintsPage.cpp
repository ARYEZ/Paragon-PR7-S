#include "ui/BlueprintsPage.h"
#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"
#include "engine/SongSections.h"

namespace
{
    const juce::Colour pageColour { 0xff0c0a0d };
    juce::Font font (float h) { return ParagonLookAndFeel::textFont (h); }
    juce::String keyName (const pr7s::Key& key) { return ChordUi::spokenKey (key).toUpperCase(); }

    // As the SECTIONS row colours its blocks (ArrangeSections.cpp).
    juce::Colour sectionColour (const juce::String& name)
    {
        switch (pr7s::composerSectionFor (name))
        {
            case 0: return pr7s::FamilyColours::paletteColour (4, 6);
            case 1: return pr7s::FamilyColours::orange;
            case 2: return pr7s::FamilyColours::pink;
            case 3: return pr7s::FamilyColours::red;
            case 4: return pr7s::FamilyColours::paletteColour (4, 1);
            case 5: return pr7s::FamilyColours::paletteColour (6, 6);
            case 6: return pr7s::FamilyColours::paletteColour (1, 3);
            default: return pr7s::FamilyColours::orange;
        }
    }

    // "Chorus 2" where a name comes more than once.
    juce::String blockLabel (const pr7s::Blueprint& bp, size_t index)
    {
        const auto& name = bp.blocks[index].name;
        int count = 0, place = 0;
        for (size_t i = 0; i < bp.blocks.size(); ++i)
            if (bp.blocks[i].name == name)
            {
                ++count;
                if (i <= index)
                    ++place;
            }
        return count > 1 ? name + " " + juce::String (place) : name;
    }

    juce::String barsText (double bars)
    {
        return std::abs (bars - std::round (bars)) < 1.0e-6 ? juce::String ((int) std::round (bars)) : juce::String (bars, 2);
    }
}

BlueprintsPage::BlueprintsPage (pr7s::Studio& s) : studio (s)
{
    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, pageColour);
    addAndMakeVisible (list);
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        keyBox.addItem ("KEY: " + keyName (keys[(size_t) i]), i + 1);
    keyBox.onChange = [this] { repaint(); };
    addAndMakeVisible (keyBox);
    chordsKey.setToggleState (true, juce::dontSendNotification);
    chordsKey.setTooltip ("On: the blueprint's chords go onto the chord track inside the row. Off: the chord track is left as it is.");
    chordsKey.onClick = [this] { repaint(); };
    addAndMakeVisible (chordsKey);
    tempoKey.setToggleState (true, juce::dontSendNotification);
    tempoKey.setTooltip ("On: the song takes the blueprint's tempo.");
    addAndMakeVisible (tempoKey);
    musicKey.setToggleState (true, juce::dontSendNotification);
    musicKey.setTooltip ("On: every section is built straight away (riffs, beats, bass, keys), each with the types and options that play "
                         "most like the song it's from, the drums a groove from your library nearest its beat. ROLL a section "
                         "(its editor, or ARCHITECT) for another take. Off: only the layout.");
    addAndMakeVisible (musicKey);
    searchBox.setTextToShowWhenEmpty ("SEARCH", ParagonColours::textDim);
    searchBox.onTextChange = [this] { refreshRows(); };
    addAndMakeVisible (searchBox);
    lengthBox.addItem ("ANY LENGTH", 1);
    lengthBox.addItem ("SHORT (UNDER 64 BARS)", 2);
    lengthBox.addItem ("MEDIUM (64-100 BARS)", 3);
    lengthBox.addItem ("LONG (OVER 100 BARS)", 4);
    tempoBox.addItem ("ANY TEMPO", 1);
    tempoBox.addItem ("SLOW (UNDER 95)", 2);
    tempoBox.addItem ("MID (95-140)", 3);
    tempoBox.addItem ("FAST (OVER 140)", 4);
    modeBox.addItem ("MAJOR AND MINOR", 1);
    modeBox.addItem ("MINOR", 2);
    modeBox.addItem ("MAJOR", 3);
    for (auto* b : { &groupBox, &lengthBox, &tempoBox, &modeBox })
    {
        b->setSelectedId (1, juce::dontSendNotification);
        b->onChange = [this] { refreshRows(); };
        addAndMakeVisible (*b);
    }
    useKey.onClick = [this] { use(); };
    addAndMakeVisible (useKey);
    saveKey.onClick = [this] { askName(); };
    saveKey.setTooltip ("This song's SECTIONS row (names, lengths, linked copies, types, transitions, chords, tempo) kept as a blueprint of your own");
    addAndMakeVisible (saveKey);
    deleteKey.onClick = [this] { deleteChosen(); };
    addAndMakeVisible (deleteKey);
    refreshList();
    choose (0);
}

BlueprintsPage::~BlueprintsPage()
{
    list.setModel (nullptr);
}

void BlueprintsPage::refreshList()
{
    const auto keepName = juce::isPositiveAndBelow (chosen, (int) blueprints.size()) ? blueprints[(size_t) chosen].name : juce::String();
    blueprints = studio.getBlueprints();
    const auto group = groupBox.getText();
    groupBox.clear (juce::dontSendNotification);
    groupBox.addItem ("ALL GROUPS", 1);
    groupBox.addItem (favouritesGroup, 2);
    auto id = 3;
    for (const auto& g : getGroups())
        groupBox.addItem (g, id++);
    groupBox.setText (group.isNotEmpty() ? group : juce::String ("ALL GROUPS"), juce::dontSendNotification);
    if (groupBox.getSelectedId() == 0)
        groupBox.setSelectedId (1, juce::dontSendNotification);
    refreshRows();
    auto row = 0;
    for (int i = 0; i < (int) blueprints.size(); ++i)
        if (blueprints[(size_t) i].name == keepName)
            row = i;
    chosen = -1;
    choose (blueprints.empty() ? -1 : row);
}

void BlueprintsPage::choose (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) blueprints.size()))
    {
        chosen = -1;
        refreshKeys();
        repaint();
        return;
    }
    const auto changedRow = row != chosen;
    chosen = row;
    for (int r = 0; r < (int) rows.size(); ++r)
        if (rows[(size_t) r].blueprint == row)
            list.selectRow (r, true, true);
    if (changedRow)
    {
        const auto& bp = blueprints[(size_t) row];
        // Its own key to start (or the song's when it has none).
        auto key = studio.getSongKey();
        if (juce::isPositiveAndBelow (bp.keyIndex, (int) pr7s::Key::all().size()))
            key = pr7s::Key::all()[(size_t) bp.keyIndex];
        setKey (key);
    }
    refreshKeys();
    repaint();
}

void BlueprintsPage::refreshKeys()
{
    const auto has = juce::isPositiveAndBelow (chosen, (int) blueprints.size());
    useKey.setEnabled (has);
    deleteKey.setEnabled (has && ! blueprints[(size_t) chosen].factory);
    chordsKey.setEnabled (has && blueprints[(size_t) chosen].hasChords());
    tempoKey.setEnabled (has && blueprints[(size_t) chosen].bpm > 0.0);
}

void BlueprintsPage::setKey (const pr7s::Key& key)
{
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        if (keys[(size_t) i] == key)
            keyBox.setSelectedId (i + 1, juce::dontSendNotification);
    repaint();
}

pr7s::Key BlueprintsPage::getKey() const
{
    const auto id = keyBox.getSelectedId();
    return id >= 1 ? pr7s::Key::all()[(size_t) id - 1] : studio.getSongKey();
}

void BlueprintsPage::setWithChords (bool on) { chordsKey.setToggleState (on, juce::dontSendNotification); repaint(); }
void BlueprintsPage::setWithTempo (bool on)  { tempoKey.setToggleState (on, juce::dontSendNotification); }
void BlueprintsPage::setWithMusic (bool on)  { musicKey.setToggleState (on, juce::dontSendNotification); }

juce::String BlueprintsPage::groupOf (const pr7s::Blueprint& bp) const
{
    return bp.factory ? (bp.group.isNotEmpty() ? bp.group.toUpperCase() : juce::String ("OUR FIRST TEN")) : juce::String (myGroup);
}

juce::StringArray BlueprintsPage::getGroups() const
{
    juce::StringArray groups;
    for (const auto& bp : blueprints)
        groups.addIfNotAlreadyThere (groupOf (bp));
    groups.sortNatural();
    if (groups.contains (myGroup))   // (yours last)
    {
        groups.removeString (myGroup);
        groups.add (myGroup);
    }
    return groups;
}

bool BlueprintsPage::isFavourite (int blueprint) const
{
    return juce::isPositiveAndBelow (blueprint, (int) blueprints.size())
        && studio.getOptionMemory().isFavourite ("blueprint", blueprints[(size_t) blueprint].name);
}

void BlueprintsPage::setFavourite (int blueprint, bool on)
{
    if (! juce::isPositiveAndBelow (blueprint, (int) blueprints.size()))
        return;
    studio.setOptionFavourite ("blueprint", blueprints[(size_t) blueprint].name, on);
    refreshRows();
}

bool BlueprintsPage::passes (const pr7s::Blueprint& bp) const
{
    const auto group = groupBox.getSelectedId() > 1 ? groupBox.getText() : juce::String();
    if (group == favouritesGroup)
    {
        if (! studio.getOptionMemory().isFavourite ("blueprint", bp.name))
            return false;
    }
    else if (group.isNotEmpty() && groupOf (bp) != group)
        return false;
    const auto bars = bp.totalBars();
    switch (lengthBox.getSelectedId())
    {
        case 2: if (bars >= 64) return false; break;
        case 3: if (bars < 64 || bars > 100) return false; break;
        case 4: if (bars <= 100) return false; break;
        default: break;
    }
    switch (tempoBox.getSelectedId())
    {
        case 2: if (bp.bpm <= 0.0 || bp.bpm >= 95.0) return false; break;
        case 3: if (bp.bpm < 95.0 || bp.bpm > 140.0) return false; break;
        case 4: if (bp.bpm <= 140.0) return false; break;
        default: break;
    }
    if ((modeBox.getSelectedId() == 2 && ! bp.minor) || (modeBox.getSelectedId() == 3 && bp.minor))
        return false;
    juce::String words = bp.name + " " + bp.description + " " + groupOf (bp);
    for (const auto& b : bp.blocks)
        words << " " << b.name << " " << b.type;
    for (const auto& w : juce::StringArray::fromTokens (searchBox.getText(), " ", ""))
        if (w.isNotEmpty() && ! words.containsIgnoreCase (w))
            return false;
    return true;
}

std::vector<int> BlueprintsPage::getShown() const
{
    std::vector<int> out;
    for (const auto& r : rows)
        if (r.blueprint >= 0)
            out.push_back (r.blueprint);
    return out;
}

void BlueprintsPage::refreshRows()
{
    rows.clear();
    for (const auto& g : getGroups())
    {
        auto headed = false;
        for (int i = 0; i < (int) blueprints.size(); ++i)
            if (groupOf (blueprints[(size_t) i]) == g && passes (blueprints[(size_t) i]))
            {
                if (! headed)
                    rows.push_back ({ -1, g });
                headed = true;
                rows.push_back ({ i, {} });
            }
    }
    list.updateContent();
    list.deselectAllRows();
    for (int r = 0; r < (int) rows.size(); ++r)
        if (rows[(size_t) r].blueprint == chosen && chosen >= 0)
            list.selectRow (r, true, true);
    list.repaint();
}

void BlueprintsPage::setSearch (const juce::String& s) { searchBox.setText (s, false); refreshRows(); }
void BlueprintsPage::setGroup (const juce::String& g)
{
    if (g.isEmpty())
        groupBox.setSelectedId (1, juce::dontSendNotification);
    else
        groupBox.setText (g, juce::dontSendNotification);
    refreshRows();
}
void BlueprintsPage::setLength (Length l) { lengthBox.setSelectedId ((int) l + 1, juce::dontSendNotification); refreshRows(); }
void BlueprintsPage::setTempo (Tempo v)   { tempoBox.setSelectedId ((int) v + 1, juce::dontSendNotification); refreshRows(); }
void BlueprintsPage::setMode (Mode m)     { modeBox.setSelectedId ((int) m + 1, juce::dontSendNotification); refreshRows(); }

bool BlueprintsPage::use()
{
    if (! juce::isPositiveAndBelow (chosen, (int) blueprints.size()))
        return false;
    const auto bp = blueprints[(size_t) chosen];
    const auto key = getKey();
    const auto chords = chordsKey.getToggleState(), tempo = tempoKey.getToggleState();
    const auto music = musicKey.getToggleState() && chords && bp.hasChords();
    juce::Component::SafePointer<BlueprintsPage> safe (this);
    auto apply = [safe, bp, key, chords, tempo, music] (bool yes)
    {
        if (safe == nullptr || ! yes)
            return;
        if (safe->studio.applyBlueprint (bp, key, chords, tempo))
        {
            safe->status = "USED: " + bp.name.toUpperCase() + " IN " + keyName (key) + ". UNDO TAKES IT BACK.";
            if (music)
            {
                const auto built = safe->studio.buildSongSections (&bp, (juce::uint32) juce::Random::getSystemRandom().nextInt (1 << 30) + 1u);
                if (built > 0)
                    safe->status = "USED: " + bp.name.toUpperCase() + " IN " + keyName (key) + ", " + juce::String (built)
                                 + " SECTIONS BUILT. ROLL ONE FOR ANOTHER TAKE; UNDO TAKES THE MUSIC BACK.";
            }
            if (safe->onUsed)
                safe->onUsed();
        }
        safe->repaint();
    };
    if (studio.getSongSections().blocks.empty())
    {
        apply (true);
        return true;
    }
    if (askToReplace)
    {
        askToReplace (apply);
        return true;
    }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Use this blueprint?",
                                        "The SECTIONS row already has blocks. The blueprint replaces them (the music in the song "
                                        "stays where it is, and Undo takes it back).",
                                        "REPLACE THEM", "CANCEL", this,
                                        juce::ModalCallbackFunction::create ([apply] (int r) { apply (r == 1); }));
    return true;
}

bool BlueprintsPage::saveSongAs (const juce::String& name)
{
    juce::String error;
    const auto file = studio.saveSongAsBlueprint (name, error);
    status = file.existsAsFile() ? "SAVED: " + file.getFileNameWithoutExtension().toUpperCase() : error.toUpperCase();
    if (file.existsAsFile())
    {
        refreshList();
        for (int i = 0; i < (int) blueprints.size(); ++i)
            if (blueprints[(size_t) i].file == file)
                choose (i);
    }
    repaint();
    return file.existsAsFile();
}

void BlueprintsPage::askName()
{
    nameBox = std::make_unique<FamilyAlertWindow> ("Save this song's sections", "A name for the blueprint:", juce::MessageBoxIconType::NoIcon, this);
    nameBox->addTextEditor ("name", "My Song Form");
    nameBox->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameBox->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<BlueprintsPage> safe (this);
    nameBox->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int r)
    {
        if (safe == nullptr || safe->nameBox == nullptr)
            return;
        const auto name = safe->nameBox->getTextEditorContents ("name");
        safe->nameBox.reset();
        if (r == 1)
            safe->saveSongAs (name);
    }), false);
}

bool BlueprintsPage::deleteChosen()
{
    if (! juce::isPositiveAndBelow (chosen, (int) blueprints.size()))
        return false;
    const auto done = studio.deleteBlueprint (blueprints[(size_t) chosen]);
    if (done)
    {
        status = "DELETED: " + blueprints[(size_t) chosen].name.toUpperCase();
        chosen = -1;
        refreshList();
    }
    return done;
}

void BlueprintsPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    ParagonColours::setFill (g, ParagonColours::accent, juce::Rectangle<float> ((float) (0), (float) (0), (float) (getWidth()), (float) (3)));
    g.fillRect (0, 0, getWidth(), 3);
    auto area = getLocalBounds().reduced (gap);
    auto title = area.removeFromTop (34);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("BLUEPRINTS", title.removeFromLeft (200), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (14.0f));
    g.drawText (status.isNotEmpty() ? status : juce::String ("A SONG'S LAYOUT TO START FROM: ITS SECTIONS, LINKED COPIES, TYPES, CHORDS AND TEMPO"),
                title, juce::Justification::centredLeft, true);

    if (! juce::isPositiveAndBelow (chosen, (int) blueprints.size()))
        return;
    const auto& bp = blueprints[(size_t) chosen];
    const auto key = getKey();
    // The strip: each block as wide as its bars.
    if (! stripArea.isEmpty() && bp.totalBars() > 0)
    {
        const auto perBar = (float) stripArea.getWidth() / (float) bp.totalBars();
        auto x = (float) stripArea.getX();
        for (size_t i = 0; i < bp.blocks.size(); ++i)
        {
            const auto& b = bp.blocks[i];
            const auto r = juce::Rectangle<float> (x, (float) stripArea.getY(), perBar * (float) b.bars, (float) stripArea.getHeight()).reduced (1.0f, 0.0f);
            const auto tint = sectionColour (b.name);
            g.setColour (tint.withAlpha (0.28f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (tint);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, b.link > 0 ? 2.0f : 1.0f);
            if (b.transition.isNotEmpty())
            {
                g.setColour (pr7s::FamilyColours::pink);
                g.fillRect (r.getRight() - 3.0f, r.getY() + 3.0f, 2.0f, r.getHeight() - 6.0f);
            }
            if (r.getWidth() > 18.0f)
            {
                g.setColour (ParagonColours::text);
                g.setFont (font (juce::jmin (15.0f, r.getHeight() * 0.3f)));
                const auto inner = r.reduced (4.0f, 4.0f).toNearestInt();
                g.drawFittedText (blockLabel (bp, i).toUpperCase(), inner.withHeight (inner.getHeight() / 2), juce::Justification::centredLeft, 2, 0.6f);
                g.setColour (ParagonColours::textDim);
                g.drawText (juce::String (b.bars), inner.withTrimmedTop (inner.getHeight() / 2), juce::Justification::centredLeft, false);
            }
            x += perBar * (float) b.bars;
        }
    }
    // Its words, then each block's type, transition and chords in the key.
    auto info = infoArea;
    g.setColour (ParagonColours::text);
    g.setFont (font (17.0f));
    juce::String head = bp.name.toUpperCase() + "   " + juce::String (bp.totalBars()) + " BARS";
    if (bp.bpm > 0.0)
        head << "   " << juce::String (bp.bpm, 0) << " BPM";
    head << (bp.factory ? "   OURS" : "   YOURS");
    g.drawText (head, info.removeFromTop (24), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (14.0f));
    g.drawFittedText (bp.description.toUpperCase(), info.removeFromTop (40), juce::Justification::topLeft, 2, 0.9f);
    info.removeFromTop (6);
    const auto lineHeight = 20;
    const auto useChords = chordsKey.getToggleState();
    for (size_t i = 0; i < bp.blocks.size() && info.getHeight() >= lineHeight; ++i)
    {
        const auto& b = bp.blocks[i];
        auto line = info.removeFromTop (lineHeight);
        g.setColour (sectionColour (b.name));
        g.fillRect (line.removeFromLeft (4).reduced (0, 3));
        line.removeFromLeft (8);
        g.setColour (ParagonColours::text);
        g.setFont (font (14.0f));
        g.drawText (blockLabel (bp, i).toUpperCase() + "  " + juce::String (b.bars), line.removeFromLeft (150), juce::Justification::centredLeft, true);
        g.setColour (ParagonColours::textDim);
        juce::String what = b.type.isNotEmpty() ? b.type : juce::String ("STANDARD");
        if (b.transition.isNotEmpty())
            what << " > " << b.transition;
        g.drawText (what, line.removeFromLeft (300), juce::Justification::centredLeft, true);
        if (useChords && ! b.chords.empty())
        {
            juce::StringArray names;
            for (const auto& [c, bars] : bp.chordsIn (key, (int) i))
                names.add (c.name (key) + (std::abs (bars - 1.0) < 1.0e-6 ? juce::String() : ":" + barsText (bars)));
            g.setColour (ParagonColours::text);
            g.setFont (ChordUi::chordFont (14.0f));
            g.drawText (names.joinIntoString ("  "), line, juce::Justification::centredLeft, true);
        }
    }
}

void BlueprintsPage::resized()
{
    auto area = getLocalBounds().reduced (gap);
    area.removeFromTop (34 + 8);
    auto left = area.removeFromLeft (listWidth);
    saveKey.setBounds (left.removeFromBottom (32));
    left.removeFromBottom (8);
    searchBox.setBounds (left.removeFromTop (28));
    left.removeFromTop (6);
    {
        auto row = left.removeFromTop (26);
        groupBox.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        modeBox.setBounds (row);
        left.removeFromTop (6);
        row = left.removeFromTop (26);
        lengthBox.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        tempoBox.setBounds (row);
    }
    left.removeFromTop (8);
    list.setBounds (left);
    area.removeFromLeft (gap);
    auto controls = area.removeFromTop (32);
    useKey.setBounds (controls.removeFromRight (120));
    controls.removeFromRight (8);
    deleteKey.setBounds (controls.removeFromRight (90));
    controls.removeFromRight (8);
    keyBox.setBounds (controls.removeFromLeft (240));
    controls.removeFromLeft (12);
    chordsKey.setBounds (controls.removeFromLeft (100));
    tempoKey.setBounds (controls.removeFromLeft (100));
    musicKey.setBounds (controls.removeFromLeft (170));
    area.removeFromTop (gap);
    stripArea = area.removeFromTop (64);
    area.removeFromTop (gap);
    infoArea = area;
}

void BlueprintsPage::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;
    const auto& item = rows[(size_t) row];
    if (item.blueprint < 0)
    {
        g.fillAll (pageColour);
        ParagonColours::setFill (g, ParagonColours::accent, juce::Rectangle<float> (10.0f, (float) height - 4.0f, (float) width - 20.0f, 2.0f));
        g.fillRect (10, height - 4, width - 20, 2);
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (18.0f));
        g.drawText (item.heading, 10, 0, width - 20, height - 6, juce::Justification::bottomLeft, true);
        return;
    }
    const auto& bp = blueprints[(size_t) item.blueprint];
    ParagonColours::setFill (g, selected ? ParagonColours::accentDim : row % 2 == 0 ? juce::Colour (0xff131015) : pageColour, juce::Rectangle<float> ((float) (0), (float) (0), (float) (width), (float) (height)));
    g.fillRect (0, 0, width, height);
    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (10, 4);
    // The star (a click on it: a favourite or not).
    const auto star = r.removeFromLeft (starWidth).toFloat();
    juce::Path p;
    p.addStar (star.getCentre(), 5, 4.0f, 9.0f, 0.0f);
    if (isFavourite (item.blueprint))
    {
        ParagonColours::setFill (g, ParagonColours::accent, star);
        g.fillPath (p);
    }
    else
    {
        g.setColour (ParagonColours::textDim.withAlpha (0.6f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    g.setColour (ParagonColours::text);
    g.setFont (font (17.0f));
    g.drawText (bp.name.toUpperCase(), r.removeFromTop (r.getHeight() / 2 + 2), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (13.0f));
    juce::String sub = juce::String (bp.blocks.size()) + " SECTIONS, " + juce::String (bp.totalBars()) + " BARS";
    if (bp.bpm > 0.0)
        sub << ", " << juce::String (bp.bpm, 0) << " BPM";
    sub << (bp.minor ? ", MINOR" : ", MAJOR");
    g.drawText (sub, r, juce::Justification::centredLeft, true);
}

void BlueprintsPage::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()) || rows[(size_t) row].blueprint < 0)
        return;
    if (e.x < 10 + starWidth)
        setFavourite (rows[(size_t) row].blueprint, ! isFavourite (rows[(size_t) row].blueprint));
}

void BlueprintsPage::selectedRowsChanged (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;
    if (rows[(size_t) row].blueprint < 0)   // (a heading: the blueprint under it)
    {
        if (row + 1 < (int) rows.size())
            list.selectRow (row + 1);
        return;
    }
    if (rows[(size_t) row].blueprint != chosen)
        choose (rows[(size_t) row].blueprint);
}

BlueprintsWindow::BlueprintsWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: Blueprints", pageColour, juce::DocumentWindow::closeButton)
{
    page = new BlueprintsPage (studio);
    page->setSize (BlueprintsPage::preferredWidth, BlueprintsPage::preferredHeight);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (900, 480, 3840, 2160);
}

void BlueprintsWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
