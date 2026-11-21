#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>
#include <vector>

namespace pr7s
{
    // (0.200.0, the owner asked: "add FLAC and mp3 to the export ... build LAME in") MP3 files written
    // by LAME (LGPL v2 or later, built in: cmake/Dependencies.cmake), as a JUCE writer, so an export
    // writes them as it writes WAV: blocks of floats in, an .mp3 out. Constant bit rate; an Info
    // (Xing / LAME) tag in the first frame so players read the length and the encoder's delay and
    // padding (gapless); ID3 v2 at the start and v1 at the end when asked.
    struct Mp3Settings
    {
        int bitRate = 320;       // kbps (constant): one of mp3BitRates()
        int sampleRate = 0;      // Hz written: 0 = as the export (LAME resamples to any other)
        // The manual's High-Quality Mode: LAME's slower, finer search (quality 2, else its usual 5)
        // and the sample rate left to LAME for the bit rate (the sample rate isn't chosen then).
        bool highQuality = true;
        bool id3 = false;        // the manual's Insert ID3 Tag
        juce::String title, artist, album, year, genre, comment, track;

        juce::String toString() const;          // for the settings file
        static Mp3Settings fromString (const juce::String&);
        bool operator== (const Mp3Settings&) const = default;
    };

    const std::vector<int>& mp3BitRates();     // 8 ... 320 kbps (MPEG-1 and MPEG-2's)
    const std::vector<int>& mp3SampleRates();  // 8000 ... 48000 Hz (what MP3 can hold)
    juce::StringArray mp3Genres();             // ID3 v1's list, A to Z

    // An MP3 writer at `sampleRate` in (any rate: LAME resamples as asked), 1 or 2 channels; null if
    // LAME refuses the settings or the stream isn't seekable (the Info tag is written over the first
    // frame at the end). Takes the stream when it succeeds. Write floats (-1 to 1).
    std::unique_ptr<juce::AudioFormatWriter> makeMp3Writer (std::unique_ptr<juce::OutputStream>& stream, double sampleRate,
                                                            int numChannels, const Mp3Settings&);
}
