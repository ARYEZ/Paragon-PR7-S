#pragma once

#include "Project.h"
#include "Pr7dGrooves.h"

#include <juce_audio_basics/juce_audio_basics.h>

// THE STUDIO LINK (0.108.0, the owner: "full integration" with PR7-D, the studio's drums). When the composer's drums
// go onto a track playing PR7-D, PR7-D plays them from its own SONG TRACK (where they're edited with PR7-D's tools) and
// the track keeps a muted copy to look at (`Part::linkCopy`). The two talk through a file and a MIDI message:
//   - PR7-S writes the copies as PR7-D's song (PR7-D's own song XML: a block per part, its hits on PR7-D's drums) to a
//     file in `pr7dLinkFolder()` named by the track's `drumLink`, and sends PR7-D, on its track, a SysEx message "load
//     that file". PR7-D swaps its song (not its kit) and plays it with the studio's playhead (FOLLOW DAW).
//   - PR7-D writes your edits there back to the same file; PR7-S reads it again and the copies follow.
// The messages (PR7-D's song/StudioLink.h has the same; keep them in step), between F0 and F7:
//   7D 50 52 37 53 cmd [payload]: cmd 01 load (the path, UTF-8, each byte as two bytes of 4 bits), 02 pause (the composer
//   is playing a drum idea through PR7-D: its song stays silent), 03 resume.
namespace pr7s
{
    enum class Pr7dCommand { load = 1, pause = 2, resume = 3 };

    // Where the link files live (the user's settings folder: "Paragon PR7-S/PR7-D Link").
    juce::File pr7dLinkFolder();
    // One of the link's messages.
    juce::MidiMessage pr7dLinkMessage (Pr7dCommand, const juce::String& path = {});

    // A track's linked copies as PR7-D's song (the `Song` element): a block per part in time order (cut short where the
    // next starts: PR7-D's blocks never overlap), each heard note on its PR7-D drum (`pr7dMap`: the track's notes are the
    // PR7-D Map's, else General MIDI's), the song's time signatures, and the link file's path.
    std::unique_ptr<juce::XmlElement> pr7dSongXml (const std::vector<Part>& copies, bool pr7dMap, int numerator, int denominator,
                                                   const std::vector<SignatureEvent>& changes, const juce::String& linkPath);
    // PR7-D's song as linked copies (muted parts: each block's hits played round to its length, its opening hit).
    std::vector<Part> pr7dPartsFromSong (const juce::XmlElement& song, bool pr7dMap);
    // What the copies hold, to tell when they've changed (start, length, name, notes).
    juce::String pr7dCopiesFingerprint (const std::vector<Part>& copies);
}
