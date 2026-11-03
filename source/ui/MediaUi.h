#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/MediaBay.h"
#include "engine/Project.h"
#include "ui/ParagonLookAndFeel.h"

// What the MediaBay window and the Media rack (0.22.0) share: small drawing and text helpers,
// and a column of the attribute filter.
namespace MediaUi
{
    inline juce::Font interFont (float height, bool bold = false) { return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain)); }

    // Each type in one of the family's colours (a small tab beside the name).
    inline juce::Colour typeColour (pr7s::MediaType type)
    {
        switch (type)
        {
            case pr7s::MediaType::audio: return pr7s::FamilyColours::orange;
            case pr7s::MediaType::midi: return pr7s::FamilyColours::pink;
            case pr7s::MediaType::trackPreset: return pr7s::FamilyColours::red;
            case pr7s::MediaType::song: return juce::Colour (0xfff4eeea);
            case pr7s::MediaType::pluginPreset: return pr7s::FamilyColours::red;
        }
        return pr7s::FamilyColours::red;
    }

    inline juce::String lengthText (double seconds)
    {
        if (seconds <= 0.0)
            return {};
        const auto whole = (int) std::round (seconds * 10.0);
        const auto minutes = whole / 600;
        const auto rest = (whole % 600) / 10.0;
        return minutes > 0 ? juce::String (minutes) + ":" + juce::String (rest, 1).paddedLeft ('0', 4) : juce::String (rest, 1) + " s";
    }

    inline juce::String sizeText (juce::int64 bytes)
    {
        if (bytes >= 1024 * 1024)
            return juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB";
        return juce::String (juce::jmax ((juce::int64) 1, bytes / 1024)) + " KB";
    }

    inline void drawStars (juce::Graphics& g, juce::Rectangle<float> area, int rating)
    {
        const auto size = juce::jmin (area.getHeight() * 0.62f, area.getWidth() / 5.0f);
        for (int i = 0; i < 5; ++i)
        {
            juce::Path star;
            const auto centre = juce::Point<float> (area.getX() + (float) i * area.getWidth() / 5.0f + area.getWidth() / 10.0f, area.getCentreY());
            star.addStar (centre, 5, size * 0.22f, size * 0.5f);   // (the first point at 12 o'clock)
            if (i < rating)
            {
                g.setColour (pr7s::FamilyColours::orange);
                g.fillPath (star);
            }
            else
            {
                g.setColour (ParagonColours::textDim.withAlpha (0.35f));
                g.strokePath (star, juce::PathStrokeType (1.0f));
            }
        }
    }

    inline juce::String formatText (const pr7s::MediaItem& item)
    {
        juce::String text;
        if (item.type == pr7s::MediaType::audio && item.sampleRate > 0.0)
            text << juce::String (item.sampleRate / 1000.0, item.sampleRate == 44100.0 ? 1 : 0) << " kHz " << (item.channels == 1 ? "mono" : item.channels == 2 ? "stereo" : juce::String (item.channels) + " ch");
        else if (item.type == pr7s::MediaType::midi)
            text << juce::String (item.notes) << " notes";
        else if (item.type == pr7s::MediaType::pluginPreset)
            text << item.presetPlugin;
        return text;
    }

    inline juce::String joinedPair (const juce::String& a, const juce::String& b)
    {
        return b.isEmpty() ? a : a + " / " + b;
    }

    // An attribute in the settings file: its kind's number, or "user:" and its name.
    inline juce::String attributeToken (const pr7s::AttributeId& id)
    {
        return id.kind == pr7s::MediaAttribute::user ? "user:" + id.user : juce::String ((int) id.kind);
    }

    inline pr7s::AttributeId attributeFromToken (const juce::String& token)
    {
        if (token.startsWith ("user:"))
            return { pr7s::MediaAttribute::user, token.substring (5) };
        const auto kind = token.getIntValue();
        if (token.containsOnly ("0123456789") && token.isNotEmpty() && kind >= 0 && kind < (int) pr7s::MediaAttribute::user)
            return { (pr7s::MediaAttribute) kind, {} };
        return {};
    }

    inline const juce::String mixedText = "(MIXED)";

    // The attributes a filter column can show (the user's after ours).
    inline std::vector<pr7s::AttributeId> filterableAttributes (const pr7s::MediaDatabase& database)
    {
        using A = pr7s::MediaAttribute;
        std::vector<pr7s::AttributeId> list;
        for (const auto kind : { A::category, A::subCategory, A::style, A::subStyle, A::character, A::key, A::tempo, A::rating })
            list.push_back ({ kind, {} });
        for (const auto& user : database.getUserAttributes())
            list.push_back ({ A::user, user.name });
        return list;
    }

    // A stored value as shown: stars for a rating, YES / NO for a yes/no attribute.
    inline juce::String shownValue (const pr7s::MediaDatabase& database, const pr7s::AttributeId& id, const juce::String& value)
    {
        if (value.isEmpty() || value == mixedText)
            return value;
        if (id.kind == pr7s::MediaAttribute::rating)
            return juce::String::repeatedString (juce::String::fromUTF8 ("\xe2\x98\x85"), juce::jlimit (0, 5, value.getIntValue()));
        if (id.kind == pr7s::MediaAttribute::tempo)
            return value + " BPM";
        if (id.kind == pr7s::MediaAttribute::user)
            if (const auto* user = database.findUserAttribute (id.user); user != nullptr && user->type == pr7s::UserAttributeType::yesNo)
                return value == "1" ? "YES" : "NO";
        return value;
    }

    inline void styleKey (juce::TextButton& key)
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setWantsKeyboardFocus (false);
    }

    // Whoever shows attribute filter columns: the MediaBay window or the Media rack.
    struct AttributeFilterHost
    {
        virtual ~AttributeFilterHost() = default;
        virtual const pr7s::MediaDatabase& getFilterDatabase() const = 0;
        virtual pr7s::AttributeId getFilterColumn (int column) const = 0;
        virtual void setFilterColumn (int column, const pr7s::AttributeId&) = 0;
        virtual std::vector<std::pair<juce::String, int>> getFilterValues (int column) const = 0;
        virtual juce::StringArray getFilterSelection (int column) const = 0;
        virtual void clickFilterValue (int column, const juce::String& value, bool addToOthers) = 0;
    };

    // One column of the attribute filter: its attribute (the title: click to choose another) and
    // the values found, with how many files have each. Click a value: only it; click it again: none;
    // Ctrl-click: one more (or one less).
    class AttributeFilterColumn final : public juce::Component,
                                        private juce::ListBoxModel
    {
    public:
        AttributeFilterColumn (AttributeFilterHost&, int index);
        ~AttributeFilterColumn() override;
        void refresh();
        void resized() override;

    private:
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void showAttributeMenu();

        AttributeFilterHost& host;
        const int index;
        juce::TextButton title;
        juce::ListBox list;
        std::vector<std::pair<juce::String, int>> values;
    };
}
