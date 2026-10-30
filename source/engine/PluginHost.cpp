#include "PluginHost.h"

#include "ParagonSynth.h"

namespace pr7s
{
    namespace
    {
        const juce::String knownPluginsKey { "knownPlugins" };
    }

    PluginHost::PluginHost (juce::PropertiesFile* settingsToUse) : settings (settingsToUse)
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
        auto instance = formats.createPluginInstance (insert.plugin, sampleRate, blockSize, error);
        if (instance == nullptr)
        {
            if (error.isEmpty())
                error = "Couldn't load " + insert.plugin.name;
            return nullptr;
        }
        if (insert.state.getSize() > 0)
            instance->setStateInformation (insert.state.getData(), (int) insert.state.getSize());
        return instance;
    }

    std::unique_ptr<juce::AudioPluginInstance> PluginHost::create (const Instrument& instrument, double sampleRate, int blockSize,
                                                                    juce::String& error)
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        if (instrument.kind == Instrument::Kind::paragonSynth)
            instance = std::make_unique<ParagonSynth>();
        else
            instance = formats.createPluginInstance (instrument.plugin, sampleRate, blockSize, error);

        if (instance == nullptr)
        {
            if (error.isEmpty())
                error = "Couldn't load " + instrument.getName();
            return nullptr;
        }
        if (instrument.state.getSize() > 0)
            instance->setStateInformation (instrument.state.getData(), (int) instrument.state.getSize());
        return instance;
    }

    juce::File PluginHost::deadMansPedalFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Paragon PR7-S").getChildFile ("ScanInProgress.txt");
    }
}
