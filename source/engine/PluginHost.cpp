#include "PluginHost.h"
#include "SessionLog.h"

#include "ParagonSynth.h"

namespace pr7s
{
    namespace
    {
        const juce::String knownPluginsKey { "knownPlugins" };
    }

    PluginHost::PluginHost (juce::PropertiesFile* settingsToUse, bool findSynth) : settings (settingsToUse), findStudioSynth (findSynth)
    {
        // VST3 only, for now (AU on macOS and others can come later).
        formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

        if (settings != nullptr)
            if (auto xml = settings->getXmlValue (knownPluginsKey))
                known.recreateFromXml (*xml);
        known.addChangeListener (this);
    }

    PluginHost::~PluginHost()
    {
        known.removeChangeListener (this);
    }

    void PluginHost::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        if (settings == nullptr)
            return;
        if (auto xml = known.createXml())
        {
            settings->setValue (knownPluginsKey, xml.get());
            settings->saveIfNeeded();
        }
    }

    namespace
    {
        juce::Array<juce::PluginDescription> sortedByMaker (juce::Array<juce::PluginDescription> list)
        {
            std::sort (list.begin(), list.end(), [] (const auto& a, const auto& b)
            {
                return a.manufacturerName == b.manufacturerName ? a.name.compareNatural (b.name) < 0
                                                                : a.manufacturerName.compareNatural (b.manufacturerName) < 0;
            });
            return list;
        }
    }

    juce::Array<juce::PluginDescription> PluginHost::getInstruments() const
    {
        juce::Array<juce::PluginDescription> instruments;
        for (const auto& type : known.getTypes())
            if (type.isInstrument)
                instruments.add (type);
        return sortedByMaker (instruments);
    }

    juce::Array<juce::PluginDescription> PluginHost::getEffects() const
    {
        juce::Array<juce::PluginDescription> effects;
        for (const auto& type : known.getTypes())
            if (! type.isInstrument)
                effects.add (type);
        return sortedByMaker (effects);
    }

    std::unique_ptr<juce::AudioPluginInstance> PluginHost::createEffect (const Insert& insert, double sampleRate, int blockSize,
                                                                          juce::String& error)
    {
        SessionLog::note ("Loading the effect " + describe (insert.plugin));
        auto instance = formats.createPluginInstance (insert.plugin, sampleRate, blockSize, error);
        if (instance == nullptr)
        {
            if (error.isEmpty())
                error = "Couldn't load " + insert.plugin.name;
            SessionLog::note ("  It didn't load: " + error);
            return nullptr;
        }
        if (insert.state.getSize() > 0)
        {
            SessionLog::note ("  Giving it its settings (" + juce::String ((int) insert.state.getSize()) + " bytes)");
            instance->setStateInformation (insert.state.getData(), (int) insert.state.getSize());
        }
        SessionLog::note ("  Loaded.");
        return instance;
    }

    juce::String PluginHost::studioSynthName() { return "PR7-K"; }

    juce::String PluginHost::describe (const juce::PluginDescription& d)
    {
        // "Kontakt 8 (Native Instruments, 8.2.0, C:\Program Files\Common Files\VST3\Kontakt 8.vst3)"
        return d.name + " (" + d.manufacturerName + (d.version.isNotEmpty() ? ", " + d.version : juce::String()) + ", " + d.fileOrIdentifier + ")";
    }

    namespace
    {
        // "0.47.0" against "0.24.0", part by part.
        bool newer (const juce::String& a, const juce::String& b)
        {
            auto pa = juce::StringArray::fromTokens (a, ".", {}), pb = juce::StringArray::fromTokens (b, ".", {});
            for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
            {
                const auto x = pa[i].getIntValue(), y = pb[i].getIntValue();
                if (x != y)
                    return x > y;
            }
            return false;
        }
    }

    const juce::PluginDescription* PluginHost::getStudioSynth()
    {
        if (! findStudioSynth)
            return nullptr;
        if (! studioSynthLooked)
        {
            studioSynthLooked = true;
            juce::VST3PluginFormat vst3;
            // Beside the app (the release ships it there; on macOS beside the .app), then the VST3 folders.
            juce::Array<juce::File> places;
            const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
            places.add (app.getParentDirectory().getChildFile (studioSynthFile));
            const auto folders = vst3.getDefaultLocationsToSearch();
            for (int i = 0; i < folders.getNumPaths(); ++i)
                places.add (folders.getRawString (i).isNotEmpty() ? juce::File (folders.getRawString (i)).getChildFile (studioSynthFile) : juce::File());
            for (const auto& place : places)
            {
                if (place == juce::File() || ! place.exists())
                    continue;
                juce::OwnedArray<juce::PluginDescription> found;
                vst3.findAllTypesForFile (found, place.getFullPathName());
                for (auto* description : found)
                    if (description->isInstrument && (! studioSynth || newer (description->version, studioSynth->version)))
                        studioSynth = *description;
            }
        }
        return studioSynth ? &*studioSynth : nullptr;
    }

    std::unique_ptr<juce::AudioPluginInstance> PluginHost::create (const Instrument& instrument, double sampleRate, int blockSize,
                                                                    juce::String& error)
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        if (instrument.kind == Instrument::Kind::paragonSynth)
        {
            // The studio's synth: PR7-K (0.9.11); the old Paragon Synth if it isn't found or won't load.
            if (const auto* synth = getStudioSynth())
            {
                SessionLog::note ("Loading the studio's synth " + describe (*synth));
                juce::String problem;
                instance = formats.createPluginInstance (*synth, sampleRate, blockSize, problem);
                if (instance == nullptr)
                    SessionLog::note ("  It didn't load (" + problem + "): the built-in Paragon Synth instead");
            }
            if (instance == nullptr)
                instance = std::make_unique<ParagonSynth>();
        }
        else
        {
            SessionLog::note ("Loading the instrument " + describe (instrument.plugin));
            instance = formats.createPluginInstance (instrument.plugin, sampleRate, blockSize, error);
        }

        if (instance == nullptr)
        {
            if (error.isEmpty())
                error = "Couldn't load " + instrument.getName();
            SessionLog::note ("  It didn't load: " + error);
            return nullptr;
        }
        if (instrument.state.getSize() > 0)
        {
            SessionLog::note ("  Giving it its settings (" + juce::String ((int) instrument.state.getSize()) + " bytes)");
            instance->setStateInformation (instrument.state.getData(), (int) instrument.state.getSize());
        }
        SessionLog::note ("  Loaded.");
        return instance;
    }

    juce::File PluginHost::deadMansPedalFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Paragon PR7-S").getChildFile ("ScanInProgress.txt");
    }
}
