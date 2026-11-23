#include "InspiredBy.h"

#include <map>

namespace pr7s
{
    namespace
    {
        juce::File& testFile() { static juce::File f; return f; }

        struct Kept
        {
            juce::File file;
            juce::Time date;
            juce::int64 size = -1;
            std::map<juce::String, juce::String> byName;
        };
        Kept& kept() { static Kept k; return k; }
    }

    juce::File inspiredByFile()
    {
        if (testFile() != juce::File())
            return testFile();
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S").getChildFile ("Inspired By.txt");
    }

    void setInspiredByFileForTests (const juce::File& f)
    {
        testFile() = f;
        kept() = {};
    }

    juce::String inspiredBy (const juce::String& styleName)
    {
        const auto file = inspiredByFile();
        auto& k = kept();
        const auto exists = file.existsAsFile();
        const auto date = exists ? file.getLastModificationTime() : juce::Time();
        const auto size = exists ? file.getSize() : (juce::int64) -1;
        if (file != k.file || date != k.date || size != k.size)
        {
            k = {};
            k.file = file;
            k.date = date;
            k.size = size;
            if (exists)
            {
                juce::StringArray lines;
                file.readLines (lines);
                for (const auto& line : lines)
                {
                    const auto trimmed = line.trim();
                    if (trimmed.isEmpty() || trimmed.startsWithChar ('#') || ! trimmed.containsChar ('|'))
                        continue;
                    const auto name = trimmed.upToFirstOccurrenceOf ("|", false, false).trim().toUpperCase();
                    const auto what = trimmed.fromFirstOccurrenceOf ("|", false, false).trim();
                    if (name.isNotEmpty() && what.isNotEmpty())
                    {
                        auto& entry = k.byName[name];
                        entry = entry.isEmpty() ? what : entry + "; " + what;
                    }
                }
            }
        }
        const auto found = k.byName.find (styleName.trim().toUpperCase());
        return found != k.byName.end() ? found->second : juce::String();
    }
}
