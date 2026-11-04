#include "ui/LinesPage.h"

#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }

    // Each part's notes in one of the family's three: bass and rhythm orange, melody and lead pink, piano red.
    juce::Colour colourOf (pr7s::LineIdea::Part part)
    {
        using P = pr7s::LineIdea::Part;
        return part == P::bass || part == P::rhythm ? pr7s::FamilyColours::orange
             : part == P::chords || part == P::pattern ? pr7s::FamilyColours::red : pr7s::FamilyColours::pink;
    }
}

//==============================================================================
class LinesPage::Card final : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    Card (LinesPage& p, pr7s::LineIdea::Part pt, int i) : page (p), part (pt), index (i)
    {
        for (auto* key : { &hear, &useIt })
        {
            key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
            key->setWantsKeyboardFocus (false);
            addAndMakeVisible (*key);
        }
        hear.onClick = [this] { isPlaying() ? page.stop() : page.play (part, index); };
        useIt.onClick = [this] { page.showUseMenu (part, index, *this); };
        useIt.setTooltip ("A MIDI part under the chords (one undo step): onto the selected instrument track or a new one; then edit it in the MIDI editor");
        setTooltip ("Click to hear it with the chords");
    }

    const pr7s::LineIdea& idea() const { return page.getIdeas (part)[(size_t) index]; }
    bool isPlaying() const { return page.playing && page.playing->first == part && page.playing->second == index; }

    void refresh()
    {
        hear.setButtonText (isPlaying() ? "STOP" : "HEAR");
        hear.setToggleState (isPlaying(), juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto lit = isPlaying();
        g.setColour (juce::Colour (lit ? 0xff1f1720 : 0xff141015));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (lit ? ParagonColours::accent : ParagonColours::panelEdge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, lit ? 1.5f : 1.0f);

        auto area = bounds.reduced (10.0f, 8.0f);
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        g.drawText (juce::String (index + 1) + "  " + idea().style.toUpperCase(), area.removeFromTop (20.0f), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (interFont (11.5f));
        g.drawFittedText (idea().description, area.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        if (added.isNotEmpty())
        {
            g.setColour (ParagonColours::accent);
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            g.drawText ("ADDED: " + added.toUpperCase(), bounds.reduced (10.0f, 8.0f).removeFromTop (20.0f), juce::Justification::centredRight, true);
        }
        paintRoll (g, roll);
    }

    void paintRoll (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff0b090c));
        g.fillRoundedRectangle (r, 3.0f);
        const auto& spans = page.spans;
        if (spans.empty())
            return;
        const auto total = spans.back().end - spans.front().start;
        const auto xFor = [&] (double beat) { return r.getX() + (float) (beat / total) * r.getWidth(); };
        // The chords: a line where each starts, its name above.
        g.setFont (ChordUi::chordFont (10.0f));
        for (const auto& s : spans)
        {
            const auto x = xFor (s.start - spans.front().start);
            g.setColour (ParagonColours::panelEdge);
            g.fillRect (x, r.getY(), 1.0f, r.getHeight());
            g.setColour (ParagonColours::textDim);
            g.drawText (s.chord.name (s.key), juce::Rectangle<float> (x + 3.0f, r.getY() + 1.0f, 60.0f, 12.0f), juce::Justification::centredLeft, false);
        }
        // The notes, low to high.
        const auto& notes = idea().notes;
        auto low = 127, high = 0;
        for (const auto& n : notes)
        {
            low = juce::jmin (low, n.pitch);
            high = juce::jmax (high, n.pitch);
        }
        low -= 2;
        high += 2;
        const auto body = r.withTrimmedTop (14.0f).reduced (0.0f, 3.0f);
        const auto row = body.getHeight() / (float) juce::jmax (1, high - low + 1);
        const auto colour = colourOf (part);
        for (const auto& n : notes)
        {
            const auto y = body.getBottom() - (float) (n.pitch - low + 1) * row;
            g.setColour (colour.withAlpha (0.45f + 0.55f * n.velocity));
            g.fillRoundedRectangle (juce::Rectangle<float> (xFor (n.beat), y, juce::jmax (2.0f, xFor (n.beat + n.length) - xFor (n.beat) - 1.0f),
                                                            juce::jmax (2.0f, row - 1.0f)), 1.5f);
        }
        if (isPlaying())
        {
            const auto x = xFor (juce::jmin (total, page.studio.getPreviewBeat()));
            g.setColour (juce::Colour (0xfff4eeea));
            g.fillRect (x, r.getY(), 1.4f, r.getHeight());
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 8);
        area.removeFromTop (52);
        auto keys = area.removeFromBottom (26);
        useIt.setBounds (keys.removeFromRight (78));
        keys.removeFromRight (6);
        hear.setBounds (keys.removeFromRight (70));
        area.removeFromBottom (6);
        roll = area.toFloat();
    }

    void mouseDown (const juce::MouseEvent&) override { isPlaying() ? page.stop() : page.play (part, index); }

    LinesPage& page;
    const pr7s::LineIdea::Part part;
    const int index;
    juce::TextButton hear { "HEAR" }, useIt { "USE IT" };
    juce::Rectangle<float> roll;
    juce::String added;   // the track it went onto
};

//==============================================================================
HumanizePanel::HumanizePanel (float a, bool f) : amount (juce::jlimit (0.0f, 1.0f, a)), feel (f)
{
    bar.setRange (0.0, 100.0, 1.0);
    bar.setTextValueSuffix ("%");
    bar.setColour (juce::Slider::trackColourId, ParagonColours::accent);
    bar.setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
    bar.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b171c));
    bar.setColour (juce::Slider::textBoxOutlineColourId, ParagonColours::panelEdge);
    bar.setTooltip ("0%: exactly on the grid. 100%: notes up to 12 ms early or late, velocities up to 10 softer or harder (inside their layer), lengths up to 12% different");
    bar.setValue (amount * 100.0, juce::dontSendNotification);
    bar.onValueChange = [this] { setAmount ((float) bar.getValue() / 100.0f); };
    addAndMakeVisible (bar);
    feelKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    feelKey.setClickingTogglesState (false);
    feelKey.setToggleState (feel, juce::dontSendNotification);
    feelKey.setTooltip ("Each part's lean: rhythm guitar and bass a little ahead of the beat (pushing), leads and melodies a little behind (laying back), piano on it; grows with the amount");
    feelKey.onClick = [this] { setFeel (! feel); };
    addAndMakeVisible (feelKey);
    setSize (width, height);
}

void HumanizePanel::setAmount (float a)
{
    a = juce::jlimit (0.0f, 1.0f, a);
    if (a == amount)
        return;
    amount = a;
    bar.setValue (amount * 100.0, juce::dontSendNotification);
    if (onChange)
        onChange (amount, feel);
}

void HumanizePanel::setFeel (bool f)
{
    if (f == feel)
        return;
    feel = f;
    feelKey.setToggleState (feel, juce::dontSendNotification);
    if (onChange)
        onChange (amount, feel);
}

void HumanizePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    auto area = getLocalBounds().reduced (14, 10);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (16.0f));
    g.drawText ("HUMANIZE", area.removeFromTop (22), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText ("Small differences in timing, velocity and length so the ideas sound played. Chords stay together, "
                      "a bar's first beat moves least, and velocities stay in their articulation's layer.",
                      area.removeFromTop (44), juce::Justification::topLeft, 3, 0.9f);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText ("AMOUNT", area.removeFromTop (30).removeFromLeft (80), juce::Justification::centredLeft, false);
}

void HumanizePanel::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    area.removeFromTop (66);
    auto row = area.removeFromTop (30);
    row.removeFromLeft (80);
    bar.setBounds (row.reduced (0, 4));
    area.removeFromTop (12);
    feelKey.setBounds (area.removeFromTop (28).removeFromLeft (120));
}

//==============================================================================
GuitarVelocityPanel::GuitarVelocityPanel (pr7s::GuitarVelocities v) : values (v)
{
    for (int i = 0; i < pr7s::GuitarVelocities::count; ++i)
    {
        auto* bar = bars.add (new juce::Slider (juce::Slider::LinearBar, juce::Slider::TextBoxLeft));
        bar->setRange (1.0, 127.0, 1.0);
        bar->setColour (juce::Slider::trackColourId, i < 3 ? pr7s::FamilyColours::orange : i < 6 ? pr7s::FamilyColours::red : pr7s::FamilyColours::pink);
        bar->setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        bar->setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b171c));
        bar->setColour (juce::Slider::textBoxOutlineColourId, ParagonColours::panelEdge);
        bar->setTooltip ("Drag, or double-click to type (1-127). Your library's layer for it: " + pr7s::GuitarVelocities::layerOf (i));
        bar->onValueChange = [this, i, bar] { setValue (i, (int) bar->getValue()); };
        addAndMakeVisible (bar);
    }
    resetKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    resetKey.setTooltip ("Back to Shreddage's layout: mutes 34 / 44 / 54, sustain 80 / 96 / 110, rake 123, pinch 127");
    resetKey.onClick = [this] { reset(); };
    addAndMakeVisible (resetKey);
    refresh();
    setSize (width, titleHeight + rowHeight * pr7s::GuitarVelocities::count + 50);
}

void GuitarVelocityPanel::refresh()
{
    for (int i = 0; i < bars.size(); ++i)
        bars[i]->setValue (values.get (i), juce::dontSendNotification);
}

void GuitarVelocityPanel::setValue (int index, int value)
{
    const auto before = values;
    values.set (index, value);
    refresh();
    if (values != before && onChange)
        onChange (values);
}

void GuitarVelocityPanel::reset()
{
    const auto before = values;
    values = {};
    refresh();
    if (values != before && onChange)
        onChange (values);
}

void GuitarVelocityPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    auto area = getLocalBounds().reduced (14, 10);
    auto title = area.removeFromTop (titleHeight - 10);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (16.0f));
    g.drawText ("GUITAR VELOCITIES", title.removeFromTop (22), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("Each articulation's velocity, set by ear. The ideas are dealt again as you change them.", title, juce::Justification::centredLeft, true);
    for (int i = 0; i < pr7s::GuitarVelocities::count; ++i)
    {
        auto row = area.removeFromTop (rowHeight);
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (13.0f));
        g.drawText (pr7s::GuitarVelocities::nameOf (i), row.removeFromLeft (150), juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (pr7s::GuitarVelocities::layerOf (i), row.removeFromRight (60), juce::Justification::centredRight, false);
    }
}

void GuitarVelocityPanel::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    area.removeFromTop (titleHeight - 10);
    for (auto* bar : bars)
    {
        auto row = area.removeFromTop (rowHeight);
        row.removeFromLeft (150);
        row.removeFromRight (66);
        bar->setBounds (row.reduced (0, 4));
    }
    area.removeFromTop (12);
    resetKey.setBounds (area.removeFromTop (28).removeFromRight (170));
}

//==============================================================================
LinesPage::LinesPage (pr7s::Studio& s, std::vector<pr7s::ChordSpan> chords) : studio (s)
{
    for (auto* key : { &hearChords, &more, &stopKey, &pianoTab, &guitarTab, &bassTab, &patternsTab })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    hearChords.setTooltip ("The chords alone, at the song's tempo");
    more.setTooltip ("New ideas in every tab (the same styles, other choices)");
    hearChords.onClick = [this] { playChords(); };
    more.onClick = [this] { moreIdeas(); };
    stopKey.onClick = [this] { stop(); };
    pianoTab.setTooltip ("Piano: chord accompaniments and melodies");
    guitarTab.setTooltip ("Guitar: rock and metal rhythm parts (power chords, chugs, gallops) and leads (pentatonic, blues, shred runs)");
    bassTab.setTooltip ("Bass lines");
    pianoTab.onClick = [this] { setTab (Tab::piano); };
    guitarTab.onClick = [this] { setTab (Tab::guitar); };
    bassTab.onClick = [this] { setTab (Tab::bass); };
    // (0.39.0) PATTERNS: the chords played through rhythm and arpeggio patterns, a group at a time.
    patternsTab.setTooltip ("Patterns: the chords played as arpeggios, strums, pulses, ballad figures and riffs (also playable live from the chord pads: PERFORM)");
    patternsTab.onClick = [this] { setTab (Tab::patterns); };
    {
        int groupId = 1;
        for (const auto& group : pr7s::patternGroups())
            groupBox.addItem (group, groupId++);
    }
    groupBox.setTooltip ("Which patterns: six at a time");
    groupBox.onChange = [this] { setPatternGroup (groupBox.getText()); };
    addChildComponent (groupBox);
    if (auto* settings = studio.getSettings())
        if (const auto saved = settings->getValue ("ideasPatternGroup"); pr7s::patternGroups().contains (saved))
            patternGroup = saved;

    tuningLabel.setFont (ParagonLookAndFeel::titleFont (13.0f));
    tuningLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
    tuningLabel.setJustificationType (juce::Justification::centredRight);
    addChildComponent (tuningLabel);
    int id = 1;
    for (const auto& t : pr7s::GuitarTuning::all())
        tuningBox.addItem (t.name, id++);
    tuningBox.setTooltip ("The guitar's strings: nothing is played below the lowest one (guitar libraries keep their keyswitches down there)");
    tuningBox.onChange = [this] { setTuning (tuningBox.getText()); };
    addChildComponent (tuningBox);
    velocitiesKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    velocitiesKey.setWantsKeyboardFocus (false);
    velocitiesKey.setTooltip ("The velocity of each articulation (palm mute, sustain, rake, pinch harmonic), set by ear for your guitar library");
    velocitiesKey.onClick = [this] { showVelocities(); };
    addChildComponent (velocitiesKey);
    humanizeKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    humanizeKey.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    humanizeKey.setWantsKeyboardFocus (false);
    humanizeKey.setTooltip ("Loosen the ideas so they sound played: timing, velocity and length, with each part's feel");
    humanizeKey.onClick = [this] { showHumanize(); };
    addAndMakeVisible (humanizeKey);

    if (auto* settings = studio.getSettings())
    {
        const auto saved = settings->getValue ("ideasTab");
        tab = saved == "guitar" ? Tab::guitar : saved == "bass" ? Tab::bass : saved == "patterns" ? Tab::patterns : Tab::piano;
    }
    setSize (1240, 760);
    setChords (std::move (chords));
    startTimerHz (30);
}

LinesPage::~LinesPage()
{
    studio.stopPreview();
}

std::vector<pr7s::LineIdea::Part> LinesPage::partsOf (Tab t)
{
    using P = pr7s::LineIdea::Part;
    switch (t)
    {
        case Tab::piano: return { P::chords, P::melody };
        case Tab::guitar: return { P::rhythm, P::lead };
        case Tab::bass: return { P::bass };
        case Tab::patterns: return { P::pattern };
    }
    return {};
}

juce::String LinesPage::titleOf (pr7s::LineIdea::Part part)
{
    using P = pr7s::LineIdea::Part;
    switch (part)
    {
        case P::bass: return "BASS";
        case P::melody: return "MELODY";
        case P::chords: return "CHORDS";
        case P::rhythm: return "RHYTHM";
        case P::lead: return "LEAD";
        case P::pattern: return "PATTERNS";
    }
    return {};
}

const pr7s::GuitarTuning& LinesPage::getTuning() const
{
    return pr7s::GuitarTuning::named (studio.getGuitarTuning());
}

void LinesPage::setChords (std::vector<pr7s::ChordSpan> chords)
{
    stop();
    spans = std::move (chords);
    barBeats = studio.getProject().beatsPerBar();
    rebuild();
}

void LinesPage::setTab (Tab t)
{
    tab = t;
    if (auto* settings = studio.getSettings())
        settings->setValue ("ideasTab", t == Tab::guitar ? "guitar" : t == Tab::bass ? "bass" : t == Tab::patterns ? "patterns" : "piano");
    makeCards();
}

void LinesPage::setPatternGroup (const juce::String& group)
{
    if (! pr7s::patternGroups().contains (group) || group == patternGroup)
        return;
    stop();
    patternGroup = group;
    if (auto* settings = studio.getSettings())
        settings->setValue ("ideasPatternGroup", group);
    rebuild();
}

void LinesPage::setTuning (const juce::String& name)
{
    if (name == getTuning().name)
        return;
    stop();
    studio.setGuitarTuning (name);
    rebuild();
}

void LinesPage::setGuitarVelocities (const pr7s::GuitarVelocities& v)
{
    if (v == studio.getGuitarVelocities())
        return;
    stop();
    studio.setGuitarVelocities (v);
    rebuild();
}

void LinesPage::showVelocities()
{
    auto panel = std::make_unique<GuitarVelocityPanel> (studio.getGuitarVelocities());
    panel->onChange = [safe = juce::Component::SafePointer<LinesPage> (this)] (const pr7s::GuitarVelocities& v)
    {
        if (safe != nullptr)
            safe->setGuitarVelocities (v);
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), velocitiesKey.getScreenBounds(), nullptr);
}

juce::String LinesPage::humanizeText (float amount)
{
    return amount <= 0.0f ? juce::String ("HUMANIZE OFF") : "HUMANIZE " + juce::String (juce::roundToInt (amount * 100.0f)) + "%";
}

void LinesPage::setHumanize (float amount, bool feel)
{
    if (amount == studio.getIdeasHumanize() && feel == studio.getIdeasFeel())
        return;
    stop();
    studio.setIdeasHumanize (amount);
    studio.setIdeasFeel (feel);
    rebuild();
}

void LinesPage::showHumanize()
{
    auto panel = std::make_unique<HumanizePanel> (studio.getIdeasHumanize(), studio.getIdeasFeel());
    panel->onChange = [safe = juce::Component::SafePointer<LinesPage> (this)] (float amount, bool feel)
    {
        if (safe != nullptr)
            safe->setHumanize (amount, feel);
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), humanizeKey.getScreenBounds(), nullptr);
}

void LinesPage::rebuild()
{
    using P = pr7s::LineIdea::Part;
    const auto& tuning = getTuning();
    ideas[(size_t) P::bass] = pr7s::bassIdeas (spans, barBeats, variation);
    ideas[(size_t) P::melody] = pr7s::melodyIdeas (spans, barBeats, variation);
    ideas[(size_t) P::chords] = pr7s::pianoChordIdeas (spans, barBeats, variation);
    const auto velocities = studio.getGuitarVelocities();
    ideas[(size_t) P::rhythm] = pr7s::guitarRhythmIdeas (spans, barBeats, variation, tuning, velocities);
    ideas[(size_t) P::lead] = pr7s::guitarLeadIdeas (spans, barBeats, variation, tuning, velocities);
    ideas[(size_t) P::pattern] = pr7s::patternIdeas (spans, barBeats, patternGroup);
    // Humanize (0.20.0): each idea loosened its own repeatable way (what HEAR plays is what USE IT adds).
    const auto amount = studio.getIdeasHumanize();
    humanizeKey.setButtonText (humanizeText (amount));
    if (amount > 0.0f && ! spans.empty())
    {
        const auto feel = studio.getIdeasFeel();
        for (const auto part : { P::bass, P::melody, P::chords, P::rhythm, P::lead, P::pattern })
        {
            auto& list = ideas[(size_t) part];
            for (size_t i = 0; i < list.size(); ++i)
            {
                pr7s::HumanizeOptions o;
                o.amount = amount;
                o.bpm = studio.getProject().bpmAt (spans.front().start);
                o.beatsPerBar = barBeats;
                o.originBeat = spans.front().start;
                const auto guitar = part == P::rhythm || part == P::lead;
                if (guitar)
                    o.layers = pr7s::guitarLayers();
                if (feel)
                    o.feelMs = amount * (part == P::rhythm || part == P::bass ? pr7s::HumanizeLimits::feelAheadMs
                                         : part == P::lead || part == P::melody ? pr7s::HumanizeLimits::feelBehindMs : 0.0);
                o.seed = (juce::uint32) (variation * 1009 + (int) part * 101 + (int) i + 1);
                list[i].notes = pr7s::humanize (std::move (list[i].notes), o);
            }
        }
    }
    makeCards();
}

void LinesPage::makeCards()
{
    cards.clear();
    for (const auto part : partsOf (tab))
        for (int i = 0; i < (int) getIdeas (part).size(); ++i)
            addAndMakeVisible (cards.add (new Card (*this, part, i)));
    pianoTab.setToggleState (tab == Tab::piano, juce::dontSendNotification);
    guitarTab.setToggleState (tab == Tab::guitar, juce::dontSendNotification);
    bassTab.setToggleState (tab == Tab::bass, juce::dontSendNotification);
    patternsTab.setToggleState (tab == Tab::patterns, juce::dontSendNotification);
    groupBox.setText (patternGroup, juce::dontSendNotification);
    groupBox.setVisible (tab == Tab::patterns);
    tuningBox.setText (getTuning().name, juce::dontSendNotification);
    tuningBox.setVisible (tab == Tab::guitar);
    tuningLabel.setVisible (tab == Tab::guitar);
    velocitiesKey.setVisible (tab == Tab::guitar);
    resized();
    repaint();
}

void LinesPage::play (pr7s::LineIdea::Part part, int index)
{
    const auto& list = getIdeas (part);
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    // Guitar ideas alone: chords voiced for a piano, played on a guitar library, would blur them.
    // (A pattern is the chords itself: alone too.)
    const auto alone = part == pr7s::LineIdea::Part::rhythm || part == pr7s::LineIdea::Part::lead || part == pr7s::LineIdea::Part::pattern;
    studio.previewLine (&list[(size_t) index], spans, ! alone);
    playing = std::pair { part, index };
    playingChords = false;
    for (auto* card : cards)
        card->refresh();
}

void LinesPage::playChords()
{
    studio.previewLine (nullptr, spans, true);
    playing.reset();
    playingChords = true;
    for (auto* card : cards)
        card->refresh();
}

void LinesPage::stop()
{
    studio.stopPreview();
    playing.reset();
    playingChords = false;
    for (auto* card : cards)
        card->refresh();
}

int LinesPage::use (pr7s::LineIdea::Part part, int index, int ontoTrackId)
{
    const auto& list = getIdeas (part);
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return 0;
    const auto trackId = studio.addLinePart (list[(size_t) index], spans, ontoTrackId);
    for (auto* card : cards)
        if (card->part == part && card->index == index)
            if (const auto* track = studio.getProject().findTrack (trackId))
            {
                card->added = track->name;
                card->repaint();
            }
    return trackId;
}

// USE IT: onto the selected track when it can take MIDI (the owner's guitar VST is there), else a new one.
void LinesPage::showUseMenu (pr7s::LineIdea::Part part, int index, Card& card)
{
    const auto selected = studio.getSelectedTrack();
    if (! studio.canTakeMidi (selected))
    {
        use (part, index, 0);
        return;
    }
    juce::PopupMenu menu;
    menu.addItem (1, "Onto " + studio.getProject().findTrack (selected)->name + " (the selected track)");
    menu.addItem (2, "Onto a new track (" + pr7s::Studio::linePartName (part) + ")");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (card.useIt),
                        [safe = juce::Component::SafePointer<LinesPage> (this), part, index, selected] (int chosen)
                        {
                            if (safe != nullptr && chosen != 0)
                                safe->use (part, index, chosen == 1 ? selected : 0);
                        });
}

void LinesPage::moreIdeas()
{
    stop();
    ++variation;
    rebuild();
}

void LinesPage::timerCallback()
{
    // The playing card's playhead; and the card unlit once the preview has played out.
    if ((playing || playingChords) && ! studio.isPreviewing())
        stop();
    if (playing)
        for (auto* card : cards)
            if (card->isPlaying())
                card->repaint();
}

void LinesPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto head = getLocalBounds().removeFromTop (headerHeight).reduced (gap, 8);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (20.0f));
    g.drawText ("IDEAS", head.removeFromTop (26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (interFont (12.5f));
    juce::String what;
    if (spans.empty())
        what = "No chords: add some to the chord track first";
    else
    {
        const auto bar = [this] (double beat) { return juce::String ((int) std::floor (beat / barBeats + 1.0e-9) + 1); };
        juce::StringArray names;
        for (const auto& s : spans)
            names.add (s.chord.name (s.key));
        what = juce::String ((int) spans.size()) + " chords, bars " + bar (spans.front().start) + " to " + bar (spans.back().end - 1.0e-6)
             + ", in " + ChordUi::spokenKey (spans.front().key) + ":  " + names.joinIntoString (" ");
    }
    g.drawText (what, head.withTrimmedRight (480), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    const auto parts = partsOf (tab);
    for (size_t i = 0; i < parts.size() && i < columnTitles.size(); ++i)
    {
        g.setColour (colourOf (parts[i]));
        g.fillRoundedRectangle (columnTitles[i].removeFromLeft (4).withSizeKeepingCentre (4, 14).toFloat(), 1.5f);
        g.setColour (ParagonColours::text);
        g.drawText (titleOf (parts[i]), columnTitles[i].withTrimmedLeft (10), juce::Justification::centredLeft, false);
    }
    if (parts.size() == 2)
    {
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (getWidth() / 2, headerHeight + tabsHeight, 1, getHeight() - headerHeight - tabsHeight - gap);
    }
}

void LinesPage::resized()
{
    auto area = getLocalBounds().reduced (gap, 0);
    auto head = area.removeFromTop (headerHeight).reduced (0, 16);
    stopKey.setBounds (head.removeFromRight (70));
    head.removeFromRight (6);
    more.setBounds (head.removeFromRight (110));
    head.removeFromRight (6);
    hearChords.setBounds (head.removeFromRight (110));
    head.removeFromRight (6);
    humanizeKey.setBounds (head.removeFromRight (150));

    auto tabs = area.removeFromTop (tabsHeight).withTrimmedBottom (6);
    for (auto* key : { &pianoTab, &guitarTab, &bassTab, &patternsTab })
    {
        key->setBounds (tabs.removeFromLeft (100));
        tabs.removeFromLeft (6);
    }
    groupBox.setBounds (tabs.withLeft (tabs.getRight() - 240));   // (PATTERNS only: where the guitar tab keeps its keys)
    velocitiesKey.setBounds (tabs.removeFromRight (110));
    tabs.removeFromRight (8);
    tuningBox.setBounds (tabs.removeFromRight (220));
    tabs.removeFromRight (8);
    tuningLabel.setBounds (tabs.removeFromRight (70));

    area.removeFromBottom (gap);
    const auto parts = partsOf (tab);
    columnTitles.clear();
    // Two columns of three cards a side; one part alone: three across, two down.
    const auto layout = [this] (juce::Rectangle<int> column, pr7s::LineIdea::Part part, int across)
    {
        const auto down = (6 + across - 1) / across;
        const auto w = (column.getWidth() - (across - 1) * gap) / across, h = (column.getHeight() - (down - 1) * gap) / down;
        auto i = 0;
        for (auto* card : cards)
            if (card->part == part)
            {
                card->setBounds (column.getX() + (i % across) * (w + gap), column.getY() + (i / across) * (h + gap), w, h);
                ++i;
            }
    };
    if (parts.size() == 2)
    {
        auto left = area.removeFromLeft (area.getWidth() / 2 - gap);
        area.removeFromLeft (gap * 2);
        auto right = area;
        columnTitles.push_back (left.removeFromTop (columnTitleHeight));
        columnTitles.push_back (right.removeFromTop (columnTitleHeight));
        layout (left, parts[0], 2);
        layout (right, parts[1], 2);
    }
    else if (parts.size() == 1)
    {
        columnTitles.push_back (area.removeFromTop (columnTitleHeight));
        layout (area, parts[0], 3);
    }
}

//==============================================================================
LinesWindow::LinesWindow (pr7s::Studio& studio, std::vector<pr7s::ChordSpan> spans)
    : juce::DocumentWindow ("Paragon PR7-S: Ideas", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new LinesPage (studio, std::move (spans));
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (900, 600, 3840, 2160);
}

void LinesWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
