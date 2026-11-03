// The MIDI editor's drum view (0.26.0, the manual's Drum Editor and Drum Maps): the drum sound
// list, its rows and menus, the MAP menu.

#include "KeyEditor.h"

#include "ui/ParagonLookAndFeel.h"

namespace
{
    juce::FontOptions font (float height, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (height, style); }
    constexpr float drumRowHeight = 20.0f;   // our choice: room for the names

    // A note picker: octaves, then their notes ("C1 (36)").
    juce::PopupMenu notesMenu (int current, std::function<void (int)> choose)
    {
        juce::PopupMenu menu;
        for (int octave = 0; octave < 11; ++octave)
        {
            juce::PopupMenu sub;
            auto any = false;
            for (int n = octave * 12; n < juce::jmin (128, octave * 12 + 12); ++n)
            {
                sub.addItem (pr7s::noteName (n) + " (" + juce::String (n) + ")", true, n == current, [choose, n] { choose (n); });
                any = true;
            }
            if (any)
                menu.addSubMenu (pr7s::noteName (octave * 12) + " - " + pr7s::noteName (juce::jmin (127, octave * 12 + 11)), sub,
                                 true, nullptr, current / 12 == octave);
        }
        return menu;
    }
}

void KeyEditor::refreshDrumRows()
{
    const auto& project = studio.getProject();
    const auto* track = project.findTrack (getTrackId());
    const auto* map = track != nullptr ? project.drumMapOf (*track) : nullptr;
    const auto on = map != nullptr && track->drumEditor;
    if (on != drumViewOn)
    {
        // The rows: taller for the names in the drum view; the piano's height kept for its return.
        if (on)
            pianoRowHeight = rowHeight, rowHeight = drumRowHeight;
        else
            rowHeight = pianoRowHeight;
        drumViewOn = on;
        scrollY = 0.0f;
        resized();
        refitAfterDrumRows = true;   // (once the rows are known: the part from its start, the list from its top)
    }
    drumRows.clear();
    drumRowOf.fill (-1);
    if (! on)
    {
        if (std::exchange (refitAfterDrumRows, false))
            fitToPart();
        return;
    }
    std::array<bool, 128> used {};
    if (const auto* part = getPart())
        for (const auto& n : part->notes)
            used[(size_t) juce::jlimit (0, 127, n.pitch)] = true;
    const auto take = [&] (DrumRowsShown shown)
    {
        drumRows.clear();
        for (const auto pitch : map->order)
        {
            const auto named = map->sounds[(size_t) pitch].name.trim().isNotEmpty();
            if (shown == DrumRowsShown::all || (shown == DrumRowsShown::named && (named || used[(size_t) pitch]))
                || (shown == DrumRowsShown::withEvents && used[(size_t) pitch]))
                drumRows.push_back (pitch);
        }
    };
    take (drumRowsShown);
    if (drumRows.empty())
        take (DrumRowsShown::named);   // (an empty part shows the kit)
    if (drumRows.empty())
        take (DrumRowsShown::all);
    for (size_t i = 0; i < drumRows.size(); ++i)
        drumRowOf[(size_t) drumRows[i]] = (int) i;
    clampScroll();
    if (std::exchange (refitAfterDrumRows, false))
        fitToPart();
}

void KeyEditor::setDrumView (bool drums)
{
    if (getTrackId() == 0)
        return;
    studio.setTrackDrumEditor (getTrackId(), drums);
    refresh();
}

void KeyEditor::setDrumRowsShown (DrumRowsShown shown)
{
    drumRowsShown = shown;
    if (auto* settings = studio.getSettings())
        settings->setValue ("drumRowsShown", (int) shown);
    refreshDrumRows();
    refreshScrollBars();
    repaint();
}

void KeyEditor::setShowDrumLengths (bool show)
{
    showDrumLengths = show;
    lengthsKey.setToggleState (show, juce::dontSendNotification);
    if (auto* settings = studio.getSettings())
        settings->setValue ("drumLengths", show);
    repaint();
}

juce::String KeyEditor::drumNameOf (int pitch) const
{
    const auto* map = studio.getDrumMapOf (getTrackId());
    return map != nullptr && juce::isPositiveAndBelow (pitch, 128) ? map->sounds[(size_t) pitch].name : juce::String();
}

void KeyEditor::toggleDrumMute (int pitch)
{
    const auto* map = studio.getDrumMapOf (getTrackId());
    if (map == nullptr || ! juce::isPositiveAndBelow (pitch, 128))
        return;
    auto edited = *map;
    edited.sounds[(size_t) pitch].mute = ! edited.sounds[(size_t) pitch].mute;
    studio.setDrumMap (map->name, edited);
}

void KeyEditor::renameDrumSound (int pitch, const juce::String& name)
{
    const auto* map = studio.getDrumMapOf (getTrackId());
    if (map == nullptr || ! juce::isPositiveAndBelow (pitch, 128))
        return;
    auto edited = *map;
    edited.sounds[(size_t) pitch].name = name.trim();
    studio.setDrumMap (map->name, edited);
}

void KeyEditor::selectNotesOfSound (int pitch)
{
    std::vector<int> all;
    const auto& list = notes();
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].pitch == pitch)
            all.push_back (i);
    lanesFocused = false;
    selectNotes (all);
}

juce::Rectangle<float> KeyEditor::getDrumMuteBox (int pitch) const
{
    const auto y = yForPitch (pitch);
    const auto size = juce::jmin (14.0f, rowHeight - 4.0f);
    return { (float) sideWidth + 5.0f, y + (rowHeight - size) * 0.5f, size, size };
}

void KeyEditor::showDrumSoundMenu (int pitch)
{
    const auto* map = studio.getDrumMapOf (getTrackId());
    if (map == nullptr || ! juce::isPositiveAndBelow (pitch, 128))
        return;
    const auto& sound = map->sounds[(size_t) pitch];
    const auto mapName = map->name;
    auto& s = studio;
    juce::Component::SafePointer<KeyEditor> safe (this);
    const auto change = [&s, mapName, pitch] (std::function<void (pr7s::DrumSound&)> edit)
    {
        if (const auto* m = s.getProject().findDrumMap (mapName))
        {
            auto edited = *m;
            edit (edited.sounds[(size_t) pitch]);
            s.setDrumMap (mapName, edited);
        }
    };
    juce::PopupMenu menu;
    menu.addSectionHeader (pr7s::noteName (pitch) + "  " + (sound.name.isNotEmpty() ? sound.name : juce::String ("(no name)")));
    menu.addItem ("Hear It", true, false, [safe, pitch]
    {
        if (safe == nullptr)
            return;
        safe->startAudition (pitch, safe->newVelocity);
        juce::Timer::callAfterDelay (300, [safe] { if (safe != nullptr) safe->stopAudition(); });
    });
    menu.addItem ("Select All Notes of This Sound", true, false, [safe, pitch] { if (safe != nullptr) safe->selectNotesOfSound (pitch); });
    menu.addItem ("Mute", true, sound.mute, [safe, pitch] { if (safe != nullptr) safe->toggleDrumMute (pitch); });
    menu.addSeparator();
    menu.addSubMenu ("I-Note (the key that plays it): " + pr7s::noteName (sound.inNote),
                     notesMenu (sound.inNote, [change] (int n) { change ([n] (pr7s::DrumSound& d) { d.inNote = n; }); }));
    menu.addSubMenu ("O-Note (the note sent to the instrument): " + pr7s::noteName (sound.outNote),
                     notesMenu (sound.outNote, [change] (int n) { change ([n] (pr7s::DrumSound& d) { d.outNote = n; }); }));
    juce::PopupMenu channel;
    channel.addItem ("The Track's", true, sound.channel < 0, [change] { change ([] (pr7s::DrumSound& d) { d.channel = -1; }); });
    for (int c = 0; c < 16; ++c)
        channel.addItem (juce::String (c + 1), true, sound.channel == c, [change, c] { change ([c] (pr7s::DrumSound& d) { d.channel = c; }); });
    menu.addSubMenu ("Channel: " + (sound.channel < 0 ? juce::String ("the track's") : juce::String (sound.channel + 1)), channel);
    menu.addSeparator();
    menu.addItem ("Show Named Sounds (and Those with Notes)", true, drumRowsShown == DrumRowsShown::named, [safe] { if (safe != nullptr) safe->setDrumRowsShown (DrumRowsShown::named); });
    menu.addItem ("Show Drum Sounds with Events", true, drumRowsShown == DrumRowsShown::withEvents, [safe] { if (safe != nullptr) safe->setDrumRowsShown (DrumRowsShown::withEvents); });
    menu.addItem ("Show All 128", true, drumRowsShown == DrumRowsShown::all, [safe] { if (safe != nullptr) safe->setDrumRowsShown (DrumRowsShown::all); });
    menu.addItem ("Drum Map Setup...", true, false, [safe] { if (safe != nullptr && safe->onDrumMapSetup) safe->onDrumMapSetup(); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (getDrumMuteBox (pitch).toNearestInt())));
}

void KeyEditor::showDrumMapMenu()
{
    const auto trackId = getTrackId();
    const auto* track = studio.getProject().findTrack (trackId);
    if (track == nullptr)
        return;
    const auto current = track->drumMap;
    auto& s = studio;
    juce::Component::SafePointer<KeyEditor> safe (this);
    juce::PopupMenu menu;
    menu.addSectionHeader ("THIS TRACK'S DRUM MAP");
    menu.addItem ("No Drum Map", true, current.isEmpty(), [&s, trackId] { s.setTrackDrumMap (trackId, {}); });
    const auto gm = pr7s::DrumMap::gm().name;
    menu.addItem (gm + " (built in)", true, current == gm, [&s, trackId, gm] { s.setTrackDrumMap (trackId, gm); });
    for (const auto& map : studio.getProject().drumMaps)
        if (map.name != gm)
            menu.addItem (map.name, true, current == map.name, [&s, trackId, name = map.name] { s.setTrackDrumMap (trackId, name); });
    const auto library = studio.getLibraryDrumMaps();
    juce::PopupMenu fromFolder;
    for (const auto& file : library)
    {
        const auto name = file.getFileNameWithoutExtension();
        if (studio.getProject().findDrumMap (name) == nullptr)
            fromFolder.addItem (name, true, false, [&s, trackId, name] { s.setTrackDrumMap (trackId, name); });
    }
    if (fromFolder.getNumItems() > 0)
        menu.addSubMenu ("From the Drum Map Folder", fromFolder);
    menu.addSeparator();
    menu.addItem ("Load a Drum Map (.drm)...", true, false, [safe, trackId]
    {
        if (safe == nullptr)
            return;
        const auto load = [safe, trackId] (const juce::File& file)
        {
            if (safe == nullptr || file == juce::File())
                return;
            juce::String error;
            const auto name = safe->studio.loadDrumMap (file, error);
            if (name.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the drum map", error);
            else
                safe->studio.setTrackDrumMap (trackId, name);
        };
        if (safe->chooseDrumMapFile)
        {
            safe->chooseDrumMapFile (load);
            return;
        }
        juce::MessageManager::callAsync ([safe, load]
        {
            if (safe == nullptr)
                return;
            safe->drumChooser = std::make_unique<juce::FileChooser> ("Load a drum map", pr7s::Studio::drumMapFolder(), "*.drm;*.xml");
            safe->drumChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                            [load] (const juce::FileChooser& chooser) { load (chooser.getResult()); });
        });
    });
    if (const auto* map = studio.getDrumMapOf (trackId))
        menu.addItem ("Save " + map->name + " as a .drm...", true, false, [safe, name = map->name]
        {
            juce::MessageManager::callAsync ([safe, name]
            {
                if (safe == nullptr)
                    return;
                const auto folder = pr7s::Studio::drumMapFolder();
                folder.createDirectory();
                safe->drumChooser = std::make_unique<juce::FileChooser> ("Save the drum map", folder.getChildFile (juce::File::createLegalFileName (name) + ".drm"), "*.drm");
                safe->drumChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                                [safe, name] (const juce::FileChooser& chooser)
                                                {
                                                    if (safe == nullptr || chooser.getResult() == juce::File())
                                                        return;
                                                    if (const auto* m = safe->studio.getProject().findDrumMap (name))
                                                        m->writeDrm (chooser.getResult().withFileExtension (".drm"));
                                                });
            });
        });
    menu.addItem ("Drum Map Setup...", true, false, [safe] { if (safe != nullptr && safe->onDrumMapSetup) safe->onDrumMapSetup(); });
    menu.addItem ("Open the Drum Map Folder", true, false, []
    {
        const auto folder = pr7s::Studio::drumMapFolder();
        folder.createDirectory();
        folder.revealToUser();
    });
    menu.addSeparator();
    menu.addItem ("Show as Drums (the Drum Editor)", track->drumMap.isNotEmpty(), track->drumEditor, [safe, on = track->drumEditor] { if (safe != nullptr) safe->setDrumView (! on); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mapKey));
}

void KeyEditor::paintDrumList (juce::Graphics& g)
{
    const auto area = keysArea();
    g.setColour (juce::Colour (0xff120e13));
    g.fillRect (area);
    const auto* map = studio.getDrumMapOf (getTrackId());
    if (map == nullptr)
        return;
    std::array<bool, 128> selectedPitch {};
    const auto& list = notes();
    for (const auto i : selected)
        if (juce::isPositiveAndBelow (i, (int) list.size()))
            selectedPitch[(size_t) juce::jlimit (0, 127, list[(size_t) i].pitch)] = true;
    const auto first = rowForY (area.getY()), last = rowForY (area.getBottom());
    for (int row = first; row <= last && row < numRows(); ++row)
    {
        const auto pitch = drumRows[(size_t) row];
        const auto& sound = map->sounds[(size_t) pitch];
        const auto r = juce::Rectangle<float> (area.getX(), yForPitch (pitch), area.getWidth(), rowHeight);
        const auto lit = auditioning == pitch;
        g.setColour (lit ? ParagonColours::accent.withAlpha (0.45f) : selectedPitch[(size_t) pitch] ? ParagonColours::accent.withAlpha (0.18f)
                                                                     : row % 2 == 0 ? juce::Colour (0xff181318) : juce::Colour (0xff120e13));
        g.fillRect (r);
        g.setColour (ParagonColours::panelEdge.withAlpha (0.6f));
        g.fillRect (r.withY (r.getBottom() - 1.0f).withHeight (1.0f));
        // M: muted.
        const auto box = getDrumMuteBox (pitch);
        g.setColour (sound.mute ? ParagonColours::accent : juce::Colour (0xff0b090c));
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (sound.mute ? juce::Colours::black : ParagonColours::textDim);
        g.setFont (font (9.0f));
        g.drawText ("M", box, juce::Justification::centred, false);
        // Its pitch and name.
        auto text = r.withTrimmedLeft (box.getRight() - r.getX() + 5.0f).withTrimmedRight (4.0f);
        g.setColour (ParagonColours::textDim.withAlpha (sound.mute ? 0.5f : 1.0f));
        g.setFont (font (9.5f, juce::Font::plain));
        g.drawText (pr7s::noteName (pitch), text.removeFromLeft (30.0f), juce::Justification::centredLeft, false);
        g.setColour ((sound.mute ? ParagonColours::textDim : ParagonColours::text).withAlpha (sound.mute ? 0.6f : 1.0f));
        g.setFont (font (11.0f));
        g.drawText (sound.name.isNotEmpty() ? sound.name.toUpperCase() : juce::String ("-"), text, juce::Justification::centredLeft, true);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (area.withX (area.getRight() - 1.0f).withWidth (1.0f));
}
