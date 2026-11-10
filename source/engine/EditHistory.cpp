#include "EditHistory.h"

#include <map>

namespace pr7s
{
    namespace
    {
        struct Found
        {
            juce::StringArray actions, details;
            void add (const juce::String& action, const juce::String& detail)
            {
                if (! actions.contains (action))
                {
                    actions.add (action);
                    details.add (detail);
                }
            }
        };

        juce::String partName (const Part& part)
        {
            if (part.name.isNotEmpty())
                return part.name;
            return part.isAudio() ? part.audioFile : juce::String ("part");
        }

        // What happened to one part kept by id.
        void comparePart (const Part& a, const Part& b, const juce::String& where, Found& found)
        {
            const auto detail = where + ": " + partName (b);
            if (a.start != b.start)       found.add ("Move Part", detail);
            if (a.length != b.length)     found.add ("Resize Part", detail);
            if (a.notes != b.notes)       found.add ("Edit Notes", detail);
            if (a.controllers != b.controllers) found.add ("Edit Controllers", detail);
            if (a.muted != b.muted)       found.add (b.muted ? "Mute Part" : "Unmute Part", detail);
            if (a.name != b.name)         found.add ("Rename Part", detail);
            if (a.fadeIn != b.fadeIn || a.fadeOut != b.fadeOut) found.add ("Fades", detail);
            if (a.audioOffset != b.audioOffset) found.add ("Trim", detail);
            if (a.volumeDb != b.volumeDb) found.add ("Event Volume", detail);
            if (a.warp != b.warp)         found.add ("Warp", detail);
            if (a.transpose != b.transpose || a.velocityOffset != b.velocityOffset) found.add ("Transpose / Velocity", detail);
            if (a.lane != b.lane || a.order != b.order) found.add ("Takes", detail);
            if (a.audioFile != b.audioFile) found.add ("Replace Audio", detail);
        }

        void compareParts (const Track& a, const Track& b, Found& found)
        {
            std::map<int, const Part*> before, after;
            for (const auto& part : a.parts) before[part.id] = &part;
            for (const auto& part : b.parts) after[part.id] = &part;
            int added = 0, removed = 0;
            const Part* firstAdded = nullptr;
            const Part* firstRemoved = nullptr;
            for (const auto& [id, part] : after)
                if (before.count (id) == 0)
                {
                    ++added;
                    if (firstAdded == nullptr) firstAdded = part;
                }
            for (const auto& [id, part] : before)
                if (after.count (id) == 0)
                {
                    ++removed;
                    if (firstRemoved == nullptr) firstRemoved = part;
                }
            const auto where = b.name;
            if (added > 0 && removed > 0)
                found.add (added > removed ? "Split" : added < removed ? "Glue" : "Replace Part", where + ": " + partName (*firstAdded));
            else if (added > 0)
                found.add (added == 1 ? "Add Part" : "Add Parts", where + ": " + partName (*firstAdded) + (added > 1 ? " (" + juce::String (added) + ")" : juce::String()));
            else if (removed > 0)
                found.add (removed == 1 ? "Delete Part" : "Delete Parts", where + ": " + partName (*firstRemoved) + (removed > 1 ? " (" + juce::String (removed) + ")" : juce::String()));
            for (const auto& [id, part] : after)
                if (const auto kept = before.find (id); kept != before.end() && ! (*kept->second == *part))
                    comparePart (*kept->second, *part, where, found);
        }

        bool sameInserts (const std::vector<Insert>& a, const std::vector<Insert>& b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i].plugin.name != b[i].plugin.name || a[i].bypass != b[i].bypass)
                    return false;
            return true;
        }

        void compareTrack (const Track& a, const Track& b, Found& found)
        {
            const auto& name = b.name;
            if (a.name != b.name)               found.add ("Rename Track", a.name + " to " + b.name);
            if (a.colour != b.colour)           found.add ("Track Colour", name);
            if (a.volumeDb != b.volumeDb)       found.add ("Volume", name + ": " + juce::String (b.volumeDb, 1) + " dB");
            if (a.pan != b.pan)                 found.add ("Pan", name);
            if (a.mute != b.mute)               found.add (b.mute ? "Mute" : "Unmute", name);
            if (a.solo != b.solo)               found.add (b.solo ? "Solo" : "Solo Off", name);
            compareParts (a, b, found);
            if (a.automation != b.automation)   found.add ("Automation", name);
            if (a.sends != b.sends || a.sendsBypass != b.sendsBypass) found.add ("Sends", name);
            if (a.output != b.output)           found.add ("Output", name);
            if (! sameInserts (a.inserts, b.inserts)) found.add ("Inserts", name);
            if (! (a.eq == b.eq))               found.add ("Channel EQ", name);
            if (! (a.boost == b.boost))         found.add ("Booster", name);
            if (a.instrument.getName() != b.instrument.getName()) found.add ("Change Instrument", name + ": " + b.instrument.getName());
            if (a.folder != b.folder)           found.add ("Track Folder", name);
            if (a.midiChannel != b.midiChannel) found.add ("MIDI Channel", name);
        }
    }

    HistoryStep describeEdit (const Project& before, const Project& after)
    {
        Found found;
        // Tracks added, removed or moved.
        for (const auto& track : after.tracks)
            if (before.findTrack (track.id) == nullptr)
                found.add ("Add Track", track.name);
        for (const auto& track : before.tracks)
            if (after.findTrack (track.id) == nullptr)
                found.add ("Remove Track", track.name);
        {
            std::vector<int> a, b;
            for (const auto& track : before.tracks) if (after.findTrack (track.id) != nullptr) a.push_back (track.id);
            for (const auto& track : after.tracks)  if (before.findTrack (track.id) != nullptr) b.push_back (track.id);
            if (a != b)
                found.add ("Move Tracks", {});
        }
        for (const auto& track : after.tracks)
            if (const auto* then = before.findTrack (track.id))
                compareTrack (*then, track, found);

        // The song's own settings.
        if (before.bpm != after.bpm || before.tempoEvents != after.tempoEvents || before.tempoTrackOn != after.tempoTrackOn)
            found.add ("Tempo", after.tempoTrackOn ? juce::String ("Tempo track") : juce::String (after.bpm, 2) + " BPM");
        if (before.numerator != after.numerator || before.denominator != after.denominator || before.signatureChanges != after.signatureChanges)
            found.add ("Signature", juce::String (after.numerator) + "/" + juce::String (after.denominator));
        if (before.loopOn != after.loopOn || before.loopStart != after.loopStart || before.loopEnd != after.loopEnd)
            found.add ("Cycle", after.loopOn ? juce::String ("On") : juce::String ("Off"));
        if (before.click != after.click)
            found.add ("Click", after.click ? juce::String ("On") : juce::String ("Off"));
        if (! (before.markerTrack.markers == after.markerTrack.markers))
            found.add ("Markers", {});
        if (! (before.chordTrack == after.chordTrack))
            found.add ("Chord Track", {});
        if (before.masterVolumeDb != after.masterVolumeDb)
            found.add ("Volume", "Master: " + juce::String (after.masterVolumeDb, 1) + " dB");
        if (! (before.masterEq == after.masterEq))
            found.add ("Channel EQ", "Master");
        if (! (before.masterBoost == after.masterBoost))
            found.add ("Booster", "Master");
        if (! sameInserts (before.masterInserts, after.masterInserts))
            found.add ("Inserts", "Master");
        if (before.masterAutomation != after.masterAutomation)
            found.add ("Automation", "Master");
        if (before.variAudio != after.variAudio)
            found.add ("VariAudio", {});
        if (! (before.linkGroups == after.linkGroups))
            found.add ("Link Groups", {});

        HistoryStep step;
        step.time = juce::Time::getCurrentTime();
        if (found.actions.isEmpty())
        {
            step.action = "Edit";
            return step;
        }
        step.action = found.actions[0];
        step.details = found.details[0];
        if (found.actions.size() > 1)
            step.details += (step.details.isEmpty() ? "" : "; ") + juce::String ("and ") + found.actions.joinIntoString (", ", 1).toLowerCase();
        return step;
    }
}
