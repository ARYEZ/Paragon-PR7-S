#include "PluginHost.h"
#include "SessionLog.h"
#include "KeepLoaded.h"

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
        // (0.73.3) Each part of a load is a marked step, so a freeze report says which one it stopped in
        // (the owner's Kontakt 8 freeze said "nothing marked": these were plain notes).
        std::unique_ptr<juce::AudioPluginInstance> instance;
        {
            SessionLog::Step step ("Loading the effect " + describe (insert.plugin));
            instance = formats.createPluginInstance (insert.plugin, sampleRate, blockSize, error);
        }
        if (instance == nullptr)
        {
            if (error.isEmpty())
                error = "Couldn't load " + insert.plugin.name;
            SessionLog::note ("  It didn't load: " + error);
            return nullptr;
        }
        keepPluginLoaded (insert.plugin.fileOrIdentifier);   // (0.80.1: never freed mid-session)
        if (insert.state.getSize() > 0)
        {
            SessionLog::Step step ("Giving " + insert.plugin.name + " its settings (" + juce::String ((int) insert.state.getSize()) + " bytes)");
            instance->setStateInformation (insert.state.getData(), (int) insert.state.getSize());
        }
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

    std::optional<juce::PluginDescription> PluginHost::findBackline (const char* file)
    {
        std::optional<juce::PluginDescription> best;
        juce::VST3PluginFormat vst3;
        // Beside the app (the release ships it there; on macOS beside the .app), then the VST3 folders.
        juce::Array<juce::File> places;
        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        places.add (app.getParentDirectory().getChildFile (file));
        const auto folders = vst3.getDefaultLocationsToSearch();
        for (int i = 0; i < folders.getNumPaths(); ++i)
            places.add (folders.getRawString (i).isNotEmpty() ? juce::File (folders.getRawString (i)).getChildFile (file) : juce::File());
        for (const auto& place : places)
        {
            if (place == juce::File() || ! place.exists())
                continue;
            juce::OwnedArray<juce::PluginDescription> found;
            vst3.findAllTypesForFile (found, place.getFullPathName());
            for (auto* description : found)
                if (description->isInstrument && (! best || newer (description->version, best->version)))
                    best = *description;
        }
        return best;
    }

    const juce::PluginDescription* PluginHost::getStudioSynth()
    {
        if (! findStudioSynth)
            return nullptr;
        if (! studioSynthLooked)
        {
            studioSynthLooked = true;
            studioSynth = findBackline (studioSynthFile);
        }
        return studioSynth ? &*studioSynth : nullptr;
    }

    const juce::PluginDescription* PluginHost::getStudioDrums()
    {
        if (! findStudioSynth)
            return nullptr;
        if (! studioDrumsLooked)
        {
            studioDrumsLooked = true;
            studioDrums = findBackline (studioDrumsFile);
            if (studioDrums)
                SessionLog::note ("The studio's drums: " + describe (*studioDrums));
        }
        return studioDrums ? &*studioDrums : nullptr;
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
                juce::String problem;
                {
                    SessionLog::Step step ("Loading the studio's synth " + describe (*synth));
                    instance = formats.createPluginInstance (*synth, sampleRate, blockSize, problem);
                }
                if (instance == nullptr)
                    SessionLog::note ("  It didn't load (" + problem + "): the built-in Paragon Synth instead");
                else
                    keepPluginLoaded (synth->fileOrIdentifier);   // (0.80.1)
            }
            if (instance == nullptr)
                instance = std::make_unique<ParagonSynth>();
        }
        else
        {
            {
                SessionLog::Step step ("Loading the instrument " + describe (instrument.plugin));
                instance = formats.createPluginInstance (instrument.plugin, sampleRate, blockSize, error);
            }
            if (instance != nullptr)
                keepPluginLoaded (instrument.plugin.fileOrIdentifier);   // (0.80.1: never freed mid-session)
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
            SessionLog::Step step ("Giving " + instrument.getName() + " its settings (" + juce::String ((int) instrument.state.getSize()) + " bytes)");
            instance->setStateInformation (instrument.state.getData(), (int) instrument.state.getSize());
        }
        return instance;
    }

    juce::File PluginHost::deadMansPedalFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Paragon PR7-S").getChildFile ("ScanInProgress.txt");
    }
}
