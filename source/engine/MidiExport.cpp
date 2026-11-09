#include "MidiExport.h"
#include "Controllers.h"

namespace pr7s
{
    juce::MidiFile makeMidiFile (const Project& project, const MidiExportOptions& options)
    {
        const auto ppq = juce::jlimit (24, 960, options.resolution);
        const auto from = options.locatorsOnly ? project.loopStart : 0.0;
        const auto to = options.locatorsOnly ? project.loopEnd : std::numeric_limits<double>::max();
        const auto inRange = [&] (double beat) { return beat >= from - 1.0e-9 && beat < to - 1.0e-9; };
        const auto tick = [&] (double beat) { return (double) juce::roundToInt ((beat - from) * ppq); };

        // The conductor: the tempo (steps; a ramp as a step every sixteenth, as a MIDI file has only steps),
        // the signatures, the markers.
        juce::MidiMessageSequence conductor;
        const auto tempo = project.tempoMap();
        const auto addTempo = [&] (double beat, double bpm)
        {
            auto m = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jlimit (Project::minBpm, Project::maxBpm, bpm)));
            m.setTimeStamp (tick (beat));
            conductor.addEvent (m);
        };
        addTempo (from, tempo.bpmAt (from));
        if (project.followsTempoTrack())
        {
            const auto& events = project.tempoEvents;
            for (size_t i = 0; i < events.size(); ++i)
            {
                const auto& e = events[i];
                const auto rampEnd = i + 1 < events.size() && events[i + 1].ramp ? events[i + 1].beat : -1.0;
                if (e.beat > from + 1.0e-9 && inRange (e.beat))
                    addTempo (e.beat, e.bpm);
                if (rampEnd > e.beat)
                    for (auto b = e.beat + 0.25; b < rampEnd - 1.0e-9; b += 0.25)
                        if (b > from + 1.0e-9 && inRange (b))
                            addTempo (b, tempo.bpmAt (b));
            }
        }
        const auto signatures = project.signatures();
        {
            const auto& first = signatures.segmentAt (from).event;
            auto m = juce::MidiMessage::timeSignatureMetaEvent (first.numerator, first.denominator);
            m.setTimeStamp (0.0);
            conductor.addEvent (m);
            for (const auto& segment : signatures.segments())
                if (segment.beat > from + 1.0e-9 && inRange (segment.beat))
                {
                    auto change = juce::MidiMessage::timeSignatureMetaEvent (segment.event.numerator, segment.event.denominator);
                    change.setTimeStamp (tick (segment.beat));
                    conductor.addEvent (change);
                }
        }
        if (options.markers)
            for (const auto& marker : project.markerTrack.markers)   // (the manual: the start of position and cycle markers)
                if (inRange (marker.start))
                {
                    auto m = juce::MidiMessage::textMetaEvent (6, marker.name.isNotEmpty() ? marker.name : markerIdText (marker));
                    m.setTimeStamp (tick (marker.start));
                    conductor.addEvent (m);
                }

        // Each instrument track: its name, then its notes and controllers as heard.
        std::vector<juce::MidiMessageSequence> tracks;
        for (const auto& track : project.tracks)
        {
            if (track.kind != Track::Kind::instrument || track.parts.empty())
                continue;
            if (! options.trackIds.empty() && std::find (options.trackIds.begin(), options.trackIds.end(), track.id) == options.trackIds.end())
                continue;
            const auto channel = juce::jlimit (1, 16, track.midiChannel);
            juce::MidiMessageSequence seq;
            auto name = juce::MidiMessage::textMetaEvent (3, track.name);
            name.setTimeStamp (0.0);
            seq.addEvent (name);
            if (options.volumePan)
            {
                const auto volume = juce::roundToInt (juce::jlimit (0.0, 1.0, (double) volumeToTravel (track.volumeDb)) * 127.0);
                const auto pan = juce::jlimit (0, 127, juce::roundToInt ((track.pan + 1.0f) * 63.5f));
                seq.addEvent (juce::MidiMessage::controllerEvent (channel, 7, volume), 0.0);
                seq.addEvent (juce::MidiMessage::controllerEvent (channel, 10, pan), 0.0);
            }
            for (const auto& part : track.parts)
            {
                if (part.muted)
                    continue;
                for (const auto& note : part.notes)
                {
                    if (note.muted || note.start < 0.0 || note.start >= part.length)
                        continue;
                    const auto on = part.start + note.start;
                    const auto off = juce::jmin (part.start + juce::jmin (note.start + note.length, part.length), to);
                    if (! inRange (on) || off <= on)
                        continue;
                    const auto pitch = part.heardPitch (note.pitch);
                    const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (part.heardVelocity (note.velocity) * 127.0f));
                    seq.addEvent (juce::MidiMessage::noteOn (channel, pitch, velocity), tick (on));
                    seq.addEvent (juce::MidiMessage::noteOff (channel, pitch), tick (off));
                }
                ControllerKinds::expandRamps (part, 1.0 / 32.0, [&] (double beat, int code, int value)
                {
                    if (beat < 0.0 || beat >= part.length || ! inRange (part.start + beat))
                        return;
                    seq.addEvent (ControllerKinds::toMidi (code, value, channel), tick (part.start + beat));
                });
            }
            seq.sort();
            seq.updateMatchedPairs();
            tracks.push_back (std::move (seq));
        }

        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        if (options.type0)
        {
            juce::MidiMessageSequence all (conductor);
            for (const auto& seq : tracks)
                all.addSequence (seq, 0.0);
            all.sort();
            all.updateMatchedPairs();
            file.addTrack (all);
        }
        else
        {
            conductor.sort();
            file.addTrack (conductor);
            for (const auto& seq : tracks)
                file.addTrack (seq);
        }
        return file;
    }

    bool writeMidiFile (const Project& project, const MidiExportOptions& options, const juce::File& destination, juce::String& error)
    {
        const auto file = makeMidiFile (project, options);
        destination.deleteFile();
        juce::FileOutputStream out (destination);
        if (! out.openedOk() || ! file.writeTo (out, options.type0 ? 0 : 1))
        {
            error = "Couldn't write " + destination.getFullPathName();
            return false;
        }
        return true;
    }
}
