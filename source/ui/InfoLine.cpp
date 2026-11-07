#include "InfoLine.h"

#include <unordered_map>

#include "ui/ArrangeView.h"
#include "ui/KeyEditor.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/TempoEditor.h"

namespace
{
    const juce::Colour stripColour { 0xff0d0a0e };
    constexpr int fieldWidth = 150, typeWidth = 220;
    using PartField = pr7s::Studio::PartField;

    // (0.47.0) How the parts' values read and are typed.
    juce::String secondsText (double seconds) { return juce::String (seconds, 3) + " s"; }
    juce::String volumeText (double db)
    {
        if (db <= pr7s::Part::leastVolumeDb)
            return "-INF";
        return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB";
    }
    juce::String signedText (int v) { return v > 0 ? "+" + juce::String (v) : juce::String (v); }
    // A number with what may follow it ("250 ms", "1.5 s", "-3 dB", "+2"); none: not a number.
    std::optional<double> numberIn (juce::String text)
    {
        text = text.trim().toLowerCase();
        const auto digits = text.initialSectionContainingOnly ("+-0123456789.,");
        if (digits.isEmpty() || ! digits.containsAnyOf ("0123456789"))
            return std::nullopt;
        return digits.replaceCharacter (',', '.').getDoubleValue();
    }
    std::optional<double> parseSeconds (const juce::String& text)
    {
        const auto n = numberIn (text);
        if (! n)
            return std::nullopt;
        const auto rest = text.trim().toLowerCase().trimCharactersAtStart ("+-0123456789.,").trim();
        return rest.startsWith ("ms") ? *n / 1000.0 : *n;
    }
    std::optional<double> parseVolume (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        if (t.contains ("inf") || t == "off")
            return pr7s::Part::leastVolumeDb;
        return numberIn (text);
    }
}

InfoLine::InfoLine (pr7s::Studio& s, ArrangeView& a) : studio (s), arrange (a)
{
    setTooltip ("The info line: the selected automation points, else the selected parts and audio events (the first one's values). "
                "Double-click a value to type: all of them move by what the first one moves. Hold Ctrl as you press Enter to set them all to it. "
                "MUTE and TERMINATOR switch with a click. Right-click to hide");
    setOpaque (true);   // (0.57.1: paint fills it all)
    refresh();
    startTimerHz (10);
}

void InfoLine::timerCallback()
{
    refresh();
}

void InfoLine::refresh()
{
    // (0.57.1) Nothing changed (the song, the parts or points selected): nothing to do. It sorted the
    // selection ten times a second, searching the whole song for each part as it went.
    const auto& chosenPoints = arrange.getPointSelection();
    auto parts = arrange.getSelectedParts();
    if (studio.getChangeVersion() == seenChange && parts == seenParts && chosenPoints.busId == seenBus
        && chosenPoints.laneIndex == seenLane && chosenPoints.points == seenPoints && ! fieldsDirty
        && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())   // (a drag shows its working copy)
        return;
    seenChange = studio.getChangeVersion();
    seenParts = std::move (parts);
    seenBus = chosenPoints.busId;
    seenLane = chosenPoints.laneIndex;
    seenPoints = chosenPoints.points;
    fieldsDirty = false;
    fields.clear();
    count = 0;
    showingParts = false;
    const auto& selection = arrange.getPointSelection();
    const auto* lane = arrange.getSelectedPointsLane();
    if (lane != nullptr && ! selection.points.empty())
    {
        const auto& points = arrange.editPointsOf (*lane);
        const auto first = selection.points.front();
        if (juce::isPositiveAndBelow (first, (int) points.size()))
        {
            const auto& p = points[(size_t) first];
            count = (int) selection.points.size();
            fields.push_back ({ "TYPE", studio.getLaneName (selection.busId, *lane) + (arrange.isTrimming (*lane) ? " (TRIM)" : ""), {}, false });
            fields.push_back ({ "START", pr7s::formatBarsBeats (p.beat, studio.getProject()).replace ("  ", " "), {}, true });
            fields.push_back ({ "VALUE", arrange.formatPointValue (selection.busId, *lane, p.value).value_or ("?"), {}, true });
            fields.push_back ({ "TERMINATOR", p.terminator ? "YES" : "NO", {}, ! arrange.isTrimming (*lane) });
        }
    }
    if (fields.empty())
        refreshParts();
    juce::String key (count);
    for (const auto& f : fields)
        key << "|" << f.name << "=" << f.value;
    if (key != shownKey)
    {
        shownKey = key;
        resized();
        repaint();
    }
}

std::vector<int> InfoLine::partsInOrder() const
{
    // The earliest first (the manual: "the first item in the selection"), then by the tracks' order.
    auto ids = arrange.getSelectedParts();
    const auto shown = studio.getShownTracks();
    // (0.57.1) Each part's place found once, from one walk of the song (it was searched for twice per
    // comparison: with Ctrl+A on thousands of parts, millions of checks ten times a second).
    std::unordered_map<int, std::pair<double, int>> keys;
    for (const auto& track : studio.getProject().tracks)
    {
        const auto row = (int) (std::find (shown.begin(), shown.end(), track.id) - shown.begin());
        for (const auto& part : track.parts)
            keys[part.id] = { part.start, row };
    }
    ids.erase (std::remove_if (ids.begin(), ids.end(), [&] (int id) { return keys.find (id) == keys.end(); }), ids.end());
    std::stable_sort (ids.begin(), ids.end(), [&] (int a, int b) { return keys[a] < keys[b]; });
    return ids;
}

void InfoLine::refreshParts()
{
    const auto ids = partsInOrder();
    if (ids.empty())
        return;
    const auto where = studio.findPart (ids.front());
    const auto& project = studio.getProject();
    const auto* track = project.findTrack (where->first);
    const auto& part = track->parts[(size_t) where->second];
    showingParts = true;
    count = (int) ids.size();
    const auto editable = ! track->frozen;
    const auto position = [&] (double beat) { return pr7s::formatBarsBeats (beat, project).replace ("  ", " "); };
    if (part.isAudio())
        fields.push_back ({ "FILE", part.audioFile, {}, false });
    fields.push_back ({ "NAME", part.name.isNotEmpty() ? part.name : track->name, {}, editable });
    fields.push_back ({ "START", position (part.start), {}, editable });
    fields.push_back ({ "END", position (part.end()), {}, editable });
    fields.push_back ({ "LENGTH", KeyEditor::lengthText (part.length, project), {}, editable });
    if (part.isAudio())
    {
        fields.push_back ({ "OFFSET", part.isWarped() ? juce::String ("WARPED") : secondsText (part.audioOffset), {}, editable && ! part.isWarped() });
        fields.push_back ({ "FADE-IN", secondsText (part.fadeIn), {}, editable });
        fields.push_back ({ "FADE-OUT", secondsText (part.fadeOut), {}, editable });
        fields.push_back ({ "VOLUME", volumeText (part.volumeDb), {}, editable });
    }
    else
    {
        fields.push_back ({ "TRANSPOSE", signedText (part.transpose), {}, editable });
        fields.push_back ({ "VELOCITY", signedText (part.velocityOffset), {}, editable });
    }
    fields.push_back ({ "MUTE", part.muted ? "YES" : "NO", {}, editable });
}

void InfoLine::resized()
{
    auto x = 12;
    for (auto& f : fields)
    {
        auto w = f.name == "TYPE" ? typeWidth : fieldWidth;
        if (showingParts)
        {
            // (Each as wide as its label and value need: ten of them must fit beside the track list.)
            const auto valueWidth = juce::GlyphArrangement::getStringWidth (ParagonLookAndFeel::textFont (15.0f), f.value.toUpperCase());
            const auto labelWidth = juce::GlyphArrangement::getStringWidth (ParagonLookAndFeel::textFont (12.0f), f.name);
            w = juce::jlimit (64, f.name == "FILE" || f.name == "NAME" ? 260 : 170, (int) std::ceil (juce::jmax (valueWidth, labelWidth)) + 22);
        }
        f.area = { x, 0, w, getHeight() };
        x += w;
    }
    if (editor != nullptr)
        for (const auto& f : fields)
            if (f.name == editing)
                editor->setBounds (f.area.withTrimmedTop (getHeight() / 2 - 2).reduced (6, 2));
}

juce::String InfoLine::getField (const juce::String& name) const
{
    for (const auto& f : fields)
        if (f.name == name)
            return f.value;
    return {};
}

juce::Rectangle<int> InfoLine::getFieldArea (const juce::String& name) const
{
    for (const auto& f : fields)
        if (f.name == name)
            return f.area;
    return {};
}

void InfoLine::paint (juce::Graphics& g)
{
    g.fillAll (stripColour);
    g.setColour (ParagonColours::panelEdge);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    if (fields.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (13.0f));
        g.drawText ("INFO LINE: SELECT A PART, AN AUDIO EVENT OR AUTOMATION POINTS TO SEE AND CHANGE THEM HERE",
                    getLocalBounds().reduced (12, 0), juce::Justification::centredLeft, true);
        return;
    }
    // Several: in pink (the manual: "shown in another color"), the first one's values.
    const auto valueColour = count > 1 ? pr7s::FamilyColours::pink.brighter (0.25f) : ParagonColours::accent;
    for (const auto& f : fields)
    {
        auto area = f.area;
        g.setColour (ParagonColours::panelEdge);
        g.fillRect (area.getRight() - 1, 6, 1, getHeight() - 12);
        g.setColour (ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (12.0f));
        g.drawText (f.name, area.removeFromTop (getHeight() / 2).reduced (8, 0), juce::Justification::bottomLeft, false);
        if (f.name == editing && editor != nullptr)
            continue;
        g.setColour (f.editable || f.name == "TYPE" || f.name == "FILE" ? valueColour : ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::textFont (15.0f));
        g.drawFittedText (f.value.toUpperCase(), area.reduced (8, 0), juce::Justification::topLeft, 1, 0.8f);
    }
    if (count > 1)
    {
        g.setColour (pr7s::FamilyColours::pink.brighter (0.25f));
        g.setFont (ParagonLookAndFeel::textFont (13.0f));
        const auto x = fields.back().area.getRight() + 12;
        g.drawText (juce::String (count) + (showingParts ? " PARTS SELECTED" : " POINTS SELECTED") + ": CHANGES MOVE THEM ALL BY THE SAME AMOUNT (CTRL+ENTER: ALL TO THE VALUE)",
                    juce::Rectangle<int> (x, 0, juce::jmax (0, getWidth() - x - 12), getHeight()), juce::Justification::centredLeft, true);
    }
}

void InfoLine::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (const auto& f : fields)
        if (f.area.contains (e.getPosition()) && f.editable && f.name != "TERMINATOR" && f.name != "MUTE")
            beginEdit (f.name);
}

void InfoLine::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem ("Hide Info Line", [safe = juce::Component::SafePointer<InfoLine> (this)]
        {
            if (safe != nullptr && safe->onHide)
                safe->onHide();
        });
        menu.showMenuAsync (juce::PopupMenu::Options());
        return;
    }
    for (const auto& f : fields)
        if (f.area.contains (e.getPosition()) && f.editable && e.getNumberOfClicks() == 1)
        {
            if (f.name == "TERMINATOR")
                toggleTerminator();
            else if (f.name == "MUTE")
                toggleMute();
        }
}

void InfoLine::toggleMute()
{
    if (! showingParts)
        return;
    studio.setPartsMuted (partsInOrder(), getField ("MUTE") != "YES");
    refresh();
}

void InfoLine::toggleTerminator()
{
    arrange.setSelectedPointsTerminator (getField ("TERMINATOR") != "YES");
    refresh();
}

void InfoLine::beginEdit (const juce::String& field)
{
    editing = field;
    editor = std::make_unique<InfoEntry>();
    editor->setFont (ParagonLookAndFeel::textFont (14.0f));
    editor->setText (getField (field), false);
    editor->setSelectAllWhenFocused (true);
    auto finish = [this] (bool apply, bool absolute)
    {
        if (editor == nullptr)
            return;
        const auto text = editor->getText();
        const auto field = editing;
        // (The manual's modifier: Ctrl held as the value goes in (Ctrl+Enter) sets them all to it.)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<InfoLine> (this)] { if (safe != nullptr) { safe->editor.reset(); safe->editing = {}; safe->refresh(); safe->repaint(); } });
        if (apply)
            enter (field, text, absolute);
    };
    static_cast<InfoEntry&> (*editor).onEnter = [finish] (bool absolute) { finish (true, absolute); };
    editor->onEscapeKey = [finish] { finish (false, false); };
    editor->onFocusLost = [finish] { finish (false, false); };
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
    repaint();
}

bool InfoLine::enter (const juce::String& field, const juce::String& text, bool absolute)
{
    if (showingParts)
    {
        const auto done = enterPart (field, text, absolute);
        refresh();
        return done;
    }
    auto done = false;
    if (field == "VALUE")
        done = arrange.setSelectedPointsValue (text, absolute);
    else if (field == "START")
        done = arrange.setSelectedPointsStart (text, absolute);
    refresh();
    return done;
}

bool InfoLine::enterPart (const juce::String& field, const juce::String& text, bool absolute)
{
    const auto ids = partsInOrder();
    if (ids.empty())
        return false;
    const auto& project = studio.getProject();
    if (field == "NAME")
        return studio.renameParts (ids, text.trim());
    std::optional<double> value;
    PartField which {};
    if (field == "START" || field == "END")
    {
        value = TempoEditor::parsePosition (text, project);
        which = field == "START" ? PartField::start : PartField::end;
    }
    else if (field == "LENGTH")
    {
        value = KeyEditor::parseLength (text, project);
        which = PartField::length;
    }
    else if (field == "OFFSET" || field == "FADE-IN" || field == "FADE-OUT")
    {
        value = parseSeconds (text);
        which = field == "OFFSET" ? PartField::offset : field == "FADE-IN" ? PartField::fadeIn : PartField::fadeOut;
    }
    else if (field == "VOLUME")
    {
        value = parseVolume (text);
        which = PartField::volume;
    }
    else if (field == "TRANSPOSE" || field == "VELOCITY")
    {
        value = numberIn (text);
        which = field == "TRANSPOSE" ? PartField::transpose : PartField::velocity;
    }
    if (! value)
        return false;
    return studio.setPartsField (ids, which, *value, absolute);
}
