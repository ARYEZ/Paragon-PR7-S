#include "TrackActions.h"

namespace TrackActions
{
    namespace
    {
        bool canFreeze (const pr7s::Track& t) { return ! t.isChannel() && ! t.disabled; }

        void warn (const juce::String& title, const juce::String& text)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, text);
        }

        // A chooser opened after the menu has gone (0.9.7: never from inside
        // another box's callback).
        void openChooser (std::unique_ptr<juce::FileChooser>& holder, bool saving, const juce::String& title,
                          const juce::File& start, std::function<void (const juce::File&)> then)
        {
            pr7s::Studio::trackPresetFolder().createDirectory();
            holder = std::make_unique<juce::FileChooser> (title, start, juce::String ("*") + pr7s::Studio::trackPresetExtension);
            auto flags = juce::FileBrowserComponent::canSelectFiles
                       | (saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                 : juce::FileBrowserComponent::openMode);
            auto* chooser = holder.get();
            juce::MessageManager::callAsync ([chooser, flags, then]
            {
                chooser->launchAsync (flags, [then] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    if (file != juce::File())
                        then (file);
                });
            });
        }
    }

    void addFreezeItems (juce::PopupMenu& menu, pr7s::Studio& studio, int trackId)
    {
        const auto* track = studio.getProject().findTrack (trackId);
        if (track == nullptr || track->isChannel())
            return;
        const auto several = studio.getSelectedTracks().size() > 1;
        if (track->frozen)
        {
            juce::PopupMenu unfreeze;
            unfreeze.addItem (75, "Keep the freeze file");
            unfreeze.addItem (76, "Delete the freeze file");
            menu.addSubMenu (several ? "Unfreeze Selected Tracks" : "Unfreeze Track", unfreeze);
            return;
        }
        juce::PopupMenu freeze;
        freeze.addSectionHeader ("TAIL (THE SOUND LEFT RINGING AFTER THE LAST PART)");
        for (int i = 0; i < (int) std::size (tails); ++i)
            freeze.addItem (70 + i, tails[i] == 0.0 ? juce::String ("No tail") : juce::String ((int) tails[i]) + " seconds");
        menu.addSubMenu (several ? "Freeze Selected Tracks" : "Freeze Track", freeze, canFreeze (*track));
    }

    void addPresetItems (juce::PopupMenu& menu, pr7s::Studio& studio, int trackId)
    {
        const auto* track = studio.getProject().findTrack (trackId);
        if (track == nullptr || track->isGroup())
            return;
        menu.addItem (77, "Save Track Preset...", ! track->disabled);
        menu.addItem (78, "Load Track Preset...", ! track->disabled && ! track->frozen);
    }

    bool freezeSelected (pr7s::Studio& studio, double tailSeconds, juce::String& error)
    {
        juce::MouseCursor::showWaitCursor();
        auto all = true;
        for (const auto id : studio.getSelectedTracks())
        {
            const auto* t = studio.getProject().findTrack (id);
            if (t == nullptr || t->frozen || ! canFreeze (*t))
                continue;
            juce::String why;
            if (! studio.freezeTrack (id, tailSeconds, why))
            {
                all = false;
                error << (error.isEmpty() ? "" : "\n") << t->name << ": " << why;
            }
        }
        juce::MouseCursor::hideWaitCursor();
        return all;
    }

    void unfreezeSelected (pr7s::Studio& studio, bool deleteFiles)
    {
        for (const auto id : studio.getSelectedTracks())
            if (studio.isFrozen (id))
                studio.unfreezeTrack (id, deleteFiles);
    }

    bool handle (int chosen, pr7s::Studio& studio, int trackId, std::unique_ptr<juce::FileChooser>& chooser)
    {
        if (chosen >= 70 && chosen < 70 + (int) std::size (tails))
        {
            // The track clicked is among the selected (the right-click selects it).
            if (! studio.isTrackSelected (trackId))
                studio.selectTrack (trackId);
            juce::String error;
            if (! freezeSelected (studio, tails[chosen - 70], error))
                warn ("Not everything was frozen", error);
            return true;
        }
        if (chosen == 75 || chosen == 76)
        {
            if (! studio.isTrackSelected (trackId))
                studio.selectTrack (trackId);
            unfreezeSelected (studio, chosen == 76);
            return true;
        }
        if (chosen == 77) { savePreset (studio, trackId, chooser); return true; }
        if (chosen == 78) { loadPreset (studio, trackId, chooser); return true; }
        return false;
    }

    bool savePresetTo (pr7s::Studio& studio, int trackId, const juce::File& file)
    {
        const auto xml = studio.makeTrackPreset (trackId);
        return xml != nullptr && xml->writeTo (file);
    }

    bool loadPresetFrom (pr7s::Studio& studio, int trackId, const juce::File& file, juce::String& error)
    {
        const auto xml = juce::parseXML (file);
        if (xml == nullptr)
        {
            error = "Couldn't read " + file.getFileName() + ".";
            return false;
        }
        return studio.applyTrackPreset (trackId, *xml, error);
    }

    int addTrackFrom (pr7s::Studio& studio, const juce::File& file, juce::String& error)
    {
        const auto xml = juce::parseXML (file);
        if (xml == nullptr)
        {
            error = "Couldn't read " + file.getFileName() + ".";
            return 0;
        }
        return studio.addTrackFromPreset (*xml, error);
    }

    void savePreset (pr7s::Studio& studio, int trackId, std::unique_ptr<juce::FileChooser>& chooser)
    {
        const auto* t = studio.getProject().findTrack (trackId);
        if (t == nullptr)
            return;
        const auto start = pr7s::Studio::trackPresetFolder().getChildFile (juce::File::createLegalFileName (t->name) + pr7s::Studio::trackPresetExtension);
        openChooser (chooser, true, "Save Track Preset", start, [&studio, trackId] (const juce::File& chosen)
        {
            const auto file = chosen.withFileExtension (pr7s::Studio::trackPresetExtension);
            if (! savePresetTo (studio, trackId, file))
                warn ("Couldn't save the preset", "Couldn't write " + file.getFullPathName() + ".");
        });
    }

    void loadPreset (pr7s::Studio& studio, int trackId, std::unique_ptr<juce::FileChooser>& chooser)
    {
        openChooser (chooser, false, "Load Track Preset", pr7s::Studio::trackPresetFolder(), [&studio, trackId] (const juce::File& file)
        {
            juce::String error;
            if (! loadPresetFrom (studio, trackId, file, error) || error.isNotEmpty())
                warn ("Track preset", error);
        });
    }

    void addTrackFromPreset (pr7s::Studio& studio, std::unique_ptr<juce::FileChooser>& chooser)
    {
        openChooser (chooser, false, "Add a Track from a Preset", pr7s::Studio::trackPresetFolder(), [&studio] (const juce::File& file)
        {
            juce::String error;
            if (addTrackFrom (studio, file, error) == 0 || error.isNotEmpty())
                warn ("Track preset", error);
        });
    }
}
