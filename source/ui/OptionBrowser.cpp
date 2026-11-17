#include "ui/OptionBrowser.h"

#include "ui/ParagonLookAndFeel.h"
#include "engine/Project.h"

namespace
{
    juce::Font words (float h, int style = juce::Font::bold) { return ParagonLookAndFeel::textFont (h, style); }
    juce::Font heading (float h) { return ParagonLookAndFeel::titleFont (h); }

    constexpr pr7s::OptionFeel feels[] = { pr7s::OptionFeel::straight, pr7s::OptionFeel::half, pr7s::OptionFeel::doubleTime, pr7s::OptionFeel::broken };
    const juce::Colour rowLine { 0xff1d191f }, selectedRow { 0xff262228 }, starOff { 0xff6f676b };
}

OptionBrowser::OptionBrowser (const juce::String& t, std::vector<Option> list, int chosenId, pr7s::OptionMemory m, const juce::String& key)
    : title (t), listKey (key), options (std::move (list)), memory (std::move (m)), chosen (chosenId)
{
    setOpaque (true);
    search.setTextToShowWhenEmpty ("SEARCH: TOMS, CRASH, GHOST...", ParagonColours::textDim);
    search.setFont (words (15.0f, juce::Font::plain));
    search.onTextChange = [this] { scroll = 0.0f; repaint(); };
    search.onEscapeKey = [this] { search.clear(); scroll = 0.0f; repaint(); };
    addAndMakeVisible (search);
    for (const auto f : feels)
    {
        auto& k = *feelKeys.emplace_back (std::make_unique<juce::TextButton> (pr7s::optionFeelName (f)));
        k.setClickingTogglesState (false);
        k.onClick = [this, f] { setFeel (feel == f ? pr7s::OptionFeel::unknown : f); };
        addAndMakeVisible (k);
    }
    for (int e = 1; e <= 3; ++e)
    {
        auto& k = *energyKeys.emplace_back (std::make_unique<juce::TextButton> (pr7s::energyName (e)));
        k.onClick = [this, e] { setEnergy (energy == e ? 0 : e); };
        addAndMakeVisible (k);
    }
    hearKey.onClick = [this] { hear(); };
    useKey.onClick = [this] { use(); };
    favouriteKey.onClick = [this] { toggleFavourite (chosen); };
    for (auto* k : { &hearKey, &useKey, &favouriteKey })
        addAndMakeVisible (*k);
    if (find (chosen) == nullptr && ! options.empty())
        chosen = options.front().id;
    for (const auto& o : options)
    {
        measured = measured || o.measure.feel != pr7s::OptionFeel::unknown;
        drawn = drawn || ! o.idea.notes.empty();
    }
    for (auto& k : feelKeys)
        k->setVisible (measured);
    for (auto& k : energyKeys)
        k->setVisible (measured);
    for (auto* k : getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (k))
        {
            b->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);   // (the studio's keys: lit when on)
            b->setWantsKeyboardFocus (false);
        }
    setSize (width, height);
    refreshKeys();
}

OptionBrowser::~OptionBrowser()
{
    if (onClosed)
        onClosed();
}

const OptionBrowser::Option* OptionBrowser::find (int id) const
{
    for (const auto& o : options)
        if (o.id == id)
            return &o;
    return nullptr;
}

bool OptionBrowser::inGroup (const Option& o, const juce::String& g) const
{
    if (g == everythingGroup)
        return true;
    if (g == favouritesGroup)
        return memory.isFavourite (listKey, o.name);
    if (g == recentGroup)
        return memory.recentOf (listKey).contains (o.name);
    return o.group == g;
}

bool OptionBrowser::passes (const Option& o) const
{
    const auto words = juce::StringArray::fromTokens (search.getText().toLowerCase(), true);
    if (! words.isEmpty())
    {
        // A search looks through every group.
        const auto text = (o.name + " " + o.description + " " + o.group).toLowerCase();
        for (const auto& w : words)
            if (! text.contains (w))
                return false;
    }
    else if (! inGroup (o, group))
        return false;
    if (feel != pr7s::OptionFeel::unknown && o.measure.feel != feel)
        return false;
    if (energy != 0 && o.measure.energy != energy)
        return false;
    return true;
}

juce::StringArray OptionBrowser::getGroups() const
{
    juce::StringArray out { favouritesGroup, recentGroup, everythingGroup };
    juce::StringArray present;
    for (const auto& o : options)
        if (o.group.isNotEmpty())
            present.addIfNotAlreadyThere (o.group);
    for (const auto& g : groupOrder)
        if (present.contains (g))
            out.addIfNotAlreadyThere (g);
    for (const auto& g : present)
        out.addIfNotAlreadyThere (g);
    return out;
}

void OptionBrowser::setGroupOrder (const juce::StringArray& order)
{
    groupOrder = order;
    repaint();
}

int OptionBrowser::countIn (const juce::String& g) const
{
    auto n = 0;
    for (const auto& o : options)
        n += inGroup (o, g) ? 1 : 0;
    return n;
}

std::vector<int> OptionBrowser::getShown() const
{
    std::vector<int> out;
    if (group == recentGroup && search.isEmpty())
    {
        // The recent in the order used.
        for (const auto& name : memory.recentOf (listKey))
            for (const auto& o : options)
                if (o.name == name && passes (o))
                    out.push_back (o.id);
        return out;
    }
    for (const auto& o : options)
        if (passes (o))
            out.push_back (o.id);
    return out;
}

bool OptionBrowser::isFavourite (int id) const
{
    const auto* o = find (id);
    return o != nullptr && memory.isFavourite (listKey, o->name);
}

void OptionBrowser::setGroup (const juce::String& g)
{
    group = g;
    search.clear();
    scroll = 0.0f;
    repaint();
}

void OptionBrowser::setSearch (const juce::String& text)
{
    search.setText (text, juce::dontSendNotification);
    scroll = 0.0f;
    repaint();
}

void OptionBrowser::setFeel (pr7s::OptionFeel f)
{
    feel = f;
    scroll = 0.0f;
    refreshKeys();
    repaint();
}

void OptionBrowser::setEnergy (int e)
{
    energy = juce::jlimit (0, 3, e);
    scroll = 0.0f;
    refreshKeys();
    repaint();
}

void OptionBrowser::choose (int id)
{
    if (find (id) == nullptr)
        return;
    chosen = id;
    showChosen();
    refreshKeys();
    repaint();
}

void OptionBrowser::showChosen()
{
    // The chosen option scrolled into the list's view.
    if (listArea.isEmpty())
        return;
    const auto shown = getShown();
    const auto at = std::find (shown.begin(), shown.end(), chosen);
    if (at == shown.end())
        return;
    const auto top = (float) std::distance (shown.begin(), at) * rowHeight;
    const auto room = (float) listArea.getHeight() - rowHeight;
    if (top < scroll)
        scroll = top;
    else if (top > scroll + room)
        scroll = top - room;
}

void OptionBrowser::hear()
{
    if (find (chosen) != nullptr && onHear)
        onHear (chosen);
}

void OptionBrowser::use()
{
    if (const auto* o = find (chosen))
    {
        memory.noteUsed (listKey, o->name);
        if (onUse)
            onUse (chosen);
    }
}

void OptionBrowser::toggleFavourite (int id)
{
    const auto* o = find (id);
    if (o == nullptr)
        return;
    const auto on = ! memory.isFavourite (listKey, o->name);
    memory.setFavourite (listKey, o->name, on);
    if (onFavourite)
        onFavourite (o->name, on);
    refreshKeys();
    repaint();
}

juce::TextButton* OptionBrowser::keyNamed (const juce::String& name)
{
    for (auto& k : feelKeys)
        if (k->getButtonText() == name)
            return k.get();
    for (auto& k : energyKeys)
        if (k->getButtonText() == name)
            return k.get();
    for (auto* k : { &hearKey, &useKey, &favouriteKey })
        if (k->getButtonText() == name)
            return k;
    if (name == "FAVOURITE")
        return &favouriteKey;
    return nullptr;
}

void OptionBrowser::refreshKeys()
{
    for (size_t i = 0; i < feelKeys.size(); ++i)
        feelKeys[i]->setToggleState (feel == feels[i], juce::dontSendNotification);
    for (size_t i = 0; i < energyKeys.size(); ++i)
        energyKeys[i]->setToggleState (energy == (int) i + 1, juce::dontSendNotification);
    const auto fav = isFavourite (chosen);
    favouriteKey.setButtonText (fav ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 FAVOURITE")) : juce::String ("FAVOURITE"));
    favouriteKey.setToggleState (fav, juce::dontSendNotification);
    const auto any = find (chosen) != nullptr;
    hearKey.setEnabled (any);
    useKey.setEnabled (any);
}

//==============================================================================
void OptionBrowser::setHearable (bool can)
{
    hearable = can;
    hearKey.setVisible (can);
    resized();
}

void OptionBrowser::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (40);
    groupsArea = r.removeFromLeft (210);
    r.removeFromLeft (10);
    detailArea = r.removeFromRight (330);
    r.removeFromRight (10);
    auto middle = r;
    search.setBounds (middle.removeFromTop (30));
    middle.removeFromTop (6);
    if (! measured)
        middle.removeFromTop (-6);   // (no FEEL / ENERGY rows: the list right under the search)
    auto feelRow = measured ? middle.removeFromTop (26) : juce::Rectangle<int>();
    feelRow.removeFromLeft (64);
    const auto feelW = feelRow.getWidth() / 4;
    for (auto& k : feelKeys)
        k->setBounds (feelRow.removeFromLeft (feelW).reduced (2, 0));
    if (measured)
        middle.removeFromTop (4);
    auto energyRow = measured ? middle.removeFromTop (26) : juce::Rectangle<int>();
    energyRow.removeFromLeft (64);
    for (auto& k : energyKeys)
        k->setBounds (energyRow.removeFromLeft (feelW).reduced (2, 0));
    middle.removeFromTop (8);
    listArea = middle;

    auto d = detailArea.reduced (12);
    auto keys = d.removeFromBottom (32);
    const auto kw = keys.getWidth() / (hearable ? 3 : 2);
    if (hearable)
        hearKey.setBounds (keys.removeFromLeft (kw).reduced (3, 0));
    useKey.setBounds (keys.removeFromLeft (kw).reduced (3, 0));
    favouriteKey.setBounds (keys.reduced (3, 0));
    d.removeFromBottom (10);
    pictureArea = drawn ? d.removeFromBottom (120) : juce::Rectangle<int> (d.getX(), d.getBottom(), d.getWidth(), 0);
    showChosen();
}

juce::Rectangle<int> OptionBrowser::groupRow (int index) const
{
    return { groupsArea.getX(), groupsArea.getY() + 34 + index * groupHeight, groupsArea.getWidth(), groupHeight };
}

juce::Rectangle<int> OptionBrowser::listRow (int index) const
{
    return { listArea.getX(), listArea.getY() + juce::roundToInt ((float) index * rowHeight - scroll), listArea.getWidth(), rowHeight };
}

void OptionBrowser::paint (juce::Graphics& g)
{
    g.fillAll (ParagonColours::background);
    auto top = getLocalBounds().reduced (12).removeFromTop (34);
    g.setColour (ParagonColours::text);
    g.setFont (heading (28.0f));
    g.drawText (title.toUpperCase(), top, juce::Justification::centredLeft);
    g.setColour (ParagonColours::textDim);
    g.setFont (words (14.0f, juce::Font::plain));
    g.drawText (juce::String ((int) getShown().size()) + " OF " + juce::String ((int) options.size()) + " SHOWN", top, juce::Justification::centredRight);

    for (const auto& area : { groupsArea, listArea.withTop (listArea.getY()), detailArea })
    {
        g.setColour (ParagonColours::panel);
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (ParagonColours::panelEdge);
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, 1.0f);
    }

    // Groups.
    g.setColour (ParagonColours::textDim);
    g.setFont (heading (18.0f));
    g.drawText ("GROUPS", groupsArea.withHeight (34).reduced (12, 0), juce::Justification::centredLeft);
    const auto groups = getGroups();
    for (int i = 0; i < groups.size(); ++i)
    {
        const auto row = groupRow (i);
        const auto on = groups[i] == group && search.isEmpty();
        if (on)
        {
            g.setColour (selectedRow);
            g.fillRect (row);
            g.setGradientFill (ParagonLookAndFeel::familyGradient (row.getTopLeft().toFloat(), row.getBottomLeft().toFloat()));
            g.fillRect (row.withWidth (3));
        }
        const auto name = groups[i] == favouritesGroup ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) + groups[i] : groups[i];
        g.setColour (on ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (words (14.0f));
        g.drawText (name, row.reduced (12, 0), juce::Justification::centredLeft);
        g.setFont (words (13.0f, juce::Font::plain));
        g.drawText (juce::String (countIn (groups[i])), row.reduced (12, 0), juce::Justification::centredRight);
    }

    // Labels for the key rows.
    g.setColour (ParagonColours::textDim);
    g.setFont (heading (16.0f));
    if (measured && ! feelKeys.empty())
        g.drawText ("FEEL", feelKeys.front()->getBounds().withX (listArea.getX()).withWidth (60), juce::Justification::centredLeft);
    if (measured && ! energyKeys.empty())
        g.drawText ("ENERGY", energyKeys.front()->getBounds().withX (listArea.getX()).withWidth (62), juce::Justification::centredLeft);

    // The list.
    {
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (listArea.reduced (1));
        const auto shown = getShown();
        if (shown.empty())
        {
            g.setColour (ParagonColours::textDim);
            g.setFont (words (14.0f, juce::Font::plain));
            g.drawFittedText (group == favouritesGroup && search.isEmpty() && feel == pr7s::OptionFeel::unknown && energy == 0
                                  ? "NO FAVOURITES YET: CLICK THE STAR BESIDE AN OPTION TO ADD IT"
                                  : measured ? "NOTHING PASSES: CLEAR THE SEARCH OR A FEEL / ENERGY KEY" : "NOTHING PASSES: CLEAR THE SEARCH",
                              listArea.reduced (16), juce::Justification::centredTop, 3);
        }
        for (int i = 0; i < (int) shown.size(); ++i)
        {
            const auto row = listRow (i);
            if (! row.intersects (listArea))
                continue;
            const auto& o = *find (shown[(size_t) i]);
            if (o.id == chosen)
            {
                g.setColour (selectedRow);
                g.fillRect (row);
                g.setGradientFill (ParagonLookAndFeel::familyGradient (row.getTopLeft().toFloat(), row.getBottomLeft().toFloat()));
                g.fillRect (row.withWidth (3));
            }
            g.setColour (rowLine);
            g.drawHorizontalLine (row.getBottom() - 1, (float) row.getX(), (float) row.getRight());
            auto r = row.reduced (8, 0);
            const auto fav = memory.isFavourite (listKey, o.name);
            g.setColour (fav ? ParagonColours::accent : starOff);
            g.setFont (words (16.0f, juce::Font::plain));
            g.drawText (juce::String (juce::CharPointer_UTF8 (fav ? "\xe2\x98\x85" : "\xe2\x98\x86")), r.removeFromLeft (22), juce::Justification::centredLeft);
            const auto tag = ! search.isEmpty() || ! measured ? o.group
                             : o.measure.feel == pr7s::OptionFeel::unknown ? juce::String()
                             : pr7s::optionFeelName (o.measure.feel) + ", " + pr7s::energyName (o.measure.energy);
            g.setColour (ParagonColours::textDim);
            g.setFont (words (12.0f, juce::Font::plain));
            g.drawText (tag, r.removeFromRight (150), juce::Justification::centredRight);
            g.setColour (o.id == chosen ? ParagonColours::text : ParagonColours::text.withAlpha (0.85f));
            g.setFont (words (14.0f));
            g.drawText (o.name, r, juce::Justification::centredLeft, true);
        }
    }

    // The chosen one.
    if (const auto* o = find (chosen))
    {
        auto d = detailArea.reduced (14);
        g.setColour (ParagonColours::textDim);
        g.setFont (heading (15.0f));
        g.drawText (o->group.toUpperCase(), d.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (ParagonColours::text);
        g.setFont (heading (26.0f));
        g.drawFittedText (o->name.toUpperCase(), d.removeFromTop (60), juce::Justification::topLeft, 2);
        if (o->measure.feel != pr7s::OptionFeel::unknown)
        {
            g.setColour (ParagonColours::accent);
            g.setFont (words (13.0f));
            g.drawText (pr7s::optionFeelName (o->measure.feel) + "  /  " + pr7s::energyName (o->measure.energy), d.removeFromTop (20), juce::Justification::centredLeft);
        }
        d.removeFromTop (6);
        g.setColour (ParagonColours::textDim);
        g.setFont (words (13.0f, juce::Font::plain));
        g.drawFittedText (o->description.isNotEmpty() ? o->description : juce::String ("(no description)"),
                          d.withBottom (pictureArea.getY() - 6), juce::Justification::topLeft, 7);
        if (drawn)
            paintPicture (g, *o, pictureArea);
    }
}

void OptionBrowser::paintPicture (juce::Graphics& g, const Option& o, juce::Rectangle<int> area) const
{
    g.setColour (juce::Colour (0xff080609));
    g.fillRect (area);
    const auto notes = o.idea.notes;
    if (notes.empty())
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (words (12.0f, juce::Font::plain));
        g.drawText ("NOTHING TO DRAW HERE (HEAR IT)", area, juce::Justification::centred);
        return;
    }
    const auto beats = 8.0;   // two bars of four
    const auto steps = 32;
    // (0.158.0) A line (a solo) drawn from its busiest two bars: the first two are often a rest before it starts.
    auto from = 0.0;
    if (! o.drums)
    {
        auto end = 0.0;
        for (const auto& n : o.idea.notes)
            end = juce::jmax (end, n.beat);
        auto most = -1;
        for (auto at = 0.0; at <= end; at += 4.0)
        {
            auto count = 0;
            for (const auto& n : o.idea.notes)
                count += ! n.keyswitch && n.beat >= at && n.beat < at + beats ? 1 : 0;
            if (count > most)
            {
                most = count;
                from = at;
            }
        }
    }
    const auto stepW = (float) (area.getWidth() - 40) / (float) steps;
    const auto left = (float) area.getX() + 40.0f;
    // The beat lines.
    for (int s = 0; s <= steps; s += 4)
    {
        g.setColour (s % 16 == 0 ? juce::Colour (0xff3a343c) : juce::Colour (0xff221e24));
        g.drawVerticalLine (juce::roundToInt (left + (float) s * stepW), (float) area.getY(), (float) area.getBottom());
    }
    const auto lit = ParagonLookAndFeel::familyGradient (area.getTopLeft().toFloat(), area.getTopRight().toFloat());
    if (o.drums)
    {
        struct Row { const char* name; std::function<bool (int)> has; };
        const Row rows[] = {
            { "CYM", [] (int p) { return p == 42 || p == 44 || p == 46 || p == 49 || p == 51 || p == 52 || p == 53 || p == 55 || p == 57 || p == 59; } },
            { "SNR", [] (int p) { return p == 37 || p == 38 || p == 39 || p == 40; } },
            { "TOM", [] (int p) { return p == 41 || p == 43 || p == 45 || p == 47 || p == 48 || p == 50; } },
            { "KICK", [] (int p) { return p == 35 || p == 36; } } };
        const auto rowH = (float) area.getHeight() / 4.0f;
        for (int r = 0; r < 4; ++r)
        {
            const auto y = (float) area.getY() + (float) r * rowH;
            g.setColour (ParagonColours::textDim);
            g.setFont (words (11.0f));
            g.drawText (rows[r].name, juce::Rectangle<float> ((float) area.getX() + 4.0f, y, 34.0f, rowH), juce::Justification::centredLeft);
            for (const auto& n : notes)
            {
                if (n.keyswitch || n.beat >= beats || ! rows[r].has (n.pitch))
                    continue;
                const auto s = (float) (n.beat / beats * steps);
                g.setGradientFill (lit);
                g.setOpacity (juce::jlimit (0.35f, 1.0f, n.velocity));
                g.fillRect (juce::Rectangle<float> (left + s * stepW + 1.0f, y + 3.0f, juce::jmax (2.0f, stepW - 2.0f), rowH - 6.0f));
                g.setOpacity (1.0f);
            }
        }
        return;
    }
    auto low = 127, high = 0;
    for (const auto& n : notes)
        if (! n.keyswitch && n.beat >= from && n.beat < from + beats)
        {
            low = juce::jmin (low, n.pitch);
            high = juce::jmax (high, n.pitch);
        }
    if (low > high)
        return;
    const auto span = (float) juce::jmax (12, high - low + 1);
    const auto rowH = (float) (area.getHeight() - 8) / span;
    for (const auto& n : notes)
    {
        if (n.keyswitch || n.beat < from || n.beat >= from + beats)
            continue;
        const auto x = left + (float) ((n.beat - from) / beats * steps) * stepW;
        const auto w = juce::jmax (2.0f, (float) (juce::jmin (n.length, from + beats - n.beat) / beats * steps) * stepW - 1.0f);
        const auto y = (float) area.getBottom() - 4.0f - (float) (n.pitch - low + 1) * rowH;
        g.setGradientFill (lit);
        g.setOpacity (juce::jlimit (0.4f, 1.0f, n.velocity));
        g.fillRect (juce::Rectangle<float> (x, y, w, juce::jmax (2.0f, rowH - 1.0f)));
        g.setOpacity (1.0f);
    }
}

void OptionBrowser::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (groupsArea.contains (p))
    {
        const auto groups = getGroups();
        for (int i = 0; i < groups.size(); ++i)
            if (groupRow (i).contains (p))
                setGroup (groups[i]);
        return;
    }
    if (listArea.contains (p))
    {
        const auto shown = getShown();
        for (int i = 0; i < (int) shown.size(); ++i)
            if (listRow (i).contains (p))
            {
                if (p.x < listArea.getX() + 30)
                    toggleFavourite (shown[(size_t) i]);
                else
                {
                    choose (shown[(size_t) i]);
                    if (e.getNumberOfClicks() > 1)
                        use();
                }
                return;
            }
    }
}

void OptionBrowser::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! listArea.contains (e.getPosition()))
        return;
    const auto most = juce::jmax (0.0f, (float) getShown().size() * rowHeight - (float) listArea.getHeight() + 8.0f);
    scroll = juce::jlimit (0.0f, most, scroll - w.deltaY * 120.0f);
    repaint (listArea);
}
