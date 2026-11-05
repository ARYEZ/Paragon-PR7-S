#include "DragOut.h"
#include "Controllers.h"

namespace pr7s
{
    juce::MidiFile makePartMidiFile (const Project& project, const Track& track, const Part& part, int resolution)
    {
        const auto ppq = juce::jlimit (24, 960, resolution);
        const auto from = part.start, to = part.end();
        const auto inside = [&] (double beat) { return beat > from + 1.0e-9 && beat < to - 1.0e-9; };
        const auto tick = [&] (double beat) { return (double) juce::roundToInt ((beat - from) * ppq); };

        // The conductor: the tempo at the part's start, the tempo track's changes inside it (a ramp
        // as a step every sixteenth), the signature at its start and changes inside it.
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
                if (inside (e.beat))
                    addTempo (e.beat, e.bpm);
                const auto rampEnd = i + 1 < events.size() && events[i + 1].ramp ? events[i + 1].beat : -1.0;
                for (auto b = e.beat + 0.25; b < rampEnd - 1.0e-9; b += 0.25)
                    if (inside (b))
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
                if (inside (segment.beat))
                {
                    auto change = juce::MidiMessage::timeSignatureMetaEvent (segment.event.numerator, segment.event.denominator);
                    change.setTimeStamp (tick (segment.beat));
                    conductor.addEvent (change);
                }
        }
        conductor.sort();

        // The part: its name, the notes as heard, its controllers.
        const auto channel = juce::jlimit (1, 16, track.midiChannel);
        juce::MidiMessageSequence seq;
        auto name = juce::MidiMessage::textMetaEvent (3, part.name.isNotEmpty() ? part.name : track.name);
        name.setTimeStamp (0.0);
        seq.addEvent (name);
        for (const auto& note : part.notes)
        {
            if (note.start < 0.0 || note.start >= part.length)
                continue;
            const auto on = from + note.start;
            const auto off = from + juce::jmin (note.start + note.length, part.length);
            if (off <= on)
                continue;
            const auto pitch = part.heardPitch (note.pitch);
            const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (part.heardVelocity (note.velocity) * 127.0f));
            seq.addEvent (juce::MidiMessage::noteOn (channel, pitch, velocity), tick (on));
            seq.addEvent (juce::MidiMessage::noteOff (channel, pitch), tick (off));
        }
        ControllerKinds::expandRamps (part, 1.0 / 32.0, [&] (double beat, int code, int value)
        {
            if (beat < 0.0 || beat >= part.length)
                return;
            seq.addEvent (ControllerKinds::toMidi (code, value, channel), tick (from + beat));
        });
        seq.sort();
        seq.updateMatchedPairs();

        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        file.addTrack (conductor);
        file.addTrack (seq);
        return file;
    }

    juce::File dragOutFolder()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Paragon PR7-S Drag Out");
    }

    void clearDragOutFolder()
    {
        dragOutFolder().deleteRecursively();
    }

    juce::File newDragOutFolder()
    {
        const auto root = dragOutFolder();
        root.createDirectory();
        for (int n = 1;; ++n)
        {
            const auto folder = root.getChildFile (juce::String (n));
            if (! folder.exists())
            {
                folder.createDirectory();
                return folder;
            }
        }
    }

    juce::String dragOutTempoText (double bpm)
    {
        const auto tenths = juce::roundToInt (bpm * 10.0);
        return tenths % 10 == 0 ? juce::String (tenths / 10) : juce::String ((double) tenths / 10.0, 1);
    }
}
