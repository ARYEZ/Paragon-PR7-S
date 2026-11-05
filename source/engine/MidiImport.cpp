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
        // Every tempo change, for the tempo track (0.10.0): steps, as a MIDI file has them.
        if (timeFormat > 0)
            for (int i = 0; i < tempos.getNumEvents(); ++i)
            {
                const auto& message = tempos.getEventPointer (i)->message;
                const auto beat = message.getTimeStamp() / (double) timeFormat;
                const auto bpm = juce::jlimit (Project::minBpm, Project::maxBpm, 60.0 / message.getTempoSecondsPerQuarterNote());
                if (! result.tempoChanges.empty() && std::abs (result.tempoChanges.back().beat - beat) < 1.0e-9)
                    result.tempoChanges.back().bpm = bpm;   // (two at one tick: the later)
                else if (result.tempoChanges.empty() || std::abs (result.tempoChanges.back().bpm - bpm) > 1.0e-6)
                    result.tempoChanges.push_back ({ beat, bpm, false });
            }
        // A first tempo after the file's start: until then it plays at MIDI's own 120.
        if (! result.tempoChanges.empty() && result.tempoChanges.front().beat > 1.0e-9)
        {
            result.tempoChanges.insert (result.tempoChanges.begin(), { 0.0, 120.0, false });
            result.bpm = 120.0;
        }
        // Every signature change after the first, at its bar (0.45.0): bars counted from the
        // signature before it (a change off a bar line goes to the bar it's in).
        if (timeFormat > 0 && isValidSignature (result.numerator, result.denominator))
        {
            SignatureEvent last { 0, result.numerator, result.denominator };
            auto lastBeat = 0.0;
            for (int i = 0; i < signatures.getNumEvents(); ++i)
            {
                const auto& message = signatures.getEventPointer (i)->message;
                const auto beat = message.getTimeStamp() / (double) timeFormat;
                int n = 4, d = 4;
                message.getTimeSignatureInfo (n, d);
                if (! isValidSignature (n, d))
                    continue;
                const auto bar = last.bar + (int) std::floor ((beat - lastBeat) / last.barLength() + 1.0e-6);
                if (bar <= 0)
                    continue;   // (the first: the song's own)
                if (n == last.numerator && d == last.denominator)
                    continue;
                if (! result.signatureChanges.empty() && result.signatureChanges.back().bar == bar)
                    result.signatureChanges.pop_back();
                lastBeat += (bar - last.bar) * last.barLength();
                last = { bar, n, d };
                result.signatureChanges.push_back (last);
            }
        }
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
                if (message.isController() || message.isPitchWheel() || message.isChannelPressure()
                    || message.isAftertouch() || message.isProgramChange())
                {
                    // (0.25.0) As steps, as recorded: a file's controllers are already as dense as they were played.
                    auto& track = byChannel[format == 0 ? message.getChannel() : 0];
                    Controller c;
                    c.start = toBeats (message.getTimeStamp());
                    if (message.isController())
                        c = { Controller::Type::cc, message.getControllerNumber(), c.start, message.getControllerValue(), false };
                    else if (message.isPitchWheel())
                        c = { Controller::Type::pitchbend, 0, c.start, message.getPitchWheelValue(), false };
                    else if (message.isChannelPressure())
                        c = { Controller::Type::aftertouch, 0, c.start, message.getChannelPressureValue(), false };
                    else if (message.isAftertouch())
                        c = { Controller::Type::polyPressure, message.getNoteNumber(), c.start, message.getAfterTouchValue(), false };
                    else
                        c = { Controller::Type::programChange, 0, c.start, message.getProgramChangeNumber(), false };
                    track.controllers.push_back (c);
                    continue;
                }
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
                if (track.notes.empty())
                    continue;   // (controllers alone, as a format-0 file's channel with no notes: left out, as before)
                track.name = name.isNotEmpty() ? name : "MIDI " + juce::String ((int) result.tracks.size() + 1).paddedLeft ('0', 2);
                if (format == 0 && byChannel.size() > 1)
                    track.name << " (Ch. " << track.channel << ")";
                std::stable_sort (track.notes.begin(), track.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
                std::stable_sort (track.controllers.begin(), track.controllers.end(), [] (const Controller& a, const Controller& b) { return a.start < b.start; });
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
