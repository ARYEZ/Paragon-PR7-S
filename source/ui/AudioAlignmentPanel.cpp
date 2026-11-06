#include "AudioAlignmentPanel.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pageColour { 0xff0d0a0e };
    const juce::Colour viewColour { 0xff141016 };
    const juce::Colour pearl { 0xfff4eeea };
    using pr7s::FamilyColours::red;
    using pr7s::FamilyColours::pink;
    using pr7s::FamilyColours::orange;
}

//==============================================================================
// The waveforms, the overlap, sync points and protected ranges; the mouse makes and removes them.
class AudioAlignmentPanel::View final : public juce::Component
{
public:
    explicit View (AudioAlignmentPanel& p) : panel (p) {}

    juce::Rectangle<float> referenceLane() const { return getLocalBounds().toFloat().reduced (8.0f, 6.0f).removeFromTop ((getHeight() - 12) * 0.5f).reduced (0.0f, 4.0f); }
    juce::Rectangle<float> targetLane() const
    {
        auto r = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
        return r.removeFromBottom (r.getHeight() * 0.5f).reduced (0.0f, 4.0f);
    }

    // The sounds read again (what the reference's and the shown target's tracks play now).
    void refreshSounds()
    {
        referencePeaks.clear();
        targetPeaks.clear();
        const auto span = panel.shownSpan();
        const auto width = juce::jmax (1, getWidth() - 16);
        if (! span || getWidth() <= 16)
            return repaint();
        const auto tempo = panel.studio.getProject().tempoMap();
        const auto from = tempo.secondsAt (span->first), to = tempo.secondsAt (span->second);
        const auto peaksOf = [&] (int trackId)
        {
            std::vector<float> peaks ((size_t) width, 0.0f);
            double rate = 0.0;
            const auto sound = panel.studio.heardMono (trackId, from, to, rate);
            if (rate <= 0.0 || sound.empty())
                return peaks;
            // A pixel's samples by the tempo map (beats are even across the view).
            for (int x = 0; x < width; ++x)
            {
                const auto a = (int) ((tempo.secondsAt (span->first + (span->second - span->first) * x / width) - from) * rate);
                const auto b = (int) ((tempo.secondsAt (span->first + (span->second - span->first) * (x + 1) / width) - from) * rate);
                auto peak = 0.0f;
                for (int s = juce::jmax (0, a); s < juce::jmin ((int) sound.size(), juce::jmax (a + 1, b)); ++s)
                    peak = juce::jmax (peak, std::abs (sound[(size_t) s]));
                peaks[(size_t) x] = peak;
            }
            const auto loudest = *std::max_element (peaks.begin(), peaks.end());
            if (loudest > 0.0f)
                for (auto& v : peaks)
                    v /= loudest;
            return peaks;
        };
        if (panel.reference)
            referencePeaks = peaksOf (panel.reference->trackId);
        if (const auto* target = shownTarget())
            if (const auto found = panel.studio.findPart (target->partId))
                targetPeaks = peaksOf (found->first);
        repaint();
    }

    const pr7s::Studio::AlignTarget* shownTarget() const
    {
        return juce::isPositiveAndBelow (panel.shown, (int) panel.targets.size()) ? &panel.targets[(size_t) panel.shown] : nullptr;
    }

    void resized() override { refreshSounds(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (viewColour);
        g.setColour (ParagonColours::panelEdge);
        g.drawRect (getLocalBounds());
        const auto refLane = referenceLane(), tgtLane = targetLane();
        g.drawHorizontalLine ((int) ((refLane.getBottom() + tgtLane.getY()) * 0.5f), 8.0f, (float) getWidth() - 8.0f);
        const auto span = panel.shownSpan();
        g.setFont (ParagonLookAndFeel::textFont (14.0f));
        // (0.57.6) An empty lane says what to do.
        g.setColour (ParagonColours::textDim);
        if (! panel.reference)
            g.drawText ("SELECT THE AUDIO EVENT TO ALIGN TO IN THE ARRANGEMENT, THEN CLICK + BESIDE REFERENCE", refLane, juce::Justification::centred, true);
        if (panel.targets.empty())
            g.drawText ("SELECT THE EVENTS TO ALIGN IN THE ARRANGEMENT, THEN CLICK + BESIDE TARGET", tgtLane, juce::Justification::centred, true);
        if (! span)
            return;
        // Bars: faint lines, numbered.
        const auto& project = panel.studio.getProject();
        const auto perBar = juce::jmax (1.0, project.beatsPerBar());
        g.setFont (ParagonLookAndFeel::textFont (12.0f));
        for (auto bar = std::ceil (span->first / perBar) * perBar; bar <= span->second; bar += perBar)
        {
            const auto x = panel.xForBeat (bar);
            g.setColour (ParagonColours::panelEdge);
            g.drawVerticalLine ((int) x, 4.0f, (float) getHeight() - 4.0f);
            g.setColour (ParagonColours::textDim);
            g.drawText (juce::String ((int) std::lround (bar / perBar) + 1), juce::Rectangle<float> (x + 3.0f, 2.0f, 40.0f, 14.0f),
                        juce::Justification::centredLeft, false);
        }
        // The overlap with the shown target.
        const auto* target = shownTarget();
        if (target != nullptr && panel.reference)
            if (const auto found = panel.studio.findPart (target->partId))
            {
                const auto& part = panel.studio.getProject().findTrack (found->first)->parts[(size_t) found->second];
                auto a = juce::jmax (part.start, panel.reference->start), b = juce::jmin (part.end(), panel.reference->end);
                if (target->start >= 0.0) a = juce::jmax (a, target->start);
                if (target->end >= 0.0) b = juce::jmin (b, target->end);
                if (b > a)
                {
                    const auto xa = panel.xForBeat (a), xb = panel.xForBeat (b);
                    g.setColour (orange.withAlpha (0.10f));
                    g.fillRect (juce::Rectangle<float> (xa, refLane.getY(), xb - xa, tgtLane.getBottom() - refLane.getY()));
                    g.setColour (orange.withAlpha (0.6f));
                    g.drawVerticalLine ((int) xa, refLane.getY(), tgtLane.getBottom());
                    g.drawVerticalLine ((int) xb, refLane.getY(), tgtLane.getBottom());
                }
            }
        // Protected ranges on the target.
        for (const auto& r : panel.protectedRanges)
        {
            const auto xa = panel.xForBeat (r.getStart()), xb = panel.xForBeat (r.getEnd());
            const juce::Rectangle<float> area (xa, tgtLane.getY(), xb - xa, tgtLane.getHeight());
            g.setColour (pearl.withAlpha (0.08f));
            g.fillRect (area);
            g.saveState();
            g.reduceClipRegion (area.toNearestInt());
            g.setColour (pearl.withAlpha (0.22f));
            for (auto x = xa - tgtLane.getHeight(); x < xb; x += 10.0f)
                g.drawLine (x, tgtLane.getBottom(), x + tgtLane.getHeight(), tgtLane.getY(), 1.0f);
            g.restoreState();
            g.setColour (pearl.withAlpha (0.7f));
            g.drawText ("PROTECTED", area.withHeight (16.0f).translated (4.0f, 2.0f), juce::Justification::centredLeft, false);
        }
        // The waveforms.
        const auto drawPeaks = [&g] (const std::vector<float>& peaks, juce::Rectangle<float> lane, juce::Colour colour)
        {
            g.setColour (colour);
            const auto mid = lane.getCentreY(), half = lane.getHeight() * 0.45f;
            for (size_t x = 0; x < peaks.size(); ++x)
                if (peaks[x] > 0.002f)
                    g.drawVerticalLine ((int) (lane.getX() + (float) x), mid - peaks[x] * half, mid + peaks[x] * half + 1.0f);
        };
        drawPeaks (referencePeaks, refLane, orange);
        drawPeaks (targetPeaks, tgtLane, pink);

        // Sync points (red: couldn't be met).
        for (int s = 0; s < (int) panel.syncs.size(); ++s)
        {
            const auto& sync = panel.syncs[(size_t) s];
            if (target != nullptr && sync.partId != target->partId)
                continue;
            const auto unmet = panel.reported && std::find (panel.report.unmetSyncs.begin(), panel.report.unmetSyncs.end(), s) != panel.report.unmetSyncs.end();
            const auto colour = unmet ? red : pearl;
            const juce::Point<float> a (panel.xForBeat (sync.referenceBeat), refLane.getCentreY()), b (panel.xForBeat (sync.targetBeat), tgtLane.getCentreY());
            g.setColour (colour.withAlpha (0.85f));
            g.drawLine ({ a, b }, 2.0f);
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (a));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (b));
        }
        // A drag in progress.
        if (dragging == Drag::sync)
        {
            g.setColour (pearl.withAlpha (0.6f));
            g.drawLine ({ downAt, dragAt }, 1.5f);
        }
        else if (dragging == Drag::protect)
        {
            const auto xa = juce::jmin (downAt.x, dragAt.x), xb = juce::jmax (downAt.x, dragAt.x);
            g.setColour (pearl.withAlpha (0.15f));
            g.fillRect (juce::Rectangle<float> (xa, tgtLane.getY(), xb - xa, tgtLane.getHeight()));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = Drag::none;
        if (! panel.shownSpan())
            return;
        if (e.mods.isShiftDown())
        {
            panel.removeAt (e.position);
            return;
        }
        downAt = dragAt = e.position;
        if (e.mods.isCommandDown() && targetLane().contains (e.position))
            dragging = Drag::protect;
        else if (referenceLane().contains (e.position) && shownTarget() != nullptr)
            dragging = Drag::sync;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == Drag::none)
            return;
        dragAt = e.position;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto was = dragging;
        dragging = Drag::none;
        if (was == Drag::sync && targetLane().contains (e.position))
            panel.addSyncPoint (panel.beatForX (downAt.x), panel.beatForX (e.position.x));
        else if (was == Drag::protect && std::abs (e.position.x - downAt.x) > 3.0f)
            panel.addProtectedRange ({ panel.beatForX (juce::jmin (downAt.x, e.position.x)), panel.beatForX (juce::jmax (downAt.x, e.position.x)) });
        repaint();
    }

private:
    AudioAlignmentPanel& panel;
    std::vector<float> referencePeaks, targetPeaks;
    enum class Drag { none, sync, protect } dragging = Drag::none;
    juce::Point<float> downAt, dragAt;
};

//==============================================================================
juce::TextButton& AudioAlignmentPanel::makeKey (const juce::String& text, std::function<void()> onClick, const juce::String& tip)
{
    auto key = std::make_unique<juce::TextButton> (text);
    key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    key->setWantsKeyboardFocus (false);
    key->onClick = [this, action = std::move (onClick)] { action(); refresh(); };
    key->setTooltip (tip);
    addAndMakeVisible (*key);
    keys.push_back (std::move (key));
    return *keys.back();
}

AudioAlignmentPanel::AudioAlignmentPanel (pr7s::Studio& s, std::function<std::vector<int>()> parts,
                                          std::function<std::optional<pr7s::Studio::Range>()> range)
    : studio (s), selectedParts (std::move (parts)), rangeOf (std::move (range))
{
    using Settings = pr7s::AlignSettings;
    referenceKey = &makeKey ("+", [this] { setReferenceFromSelection(); },
                             "Add Selection as Alignment Reference: the audio event selected in the arrangement (or the range, "
                             "on its first audio track) is what the others are aligned to");
    targetsKey = &makeKey ("+", [this] { addTargetsFromSelection(); },
                           "Add Selection as Alignment Target: the audio events selected in the arrangement (or the range's "
                           "events on other tracks) are aligned to the reference");
    clearReferenceKey = &makeKey ("CLEAR", [this]
    {
        reference.reset();
        syncs.clear();
        reported = false;
    }, "No reference");
    clearKey = &makeKey ("CLEAR", [this] { clearTargets(); }, "No targets");
    const auto change = [this] (std::function<void (Settings&)> edit)
    {
        auto settings = studio.getAlignSettings();
        edit (settings);
        setSettings (settings);
    };
    wordsKey = &makeKey ("MATCH WORDS", [change] { change ([] (Settings& a) { a.matchWords = ! a.matchWords; }); },
                         "Match the sound's shape (syllables, words) more than its loudness: best when the words are the same");
    shiftKey = &makeKey ("PREFER TIME SHIFTING", [change] { change ([] (Settings& a) { a.preferShifting = ! a.preferShifting; }); },
                         "Move the audio rather than stretch it whenever a move is close enough (several microphones on one performance)");
    bothKey = &makeKey ("ONLY WHERE BOTH PLAY", [change] { change ([] (Settings& a) { a.onlyWhereBothPlay = ! a.onlyWhereBothPlay; }); },
                        "Match only where both have sound; where either is silent the target keeps its pace");
    const char* flexNames[] = { "LOW", "NORMAL", "HIGH" };
    const char* flexTips[] = { "Stretches gently: the most natural sound, the loosest match",
                               "A balance", "Follows every change of pace: the tightest match, the most stretching" };
    for (int f = 0; f < 3; ++f)
        flexKeys[(size_t) f] = &makeKey (flexNames[f], [change, f] { change ([f] (Settings& a) { a.flexibility = (Settings::Flexibility) f; }); }, flexTips[f]);
    clearMarksKey = &makeKey ("CLEAR SYNC AND PROTECTED", [this]
    {
        syncs.clear();
        protectedRanges.clear();
        reported = false;
        view->repaint();
    });
    alignKey = &makeKey ("ALIGN AUDIO", [this] { alignNow(); }, "Align the targets to the reference (one undo step)");
    alignKey->getProperties().set ("lit", true);

    for (auto* slider : { &precision, &maxShift })
    {
        slider->setSliderStyle (juce::Slider::LinearBar);
        slider->setColour (juce::Slider::trackColourId, ParagonColours::accent.withAlpha (0.6f));
        slider->setColour (juce::Slider::textBoxTextColourId, ParagonColours::text);
        addAndMakeVisible (*slider);
    }
    precision.setRange (0.0, 100.0, 1.0);
    precision.setTextValueSuffix (" %");
    precision.setTooltip ("How closely the target follows: 100 %: each moment within 2 ms; lower keeps a looser, more natural feel");
    precision.onValueChange = [this, change] { change ([this] (Settings& a) { a.precision = precision.getValue() / 100.0; }); };
    maxShift.setRange (Settings::leastShift * 1000.0, Settings::mostShift * 1000.0, 1.0);
    maxShift.setTextValueSuffix (" ms");
    maxShift.setTooltip ("How far any moment of the target may move");
    maxShift.onValueChange = [this, change] { change ([this] (Settings& a) { a.maxShiftSeconds = maxShift.getValue() / 1000.0; }); };

    for (auto* label : { &referenceText, &targetText, &precisionLabel, &maxShiftLabel, &flexLabel, &reportText, &hint })
    {
        label->setFont (ParagonLookAndFeel::textFont (14.0f));
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        label->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*label);
    }
    precisionLabel.setText ("PRECISION", juce::dontSendNotification);
    maxShiftLabel.setText ("MAX SHIFT", juce::dontSendNotification);
    flexLabel.setText ("FLEXIBILITY", juce::dontSendNotification);
    referenceText.setColour (juce::Label::textColourId, orange);
    targetText.setColour (juce::Label::textColourId, pink);
    for (auto* label : { &referenceText, &targetText })
    {
        label->setJustificationType (juce::Justification::topLeft);
        label->setMinimumHorizontalScale (0.7f);
    }
    reportText.setColour (juce::Label::textColourId, ParagonColours::text);
    reportText.setJustificationType (juce::Justification::topLeft);
    hint.setText ("DRAG FROM THE REFERENCE DOWN TO THE TARGET: A SYNC POINT.   CTRL-DRAG ON THE TARGET: A PROTECTED RANGE.   SHIFT-CLICK: REMOVE.",
                  juce::dontSendNotification);
    targetBox.onChange = [this] { showTarget (targetBox.getSelectedItemIndex()); };
    addAndMakeVisible (targetBox);

    view = std::make_unique<View> (*this);
    addAndMakeVisible (*view);
    studio.addChangeListener (this);
    setSize (preferredWidth, preferredHeight);
    refresh();
}

AudioAlignmentPanel::~AudioAlignmentPanel()
{
    studio.removeChangeListener (this);
}

void AudioAlignmentPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Events gone (removed, undone): out of the panel.
    targets.erase (std::remove_if (targets.begin(), targets.end(), [this] (const auto& t) { return ! studio.findPart (t.partId); }), targets.end());
    if (reference && studio.getProject().findTrack (reference->trackId) == nullptr)
        reference.reset();
    refresh();
}

juce::TextButton* AudioAlignmentPanel::keyNamed (const juce::String& text)
{
    for (auto& key : keys)
        if (key->getButtonText() == text)
            return key.get();
    return nullptr;
}

void AudioAlignmentPanel::setSettings (const pr7s::AlignSettings& settings)
{
    studio.setAlignSettings (settings);
    refresh();
}

juce::String AudioAlignmentPanel::describe (int trackId, double start, double end) const
{
    const auto* track = studio.getProject().findTrack (trackId);
    const auto& project = studio.getProject();
    return (track != nullptr ? track->name : juce::String()) + ":  " + pr7s::formatBarsBeats (start, project) + "  TO  " + pr7s::formatBarsBeats (end, project);
}

std::optional<std::pair<double, double>> AudioAlignmentPanel::shownSpan() const
{
    std::optional<std::pair<double, double>> span;
    if (reference)
        span = std::pair { reference->start, reference->end };
    else if (! targets.empty())
        if (const auto found = studio.findPart (targets[(size_t) juce::jlimit (0, (int) targets.size() - 1, shown)].partId))
        {
            const auto& part = studio.getProject().findTrack (found->first)->parts[(size_t) found->second];
            span = std::pair { part.start, part.end() };
        }
    if (span)
    {
        const auto margin = juce::jmax (0.5, (span->second - span->first) * 0.04);
        span->first = juce::jmax (0.0, span->first - margin);
        span->second += margin;
    }
    return span;
}

float AudioAlignmentPanel::xForBeat (double beat) const
{
    const auto span = shownSpan();
    if (! span || view == nullptr)
        return 0.0f;
    const auto width = (double) juce::jmax (1, view->getWidth() - 16);
    return (float) (8.0 + (beat - span->first) / (span->second - span->first) * width);
}

double AudioAlignmentPanel::beatForX (float x) const
{
    const auto span = shownSpan();
    if (! span || view == nullptr)
        return 0.0;
    const auto width = (double) juce::jmax (1, view->getWidth() - 16);
    return span->first + (x - 8.0) / width * (span->second - span->first);
}

bool AudioAlignmentPanel::setReferenceFromSelection()
{
    const auto& project = studio.getProject();
    if (const auto range = rangeOf ? rangeOf() : std::nullopt; range && ! range->isEmpty())
        for (const auto id : range->trackIds)
            if (const auto* track = project.findTrack (id); track != nullptr && track->isAudio())
            {
                reference = pr7s::Studio::AlignReference { id, range->start, range->end };
                reported = false;
                refresh();
                return true;
            }
    for (const auto id : selectedParts ? selectedParts() : std::vector<int> {})
        if (const auto found = studio.findPart (id))
        {
            const auto& part = project.findTrack (found->first)->parts[(size_t) found->second];
            if (! part.isAudio())
                continue;
            reference = pr7s::Studio::AlignReference { found->first, part.start, part.end() };
            // (Not its own target.)
            targets.erase (std::remove_if (targets.begin(), targets.end(), [&] (const auto& t)
            {
                const auto f = studio.findPart (t.partId);
                return f && f->first == found->first;
            }), targets.end());
            reported = false;
            refresh();
            return true;
        }
    return false;
}

int AudioAlignmentPanel::addTargetsFromSelection()
{
    const auto& project = studio.getProject();
    auto added = 0;
    const auto add = [&] (int partId, double start, double end)
    {
        const auto found = studio.findPart (partId);
        if (! found || (reference && found->first == reference->trackId))
            return;
        if (! project.findTrack (found->first)->parts[(size_t) found->second].isAudio())
            return;
        if (std::any_of (targets.begin(), targets.end(), [partId] (const auto& t) { return t.partId == partId; }))
            return;
        targets.push_back ({ partId, start, end });
        ++added;
    };
    if (const auto range = rangeOf ? rangeOf() : std::nullopt; range && ! range->isEmpty())
    {
        for (const auto id : range->trackIds)
            if (const auto* track = project.findTrack (id); track != nullptr && track->isAudio())
                for (const auto& part : track->parts)
                    if (part.end() > range->start && part.start < range->end)
                        add (part.id, range->start, range->end);
    }
    else
        for (const auto id : selectedParts ? selectedParts() : std::vector<int> {})
            add (id, -1.0, -1.0);
    if (added > 0)
    {
        shown = (int) targets.size() - added;
        reported = false;
    }
    refresh();
    return added;
}

void AudioAlignmentPanel::clearTargets()
{
    targets.clear();
    syncs.clear();
    shown = 0;
    reported = false;
    refresh();
}

void AudioAlignmentPanel::showTarget (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) targets.size()) || index == shown)
        return;
    shown = index;
    refresh();
}

void AudioAlignmentPanel::addSyncPoint (double referenceBeat, double targetBeat)
{
    if (! juce::isPositiveAndBelow (shown, (int) targets.size()))
        return;
    syncs.push_back ({ targets[(size_t) shown].partId, referenceBeat, targetBeat });
    reported = false;
    view->repaint();
}

void AudioAlignmentPanel::addProtectedRange (juce::Range<double> beats)
{
    if (beats.getLength() <= 0.0)
        return;
    protectedRanges.push_back (beats);
    view->repaint();
}

void AudioAlignmentPanel::removeAt (juce::Point<float> at)
{
    const auto targetId = juce::isPositiveAndBelow (shown, (int) targets.size()) ? targets[(size_t) shown].partId : 0;
    for (int s = (int) syncs.size() - 1; s >= 0; --s)
    {
        const auto& sync = syncs[(size_t) s];
        if (sync.partId != targetId)
            continue;
        const juce::Point<float> a (xForBeat (sync.referenceBeat), view->referenceLane().getCentreY()),
                                 b (xForBeat (sync.targetBeat), view->targetLane().getCentreY());
        juce::Point<float> nearest;
        juce::Line<float> (a, b).getDistanceFromPoint (at, nearest);
        if (nearest.getDistanceFrom (at) <= 6.0f)
        {
            syncs.erase (syncs.begin() + s);
            reported = false;
            view->repaint();
            return;
        }
    }
    if (view->targetLane().contains (at))
    {
        const auto beat = beatForX (at.x);
        for (int r = (int) protectedRanges.size() - 1; r >= 0; --r)
            if (protectedRanges[(size_t) r].contains (beat))
            {
                protectedRanges.erase (protectedRanges.begin() + r);
                view->repaint();
                return;
            }
    }
}

bool AudioAlignmentPanel::alignNow()
{
    report = {};
    reported = true;
    if (! reference || targets.empty())
    {
        report.notes.add (! reference ? "Set a reference first." : "Add a target first.");
        refresh();
        return false;
    }
    const auto before = targets;
    const auto ok = studio.alignAudio (*reference, targets, studio.getAlignSettings(), syncs, protectedRanges, report);
    // The aligned pieces are the targets now (a target split at the overlap kept its id on the piece before it).
    for (size_t t = 0; t < targets.size() && t < report.alignedParts.size(); ++t)
        if (report.alignedParts[t] != 0)
        {
            for (auto& sync : syncs)
                if (sync.partId == before[t].partId)
                    sync.partId = report.alignedParts[t];
            targets[t].partId = report.alignedParts[t];
        }
    refresh();
    return ok;
}

juce::String AudioAlignmentPanel::getReportText() const
{
    if (! reported)
        return {};
    juce::String text;
    if (report.aligned > 0)
    {
        text << "ALIGNED " << report.aligned << (report.aligned == 1 ? " EVENT" : " EVENTS") << ": "
             << report.stretched << " STRETCHED, " << report.moved << " MOVED.   APART ON AVERAGE: "
             << juce::roundToInt (report.before * 1000.0) << " MS BEFORE, " << juce::roundToInt (report.after * 1000.0) << " MS AFTER (MEASURED EVERY 10 MS).";
        if (! report.unmetSyncs.empty())
            text << "   " << (int) report.unmetSyncs.size() << (report.unmetSyncs.size() == 1 ? " SYNC POINT" : " SYNC POINTS") << " COULDN'T BE MET (RED).";
    }
    for (const auto& note : report.notes)
        text << (text.isEmpty() ? "" : "\n") << note;
    return text;
}

void AudioAlignmentPanel::refresh()
{
    const auto settings = studio.getAlignSettings();
    wordsKey->setToggleState (settings.matchWords, juce::dontSendNotification);
    shiftKey->setToggleState (settings.preferShifting, juce::dontSendNotification);
    bothKey->setToggleState (settings.onlyWhereBothPlay, juce::dontSendNotification);
    for (int f = 0; f < 3; ++f)
        flexKeys[(size_t) f]->setToggleState ((int) settings.flexibility == f, juce::dontSendNotification);
    precision.setValue (settings.precision * 100.0, juce::dontSendNotification);
    maxShift.setValue (settings.maxShiftSeconds * 1000.0, juce::dontSendNotification);

    referenceText.setText (reference ? describe (reference->trackId, reference->start, reference->end) : juce::String ("NO REFERENCE"),
                           juce::dontSendNotification);
    targetBox.clear (juce::dontSendNotification);
    for (const auto& t : targets)
        if (const auto found = studio.findPart (t.partId))
        {
            const auto& part = studio.getProject().findTrack (found->first)->parts[(size_t) found->second];
            targetBox.addItem (describe (found->first, t.start >= 0.0 ? juce::jmax (t.start, part.start) : part.start,
                                         t.end >= 0.0 ? juce::jmin (t.end, part.end()) : part.end()), targetBox.getNumItems() + 1);
        }
    shown = juce::jlimit (0, juce::jmax (0, (int) targets.size() - 1), shown);
    targetBox.setSelectedItemIndex (shown, juce::dontSendNotification);
    targetBox.setEnabled (targets.size() > 1);
    // (0.57.6) One target: what it is; several: how many, and the box to choose the one shown.
    targetText.setText (targets.empty() ? juce::String ("NO TARGET")
                        : targets.size() == 1 ? targetBox.getItemText (0)
                        : juce::String ((int) targets.size()) + " TARGETS: THE ONE SHOWN",
                        juce::dontSendNotification);
    targetBox.setVisible (targets.size() > 1);
    clearReferenceKey->setEnabled (reference.has_value());
    clearKey->setEnabled (! targets.empty());
    alignKey->setEnabled (reference.has_value() && ! targets.empty());
    reportText.setText (getReportText(), juce::dontSendNotification);
    view->refreshSounds();
}

void AudioAlignmentPanel::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    // (0.57.6) The lanes' names, beside them.
    for (const auto& [area, name, colour] : { std::tuple { referenceHeader, "REFERENCE", orange }, std::tuple { targetHeader, "TARGET", pink } })
    {
        if (area.isEmpty())
            continue;
        g.setColour (viewColour);
        g.fillRect (area);
        g.setColour (colour);
        g.fillRect (area.withWidth (3));
        g.setColour (ParagonColours::panelEdge);
        g.drawRect (area);
        g.setFont (ParagonLookAndFeel::titleFont (20.0f));
        g.setColour (colour);
        g.drawText (name, area.reduced (14, 8).removeFromTop (26), juce::Justification::centredLeft, false);
    }
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    for (const auto& [text, area] : headings)
    {
        g.setColour (ParagonColours::accent);
        g.drawText (text, area, juce::Justification::centredLeft, false);
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (area.withY (area.getBottom() - 1).withHeight (1));
    }
}

void AudioAlignmentPanel::resized()
{
    headings.clear();
    auto r = getLocalBounds().reduced (14, 12);
    const auto heading = [&] (const juce::String& text)
    {
        headings.push_back ({ text, r.removeFromTop (20) });
        r.removeFromTop (6);
    };
    heading ("AUDIO ALIGNMENT SELECTION");
    juce::Rectangle<int> line;

    // The bottom: settings, ALIGN AUDIO and the report.
    auto bottom = r.removeFromBottom (190);
    auto alignRow = bottom.removeFromBottom (64);
    alignKey->setBounds (alignRow.removeFromLeft (200).withSizeKeepingCentre (200, 40).reduced (2));
    alignRow.removeFromLeft (14);
    reportText.setBounds (alignRow);
    bottom.removeFromBottom (8);
    headings.push_back ({ "SETTINGS", bottom.removeFromTop (20) });
    bottom.removeFromTop (6);
    line = bottom.removeFromTop (32);
    wordsKey->setBounds (line.removeFromLeft (140).reduced (2));
    shiftKey->setBounds (line.removeFromLeft (200).reduced (2));
    bothKey->setBounds (line.removeFromLeft (200).reduced (2));
    clearMarksKey->setBounds (line.removeFromRight (230).reduced (2));
    bottom.removeFromTop (8);
    line = bottom.removeFromTop (32);
    precisionLabel.setBounds (line.removeFromLeft (90));
    precision.setBounds (line.removeFromLeft (220).reduced (2, 4));
    line.removeFromLeft (24);
    flexLabel.setBounds (line.removeFromLeft (96));
    for (auto* key : flexKeys)
        key->setBounds (line.removeFromLeft (90).reduced (2));
    line.removeFromLeft (24);
    maxShiftLabel.setBounds (line.removeFromLeft (90));
    maxShift.setBounds (line.removeFromLeft (220).reduced (2, 4));

    hint.setBounds (r.removeFromBottom (22));
    // (0.57.6) Each lane's header at its left, level with the lane (the view's halves, as View lays them).
    auto headers = r.removeFromLeft (250);
    r.removeFromLeft (6);
    view->setBounds (r);
    headers.reduce (0, 6);
    referenceHeader = headers.removeFromTop (headers.getHeight() / 2).reduced (0, 4);
    targetHeader = headers.reduced (0, 4);
    const auto layoutHeader = [] (juce::Rectangle<int> area, juce::TextButton& plus, juce::TextButton& clear, juce::Component& under)
    {
        auto inside = area.reduced (14, 8);
        inside.removeFromTop (30);
        auto keysRow = inside.removeFromTop (34);
        plus.setBounds (keysRow.removeFromLeft (52).reduced (2));
        keysRow.removeFromLeft (6);
        clear.setBounds (keysRow.removeFromLeft (90).reduced (2));
        inside.removeFromTop (6);
        under.setBounds (inside.removeFromTop (juce::jmin (inside.getHeight(), 44)));
        return inside;
    };
    layoutHeader (referenceHeader, *referenceKey, *clearReferenceKey, referenceText);
    auto rest = layoutHeader (targetHeader, *targetsKey, *clearKey, targetText);
    targetBox.setBounds (rest.removeFromTop (juce::jmin (rest.getHeight(), 30)).reduced (0, 2));
}

//==============================================================================
AudioAlignmentWindow::AudioAlignmentWindow (pr7s::Studio& s, std::function<std::vector<int>()> parts,
                                            std::function<std::optional<pr7s::Studio::Range>()> range)
    : DocumentWindow ("Paragon PR7-S: audio alignment", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    panel = new AudioAlignmentPanel (s, std::move (parts), std::move (range));
    setContentOwned (panel, true);
    setResizable (true, false);
    setResizeLimits (AudioAlignmentPanel::preferredWidth, 520, 4000, 2000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void AudioAlignmentWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

juce::Component& AudioAlignmentPanel::getView() noexcept
{
    return *view;
}
