#include "ui/SectionEditor.h"

#include "engine/SectionTypes.h"
#include "engine/SongSections.h"

namespace
{
    juce::Font words (float h) { return ParagonLookAndFeel::textFont (h); }

    juce::String barsText (double bars)
    {
        if (std::abs (bars - 0.5) < 1.0e-6) return "1/2 BAR";
        if (std::abs (bars - std::round (bars)) < 1.0e-6)
        {
            const auto n = juce::roundToInt (bars);
            return juce::String (n) + (n == 1 ? " BAR" : " BARS");
        }
        return juce::String (bars, 2) + " BARS";
    }

    constexpr int lengthChoices[] { 1, 2, 4, 6, 8, 12, 16, 24, 32 };
}

//==============================================================================
// The block's chords as cells, as wide as they last: a click opens the chord's menu, a drag sideways moves it.
class SectionEditor::Strip final : public juce::Component
{
public:
    explicit Strip (SectionEditor& e) : editor (e) {}

    juce::Rectangle<int> cell (int i) const
    {
        const auto& chords = editor.chords;
        if (! juce::isPositiveAndBelow (i, (int) chords.size()))
            return {};
        const auto block = juce::jmax (1.0e-6, editor.blockBeats());
        const auto w = (double) getWidth();
        auto x = 0.0;
        for (int k = 0; k < i; ++k)
            x += juce::jmax (48.0, chords[(size_t) k].beats / block * w);
        const auto cw = juce::jmax (48.0, chords[(size_t) i].beats / block * w);
        return juce::Rectangle<double> (x, 0.0, cw, (double) getHeight()).toNearestInt().reduced (2, 0);
    }

    int cellAt (int x) const
    {
        for (int i = 0; i < (int) editor.chords.size(); ++i)
            if (const auto r = cell (i); x >= r.getX() - 2 && x < r.getRight() + 2)
                return i;
        return -1;
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff0e0c10));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        const auto key = editor.getKey();
        if (editor.chords.empty())
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (words (16.0f));
            g.drawText ("NO CHORDS IN THIS BLOCK YET: + CHORD ADDS ONE FROM THE KEY", getLocalBounds(), juce::Justification::centred);
            return;
        }
        for (int i = 0; i < (int) editor.chords.size(); ++i)
        {
            auto r = cell (i).toFloat();
            if (dragging && i == from)
                r = r.translated ((float) (dragX - downX), 0.0f);
            const auto& c = editor.chords[(size_t) i];
            const auto out = ! c.chord.isInKey (key);
            g.setColour (juce::Colour (0xff1d191f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (out ? juce::Colour (pr7s::FamilyColours::pink) : (dragging && i == from ? ParagonColours::accent : juce::Colour (0xff3a333d)));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, dragging && i == from ? 2.0f : 1.0f);
            auto inner = r.reduced (6.0f, 4.0f);
            g.setColour (ParagonColours::text);
            g.setFont (ChordUi::chordFont (22.0f));
            g.drawFittedText (c.chord.name (key), inner.removeFromTop (inner.getHeight() * 0.45f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            g.setColour (out ? juce::Colour (pr7s::FamilyColours::pink) : ParagonColours::textDim);
            g.setFont (ChordUi::chordFont (14.0f));
            g.drawFittedText (c.chord.romanName (key), inner.removeFromTop (inner.getHeight() * 0.5f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            g.setFont (words (13.0f));
            g.drawFittedText (barsText (c.beats / editor.beatsPerBar()), inner.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
        if (dragging && target >= 0)
        {
            const auto x = target < (int) editor.chords.size() ? cell (target).getX() - 2
                         : cell ((int) editor.chords.size() - 1).getRight() + 2;
            g.setColour (ParagonColours::accent);
            g.fillRect (x - 1, 0, 3, getHeight());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        from = cellAt (e.x);
        downX = dragX = e.x;
        dragging = false;
        target = -1;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (from < 0)
            return;
        dragX = e.x;
        if (! dragging && std::abs (e.x - downX) < 6)
            return;
        dragging = true;
        // Its place: before the first other cell whose middle is right of the mouse.
        target = (int) editor.chords.size();
        for (int i = 0; i < (int) editor.chords.size(); ++i)
            if (i != from && cell (i).getCentreX() > e.x)
            {
                target = i;
                break;
            }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        const auto was = from;
        const auto moved = dragging;
        const auto to = target;
        from = target = -1;
        dragging = false;
        repaint();
        if (was < 0)
            return;
        if (! moved)
        {
            editor.showChordMenu (was);
            return;
        }
        // `to` is the index before which it goes, among all cells: once taken out, the ones after it move down.
        const auto place = to > was ? to - 1 : to;
        if (place != was)
            editor.moveChord (was, place);
    }

private:
    SectionEditor& editor;
    int from = -1, target = -1, downX = 0, dragX = 0;
    bool dragging = false;
};

//==============================================================================
SectionEditor::SectionEditor (pr7s::Studio& s, int id) : studio (s), blockId (id), strip (std::make_unique<Strip> (*this))
{
    for (auto* l : { &title, &info, &chordsLabel, &hint, &nameLabel, &typeLabel, &lengthLabel, &keyLabel })
    {
        l->setColour (juce::Label::textColourId, ParagonColours::text);
        l->setFont (words (15.0f));
        addAndMakeVisible (*l);
    }
    title.setFont (ParagonLookAndFeel::titleFont (28.0f));
    info.setColour (juce::Label::textColourId, ParagonColours::textDim);
    hint.setColour (juce::Label::textColourId, ParagonColours::textDim);
    hint.setFont (words (13.0f));
    hint.setText ("CLICK A CHORD TO CHANGE IT, ITS LENGTH OR MOVE IT; DRAG ONE SIDEWAYS TO PUT IT ELSEWHERE", juce::dontSendNotification);
    chordsLabel.setText ("CHORDS FROM THE KEY", juce::dontSendNotification);
    for (auto* l : { &nameLabel, &typeLabel, &lengthLabel, &keyLabel })
        l->setColour (juce::Label::textColourId, ParagonColours::textDim);

    for (auto* box : { &nameBox, &typeBox, &lengthBox, &keyBox })
        addAndMakeVisible (*box);
    const auto& names = pr7s::songSectionNames();
    for (int i = 0; i < names.size(); ++i)
        nameBox.addItem (names[i].toUpperCase(), i + 1);
    for (int i = 0; i < (int) pr7s::Key::all().size(); ++i)
        keyBox.addItem (ChordUi::spokenKey (pr7s::Key::all()[(size_t) i]).toUpperCase(), i + 1);
    nameBox.onChange = [this] { if (! refreshing && nameBox.getSelectedId() > 0) setName (pr7s::songSectionNames()[nameBox.getSelectedId() - 1]); };
    typeBox.onChange = [this]
    {
        if (refreshing || typeBox.getSelectedId() <= 0)
            return;
        setType (typeBox.getSelectedId() == 1 ? juce::String() : typeBox.getText());
    };
    lengthBox.onChange = [this] { if (! refreshing && lengthBox.getSelectedId() > 0) setBars (lengthBox.getSelectedId()); };
    keyBox.onChange = [this]
    {
        if (! refreshing && keyBox.getSelectedId() > 0)
            setSongKey (pr7s::Key::all()[(size_t) keyBox.getSelectedId() - 1]);
    };

    addAndMakeVisible (*strip);
    for (auto* k : { &addKey, &clearKey, &openKey, &moreKey })
    {
        ChordUi::styleKey (*k);
        addAndMakeVisible (*k);
    }
    addKey.onClick = [this] { addChord(); };
    clearKey.onClick = [this] { clearChords(); };
    openKey.onClick = [this] { if (onOpen && getIndex() >= 0) onOpen (getIndex()); };
    moreKey.onClick = [this] { if (onMore && getIndex() >= 0) onMore (getIndex()); };
    setSize (width, height);
    refresh();
}

SectionEditor::~SectionEditor() = default;

int SectionEditor::getIndex() const
{
    const auto& blocks = studio.getSongSections().blocks;
    for (int i = 0; i < (int) blocks.size(); ++i)
        if (blocks[(size_t) i].id == blockId)
            return i;
    return -1;
}

double SectionEditor::beatsPerBar() const
{
    const auto place = studio.getSongSectionPlace (getIndex());
    return place.has_value() ? studio.getProject().beatsPerBarAt (place->start) : studio.getProject().beatsPerBar();
}

double SectionEditor::blockBeats() const
{
    const auto place = studio.getSongSectionPlace (getIndex());
    return place.has_value() ? place->end - place->start : 0.0;
}

pr7s::Key SectionEditor::getKey() const
{
    const auto place = studio.getSongSectionPlace (getIndex());
    return studio.chordTrackKeyAt (place.has_value() ? place->start : 0.0);
}

void SectionEditor::refresh()
{
    const juce::ScopedValueSetter<bool> busy (refreshing, true);
    const auto index = getIndex();
    if (index < 0)
    {
        title.setText ("THIS BLOCK IS GONE", juce::dontSendNotification);
        chords.clear();
        repaint();
        return;
    }
    const auto& blocks = studio.getSongSections().blocks;
    const auto& b = blocks[(size_t) index];
    title.setText (pr7s::sectionLabel (blocks, index).toUpperCase(), juce::dontSendNotification);
    if (const auto place = studio.getSongSectionPlace (index))
        info.setText ("BARS " + juce::String (place->firstBar + 1) + " TO " + juce::String (place->firstBar + place->bars)
                          + (b.link > 0 ? "   (LINKED: ITS COPIES CHANGE WITH IT)" : ""), juce::dontSendNotification);
    nameBox.setSelectedId (pr7s::songSectionNames().indexOf (b.name) + 1, juce::dontSendNotification);
    typeBox.clear (juce::dontSendNotification);
    typeBox.addItem (pr7s::standardSectionType, 1);
    const auto kinds = pr7s::sectionTypesFor (b.name);
    for (int i = 0; i < (int) kinds.size(); ++i)
        typeBox.addItem (kinds[(size_t) i].name, i + 2);
    auto typeId = 1;
    for (int i = 0; i < (int) kinds.size(); ++i)
        if (kinds[(size_t) i].name == b.type)
            typeId = i + 2;
    typeBox.setSelectedId (typeId, juce::dontSendNotification);
    lengthBox.clear (juce::dontSendNotification);
    auto hasOwn = false;
    for (const auto bars : lengthChoices)
    {
        lengthBox.addItem (barsText (bars), bars);
        hasOwn = hasOwn || bars == b.bars;
    }
    if (! hasOwn)
        lengthBox.addItem (barsText (b.bars), b.bars);
    lengthBox.setSelectedId (b.bars, juce::dontSendNotification);
    const auto key = getKey();
    const auto& keys = pr7s::Key::all();
    for (int i = 0; i < (int) keys.size(); ++i)
        if (keys[(size_t) i] == key)
            keyBox.setSelectedId (i + 1, juce::dontSendNotification);
    chords = studio.getSectionChords (index);
    auto filled = 0.0;
    for (const auto& c : chords)
        filled += c.beats;
    chordsLabel.setText ("CHORDS FROM " + ChordUi::spokenKey (key).toUpperCase() + "   (" + barsText (filled / beatsPerBar())
                             + " OF " + barsText (blockBeats() / beatsPerBar()) + ")", juce::dontSendNotification);
    repaint();
    strip->repaint();
}

void SectionEditor::setName (const juce::String& name)
{
    studio.renameSongSection (getIndex(), name);
    refresh();
}

void SectionEditor::setType (const juce::String& type)
{
    const auto index = getIndex();
    if (index < 0)
        return;
    if (onType)
        onType (index, type);
    else
        studio.setSongSectionType (index, type);
    refresh();
}

void SectionEditor::setBars (int bars)
{
    studio.setSongSectionBars (getIndex(), bars);
    refresh();
}

void SectionEditor::setSongKey (const pr7s::Key& key)
{
    studio.setChordTrackKey (0.0, key);
    refresh();
}

void SectionEditor::write()
{
    studio.setSectionChords (getIndex(), chords);
    refresh();
}

void SectionEditor::changeChord (int i, const pr7s::Chord& chord)
{
    if (! juce::isPositiveAndBelow (i, (int) chords.size()))
        return;
    chords[(size_t) i].chord = chord;
    write();
}

void SectionEditor::setChordBars (int i, double bars)
{
    if (! juce::isPositiveAndBelow (i, (int) chords.size()) || bars <= 0.0)
        return;
    chords[(size_t) i].beats = bars * beatsPerBar();
    write();
}

void SectionEditor::moveChord (int from, int to)
{
    if (! juce::isPositiveAndBelow (from, (int) chords.size()))
        return;
    to = juce::jlimit (0, (int) chords.size() - 1, to);
    if (to == from)
        return;
    const auto c = chords[(size_t) from];
    chords.erase (chords.begin() + from);
    chords.insert (chords.begin() + to, c);
    write();
}

void SectionEditor::removeChord (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) chords.size()))
        return;
    chords.erase (chords.begin() + i);
    write();
}

void SectionEditor::clearChords()
{
    chords.clear();
    write();
}

void SectionEditor::addChord()
{
    const auto index = getIndex();
    const auto place = studio.getSongSectionPlace (index);
    if (! place.has_value())
        return;
    const auto bar = beatsPerBar();
    auto filled = 0.0;
    for (const auto& c : chords)
        filled += c.beats;
    // No room left: the longest chord over a bar gives up a bar.
    if (place->end - place->start - filled < 1.0e-6)
    {
        auto longest = -1;
        for (int i = 0; i < (int) chords.size(); ++i)
            if (chords[(size_t) i].beats > bar + 1.0e-6 && (longest < 0 || chords[(size_t) i].beats > chords[(size_t) longest].beats))
                longest = i;
        if (longest < 0)
            return;
        chords[(size_t) longest].beats -= bar;
        filled -= bar;
    }
    const auto key = getKey();
    // The key's own first; then what the chord before suggests, when it's in the key (else the key's next in order).
    const auto own = pr7s::keyChords (key);
    auto chord = chords.empty() ? own.front() : studio.suggestChordAt (place->start + filled);
    if (! chord.isInKey (key) && ! own.empty())
        chord = own[chords.size() % own.size()];
    chords.push_back ({ chord, juce::jmin (bar, place->end - place->start - filled) });
    write();
}

void SectionEditor::showChordMenu (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) chords.size()))
        return;
    const auto key = getKey();
    const auto choices = pr7s::keyChords (key);
    juce::PopupMenu menu, change, length;
    menu.addSectionHeader (chords[(size_t) i].chord.name (key) + "   " + chords[(size_t) i].chord.romanName (key));
    for (int k = 0; k < (int) choices.size(); ++k)
        change.addItem (100 + k, choices[(size_t) k].name (key) + "   (" + choices[(size_t) k].romanName (key) + ")", true,
                        choices[(size_t) k] == chords[(size_t) i].chord);
    menu.addSubMenu ("Change to (the key's chords)", change);
    const double lengths[] { 0.5, 1.0, 2.0, 3.0, 4.0, 8.0 };
    for (int k = 0; k < 6; ++k)
        length.addItem (200 + k, barsText (lengths[k]), true, std::abs (chords[(size_t) i].beats - lengths[k] * beatsPerBar()) < 1.0e-6);
    menu.addSubMenu ("Length", length);
    menu.addItem (1, "Move Earlier", i > 0);
    menu.addItem (2, "Move Later", i + 1 < (int) chords.size());
    menu.addItem (3, "Remove");
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<SectionEditor> (this), i, choices, lengths] (int chosen)
    {
        if (safe == nullptr || chosen <= 0)
            return;
        if (chosen == 1) safe->moveChord (i, i - 1);
        else if (chosen == 2) safe->moveChord (i, i + 1);
        else if (chosen == 3) safe->removeChord (i);
        else if (chosen >= 100 && chosen < 100 + (int) choices.size()) safe->changeChord (i, choices[(size_t) chosen - 100]);
        else if (chosen >= 200 && chosen < 206) safe->setChordBars (i, lengths[chosen - 200]);
    });
}

juce::Rectangle<int> SectionEditor::getChordBounds (int i) const
{
    return strip->cell (i).translated (strip->getX(), strip->getY());
}

void SectionEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141116));
}

void SectionEditor::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    auto top = r.removeFromTop (34);
    title.setBounds (top.removeFromLeft (r.getWidth() / 2));
    info.setBounds (top);
    r.removeFromTop (6);
    auto labels = r.removeFromTop (20);
    auto boxes = r.removeFromTop (28);
    const auto w = (r.getWidth() - 3 * 10) / 4;
    const std::array<std::pair<juce::Label*, juce::ComboBox*>, 4> pairs { { { &nameLabel, &nameBox }, { &typeLabel, &typeBox },
                                                                          { &lengthLabel, &lengthBox }, { &keyLabel, &keyBox } } };
    for (auto [label, box] : pairs)
    {
        label->setBounds (labels.removeFromLeft (w));
        box->setBounds (boxes.removeFromLeft (w));
        labels.removeFromLeft (10);
        boxes.removeFromLeft (10);
    }
    r.removeFromTop (10);
    chordsLabel.setBounds (r.removeFromTop (22));
    strip->setBounds (r.removeFromTop (110));
    hint.setBounds (r.removeFromTop (22));
    auto keys = r.removeFromBottom (32);
    for (auto* k : { &addKey, &clearKey })
    {
        k->setBounds (keys.removeFromLeft (130));
        keys.removeFromLeft (8);
    }
    moreKey.setBounds (keys.removeFromRight (110));
    keys.removeFromRight (8);
    openKey.setBounds (keys.removeFromRight (220));
}
