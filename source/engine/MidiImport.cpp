#include "MidiImport.h"

namespace pr7s
{
    std::optional<MidiImport> readMidiFile (const juce::File& file, juce::String& error)
    {
        juce::FileInputStream stream (file);
        if (! stream.openedOk())
        {
            error = "Couldn't open " + file.getFullPathName();
            return std::nullopt;
        }
        return readMidiFile (stream, error);
    }

    std::optional<MidiImport> readMidiFile (juce::InputStream& stream, juce::String& error)
    {
        juce::MidiFile midi;
        int format = 1;
        if (! midi.readFrom (stream, true, &format))
        {
            error = "It isn't a standard MIDI file.";
            return std::nullopt;
        }

        MidiImport result;
        // Its first tempo and time signature (the song keeps one of each).
        juce::MidiMessageSequence tempos, signatures;
        midi.findAllTempoEvents (tempos);
        midi.findAllTimeSigEvents (signatures);
        if (tempos.getNumEvents() > 0)
            result.bpm = juce::jlimit (Project::minBpm, Project::maxBpm, 60.0 / tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote());
        if (signatures.getNumEvents() > 0)
            signatures.getEventPointer (0)->message.getTimeSignatureInfo (result.numerator, result.denominator);

        // Ticks to beats: ticks per quarter note; or, for a file timed in
        // SMPTE frames, its seconds at its tempo.
        const auto timeFormat = midi.getTimeFormat();
        if (timeFormat <= 0)
            midi.convertTimestampTicksToSeconds();
        const auto toBeats = [&] (double time)
        {
            return timeFormat > 0 ? time / (double) timeFormat : time * result.bpm / 60.0;
        };

        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence sequence (*midi.getTrack (t));
            sequence.updateMatchedPairs();
            juce::String name;
            // By channel (a type-0 file's one track becomes one per channel; other files' tracks stay whole).
            std::map<int, MidiImport::Track> byChannel;
            for (int i = 0; i < sequence.getNumEvents(); ++i)
            {
                const auto* event = sequence.getEventPointer (i);
                const auto& message = event->message;
                if (message.isTrackNameEvent() && name.isEmpty())
                    name = message.getTextFromTextMetaEvent().trim();
                if (! message.isNoteOn())
                    continue;
                const auto start = toBeats (message.getTimeStamp());
                const auto end = event->noteOffObject != nullptr ? toBeats (event->noteOffObject->message.getTimeStamp()) : start + 0.25;
                auto& track = byChannel[format == 0 ? message.getChannel() : 0];
                if (track.notes.empty())
                    track.channel = message.getChannel();
                track.notes.push_back ({ start, juce::jmax (1.0 / 64.0, end - start), message.getNoteNumber(), message.getFloatVelocity() });
            }
            for (auto& [channel, track] : byChannel)
            {
                juce::ignoreUnused (channel);
                track.name = name.isNotEmpty() ? name : "MIDI " + juce::String ((int) result.tracks.size() + 1).paddedLeft ('0', 2);
                if (format == 0 && byChannel.size() > 1)
                    track.name << " (Ch. " << track.channel << ")";
                std::stable_sort (track.notes.begin(), track.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
                result.tracks.push_back (std::move (track));
            }
        }
        if (result.tracks.empty())
        {
            error = "There are no notes in it.";
            return std::nullopt;
        }
        return result;
    }
}
