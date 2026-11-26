#include "Mp3Writer.h"

#include <lame.h>

namespace pr7s
{
    namespace
    {
        // ID3 v1's fields are Latin-1: anything else becomes '?'.
        std::string latin1 (const juce::String& text)
        {
            std::string out;
            for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
            {
                const auto c = (juce::juce_wchar) *p;
                out.push_back (c > 0 && c < 256 ? (char) (unsigned char) c : '?');
            }
            return out;
        }

        class Mp3Writer final : public juce::AudioFormatWriter
        {
        public:
            Mp3Writer (juce::OutputStream* out, double rate, unsigned int channels, lame_t encoder, juce::int64 tagAt)
                : AudioFormatWriter (out, "MP3 file", rate, channels, 32), lame (encoder), tagStart (tagAt)
            {
                usesFloatingPointData = true;   // write() is given floats (AudioFormatWriter::writeFromFloatArrays)
            }

            ~Mp3Writer() override
            {
                if (output != nullptr)
                {
                    // What LAME still holds, then ID3 v1 at the end, then the Info tag over the first frame.
                    encoded.resize (7200 + 4096);
                    const auto flushed = lame_encode_flush (lame, encoded.data(), (int) encoded.size());
                    if (flushed > 0)
                        output->write (encoded.data(), (size_t) flushed);
                    unsigned char v1[256];
                    const auto v1Size = lame_get_id3v1_tag (lame, v1, sizeof (v1));
                    if (v1Size > 0 && v1Size <= sizeof (v1))
                        output->write (v1, v1Size);
                    std::vector<unsigned char> frame (lame_get_lametag_frame (lame, nullptr, 0));
                    if (! frame.empty() && lame_get_lametag_frame (lame, frame.data(), frame.size()) == frame.size())
                    {
                        const auto end = output->getPosition();
                        if (output->setPosition (tagStart))
                        {
                            output->write (frame.data(), frame.size());
                            output->setPosition (end);
                        }
                    }
                    output->flush();
                }
                lame_close (lame);
            }

            bool write (const int** samples, int numSamples) override
            {
                if (numSamples <= 0)
                    return true;
                const auto* const* floats = reinterpret_cast<const float* const*> (samples);
                const auto* left = floats[0];
                const auto* right = numChannels > 1 && floats[1] != nullptr ? floats[1] : left;
                // LAME's own rule for the buffer: 1.25 x the samples + 7200 bytes is always enough.
                constexpr int piece = 8192;
                for (int done = 0; done < numSamples; done += piece)
                {
                    const auto n = juce::jmin (piece, numSamples - done);
                    encoded.resize ((size_t) (n + n / 4 + 7200));
                    const auto bytes = lame_encode_buffer_ieee_float (lame, left + done, right + done, n, encoded.data(), (int) encoded.size());
                    if (bytes < 0)
                        return false;
                    if (bytes > 0 && ! output->write (encoded.data(), (size_t) bytes))
                        return false;
                }
                return true;
            }

        private:
            lame_t lame;
            juce::int64 tagStart;
            std::vector<unsigned char> encoded;
        };
    }

    const std::vector<int>& mp3BitRates()
    {
        static const std::vector<int> rates { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
        return rates;
    }

    const std::vector<int>& mp3SampleRates()
    {
        static const std::vector<int> rates { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 };
        return rates;
    }

    juce::StringArray mp3Genres()
    {
        juce::StringArray genres;
        id3tag_genre_list ([] (int, const char* name, void* list) { static_cast<juce::StringArray*> (list)->add (name); }, &genres);
        genres.sortNatural();
        return genres;
    }

    juce::String Mp3Settings::toString() const
    {
        juce::StringArray parts { juce::String (bitRate), juce::String (sampleRate), highQuality ? "1" : "0", id3 ? "1" : "0",
                                  title, artist, album, year, genre, comment, track };
        for (auto& p : parts)
            p = p.replace ("|", "/");
        return parts.joinIntoString ("|");
    }

    Mp3Settings Mp3Settings::fromString (const juce::String& text)
    {
        Mp3Settings s;
        juce::StringArray parts;
        parts.addTokens (text, "|", {});
        if (parts.size() < 4)
            return s;
        const auto rate = parts[0].getIntValue();
        if (std::find (mp3BitRates().begin(), mp3BitRates().end(), rate) != mp3BitRates().end())
            s.bitRate = rate;
        const auto hz = parts[1].getIntValue();
        if (hz == 0 || std::find (mp3SampleRates().begin(), mp3SampleRates().end(), hz) != mp3SampleRates().end())
            s.sampleRate = hz;
        s.highQuality = parts[2] == "1";
        s.id3 = parts[3] == "1";
        s.title = parts[4];
        s.artist = parts[5];
        s.album = parts[6];
        s.year = parts[7];
        s.genre = parts[8];
        s.comment = parts[9];
        s.track = parts[10];
        return s;
    }

    std::unique_ptr<juce::AudioFormatWriter> makeMp3Writer (std::unique_ptr<juce::OutputStream>& stream, double sampleRate,
                                                            int numChannels, const Mp3Settings& settings)
    {
        if (stream == nullptr || sampleRate < 1000.0 || numChannels < 1 || numChannels > 2)
            return nullptr;
        auto* lame = lame_init();
        if (lame == nullptr)
            return nullptr;
        const auto fail = [lame]() -> std::unique_ptr<juce::AudioFormatWriter> { lame_close (lame); return nullptr; };
        lame_set_num_channels (lame, numChannels);
        lame_set_in_samplerate (lame, juce::roundToInt (sampleRate));
        lame_set_out_samplerate (lame, settings.highQuality ? 0 : settings.sampleRate);   // 0: LAME's choice (as the input, if it can)
        lame_set_mode (lame, numChannels == 1 ? MONO : JOINT_STEREO);
        lame_set_VBR (lame, vbr_off);
        lame_set_brate (lame, settings.bitRate);
        lame_set_quality (lame, settings.highQuality ? 2 : 5);
        lame_set_bWriteVbrTag (lame, 1);             // the Info tag (length, delay and padding)
        lame_set_write_id3tag_automatic (lame, 0);   // the tags written here, where they go
        if (settings.id3)
        {
            id3tag_init (lame);
            id3tag_add_v2 (lame);
            const auto set = [lame] (void (*setter) (lame_t, const char*), const juce::String& value)
            {
                if (value.isNotEmpty())
                    setter (lame, latin1 (value).c_str());
            };
            set (id3tag_set_title, settings.title);
            set (id3tag_set_artist, settings.artist);
            set (id3tag_set_album, settings.album);
            set (id3tag_set_year, settings.year);
            set (id3tag_set_comment, settings.comment);
            if (settings.genre.isNotEmpty())
                id3tag_set_genre (lame, latin1 (settings.genre).c_str());
            if (settings.track.isNotEmpty())
                id3tag_set_track (lame, latin1 (settings.track).c_str());
        }
        if (lame_init_params (lame) < 0)
            return fail();
        if (settings.id3)
        {
            std::vector<unsigned char> v2 (lame_get_id3v2_tag (lame, nullptr, 0));
            if (! v2.empty())
            {
                if (lame_get_id3v2_tag (lame, v2.data(), v2.size()) != v2.size() || ! stream->write (v2.data(), v2.size()))
                    return fail();
            }
        }
        // The Info tag goes over the first frame at the end: the stream must go back there.
        const auto tagAt = stream->getPosition();
        if (! stream->setPosition (tagAt))
            return fail();
        return std::make_unique<Mp3Writer> (stream.release(), sampleRate, (unsigned int) numChannels, lame, tagAt);
    }
}
