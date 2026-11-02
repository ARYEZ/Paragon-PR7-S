#include "ui/LinesPage.h"

#include "ui/ChordUi.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }
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
        useIt.onClick = [this] { page.use (part, index); added = true; repaint(); };
        useIt.setTooltip ("Onto a new track: a MIDI part under the chords (one undo step), then edit it in the MIDI editor");
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
        if (added)
        {
            g.setColour (ParagonColours::accent);
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            g.drawText ("ADDED ON A NEW TRACK", bounds.reduced (10.0f, 8.0f).removeFromTop (20.0f), juce::Justification::centredRight, false);
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
        const auto colour = part == pr7s::LineIdea::Part::bass ? pr7s::FamilyColours::orange : pr7s::FamilyColours::pink;
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
    bool added = false;
};

//==============================================================================
LinesPage::LinesPage (pr7s::Studio& s, std::vector<pr7s::ChordSpan> chords) : studio (s)
{
    for (auto* key : { &hearChords, &more, &stopKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key->setWantsKeyboardFocus (false);
        addAndMakeVisible (*key);
    }
    hearChords.setTooltip ("The chords alone, at the song's tempo");
    more.setTooltip ("Six new bass ideas and six new melodies (the same styles, other choices)");
    hearChords.onClick = [this] { playChords(); };
    more.onClick = [this] { moreIdeas(); };
    stopKey.onClick = [this] { stop(); };
    setSize (1240, 720);
    setChords (std::move (chords));
    startTimerHz (30);
}

LinesPage::~LinesPage()
{
    studio.stopPreview();
}

void LinesPage::setChords (std::vector<pr7s::ChordSpan> chords)
{
    stop();
    spans = std::move (chords);
    barBeats = studio.getProject().beatsPerBar();
    rebuild();
}

void LinesPage::rebuild()
{
    bass = pr7s::bassIdeas (spans, barBeats, variation);
    melody = pr7s::melodyIdeas (spans, barBeats, variation);
    cards.clear();
    for (const auto part : { pr7s::LineIdea::Part::bass, pr7s::LineIdea::Part::melody })
        for (int i = 0; i < (int) getIdeas (part).size(); ++i)
            addAndMakeVisible (cards.add (new Card (*this, part, i)));
    resized();
    repaint();
}

void LinesPage::play (pr7s::LineIdea::Part part, int index)
{
    const auto& ideas = getIdeas (part);
    if (! juce::isPositiveAndBelow (index, (int) ideas.size()))
        return;
    studio.previewLine (&ideas[(size_t) index], spans, true);
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

int LinesPage::use (pr7s::LineIdea::Part part, int index)
{
    const auto& ideas = getIdeas (part);
    if (! juce::isPositiveAndBelow (index, (int) ideas.size()))
        return 0;
    return studio.addLinePart (ideas[(size_t) index], spans);
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
    g.drawText ("BASS & MELODY", head.removeFromTop (26), juce::Justification::centredLeft, false);
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
    g.drawText (what, head, juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText ("BASS", bassTitle, juce::Justification::centredLeft, false);
    g.drawText ("MELODY", melodyTitle, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (getWidth() / 2, headerHeight, 1, getHeight() - headerHeight - gap);
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
    area.removeFromBottom (gap);
    auto left = area.removeFromLeft (area.getWidth() / 2 - gap);
    area.removeFromLeft (gap * 2);
    auto right = area;
    bassTitle = left.removeFromTop (columnTitleHeight);
    melodyTitle = right.removeFromTop (columnTitleHeight);
    // Two columns of three cards a side.
    const auto layout = [this] (juce::Rectangle<int> column, pr7s::LineIdea::Part part)
    {
        const auto w = (column.getWidth() - gap) / 2, h = (column.getHeight() - 2 * gap) / 3;
        auto i = 0;
        for (auto* card : cards)
            if (card->part == part)
            {
                card->setBounds (column.getX() + (i % 2) * (w + gap), column.getY() + (i / 2) * (h + gap), w, h);
                ++i;
            }
    };
    layout (left, pr7s::LineIdea::Part::bass);
    layout (right, pr7s::LineIdea::Part::melody);
}

//==============================================================================
LinesWindow::LinesWindow (pr7s::Studio& studio, std::vector<pr7s::ChordSpan> spans)
    : juce::DocumentWindow ("Paragon PR7-S: Bass & Melody", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new LinesPage (studio, std::move (spans));
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (900, 560, 3840, 2160);
}

void LinesWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}
