#include "Pr7dGrooves.h"

namespace pr7s
{
    namespace
    {
        // PR7-D's drums ("slots", its NoteMap.h order): BD RIM SD CLP SD2 LT CH MT PH HT OH CY RD RB CR2 CHN SPL HH TMB
        // SHK RE HO1 HO3 T4 HSH C1C C2C CHC SPC C3 C4 C5 C6 CFX CH2 T5, then THE PADS P1-P6.
        constexpr int numSlots = 42;

        // Each note's slot in PR7-D's GROOVE LIBRARY layout (its libraryNotes, the hats' pedal-following notes as the
        // closed hat, as PR7-D reads grooves). Made from PR7-D's table by a script, not typed.
        constexpr std::array<int, 128> librarySlot {
            36, 37, 38, 39, 40, 41, 2, 6, 6, 6, 8, 24, 21, 21, 17, 22, 10, 10, 6, 6, 6, 8, 24, 8, 21, 17, 22, 15, 11, 33, 29,
            30, 32, 2, 0, 0, 0, 1, 2, 2, 4, 35, 6, 23, 8, 5, 17, 7, 9, 14, 26, 12, 34, 13, 34, 16, 28, 31, 31, 20, 10, 6, 24,
            6, 21, 6, 2, 1, 2, 2, 2, 1, 35, 35, 23, 23, 33, 5, 5, 7, 7, 9, 9, 27, 12, 13, 20, 12, 13, 12, 13, 20, 12, 13, 25,
            29, 12, 13, 20, 12, 13, 12, 13, 20, 12, 13, 30, 32, 12, 13, 20, 12, 13, 12, 13, 20, 12, 13, 12, 6, 21, 22, 24, 21,
            10, 2, 4, 1 };

        // General MIDI's notes on PR7-D's slots (its generalMidi()).
        int gmSlot (int note)
        {
            switch (note)
            {
                case 35: case 36: return 0;
                case 37: return 1;
                case 38: return 2;
                case 39: return 3;
                case 40: return 4;
                case 43: return 5;
                case 41: return 23;
                case 42: return 6;
                case 45: case 47: return 7;
                case 44: return 8;
                case 48: case 50: return 9;
                case 46: return 10;
                case 49: return 11;
                case 51: case 59: return 12;
                case 52: return 15;
                case 53: return 13;
                case 54: return 18;
                case 55: return 16;
                case 57: return 14;
                case 69: case 70: return 19;
                default: return -1;
            }
        }

        // The note each slot is written on: General MIDI's (PR7-D's homeNote; where General MIDI has none, the nearest
        // General MIDI sound: our choice), and the PR7-D Map's (PR7-D's homeNote for GROOVE LIBRARY; the clap on the
        // snare). -1: no such sound there (left out).
        constexpr std::array<int, numSlots> gmHome {
            36, 37, 38, 39, 40, 43, 42, 45, 44, 48, 46, 49, 51, 53, 57, 52, 55, 46, 54, 69,
            59, 46, 46, 41, 42, 49, 57, 52, 55, 49, 57, 57, 49, 49, 52, 41,
            -1, -1, -1, -1, -1, -1 };
        constexpr std::array<int, numSlots> libraryHome {
            36, 37, 38, 38, 40, 45, 42, 47, 44, 48, 60, 28, 51, 53, 49, 27, 55, 25, -1, -1,
            59, 24, 26, 43, 22, 94, 50, 83, 56, 30, 31, 57, 32, 29, 52, 41,
            0, 1, 2, 3, 4, 5 };
    }

    juce::File pr7dPresetsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              #if JUCE_MAC
               .getChildFile ("Application Support")
              #endif
               .getChildFile ("Paragon").getChildFile ("Paragon PR7-D").getChildFile ("Rigs");
    }

    Pr7dLibrary readPr7dLibrary (const juce::File& folder)
    {
        Pr7dLibrary library;
        library.folder = folder;
        const auto settings = folder.getChildFile ("Grooves.xml"), index = folder.getChildFile ("Groove Index.xml");
        const auto linked = juce::XmlDocument::parse (settings);
        if (linked == nullptr)
            return library;
        library.found = true;
        struct Linked { juce::File folder; int layout; };
        std::vector<Linked> folders;
        for (const auto* f : linked->getChildWithTagNameIterator ("Folder"))
            if (! f->getBoolAttribute ("excluded"))
                folders.push_back ({ juce::File (f->getStringAttribute ("path")), juce::jlimit (0, 3, f->getIntAttribute ("map", 1)) });
        library.indexDate = index.getLastModificationTime();
        const auto listed = juce::XmlDocument::parse (index);
        if (listed == nullptr)
            return library;
        for (const auto* x : listed->getChildWithTagNameIterator ("G"))
        {
            if (x->getStringAttribute ("stamp").startsWith ("db"))
                continue;   // (inside a groove database: not a .mid file)
            Pr7dGroove g;
            g.file = juce::File (x->getStringAttribute ("path"));
            const Linked* home = nullptr;
            for (const auto& f : folders)
                if (g.file.isAChildOf (f.folder))
                {
                    home = &f;
                    break;
                }
            if (home == nullptr || ! g.file.hasFileExtension ("mid;midi"))
                continue;
            g.layout = home->layout;
            g.name = g.file.getFileNameWithoutExtension();
            g.folder = g.file.getParentDirectory().getFileName();
            g.bpm = x->getIntAttribute ("bpmKnown") != 0 ? x->getDoubleAttribute ("bpm", 0.0) : 0.0;
            g.numerator = juce::jlimit (1, 32, x->getIntAttribute ("num", 4));
            g.denominator = juce::jlimit (1, 32, x->getIntAttribute ("den", 4));
            g.bars = juce::jmax (1, x->getIntAttribute ("bars", 1));
            g.intensity = (float) x->getDoubleAttribute ("intensity");
            g.tags = juce::StringArray::fromTokens (x->getStringAttribute ("tags"), "|", "");
            g.tags.removeEmptyStrings();
            library.grooves.push_back (std::move (g));
        }
        return library;
    }

    int drumSlotOf (int note, int layout)
    {
        if (note < 0 || note > 127)
            return -1;
        const auto slot = layout == 1 ? librarySlot[(size_t) note]
                        : layout == 2 ? gmSlot (note)
                        : (note - 36 >= 0 && note - 36 < numSlots ? note - 36 : -1);
        return slot >= 0 && slot < numSlots ? slot : -1;
    }

    int drumNoteOf (int slot, DrumNotes target)
    {
        if (slot < 0 || slot >= numSlots)
            return -1;
        return (target == DrumNotes::pr7d ? libraryHome : gmHome)[(size_t) slot];
    }

    int convertDrumNote (int note, int layout, DrumNotes target)
    {
        if (note < 0 || note > 127)
            return -1;
        if (target == DrumNotes::asWritten || layout == 3)
            return note;   // (CUSTOM: only PR7-D knows its map)
        if ((layout == 1 && target == DrumNotes::pr7d) || (layout == 2 && target == DrumNotes::generalMidi))
            return note;
        const auto slot = layout == 1 ? librarySlot[(size_t) note]
                        : layout == 2 ? gmSlot (note)
                        : (note - 36 >= 0 && note - 36 < numSlots ? note - 36 : -1);
        if (slot < 0 || slot >= numSlots)
            return -1;
        return (target == DrumNotes::pr7d ? libraryHome : gmHome)[(size_t) slot];
    }
}
