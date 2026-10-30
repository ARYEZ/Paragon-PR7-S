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

    juce::Array<juce::PluginDescription> PluginHost::getInstruments() const
    {
        juce::Array<juce::PluginDescription> instruments;
        for (const auto& type : known.getTypes())
            if (type.isInstrument)
                instruments.add (type);
        std::sort (instruments.begin(), instruments.end(), [] (const auto& a, const auto& b)
        {
            return a.manufacturerName == b.manufacturerName ? a.name.compareNatural (b.name) < 0
                                                            : a.manufacturerName.compareNatural (b.manufacturerName) < 0;
        });
        return instruments;
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
