#include "ui/InstrumentsPage.h"

#include "ui/LinesPage.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    using Profile = pr7s::InstrumentProfile;
    const auto panelColour = juce::Colour (0xff120e13);

    juce::Font font (float size) { return ParagonLookAndFeel::textFont (size); }

    void pedal (juce::Button& key, bool arrow = false)
    {
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        if (arrow)
            key.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
        key.setWantsKeyboardFocus (false);
    }

    void field (juce::TextEditor& e)
    {
        e.setFont (font (15.0f));
        e.setJustification (juce::Justification::centredLeft);
        e.setSelectAllWhenFocused (true);
    }

    juce::String noteText (int note) { return note < 0 ? juce::String() : pr7s::noteName (note); }

    // A field's or key's change made just after its callback: the change can rebuild the editor, and a
    // component mustn't be deleted inside its own callback.
    void later (InstrumentsPage& page, std::function<void()> f)
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<InstrumentsPage> (&page), f = std::move (f)]
        {
            if (safe != nullptr)
                f();
        });
    }
}

//==============================================================================
class InstrumentsPage::List final : public juce::ListBox, private juce::ListBoxModel
{
public:
    explicit List (InstrumentsPage& p) : page (p)
    {
        setModel (this);
        setRowHeight (40);
        setColour (juce::ListBox::backgroundColourId, panelColour);
        setColour (juce::ListBox::outlineColourId, ParagonColours::panelEdge);
        setOutlineThickness (1);
    }
    int getNumRows() override { return (int) page.profiles.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selectedRow) override
    {
        if (! juce::isPositiveAndBelow (row, (int) page.profiles.size()))
            return;
        const auto& p = page.profiles[(size_t) row];
        if (selectedRow)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.18f));
            g.fillRect (0, 0, w, h);
            g.setColour (ParagonColours::accent);
            g.fillRect (0, 4, 3, h - 8);
        }
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (17.0f));
        g.drawText (p.name, 12, 2, w - 16, h / 2 + 2, juce::Justification::bottomLeft, true);
        juce::StringArray uses;
        if (p.forBass) uses.add ("BASS LANE");
        if (p.forGuitar) uses.add ("GUITAR LANE");
        if (p.forLead) uses.add ("LEAD LANE");
        g.setColour (ParagonColours::textDim);
        g.setFont (font (11.0f));
        g.drawText (juce::String (p.kind == Profile::Kind::bass ? "BASS" : "GUITAR") + "  " + uses.joinIntoString (", "),
                    12, h / 2 + 2, w - 16, h / 2 - 4, juce::Justification::topLeft, true);
    }
    void selectedRowsChanged (int row) override
    {
        if (row != page.selected && row >= 0)
            page.selectProfile (row);
    }

private:
    InstrumentsPage& page;
};

//==============================================================================
// The chosen profile's fields: made again by rebuild() whenever its shape changes.
class InstrumentsPage::Editor final : public juce::Component
{
public:
    explicit Editor (InstrumentsPage& p) : page (p) {}

    void make()
    {
        removeAllChildren();
        owned.clear();
        rows.clear();
        auto* profile = page.current();
        if (profile == nullptr)
        {
            setSize (600, 120);
            return;
        }
        auto y = 8;
        const auto label = [this] (const juce::String& text, juce::Rectangle<int> r)
        {
            auto* l = own (new juce::Label ({}, text));
            l->setFont (font (12.0f));
            l->setColour (juce::Label::textColourId, ParagonColours::textDim);
            l->setBounds (r);
        };
        const auto key = [this] (const juce::String& text, juce::Rectangle<int> r, bool lit, std::function<void()> click, bool arrow = false)
        {
            auto* b = own (new juce::TextButton (text));
            pedal (*b, arrow);
            b->setToggleState (lit, juce::dontSendNotification);
            b->onClick = [this, click = std::move (click)] { later (page, click); };
            b->setBounds (r);
            return b;
        };
        const auto edit = [this] (const juce::String& text, juce::Rectangle<int> r, std::function<void (const juce::String&)> commit)
        {
            auto* e = own (new juce::TextEditor());
            field (*e);
            e->setText (text, false);
            e->setBounds (r);
            auto done = [this, e, commit]
            {
                if (commit)
                {
                    const auto text = e->getText();
                    later (page, [commit, text] { commit (text); });
                }
            };
            e->onReturnKey = done;
            e->onFocusLost = done;
            return e;
        };

        // NAME, BASS / GUITAR.
        label ("NAME", { 0, y, 120, rowHeight });
        edit (profile->name, { 120, y, 240, rowHeight }, [this] (const juce::String& t) { page.setName (t); });
        const auto bass = profile->kind == Profile::Kind::bass;
        key ("BASS", { 376, y, 84, rowHeight }, bass, [this] { page.setKind (Profile::Kind::bass); });
        key ("GUITAR", { 466, y, 86, rowHeight }, ! bass, [this] { page.setKind (Profile::Kind::guitar); });
        y += rowHeight + 12;

        // USED FOR.
        label ("USED FOR", { 0, y, 120, rowHeight });
        key ("BASS LANE", { 120, y, 140, rowHeight }, profile->forBass, [this, on = ! profile->forBass] { page.setUse (pr7s::Studio::ProfileUse::bass, on); });
        key ("GUITAR LANE", { 266, y, 140, rowHeight }, profile->forGuitar, [this, on = ! profile->forGuitar] { page.setUse (pr7s::Studio::ProfileUse::guitar, on); });
        key ("LEAD LANE", { 412, y, 140, rowHeight }, profile->forLead, [this, on = ! profile->forLead] { page.setUse (pr7s::Studio::ProfileUse::lead, on); });
        y += rowHeight + 4;
        label ("(THE COMPOSER'S LANES: ONE INSTRUMENT EACH)", { 120, y, 500, 18 });
        y += 26;

        // STRINGS, FRETS.
        label ("STRINGS (LOW TO HIGH)", { 0, y, 200, 18 });
        y += 20;
        auto x = 0;
        for (int i = 0; i < (int) profile->strings.size(); ++i)
        {
            edit (noteText (profile->strings[(size_t) i]), { x, y, 70, rowHeight }, [this, i] (const juce::String& t) { page.setString (i, t); });
            x += 76;
        }
        key ("+", { x, y, 34, rowHeight }, false, [this] { page.addString(); });
        key ("-", { x + 40, y, 34, rowHeight }, false, [this] { page.removeString(); });
        label ("FRETS", { x + 96, y, 54, rowHeight });
        edit (juce::String (profile->frets), { x + 150, y, 50, rowHeight }, [this] (const juce::String& t) { page.setFrets (t); });
        y += rowHeight + 14;

        // RANGE, BEND.
        label ("LOWEST", { 0, y, 70, rowHeight });
        auto* low = edit (noteText (profile->lowest), { 70, y, 80, rowHeight }, {});
        label ("HIGHEST", { 166, y, 76, rowHeight });
        auto* high = edit (noteText (profile->highest), { 242, y, 80, rowHeight }, {});
        const auto commitRange = [this, low, high] (const juce::String&)
        {
            const auto lowText = low->getText(), highText = high->getText();
            later (page, [this, lowText, highText] { page.setRange (lowText, highText); });
        };
        for (auto* e : { low, high })
        {
            e->setTextToShowWhenEmpty ("AUTO", ParagonColours::textDim);
            e->onReturnKey = [commitRange] { commitRange ({}); };
            e->onFocusLost = [commitRange] { commitRange ({}); };
        }
        label ("BEND RANGE", { 342, y, 100, rowHeight });
        edit (juce::String (profile->bendRange), { 442, y, 50, rowHeight }, [this] (const juce::String& t) { page.setBendRange (t); });
        y += rowHeight + 8;
        // (0.63.1) The shortest note, and the palm-muted hits a sixteenth apart in a row.
        label ("SHORTEST NOTE", { 0, y, 120, rowHeight });
        {
            auto* box = own (new juce::ComboBox());
            const std::pair<double, const char*> lengths[] = { { 0.0, "ANY" }, { 0.125, "1/32" }, { 0.25, "1/16" }, { 0.5, "1/8" } };
            for (int i = 0; i < 4; ++i)
                box->addItem (lengths[i].second, i + 1);
            for (int i = 0; i < 4; ++i)
                if (std::abs (lengths[i].first - profile->shortestNote) < 1.0e-6)
                    box->setSelectedId (i + 1, juce::dontSendNotification);
            box->setBounds (120, y, 90, rowHeight);
            box->onChange = [this, box]
            {
                const double values[] = { 0.0, 0.125, 0.25, 0.5 };
                const auto chosen = values[juce::jlimit (0, 3, box->getSelectedId() - 1)];
                later (page, [this, chosen] { page.setShortestNote (chosen); });
            };
        }
        label ("16TH CHUGS IN A ROW", { 230, y, 170, rowHeight });
        {
            auto* box = own (new juce::ComboBox());
            box->addItem ("ANY", 1);
            for (int n = 1; n <= 4; ++n)
                box->addItem (juce::String (n), n + 1);
            box->setSelectedId (profile->chugRun <= 0 ? 1 : juce::jmin (5, profile->chugRun + 1), juce::dontSendNotification);
            box->setBounds (400, y, 80, rowHeight);
            box->onChange = [this, box]
            {
                const auto chosen = box->getSelectedId() - 1;
                later (page, [this, chosen] { page.setChugRun (chosen); });
            };
        }
        y += rowHeight + 4;
        label ("NOTHING IS WRITTEN OUTSIDE " + pr7s::noteName (profile->rangeLow()) + " TO " + pr7s::noteName (profile->rangeHigh())
                   + " (EMPTY: THE LOWEST STRING TO THE TOP STRING'S LAST FRET)", { 0, y, 600, 18 });
        y += 28;

        // VELOCITY LAYERS (guitars).
        if (! bass)
        {
            label ("VELOCITY LAYERS", { 0, y, 150, rowHeight });
            velocityKey = key (profile->velocities.has_value() ? "EDIT (THIS GUITAR'S OWN)" : "EDIT (NOW: IDEAS' VELOCITIES)", { 150, y, 260, rowHeight }, false,
                               [this] { showVelocities(); }, true);
            y += rowHeight + 14;
        }

        // KEYSWITCHES.
        label ("KEYSWITCHES", { 0, y, 150, 18 });
        y += 22;
        for (int i = 0; i < (int) profile->keyswitches.size(); ++i)
        {
            const auto& k = profile->keyswitches[(size_t) i];
            Row row;
            row.name = edit (k.name, { 0, y, 280, rowHeight - 4 }, {});
            row.note = edit (noteText (k.note), { 286, y, 70, rowHeight - 4 }, {});
            row.use = own (new juce::ComboBox());
            row.use->addItem ("Articulation", 1);
            for (int s = 1; s <= juce::jmax (1, (int) profile->strings.size()); ++s)
                row.use->addItem ("Forces string " + juce::String (s), 1 + s);
            row.use->addItem ("Turns forcing off", 100);
            row.use->addItem ("Power chords (a root alone)", 101);
            row.use->addItem ("Single notes", 102);
            row.use->setSelectedId (k.forcesString < 0 ? 100 : k.use == pr7s::Keyswitch::Use::powerChords ? 101
                                    : k.use == pr7s::Keyswitch::Use::singleNotes ? 102 : 1 + k.forcesString, juce::dontSendNotification);
            row.use->setBounds (362, y, 190, rowHeight - 4);
            const auto commit = [this, i]
            {
                const auto& r = rows[(size_t) i];
                const auto name = r.name->getText(), note = r.note->getText();
                const auto id = r.use->getSelectedId();
                const auto use = id == 100 ? -1 : id == 101 ? -101 : id == 102 ? -102 : id - 1;
                later (page, [this, i, name, note, use] { page.setKeyswitch (i, name, note, use); });
            };
            for (auto* e : { row.name, row.note })
            {
                e->onReturnKey = commit;
                e->onFocusLost = commit;
            }
            row.use->onChange = commit;
            key ("X", { 558, y, 34, rowHeight - 4 }, false, [this, i] { page.removeKeyswitch (i); });
            rows.push_back (row);
            y += rowHeight;
        }
        key ("+ KEYSWITCH", { 0, y + 4, 160, rowHeight }, false, [this] { page.addKeyswitch(); });
        y += rowHeight + 16;
        setSize (600, y);
    }

    juce::Button* keyNamed (const juce::String& text)
    {
        for (auto* c : owned)
            if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText() == text)
                return b;
        return nullptr;
    }

private:
    struct Row { juce::TextEditor* name = nullptr; juce::TextEditor* note = nullptr; juce::ComboBox* use = nullptr; };
    InstrumentsPage& page;
    juce::OwnedArray<juce::Component> owned;
    std::vector<Row> rows;
    juce::Button* velocityKey = nullptr;

    template <typename C>
    C* own (C* c)
    {
        owned.add (c);
        addAndMakeVisible (c);
        return c;
    }

    void showVelocities()
    {
        auto* profile = page.current();
        if (profile == nullptr || velocityKey == nullptr)
            return;
        auto panel = std::make_unique<GuitarVelocityPanel> (profile->velocities.value_or (page.studio.getGuitarVelocities()));
        panel->onChange = [safe = juce::Component::SafePointer<InstrumentsPage> (&page)] (const pr7s::GuitarVelocities& v)
        {
            if (safe != nullptr)
                safe->setVelocities (v);
        };
        juce::CallOutBox::launchAsynchronously (std::move (panel), velocityKey->getScreenBounds(), nullptr);
    }
};

//==============================================================================
InstrumentsPage::InstrumentsPage (pr7s::Studio& s) : studio (s)
{
    profiles = studio.getInstrumentProfiles();
    list = std::make_unique<List> (*this);
    editor = std::make_unique<Editor> (*this);
    addAndMakeVisible (*list);
    editorView.setViewedComponent (editor.get(), false);
    editorView.setScrollBarsShown (true, false);
    addAndMakeVisible (editorView);
    for (auto* key : { &newKey, &deleteKey })
    {
        pedal (*key, key == &newKey);
        addAndMakeVisible (*key);
    }
    newKey.setTooltip ("A new instrument: Djinn Bass 2, Shreddage Jupiter or Rogue (set up), a bass, a guitar, or a copy of this one");
    newKey.onClick = [this] { showNewMenu(); };
    deleteKey.onClick = [this] { deleteProfile(); };
    selectProfile (profiles.empty() ? -1 : 0);
}

InstrumentsPage::~InstrumentsPage()
{
    editorView.setViewedComponent (nullptr, false);
}

pr7s::InstrumentProfile* InstrumentsPage::current()
{
    return juce::isPositiveAndBelow (selected, (int) profiles.size()) ? &profiles[(size_t) selected] : nullptr;
}

void InstrumentsPage::store()
{
    studio.setInstrumentProfiles (profiles);
    list->updateContent();
    list->repaint();
    repaint();
}

void InstrumentsPage::rebuild()
{
    editor->make();
    deleteKey.setEnabled (current() != nullptr);
    repaint();
}

void InstrumentsPage::selectProfile (int index)
{
    selected = juce::isPositiveAndBelow (index, (int) profiles.size()) ? index : -1;
    list->updateContent();
    if (selected >= 0)
        list->selectRow (selected, false, true);
    rebuild();
}

void InstrumentsPage::addProfile (NewKind kind)
{
    Profile p;
    switch (kind)
    {
        case NewKind::djinnBass2: p = Profile::djinnBass2(); p.forBass = false; break;
        case NewKind::jupiter: p = Profile::shreddage ("Shreddage Jupiter", studio.getGuitarVelocities()); break;
        case NewKind::rogue: p = Profile::shreddage ("Shreddage Rogue", studio.getGuitarVelocities()); break;
        case NewKind::bass: p = Profile::blank (Profile::Kind::bass); break;
        case NewKind::guitar: p = Profile::blank (Profile::Kind::guitar); break;
        case NewKind::copy:
            if (current() == nullptr)
                return;
            p = *current();
            p.name += " copy";
            p.forBass = p.forGuitar = p.forLead = false;
            break;
    }
    // A name no other has.
    const auto base = p.name;
    for (int n = 2; std::any_of (profiles.begin(), profiles.end(), [&p] (const Profile& o) { return o.name == p.name; }); ++n)
        p.name = base + " " + juce::String (n);
    profiles.push_back (p);
    store();
    selectProfile ((int) profiles.size() - 1);
}

void InstrumentsPage::deleteProfile()
{
    if (current() == nullptr)
        return;
    profiles.erase (profiles.begin() + selected);
    store();
    selectProfile (juce::jmin (selected, (int) profiles.size() - 1));
}

void InstrumentsPage::setName (const juce::String& name)
{
    auto* p = current();
    if (p == nullptr || name.trim().isEmpty() || name.trim() == p->name)
        return;
    p->name = name.trim();
    store();
}

void InstrumentsPage::setKind (Profile::Kind kind)
{
    auto* p = current();
    if (p == nullptr || p->kind == kind)
        return;
    p->kind = kind;
    if (kind == Profile::Kind::guitar && ! p->velocities.has_value())
        p->velocities = studio.getGuitarVelocities();
    store();
    rebuild();
}

void InstrumentsPage::setUse (pr7s::Studio::ProfileUse use, bool on)
{
    auto* p = current();
    if (p == nullptr)
        return;
    using U = pr7s::Studio::ProfileUse;
    // One profile per lane: marking this one unmarks the others.
    for (auto& o : profiles)
    {
        auto& flag = use == U::bass ? o.forBass : use == U::guitar ? o.forGuitar : o.forLead;
        flag = (&o == p) ? on : (on ? false : flag);
    }
    store();
    rebuild();
}

bool InstrumentsPage::setString (int index, const juce::String& text)
{
    auto* p = current();
    const auto note = pr7s::parseNoteText (text);
    if (p == nullptr || ! juce::isPositiveAndBelow (index, (int) p->strings.size()) || ! note.has_value())
    {
        rebuild();   // (the field back to what it was)
        return false;
    }
    if (p->strings[(size_t) index] == *note)
        return true;
    p->strings[(size_t) index] = *note;
    std::sort (p->strings.begin(), p->strings.end());
    store();
    rebuild();
    return true;
}

void InstrumentsPage::addString()
{
    auto* p = current();
    if (p == nullptr || p->strings.size() >= 9)
        return;
    p->strings.push_back (p->strings.empty() ? 40 : juce::jmin (127, p->strings.back() + 5));
    store();
    rebuild();
}

void InstrumentsPage::removeString()
{
    auto* p = current();
    if (p == nullptr || p->strings.size() <= 1)
        return;
    p->strings.pop_back();
    for (auto& k : p->keyswitches)
        if (k.forcesString > (int) p->strings.size())
            k.forcesString = 0;
    store();
    rebuild();
}

bool InstrumentsPage::setFrets (const juce::String& text)
{
    auto* p = current();
    if (p == nullptr || ! text.trim().containsOnly ("0123456789") || text.trim().isEmpty())
    {
        rebuild();
        return false;
    }
    const auto frets = juce::jlimit (1, 36, text.getIntValue());
    if (frets != p->frets)
    {
        p->frets = frets;
        store();
        rebuild();
    }
    return true;
}

bool InstrumentsPage::setRange (const juce::String& lowText, const juce::String& highText)
{
    auto* p = current();
    if (p == nullptr)
        return false;
    const auto read = [] (const juce::String& t, int& out)
    {
        if (t.trim().isEmpty() || t.trim().equalsIgnoreCase ("auto"))
        {
            out = -1;
            return true;
        }
        if (const auto n = pr7s::parseNoteText (t))
        {
            out = *n;
            return true;
        }
        return false;
    };
    auto low = p->lowest, high = p->highest;
    const auto ok = read (lowText, low) && read (highText, high);
    if (ok && (low < 0 || high < 0 || low < high))
    {
        if (low != p->lowest || high != p->highest)
        {
            p->lowest = low;
            p->highest = high;
            store();
            rebuild();
        }
        return true;
    }
    rebuild();
    return false;
}

bool InstrumentsPage::setBendRange (const juce::String& text)
{
    auto* p = current();
    if (p == nullptr || ! text.trim().containsOnly ("0123456789") || text.trim().isEmpty())
    {
        rebuild();
        return false;
    }
    p->bendRange = juce::jlimit (0, 48, text.getIntValue());
    store();
    return true;
}

void InstrumentsPage::setShortestNote (double beats)
{
    auto* p = current();
    if (p == nullptr)
        return;
    p->shortestNote = juce::jlimit (0.0, 1.0, beats);
    store();
}

void InstrumentsPage::setChugRun (int run)
{
    auto* p = current();
    if (p == nullptr)
        return;
    p->chugRun = juce::jlimit (0, 16, run);
    store();
}

void InstrumentsPage::addKeyswitch()
{
    auto* p = current();
    if (p == nullptr)
        return;
    p->keyswitches.push_back ({ "New keyswitch", 0, 0 });
    store();
    rebuild();
}

bool InstrumentsPage::setKeyswitch (int index, const juce::String& name, const juce::String& noteText, int forcesString)
{
    auto* p = current();
    const auto note = pr7s::parseNoteText (noteText);
    if (p == nullptr || ! juce::isPositiveAndBelow (index, (int) p->keyswitches.size()) || ! note.has_value())
    {
        rebuild();
        return false;
    }
    // (forcesString -101 / -102: marked for power chords / single notes, 0.63.0)
    const auto use = forcesString == -101 ? pr7s::Keyswitch::Use::powerChords : forcesString == -102 ? pr7s::Keyswitch::Use::singleNotes : pr7s::Keyswitch::Use::none;
    pr7s::Keyswitch k { name.trim().isEmpty() ? p->keyswitches[(size_t) index].name : name.trim(), *note,
                        forcesString <= -101 ? 0 : forcesString < 0 ? -1 : juce::jlimit (0, (int) p->strings.size(), forcesString), use };
    // One key per use.
    if (use != pr7s::Keyswitch::Use::none)
        for (int i = 0; i < (int) p->keyswitches.size(); ++i)
            if (i != index && p->keyswitches[(size_t) i].use == use)
                p->keyswitches[(size_t) i].use = pr7s::Keyswitch::Use::none;
    // One key per string.
    if (k.forcesString > 0)
        for (int i = 0; i < (int) p->keyswitches.size(); ++i)
            if (i != index && p->keyswitches[(size_t) i].forcesString == k.forcesString)
                p->keyswitches[(size_t) i].forcesString = 0;
    if (! (p->keyswitches[(size_t) index] == k))
    {
        p->keyswitches[(size_t) index] = k;
        store();
        rebuild();
    }
    return true;
}

void InstrumentsPage::removeKeyswitch (int index)
{
    auto* p = current();
    if (p == nullptr || ! juce::isPositiveAndBelow (index, (int) p->keyswitches.size()))
        return;
    p->keyswitches.erase (p->keyswitches.begin() + index);
    store();
    rebuild();
}

void InstrumentsPage::setVelocities (const pr7s::GuitarVelocities& v)
{
    auto* p = current();
    if (p == nullptr)
        return;
    p->velocities = v;
    store();
}

juce::Button* InstrumentsPage::keyNamed (const juce::String& text)
{
    if (text == "+ NEW") return &newKey;
    if (text == "DELETE") return &deleteKey;
    return editor->keyNamed (text);
}

void InstrumentsPage::showNewMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Djinn Bass 2 (its keyswitch map)");
    menu.addItem (5, "Shreddage Jupiter (7-string drop A)");
    menu.addItem (6, "Shreddage Rogue (7-string drop A)");
    menu.addItem (2, "A bass");
    menu.addItem (3, "A guitar");
    menu.addItem (4, "A copy of this one", current() != nullptr);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (newKey), [safe = juce::Component::SafePointer<InstrumentsPage> (this)] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        const NewKind kinds[] = { NewKind::djinnBass2, NewKind::bass, NewKind::guitar, NewKind::copy, NewKind::jupiter, NewKind::rogue };
        safe->addProfile (kinds[chosen - 1]);
    });
}

void InstrumentsPage::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("INSTRUMENTS", getLocalBounds().removeFromTop (26).toFloat(), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("EACH SAMPLE LIBRARY'S RANGE, TUNING, VELOCITY LAYERS AND KEYSWITCHES: WHAT THE COMPOSER WRITES FOR IT",
                getLocalBounds().withTrimmedLeft (160).removeFromTop (26).toFloat(), juce::Justification::centredLeft, true);
    if (profiles.empty())
        g.drawText ("NO INSTRUMENTS YET: + NEW", editorView.getBounds().toFloat(), juce::Justification::centred, false);
}

void InstrumentsPage::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (40);
    auto left = area.removeFromLeft (listWidth);
    auto keys = left.removeFromBottom (34);
    newKey.setBounds (keys.removeFromLeft (keys.getWidth() / 2 - 4));
    keys.removeFromLeft (8);
    deleteKey.setBounds (keys);
    left.removeFromBottom (8);
    list->setBounds (left);
    area.removeFromLeft (20);
    editorView.setBounds (area);
    editor->setTopLeftPosition (0, 0);
}
