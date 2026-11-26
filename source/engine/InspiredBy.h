#pragma once

#include <juce_core/juce_core.h>

namespace pr7s
{
    // (0.216.0) The owner's own notes of which songs inspired each ARCHITECT style. They live only on the owner's
    // computer, in a plain text file (Documents/Paragon PR7-S/Inspired By.txt): one line a style,
    // "STYLE NAME | what inspired it", lines starting with # left out. The repo and the releases never hold it; without
    // the file nothing is added. Read again when the file's date changes. Message thread.
    juce::String inspiredBy (const juce::String& styleName);
    juce::File inspiredByFile();
    void setInspiredByFileForTests (const juce::File&);   // (an empty File: back to the usual one)
}
