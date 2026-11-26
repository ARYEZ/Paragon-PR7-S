// Speed, round nine (0.190.0): the engine's and the message thread's findings (the reviewers' engine.md and
// message.md). Each change keeps what is heard and shown the same; these check it does, and that the work is gone.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/Engine.h"
#include "engine/Warp.h"
#include "engine/MediaBay.h"
#include "engine/Composer.h"
#include "engine/SectionTypes.h"
#include "engine/AudioWorkers.h"

using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    juce::AudioBuffer<float> rampOf (int samples)
    {
        juce::AudioBuffer<float> b (2, samples);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < samples; ++i)
                b.setSample (c, i, 0.1f * (float) std::sin (i * 0.013 + c));
        return b;
    }

    Part warpedPart (double markerBeat)
    {
        Part part;
        part.audioFile = "take.wav";
        part.length = 8.0;
        part.warp = { { 0.0, 0.0 }, { markerBeat, 1.7 }, { 8.0, 4.0 } };
        return part;
    }

    double msOf (const std::function<void()>& f, int times = 1)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < times; ++i)
            f();
        return (juce::Time::getMillisecondCounterHiRes() - start) / times;
    }

    struct ThreeTracks
    {
        Studio studio { false };
        std::vector<int> ids;
        ThreeTracks()
        {
            ids.push_back (studio.getProject().tracks[0].id);
            for (int t = 0; t < 2; ++t)
            {
                studio.addTrack ({});
                ids.push_back (studio.getProject().tracks.back().id);
            }
            for (const auto id : ids)
                for (int p = 0; p < 2; ++p)
                {
                    Part part;
                    part.start = p * 8.0;
                    part.length = 4.0;
                    for (int i = 0; i < 16; ++i)
                        part.notes.push_back ({ i * 0.25 + 0.03, 0.2, 50 + i, 0.7f });
                    studio.addPart (id, part);
                }
        }
        const Track& track (int i) const { return *studio.getProject().findTrack (ids[(size_t) i]); }
    };
}

TEST_CASE ("0.190.0: the warp cache keeps a sound a clip still holds, and forgets the least recently used first", "[speed190]")
{
    const auto recording = rampOf ((int) rate * 6);
    const auto tempo = Project().tempoMap();
    WarpRenderer renderer;
    const auto held = renderer.render (warpedPart (3.0), recording, rate, tempo);
    REQUIRE (held != nullptr);
    // Far more whole clips than may be kept: only the unheld ones go.
    for (int i = 0; i < (int) WarpRenderer::mostWhole + 20; ++i)
        renderer.render (warpedPart (2.0 + i * 0.01), recording, rate, tempo);
    CHECK (renderer.wholeKept() <= WarpRenderer::mostWhole + 2);   // (the held one, and the newest while it is handed back)
    CHECK (renderer.render (warpedPart (3.0), recording, rate, tempo) == held);   // (still there: the very same sound)
    // The least recently used of the unheld went first: the last one made is still kept.
    const auto last = renderer.render (warpedPart (2.0 + (WarpRenderer::mostWhole + 19) * 0.01), recording, rate, tempo);
    const auto again = renderer.render (warpedPart (2.0 + (WarpRenderer::mostWhole + 19) * 0.01), recording, rate, tempo);
    CHECK (last == again);
    // One file forgotten, the others kept.
    auto other = warpedPart (3.0);
    other.audioFile = "other.wav";
    const auto otherSound = renderer.render (other, recording, rate, tempo);
    renderer.forgetFile ("take.wav");
    CHECK (renderer.render (other, recording, rate, tempo) == otherSound);
    CHECK (renderer.render (warpedPart (3.0), recording, rate, tempo) != held);   // (made again: a new sound)
}

TEST_CASE ("0.190.0: the warp assembly gives the samples the sample-by-sample copy gave", "[speed190]")
{
    // At its own speed (markers 1:1 with the recording's seconds at 120 BPM), the clip is the recording itself,
    // the joins crossfaded with the recording before each marker (the same sound: unchanged).
    const auto recording = rampOf ((int) rate * 6);
    const auto tempo = Project().tempoMap();
    Part part;
    part.audioFile = "take.wav";
    part.length = 8.0;
    part.warp = { { 0.0, 0.25 }, { 2.0, 1.25 }, { 4.0, 2.25 }, { 8.0, 4.25 } };
    WarpRenderer renderer;
    const auto sound = renderer.render (part, recording, rate, tempo);
    REQUIRE (sound != nullptr);
    const auto from = (int) std::llround (0.25 * rate);
    auto worst = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < sound->getNumSamples() && from + i < recording.getNumSamples(); ++i)
            worst = juce::jmax (worst, std::abs (sound->getSample (c, i) - recording.getSample (c, from + i)));
    CHECK (worst < 1.0e-6f);
}

TEST_CASE ("0.190.0: an unchanged update keeps a warped clip without asking for its stretch again", "[speed190]")
{
    Engine engine;
    engine.prepare (rate, 512);
    const auto recording = std::make_shared<juce::AudioBuffer<float>> (rampOf ((int) rate * 6));
    WarpRenderer renderer;
    engine.setAudioSource ([&] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return recording; });
    engine.setWarpSource ([&] (const Part& part, const TempoMap& tempo) { return renderer.render (part, *recording, rate, tempo); });
    juce::uint64 stamp = 1;
    engine.setSoundStamp ([&] (const juce::String&) { return stamp; });
    Project project;
    Track track;
    track.id = 1;
    track.kind = Track::Kind::audio;
    track.parts = { warpedPart (3.0) };
    project.tracks.push_back (track);
    engine.addAudioTrack (1);
    engine.update (project);
    const auto calls = engine.getWarpSourceCalls();
    const auto builds = engine.getClipBuilds();
    CHECK (calls == 1);
    // A fader move, a pan: kept, nothing asked.
    project.tracks[0].volumeDb = -3.0f;
    engine.update (project);
    project.tracks[0].pan = 0.2f;
    engine.update (project);
    CHECK (engine.getWarpSourceCalls() == calls);
    CHECK (engine.getClipBuilds() == builds);
    // The recording's sound changed (its stamp): asked again, built again.
    ++stamp;
    engine.update (project);
    CHECK (engine.getWarpSourceCalls() == calls + 1);
    CHECK (engine.getClipBuilds() == builds + 1);
    // A marker moved: the part differs, built again.
    project.tracks[0].parts[0].warp[1].beat = 3.5;
    engine.update (project);
    CHECK (engine.getClipBuilds() == builds + 2);
}

TEST_CASE ("0.190.0: edits to some parts leave the other tracks' parts shared", "[speed190]")
{
    ThreeTracks song;
    auto& studio = song.studio;
    const auto partOf = [&] (int t, int p) { return song.track (t).parts[(size_t) p].id; };
    const auto heldOthers = [&] { return std::vector<PartList> { song.track (1).parts, song.track (2).parts }; };
    const auto stillShared = [&] (const std::vector<PartList>& held)
    {
        return song.track (1).parts.sharesWith (held[0]) && song.track (2).parts.sharesWith (held[1]);
    };
    {
        const auto held = heldOthers();
        studio.quantizeParts ({ partOf (0, 0) }, 0.25);
        CHECK (stillShared (held));
    }
    {
        const auto held = heldOthers();
        studio.humanizeParts ({ partOf (0, 0) }, 0.5f);
        CHECK (stillShared (held));
    }
    {
        const auto held = heldOthers();
        studio.setPartLane (partOf (0, 1), 1);
        CHECK (stillShared (held));
        CHECK (song.track (0).parts[1].lane == 1);
    }
    {
        const auto held = heldOthers();
        studio.resetWarp ({ partOf (0, 0) });
        studio.quantizeAudio ({ partOf (0, 0) }, 0.25);
        CHECK (stillShared (held));
    }
    {
        const auto held = heldOthers();
        const auto gone = partOf (0, 0);
        studio.deleteParts ({ gone });
        CHECK (stillShared (held));
        CHECK (song.track (0).parts.size() == 1);
    }
}

TEST_CASE ("0.190.0: the song's key guessed once per change, and seen again after an edit", "[speed190]")
{
    ThreeTracks song;
    auto& studio = song.studio;
    const auto first = studio.getGuessedSongKey();
    REQUIRE (first.has_value());
    CHECK (studio.getGuessedSongKey() == first);
    CHECK (studio.getGuessedSongKey() == guessSongKey (studio.getProject()));
    // Every note moved a tritone (no announce: the parts changed under it): guessed again.
    auto& project = const_cast<Project&> (studio.getProject());
    for (auto& track : project.tracks)
        for (auto& part : track.parts)
            for (auto& n : part.notes)
                n.pitch += 6;
    CHECK (studio.getGuessedSongKey() == guessSongKey (studio.getProject()));
}

TEST_CASE ("0.190.0: the filter counts in one walk are the counts a walk per column gave", "[speed190]")
{
    std::vector<MediaItem> items;
    for (int i = 0; i < 400; ++i)
    {
        MediaItem item;
        item.file = juce::File ("/library").getChildFile ("Pack " + juce::String (i % 7)).getChildFile ("sound " + juce::String (i) + ".wav");
        item.type = i % 5 == 0 ? MediaType::midi : MediaType::audio;
        item.name = item.file.getFileNameWithoutExtension();
        item.bpm = 80 + i % 4 * 10;
        items.push_back (item);
    }
    juce::TemporaryFile store (".xml");
    MediaDatabase db (store.getFile());
    db.setItemsForTests (items);
    const auto& all = db.getItems();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const juce::Array<juce::File> one { all[(size_t) i].file };
        if (i % 3 == 0) db.setAttribute (one, { MediaAttribute::category }, i % 2 ? "Drums" : "Bass");
        if (i % 4 == 0) db.setAttribute (one, { MediaAttribute::style }, i % 3 ? "Rock" : "Pop");
        if (i % 6 == 0) db.setRatings (one, 1 + i % 5);
    }
    const std::vector<AttributeId> columns { { MediaAttribute::category }, { MediaAttribute::style }, { MediaAttribute::tempo }, { MediaAttribute::rating } };
    // The old way: for each column, every item that passes the query without that column's filters.
    const auto oldWay = [&] (const MediaQuery& query, const AttributeId& column)
    {
        std::map<juce::String, int> counts;
        for (const auto& item : db.getItems())
        {
            if (! query.types.empty() && query.types.count (item.type) == 0)
                continue;
            if (item.rating < query.minimumRating)
                continue;
            auto ok = true;
            for (const auto& f : query.attributes)
            {
                if (f.attribute == column || f.values.isEmpty())
                    continue;
                const auto has = MediaDatabase::attributeValues (item, f.attribute);
                auto any = false;
                for (const auto& v : f.values)
                    any = any || has.contains (v);
                ok = ok && any;
            }
            if (ok)
                for (const auto& v : MediaDatabase::attributeValues (item, column))
                    ++counts[v];
        }
        std::vector<std::pair<juce::String, int>> list (counts.begin(), counts.end());
        const auto numeric = column.kind == MediaAttribute::tempo || column.kind == MediaAttribute::rating;
        std::sort (list.begin(), list.end(), [numeric] (const auto& a, const auto& b)
        {
            return numeric ? a.first.getDoubleValue() < b.first.getDoubleValue() : a.first.compareNatural (b.first) < 0;
        });
        return list;
    };
    for (int round = 0; round < 4; ++round)
    {
        MediaQuery query;
        if (round >= 1)
            query.attributes.push_back ({ { MediaAttribute::category }, juce::StringArray ("Drums") });
        if (round >= 2)
            query.attributes.push_back ({ { MediaAttribute::style }, juce::StringArray ("Rock", "Pop") });
        if (round >= 3)
        {
            query.types = { MediaType::audio };
            query.minimumRating = 1;
        }
        const auto counts = db.valueCountsAll (query, columns);
        for (size_t c = 0; c < columns.size(); ++c)
        {
            CHECK (counts[c] == oldWay (query, columns[c]));
            CHECK (db.valueCounts (query, columns[c]) == counts[c]);
        }
        // And what's found is what the filters give.
        auto expected = 0;
        for (const auto& item : db.getItems())
        {
            auto ok = (query.types.empty() || query.types.count (item.type) > 0) && item.rating >= query.minimumRating;
            for (const auto& f : query.attributes)
            {
                const auto has = MediaDatabase::attributeValues (item, f.attribute);
                auto any = false;
                for (const auto& v : f.values)
                    any = any || has.contains (v);
                ok = ok && any;
            }
            expected += ok ? 1 : 0;
        }
        CHECK ((int) db.find (query).size() == expected);
    }
    // A tag changed: the counts follow.
    db.setAttribute ({ db.getItems()[1].file }, { MediaAttribute::category }, "Keys");
    MediaQuery none;
    CHECK (db.valueCountsAll (none, columns)[0] == oldWay (none, columns[0]));
}

TEST_CASE ("0.190.0: the composer's memo gives every lane exactly what composing without it gives", "[speed190]")
{
    ComposerContext c;
    const auto compare = [&] (const ComposerState& s)
    {
        const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
        setComposeMemoForTests (false);
        const auto plain = composeLayers (s, spans, c);
        setComposeMemoForTests (true);
        const auto memo = composeLayers (s, spans, c);
        for (const auto l : composerLayers())
        {
            INFO ("type " << s.sectionType << ", lane " << composerLayerName (l));
            CHECK (memo[(size_t) l].notes == plain[(size_t) l].notes);
            CHECK (memo[(size_t) l].style == plain[(size_t) l].style);
            CHECK (composeLayer (l, s, spans, c).notes == plain[(size_t) l].notes);
        }
    };
    ComposerState s;
    s.lane (ComposerLayer::lead).on = true;
    compare (s);
    s.syncLane = (int) ComposerLayer::guitar;
    compare (s);
    s.lane (ComposerLayer::bass).doubleSync = true;
    compare (s);
    s.syncLane = -1;
    s.lane (ComposerLayer::drums).syncTo = (int) ComposerLayer::bass;
    compare (s);
    s.lane (ComposerLayer::drums).syncTo = -1;
    auto shown = 0;
    for (const auto* name : { "Verse", "Chorus", "Bridge", "Breakdown", "Intro" })
    {
        const auto types = sectionTypesFor (name);
        for (size_t i = 0; i < types.size(); i += 7)
        {
            auto typed = s;
            applySectionType (typed, name, types[i].name);
            compare (typed);
            ++shown;
        }
    }
    CHECK (shown > 10);
}

TEST_CASE ("0.190.0: the helper-thread choice", "[speed190]")
{
    Studio studio (false);
    const auto most = AudioWorkers::helpersForThisComputer();
    CHECK (Studio::helpersForChoice (0) == most);
    CHECK (Studio::helpersForChoice (1) == juce::jmin (1, most));
    CHECK (Studio::helpersForChoice (99) == most);
    if (juce::SystemStats::getEnvironmentVariable ("PARAGON_HELPERS", {}).isNotEmpty())
        return;   // (forced: the choice doesn't count)
    CHECK (studio.getAudioHelperThreads() == 0);   // (multi-core off without devices)
    studio.setMultiCoreAudio (true);
    CHECK (studio.getAudioHelperThreads() == most);
    studio.setAudioHelperChoice (1);
    CHECK (studio.getAudioHelperChoice() == 1);
    CHECK (studio.getAudioHelperThreads() == juce::jmin (1, most));
    studio.setAudioHelperChoice (0);
    CHECK (studio.getAudioHelperThreads() == most);
    studio.setMultiCoreAudio (false);
    CHECK (studio.getAudioHelperThreads() == 0);
}

TEST_CASE ("0.190.0: an update with the automation unchanged plays the lanes as before", "[speed190]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    Track track;
    track.id = 1;
    AutomationLane lane;
    lane.target = AutomationLane::Target::volume;
    lane.points = { { 0.0, 0.2 }, { 4.0, 0.9 } };
    track.automation = { lane };
    track.automationRead = true;
    project.tracks.push_back (track);
    engine.setInstrument (1, std::make_unique<SteadyTone> (0.5f, 0.5f));
    engine.update (project);
    engine.play();
    const auto a = renderSamples (engine, 48000);
    project.tracks[0].pan = 0.0f;   // (nothing changed in the lanes)
    engine.update (project);
    engine.stop();
    engine.setPosition (0.0);
    engine.play();
    const auto b = renderSamples (engine, 48000);
    CHECK (a == b);
    // A point moved: heard.
    project.tracks[0].automation[0].points[1].value = 0.3;
    engine.update (project);
    engine.stop();
    engine.setPosition (0.0);
    engine.play();
    CHECK (renderSamples (engine, 48000) != a);
    const auto played = [&]
    {
        engine.update (project);
        engine.stop();
        engine.setPosition (0.0);
        engine.play();
        return renderSamples (engine, 48000);
    };
    project.tracks[0].automation[0].points[1].value = 0.9;
    CHECK (played() == a);
    // A curve, a mute, the read switch: each heard (the lanes read again, not kept).
    project.tracks[0].automation[0].points[1].curve = 0.8f;
    const auto curved = played();
    CHECK (curved != a);
    project.tracks[0].automation[0].points[1].curve = 0.0f;
    CHECK (played() == a);
    project.tracks[0].automation[0].muted = true;
    const auto muted = played();
    CHECK (muted != a);
    project.tracks[0].automation[0].muted = false;
    CHECK (played() == a);
    project.tracks[0].automationRead = false;
    CHECK (played() == muted);
    project.tracks[0].automationRead = true;
    CHECK (played() == a);
    // Another control: the lane aimed at the pan now.
    project.tracks[0].automation[0].target = AutomationLane::Target::pan;
    CHECK (played() != a);
}

TEST_CASE ("0.190.0: timings", "[.perf-190]")
{
    // Composing every lane, with and without the memo.
    {
        ComposerContext c;
        ComposerState s;
        s.lane (ComposerLayer::lead).on = true;
        const auto types = sectionTypesFor ("Chorus");
        applySectionType (s, "Chorus", types[types.size() / 2].name);
        const auto spans = composerSpans (s, 0.0, c.beatsPerBar);
        setComposeMemoForTests (false);
        const auto without = msOf ([&] { composeLayers (s, spans, c); }, 5);
        setComposeMemoForTests (true);
        const auto with = msOf ([&] { composeLayers (s, spans, c); }, 5);
        std::printf ("compose all lanes (%s): %.2f ms -> %.2f ms\n", s.sectionType.toRawUTF8(), without, with);
    }
    // The filter counts over 120,000 items, four columns.
    {
        std::vector<MediaItem> items;
        for (int i = 0; i < 120000; ++i)
        {
            MediaItem item;
            item.file = juce::File ("/library").getChildFile ("Pack " + juce::String (i % 60)).getChildFile ("s" + juce::String (i) + ".wav");
            item.name = item.file.getFileNameWithoutExtension();
            item.bpm = 80 + i % 90;
            item.key = i % 3 ? "Am" : "C";
            items.push_back (item);
        }
        juce::TemporaryFile store (".xml");
        MediaDatabase db (store.getFile());
        db.setItemsForTests (items);
        const std::vector<AttributeId> columns { { MediaAttribute::category }, { MediaAttribute::style }, { MediaAttribute::tempo }, { MediaAttribute::key } };
        MediaQuery query;
        query.attributes.push_back ({ { MediaAttribute::key }, juce::StringArray ("Am") });
        db.valueCountsAll (query, columns);   // (the ids made once)
        // The old way (0.189.0's): a walk per column, a list of strings per item and filter, counted in a map.
        const auto oldWay = msOf ([&]
        {
            for (const auto& column : columns)
            {
                std::map<juce::String, int> counts;
                for (const auto& item : db.getItems())
                {
                    auto ok = true;
                    for (const auto& f : query.attributes)
                        if (! (f.attribute == column))
                        {
                            const auto has = MediaDatabase::attributeValues (item, f.attribute);
                            auto any = false;
                            for (const auto& v : f.values)
                                any = any || has.contains (v);
                            ok = ok && any;
                        }
                    if (ok)
                        for (const auto& v : MediaDatabase::attributeValues (item, column))
                            ++counts[v];
                }
            }
        }, 3);
        std::printf ("filter counts the old way: %.1f ms\n", oldWay);
        const auto perColumn = msOf ([&] { for (const auto& a : columns) db.valueCounts (query, a); }, 3);
        const auto oneWalk = msOf ([&] { db.valueCountsAll (query, columns); }, 3);
        std::printf ("filter counts, 4 columns, 120,000 files: per column %.1f ms, one walk %.1f ms\n", perColumn, oneWalk);
    }
    // A warped clip's update after a fader move (the stretch asked for again before).
    {
        Engine engine;
        engine.prepare (rate, 512);
        const auto recording = std::make_shared<juce::AudioBuffer<float>> (rampOf ((int) rate * 60));
        WarpRenderer renderer;
        engine.setAudioSource ([&] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return recording; });
        engine.setWarpSource ([&] (const Part& part, const TempoMap& tempo) { return renderer.render (part, *recording, rate, tempo); });
        engine.setSoundStamp ([] (const juce::String&) { return (juce::uint64) 1; });
        Project project;
        Track track;
        track.id = 1;
        track.kind = Track::Kind::audio;
        for (int p = 0; p < 20; ++p)
        {
            auto part = warpedPart (3.0);
            part.start = p * 8.0;
            track.parts.push_back (part);
        }
        project.tracks.push_back (track);
        engine.addAudioTrack (1);
        engine.update (project);
        const auto ms = msOf ([&] { project.tracks[0].volumeDb -= 0.1f; engine.update (project); }, 50);
        std::printf ("update after a fader move, 20 warped clips: %.3f ms (stretches asked %d times)\n", ms, engine.getWarpSourceCalls());
    }
}
