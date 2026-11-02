#include <catch2/catch_test_macros.hpp>

#include "DeviceTests.h"
#include "engine/AudioAdvice.h"
#include "engine/Studio.h"
#include "ui/Windows.h"

using namespace pr7s;

TEST_CASE ("The first time, an ASIO driver is chosen over Windows Audio", "[devices]")
{
    CHECK (AudioAdvice::firstDeviceType ({ "Windows Audio", "Windows Audio (Exclusive Mode)", "DirectSound", "ASIO" }) == "ASIO");
    CHECK (AudioAdvice::firstDeviceType ({ "Windows Audio", "DirectSound" }).isEmpty());   // JUCE's choice stays
    CHECK (AudioAdvice::firstDeviceType ({ "CoreAudio" }).isEmpty());
}

TEST_CASE ("The buffer's line says the size now and where a bigger one is set", "[devices]")
{
    SECTION ("Windows Audio: the one size Windows sets; ASIO or Exclusive Mode for more")
    {
        const auto text = AudioAdvice::bufferText ("Windows Audio", 480, 48000.0, { 480 }, false);
        CHECK (text.contains ("480 samples (10.0 ms at 48 kHz)"));
        CHECK (text.contains ("can't be raised"));
        CHECK (text.contains ("ASIO"));
        CHECK (text.contains ("Exclusive Mode"));
    }
    SECTION ("ASIO with one size: its CONTROL PANEL, then RELOAD DRIVER")
    {
        const auto text = AudioAdvice::bufferText ("ASIO", 64, 44100.0, { 64 }, true);
        CHECK (text.contains ("64 samples (1.5 ms at 44.1 kHz)"));
        CHECK (text.contains ("only this size"));
        CHECK (text.contains ("CONTROL PANEL"));
        CHECK (text.contains ("RELOAD DRIVER"));
    }
    SECTION ("ASIO with a range: its range")
    {
        CHECK (AudioAdvice::bufferText ("ASIO", 256, 48000.0, { 32, 64, 128, 256, 512, 1024, 2048 }, true).contains ("32 to 2048"));
    }
    SECTION ("Others: up to their largest")
    {
        CHECK (AudioAdvice::bufferText ("Windows Audio (Exclusive Mode)", 512, 48000.0, { 64, 512, 2048 }, false).contains ("Up to 2048"));
    }
    CHECK (AudioAdvice::bufferText ({}, 0, 0.0, {}, false).contains ("no audio device"));
}

TEST_CASE ("SETTINGS: a size set in the driver's own panel is read with RELOAD DRIVER", "[devices][ui]")
{
    Studio studio (false);
    auto& devices = studio.getDeviceManager();
    auto* driver = new FakeDriver ("ASIO", { 64 }, true);   // as the AXE I/O's: only the panel's size
    devices.addAudioDeviceType (std::unique_ptr<juce::AudioIODeviceType> (driver));
    devices.setCurrentAudioDeviceType ("ASIO", true);
    REQUIRE (devices.getCurrentAudioDevice() != nullptr);
    CHECK (devices.getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 64);

    Windows::Settings settings (studio, {}, {});
    CHECK (settings.getBufferText().contains ("64 samples"));
    CHECK (settings.getBufferText().contains ("CONTROL PANEL"));
    CHECK (settings.isReloadShown());

    // In the driver's panel: 2048. RELOAD DRIVER opens it again at that.
    driver->panelSize = 2048;
    settings.clickReload();
    REQUIRE (devices.getCurrentAudioDevice() != nullptr);
    CHECK (devices.getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 2048);
    CHECK (devices.getAudioDeviceSetup().bufferSize == 2048);
    CHECK (settings.getBufferText().contains ("2048 samples"));
}

TEST_CASE ("SETTINGS: Windows Audio's line points to ASIO; no RELOAD DRIVER", "[devices][ui]")
{
    Studio studio (false);
    auto& devices = studio.getDeviceManager();
    devices.addAudioDeviceType (std::make_unique<FakeDriver> ("Windows Audio", juce::Array<int> { 480 }, false));
    devices.setCurrentAudioDeviceType ("Windows Audio", true);
    REQUIRE (devices.getCurrentAudioDevice() != nullptr);

    Windows::Settings settings (studio, {}, {});
    CHECK (settings.getBufferText().contains ("480 samples"));
    CHECK (settings.getBufferText().contains ("pick ASIO"));
    CHECK_FALSE (settings.isReloadShown());
}
