#include "ui/MediaBayPage.h"

#include "ui/ParagonLookAndFeel.h"

using namespace MediaUi;

namespace
{
    enum Column { ratingColumn = 1, nameColumn, typeColumn, lengthColumn, tempoColumn, keyColumn, formatColumn, folderColumn, sizeColumn, dateColumn,
                  categoryColumn, styleColumn, characterColumn };

    std::optional<pr7s::MediaColumn> mediaColumnOf (int column)
    {
        using C = pr7s::MediaColumn;
        switch (column)
        {
            case ratingColumn: return C::rating;
            case nameColumn: return C::name;
            case typeColumn: return C::type;
            case lengthColumn: return C::length;
            case tempoColumn: return C::tempo;
            case keyColumn: return C::key;
            case folderColumn: return C::folder;
            case sizeColumn: return C::size;
            case dateColumn: return C::date;
            case categoryColumn: return C::category;
            case styleColumn: return C::style;
            default: return std::nullopt;
        }
    }
}

//==============================================================================
// The FILE BROWSER (0.22.1, the manual's File Browser section): a tree of folders under SCANNED
// FOLDERS (ours: what's ticked, at a glance), FAVORITES, THIS COMPUTER (home, desktop, documents,
// the drives) and USER CONTENT. Each folder: the arrow opens it (its subfolders read then), the
// box scans it (a tick: all of it; a square: some of it; empty: none), a click shows its files.
// A folder being scanned is drawn in the accent colour, a scanned one in pearl white.
class MediaBayPage::Browser final : public juce::TreeView
{
public:
    class FolderItem;

    // A root: a heading with folders under it; clicking it shows every scanned file.
    class RootItem final : public juce::TreeViewItem
    {
    public:
        RootItem (Browser& b, juce::String n) : browser (b), name (std::move (n)) {}
        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return "root:" + name; }
        int getItemHeight() const override { return 26; }
        void paintItem (juce::Graphics& g, int width, int height) override
        {
            if (isSelected())
            {
                g.setColour (ParagonColours::accent.withAlpha (0.18f));
                g.fillRect (0, 0, width, height);
            }
            g.setColour (ParagonColours::accent);
            g.setFont (ParagonLookAndFeel::titleFont (13.5f));
            g.drawText (name, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
        }
        void itemSelectionChanged (bool nowSelected) override
        {
            if (nowSelected && ! browser.updating)
                browser.page.selectFolder ({});
        }
        void itemOpennessChanged (bool open) override
        {
            if (open && getNumSubItems() == 0)
                fill();
            browser.opennessChanged();
        }
        void fill();
        void refill()
        {
            const auto wasOpen = isOpen();
            clearSubItems();
            if (wasOpen)
                fill();
        }
        Browser& browser;
        const juce::String name;
    };

    class FolderItem final : public juce::TreeViewItem
    {
    public:
        FolderItem (Browser& b, juce::File f, juce::String l) : browser (b), folder (std::move (f)), label (std::move (l)) {}
        bool mightContainSubItems() override { return ! loaded || getNumSubItems() > 0; }
        juce::String getUniqueName() const override { return folder.getFullPathName(); }
        int getItemHeight() const override { return 22; }

        void paintItem (juce::Graphics& g, int width, int height) override
        {
            if (isSelected())
            {
                g.setColour (ParagonColours::accent.withAlpha (0.22f));
                g.fillRect (0, 0, width, height);
            }
            // The box.
            const auto box = juce::Rectangle<float> (2.0f, (float) height / 2.0f - 6.0f, 12.0f, 12.0f);
            const auto state = browser.page.database.scanState (folder);
            g.setColour (ParagonColours::panelEdge.brighter (0.4f));
            g.drawRoundedRectangle (box, 2.0f, 1.0f);
            if (state == pr7s::MediaDatabase::ScanState::all)
            {
                g.setColour (ParagonColours::accent);
                g.fillRoundedRectangle (box, 2.0f);
                juce::Path tick;
                tick.startNewSubPath (box.getX() + 2.5f, box.getCentreY());
                tick.lineTo (box.getX() + 5.0f, box.getBottom() - 3.0f);
                tick.lineTo (box.getRight() - 2.5f, box.getY() + 3.0f);
                g.setColour (juce::Colours::black);
                g.strokePath (tick, juce::PathStrokeType (1.8f));
            }
            else if (state == pr7s::MediaDatabase::ScanState::some)
            {
                g.setColour (ParagonColours::accent);
                g.fillRoundedRectangle (box.reduced (3.0f), 1.0f);
            }
            // A folder shape and the name: accent while it's being scanned, pearl white scanned, dim not.
            const auto included = browser.page.database.isIncluded (folder);
            const auto colour = included && browser.page.database.isScanning() ? ParagonColours::accent
                              : included || state == pr7s::MediaDatabase::ScanState::some ? juce::Colour (0xfff4eeea)
                              : ParagonColours::textDim;
            auto icon = juce::Rectangle<float> (20.0f, (float) height / 2.0f - 5.0f, 14.0f, 10.0f);
            juce::Path shape;
            shape.addRoundedRectangle (icon.withTrimmedTop (2.0f), 1.5f);
            shape.addRoundedRectangle (icon.withWidth (6.0f).withHeight (4.0f), 1.0f);
            g.setColour (colour.withAlpha (0.75f));
            g.fillPath (shape);
            g.setColour (colour);
            g.setFont (interFont (12.0f));
            g.drawText (label, 40, 0, width - 42, height, juce::Justification::centredLeft, true);
        }

        void itemClicked (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return browser.showFolderMenu (folder);
            if (e.x < 18)   // (the box)
                browser.page.setFolderScanned (folder, browser.page.database.scanState (folder) != pr7s::MediaDatabase::ScanState::all);
        }

        void itemDoubleClicked (const juce::MouseEvent&) override { setOpen (! isOpen()); }

        void itemSelectionChanged (bool nowSelected) override
        {
            if (nowSelected && ! browser.updating)
                browser.page.selectFolder (folder);
        }

        void itemOpennessChanged (bool open) override
        {
            if (open && ! loaded)
                load();
            browser.opennessChanged();
        }

        void load()
        {
            loaded = true;
            clearSubItems();
            auto folders = folder.findChildFiles (juce::File::findDirectories, false);
            std::sort (folders.begin(), folders.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
            for (const auto& f : folders)
                if (! f.isHidden() && ! f.getFileName().startsWithChar ('.'))
                    addSubItem (new FolderItem (browser, f, f.getFileName()));
            treeHasChanged();
        }

        void refreshViews()
        {
            // (The manual's Refresh Views: what's in it read again, open folders kept open.)
            juce::StringArray open;
            for (int i = 0; i < getNumSubItems(); ++i)
                if (getSubItem (i)->isOpen())
                    open.add (getSubItem (i)->getUniqueName());
            if (! loaded)
                return;
            load();
            for (int i = 0; i < getNumSubItems(); ++i)
                if (open.contains (getSubItem (i)->getUniqueName()))
                    getSubItem (i)->setOpen (true);
        }

        Browser& browser;
        const juce::File folder;
        const juce::String label;
        bool loaded = false;
    };

    explicit Browser (MediaBayPage& p) : page (p)
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);   // (nothing saved while it's being made)
        setRootItem (&root);
        setRootItemVisible (false);
        setDefaultOpenness (false);
        setIndentSize (16);
        setColour (juce::TreeView::backgroundColourId, juce::Colour (0xff100d11));
        setColour (juce::TreeView::linesColourId, ParagonColours::panelEdge);
        setColour (juce::TreeView::selectedItemBackgroundColourId, juce::Colours::transparentBlack);
        for (const auto* name : { "SCANNED FOLDERS", "FAVORITES", "THIS COMPUTER", "USER CONTENT" })
            root.addSubItem (new RootItem (*this, name));
        root.setOpen (true);   // (hidden, but its headings show only while it's open)
        root.getSubItem (0)->setOpen (true);
        root.getSubItem (2)->setOpen (true);
    }

    ~Browser() override { setRootItem (nullptr); }

    // Remembering what's open (0.22.2, the owner asked: the MediaBay opens where it was left).
    // Each open item by its heading and path: "root:THIS COMPUTER", "THIS COMPUTER|F:\Samples".
    static juce::String keyFor (juce::TreeViewItem& item)
    {
        if (auto* heading = dynamic_cast<RootItem*> (&item))
            return "root:" + heading->name;
        auto* up = item.getParentItem();
        while (up != nullptr && dynamic_cast<RootItem*> (up) == nullptr)
            up = up->getParentItem();
        const auto* heading = dynamic_cast<RootItem*> (up);
        return (heading != nullptr ? heading->name : juce::String()) + "|" + item.getUniqueName();
    }

    juce::StringArray getOpenKeys()
    {
        juce::StringArray keys;
        std::function<void (juce::TreeViewItem&)> walk = [&] (juce::TreeViewItem& item)
        {
            for (int i = 0; i < item.getNumSubItems(); ++i)
                if (auto* sub = item.getSubItem (i); sub->isOpen())
                {
                    keys.add (keyFor (*sub));
                    walk (*sub);
                }
        };
        walk (root);
        return keys;
    }

    void restoreOpenKeys (const juce::StringArray& keys)
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        std::function<void (juce::TreeViewItem&)> walk = [&] (juce::TreeViewItem& item)
        {
            for (int i = 0; i < item.getNumSubItems(); ++i)
            {
                auto* sub = item.getSubItem (i);
                const auto open = keys.contains (keyFor (*sub));
                sub->setOpen (open);
                if (open)
                {
                    if (auto* folderItem = dynamic_cast<FolderItem*> (sub); folderItem != nullptr && ! folderItem->loaded)
                        folderItem->load();
                    walk (*sub);
                }
            }
        };
        walk (root);
    }

    // The chosen folder shown chosen (and scrolled to), without choosing it again.
    void showSelected (const juce::File& folder)
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        if (folder == juce::File())
            return;
        if (auto* item = reveal (folder))
        {
            item->setSelected (true, true, juce::dontSendNotification);
            scrollToKeepItemVisible (item);
        }
    }

    void opennessChanged()
    {
        if (! updating)
            page.saveBrowserState();
    }

    // The scanned folders' and the favorites' lists again (after a tick, a new favorite).
    void refreshLists()
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        // (What was open stays open.)
        juce::StringArray open;
        std::function<void (juce::TreeViewItem&)> note = [&] (juce::TreeViewItem& item)
        {
            for (int i = 0; i < item.getNumSubItems(); ++i)
                if (auto* sub = item.getSubItem (i); sub->isOpen())
                {
                    open.add (sub->getUniqueName());
                    note (*sub);
                }
        };
        std::function<void (juce::TreeViewItem&)> reopen = [&] (juce::TreeViewItem& item)
        {
            for (int i = 0; i < item.getNumSubItems(); ++i)
                if (auto* sub = item.getSubItem (i); open.contains (sub->getUniqueName()))
                {
                    sub->setOpen (true);
                    if (auto* folderItem = dynamic_cast<FolderItem*> (sub); folderItem != nullptr && ! folderItem->loaded)
                        folderItem->load();
                    reopen (*sub);
                }
        };
        for (int i : { 0, 1 })
        {
            auto* heading = root.getSubItem (i);
            open.clearQuick();
            note (*heading);
            static_cast<RootItem*> (heading)->refill();
            reopen (*heading);
        }
        repaint();
    }

    // The item for a folder, opening the way to it from the root that holds it (nullptr: none).
    FolderItem* reveal (const juce::File& target)
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        for (int r = 0; r < root.getNumSubItems(); ++r)
        {
            auto* heading = root.getSubItem (r);
            if (heading->getNumSubItems() == 0)
                static_cast<RootItem*> (heading)->fill();
            for (int i = 0; i < heading->getNumSubItems(); ++i)
            {
                auto* item = static_cast<FolderItem*> (heading->getSubItem (i));
                if (item->folder == target)
                    return heading->setOpen (true), item;
                if (! target.isAChildOf (item->folder))
                    continue;
                heading->setOpen (true);
                // Down the way.
                auto* at = item;
                while (at->folder != target)
                {
                    at->setOpen (true);
                    if (! at->loaded)
                        at->load();
                    FolderItem* next = nullptr;
                    for (int j = 0; j < at->getNumSubItems() && next == nullptr; ++j)
                    {
                        auto* child = static_cast<FolderItem*> (at->getSubItem (j));
                        if (child->folder == target || target.isAChildOf (child->folder))
                            next = child;
                    }
                    if (next == nullptr)
                        return nullptr;
                    at = next;
                }
                return at;
            }
        }
        return nullptr;
    }

    void showFolderMenu (const juce::File& folder)
    {
        juce::PopupMenu menu;
        const auto state = page.database.scanState (folder);
        menu.addItem (1, "Scan This Folder (and its subfolders)", state != pr7s::MediaDatabase::ScanState::all);
        menu.addItem (2, "Don't Scan This Folder", state != pr7s::MediaDatabase::ScanState::none);
        menu.addSeparator();
        menu.addItem (3, "Refresh Views (read the folder again, and rescan)");
        menu.addItem (4, "Add Favorite (also in the Media rack)");
        menu.showMenuAsync ({}, [safe = juce::Component::SafePointer<Browser> (this), folder] (int chosen)
        {
            if (safe == nullptr)
                return;
            switch (chosen)
            {
                case 1: safe->page.setFolderScanned (folder, true); break;
                case 2: safe->page.setFolderScanned (folder, false); break;
                case 3: safe->page.refreshFolder (folder); break;
                case 4: safe->page.addFavorite (folder); break;
                default: break;
            }
        });
    }

    struct Root final : public juce::TreeViewItem
    {
        bool mightContainSubItems() override { return true; }
    };
    MediaBayPage& page;
    Root root;
    bool updating = false;
};

void MediaBayPage::Browser::RootItem::fill()
{
    clearSubItems();
    const auto add = [this] (const juce::File& f, const juce::String& label)
    {
        if (f.isDirectory())
            addSubItem (new FolderItem (browser, f, label));
    };
    if (name == "SCANNED FOLDERS")
        for (const auto& path : browser.page.database.getLocations())
            add (juce::File (path), juce::File (path).getFullPathName());
    else if (name == "FAVORITES")
        for (const auto& path : browser.page.getFavorites())
            add (juce::File (path), juce::File (path).getFileName());
    else if (name == "THIS COMPUTER")
    {
        add (juce::File::getSpecialLocation (juce::File::userHomeDirectory), "Home");
        add (juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "Desktop");
        add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "Documents");
        juce::Array<juce::File> drives;
        juce::File::findFileSystemRoots (drives);
        for (const auto& drive : drives)
            add (drive, drive.getFullPathName());
    }
    else
        add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Paragon PR7-S"), "Paragon PR7-S");
    treeHasChanged();
}

//==============================================================================
// The ATTRIBUTE INSPECTOR: the selected files' attributes in groups (MEDIA: what the files are;
// MUSICAL: your tags; INFO: rating and comment; YOUR ATTRIBUTES). Click a value to set it for
// every selected file; right-click to remove it.
class MediaBayPage::Inspector final : public juce::Component
{
public:
    explicit Inspector (MediaBayPage& p) : page (p) {}

    struct Row
    {
        enum class Kind { heading, info, attribute } kind = Kind::info;
        juce::String label, value;
        pr7s::AttributeId id;
    };

    void rebuild()
    {
        using A = pr7s::MediaAttribute;
        rows.clear();
        const auto files = page.getSelectedFiles();
        std::vector<const pr7s::MediaItem*> items;
        for (const auto& item : page.database.getItems())
            if (files.contains (item.file))
                items.push_back (&item);

        const auto heading = [this] (const juce::String& text) { rows.push_back ({ Row::Kind::heading, text, {}, {} }); };
        const auto common = [&] (const std::function<juce::String (const pr7s::MediaItem&)>& text)
        {
            if (items.empty())
                return juce::String();
            const auto first = text (*items.front());
            for (const auto* item : items)
                if (text (*item) != first)
                    return mixedText;
            return first;
        };
        heading ("MEDIA");
        if (items.empty())
            rows.push_back ({ Row::Kind::info, "SELECTION", "Select files in the results", {} });
        else
        {
            rows.push_back ({ Row::Kind::info, "NAME", items.size() == 1 ? items.front()->name : juce::String ((int) items.size()) + " FILES", {} });
            rows.push_back ({ Row::Kind::info, "TYPE", common ([] (const pr7s::MediaItem& i) { return pr7s::mediaTypeName (i.type); }), {} });
            rows.push_back ({ Row::Kind::info, "LENGTH", common ([] (const pr7s::MediaItem& i)
                              { return i.type == pr7s::MediaType::midi && i.beats > 0.0 ? juce::String (i.beats / 4.0, 1) + " bars" : lengthText (i.seconds); }), {} });
            rows.push_back ({ Row::Kind::info, "FORMAT", common ([] (const pr7s::MediaItem& i) { return formatText (i); }), {} });
            juce::int64 bytes = 0;
            for (const auto* item : items)
                bytes += item->size;
            rows.push_back ({ Row::Kind::info, "SIZE", sizeText (bytes), {} });
            rows.push_back ({ Row::Kind::info, "DATE", common ([] (const pr7s::MediaItem& i) { return i.modified.formatted ("%Y-%m-%d %H:%M"); }), {} });
            rows.push_back ({ Row::Kind::info, "FOLDER", common ([] (const pr7s::MediaItem& i) { return i.file.getParentDirectory().getFullPathName(); }), {} });
        }
        const auto attribute = [&] (const pr7s::AttributeId& id)
        {
            const auto value = page.getAttributeText (id);
            if (page.dynamic && value.isEmpty())
                return;
            rows.push_back ({ Row::Kind::attribute, id.name(), value, id });
        };
        const auto group = [&] (const juce::String& title, const std::vector<pr7s::AttributeId>& ids)
        {
            const auto at = rows.size();
            heading (title);
            for (const auto& id : ids)
                attribute (id);
            if (rows.size() == at + 1 && page.dynamic && title != "YOUR ATTRIBUTES")
                rows.pop_back();
        };
        group ("MUSICAL", { { A::category, {} }, { A::subCategory, {} }, { A::style, {} }, { A::subStyle, {} },
                            { A::character, {} }, { A::tempo, {} }, { A::key, {} } });
        group ("INFO", { { A::rating, {} }, { A::comment, {} } });
        std::vector<pr7s::AttributeId> users;
        for (const auto& user : page.database.getUserAttributes())
            users.push_back ({ A::user, user.name });
        group ("YOUR ATTRIBUTES", users);
        setSize (getWidth(), juce::jmax (40, (int) rows.size() * rowHeight + 8));
        repaint();
    }

    juce::StringArray getRowNames() const
    {
        juce::StringArray names;
        for (const auto& row : rows)
            if (row.kind == Row::Kind::attribute)
                names.add (row.label);
        return names;
    }

    void paint (juce::Graphics& g) override
    {
        const auto labelWidth = getLabelWidth();
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& row = rows[(size_t) i];
            auto area = juce::Rectangle<int> (0, i * rowHeight, getWidth(), rowHeight);
            if (row.kind == Row::Kind::heading)
            {
                g.setColour (ParagonColours::accent);
                g.setFont (ParagonLookAndFeel::titleFont (13.0f));
                g.drawText (row.label, area.reduced (6, 0).withTrimmedTop (6), juce::Justification::centredLeft, false);
                g.setColour (ParagonColours::panelEdge);
                g.fillRect (area.getX() + 6, area.getBottom() - 1, area.getWidth() - 12, 1);
                continue;
            }
            if (i == hover && row.kind == Row::Kind::attribute)
            {
                g.setColour (ParagonColours::accent.withAlpha (0.10f));
                g.fillRect (area);
            }
            auto label = area.removeFromLeft (labelWidth).reduced (8, 0);
            g.setColour (ParagonColours::textDim);
            g.setFont (ParagonLookAndFeel::titleFont (12.5f));
            g.drawText (row.label, label, juce::Justification::centredLeft, true);
            auto value = area.reduced (6, 0);
            if (row.kind == Row::Kind::attribute && row.id.kind == pr7s::MediaAttribute::rating && row.value != mixedText)
            {
                drawStars (g, value.withWidth (juce::jmin (value.getWidth(), 100)).toFloat(), row.value.getIntValue());
                continue;
            }
            g.setFont (interFont (12.5f));
            if (row.kind == Row::Kind::info)
                g.setColour (ParagonColours::text.withAlpha (0.75f));
            else if (row.value.isEmpty())
                g.setColour (ParagonColours::textDim.withAlpha (0.45f));
            else
                g.setColour (row.value == mixedText ? ParagonColours::textDim : ParagonColours::text);
            const auto text = row.kind == Row::Kind::attribute ? (row.value.isEmpty() ? juce::String ("-") : shownValue (page.database, row.id, row.value)) : row.value;
            g.drawText (text, value, juce::Justification::centredLeft, true);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto row = e.y / rowHeight;
        if (row != hover)
        {
            hover = row;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto index = e.y / rowHeight;
        if (! juce::isPositiveAndBelow (index, (int) rows.size()) || rows[(size_t) index].kind != Row::Kind::attribute
            || page.getSelectedFiles().isEmpty())
            return;
        const auto row = rows[(size_t) index];
        const auto area = localAreaToGlobal (juce::Rectangle<int> (getLabelWidth(), index * rowHeight, getWidth() - getLabelWidth(), rowHeight));
        if (e.mods.isPopupMenu())
            return showRemoveMenu (row, area);
        if (row.id.kind == pr7s::MediaAttribute::rating)
        {
            const auto x = e.x - getLabelWidth() - 6;
            const auto star = juce::jlimit (1, 5, x / 20 + 1);
            page.setAttribute (row.id, row.value.getIntValue() == star ? juce::String() : juce::String (star));
            return;
        }
        edit (row, area);
    }

    static constexpr int rowHeight = 26;

private:
    int getLabelWidth() const { return juce::jmin (120, getWidth() / 2); }

    using Id = pr7s::AttributeId;
    using A = pr7s::MediaAttribute;

    void showRemoveMenu (const Row& row, juce::Rectangle<int> area)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Remove Attribute", row.value.isNotEmpty());
        if (row.id.kind == A::user)
            menu.addItem (2, "Delete the User Attribute \"" + row.id.user + "\" (from every file)");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area),
                            [safe = juce::Component::SafePointer<Inspector> (this), id = row.id] (int chosen)
                            {
                                if (safe == nullptr)
                                    return;
                                if (chosen == 1)
                                    safe->page.removeAttribute (id);
                                else if (chosen == 2)
                                    safe->page.deleteUserAttribute (id.user);
                            });
    }

    void edit (const Row& row, juce::Rectangle<int> area)
    {
        switch (row.id.kind)
        {
            case A::category: case A::style: return showListMenu (row, area, false);
            case A::subCategory: case A::subStyle: return showListMenu (row, area, true);
            case A::key: return showKeyMenu (row, area);
            case A::character: return showCharacterEditor (area);
            case A::tempo: return askText (row, "Tempo", "Its tempo in BPM (20 to 400):");
            case A::comment: return askText (row, "Comment", "A comment (searched too):");
            case A::user:
            {
                const auto* user = page.database.findUserAttribute (row.id.user);
                if (user == nullptr)
                    return;
                if (user->type == pr7s::UserAttributeType::yesNo)
                    return page.setAttribute (row.id, row.value == "1" ? "0" : "1");
                return askText (row, user->name, user->type == pr7s::UserAttributeType::number ? "A number:" : "Its value:");
            }
            default: return;
        }
    }

    // Category / Style (sub = false) or their subs: the list, then ADD...
    void showListMenu (const Row& row, juce::Rectangle<int> area, bool sub)
    {
        const auto isStyle = row.id.kind == A::style || row.id.kind == A::subStyle;
        const Id parent { isStyle ? A::style : A::category, {} };
        juce::PopupMenu menu;
        juce::StringArray choices;
        if (! sub)
        {
            for (const auto& list : isStyle ? page.database.getStyles() : page.database.getCategories())
                choices.add (list.name);
        }
        else
        {
            // The subs of the selected files' categories (or styles).
            juce::StringArray parents;
            for (const auto& item : page.database.getItems())
                if (page.getSelectedFiles().contains (item.file))
                    parents.addArray (pr7s::MediaDatabase::attributeValues (item, parent));
            parents.removeDuplicates (true);
            for (const auto& name : parents)
                choices.addArray (page.database.subsOf (parent, name));
            choices.removeDuplicates (true);
            if (parents.isEmpty())
                menu.addItem (-1, "Choose a " + parent.name().toLowerCase() + " first", false);
        }
        for (int i = 0; i < choices.size(); ++i)
            menu.addItem (i + 1, choices[i], true, choices[i] == row.value);
        menu.addSeparator();
        menu.addItem (10000, sub ? "Add a " + Id { row.id.kind, {} }.name().toLowerCase() + "..." : "Add a " + parent.name().toLowerCase() + "...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area),
                            [safe = juce::Component::SafePointer<Inspector> (this), choices, id = row.id, isStyle] (int chosen)
                            {
                                if (safe == nullptr || chosen == 0)
                                    return;
                                if (chosen == 10000)
                                    return safe->askNewValue (isStyle);
                                safe->page.setAttribute (id, choices[chosen - 1]);
                            });
    }

    // The manual's Adding User Attribute Values: a category and sub category (or style and sub
    // style), added to the lists and set on the selected files.
    void askNewValue (bool isStyle)
    {
        const Id parent { isStyle ? A::style : A::category, {} }, sub { isStyle ? A::subStyle : A::subCategory, {} };
        auto* box = new juce::AlertWindow ("Add a " + parent.name().toLowerCase(),
                                           "A " + parent.name().toLowerCase() + " (one in the list, or a new one) and, if you like, a "
                                               + sub.name().toLowerCase() + " under it:", juce::MessageBoxIconType::NoIcon);
        const auto current = page.getAttributeText (parent);
        box->addTextEditor ("name", current == mixedText ? juce::String() : current, parent.name());
        box->addTextEditor ("sub", {}, sub.name());
        box->addButton ("ADD", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, isStyle, parent, sub, safe = juce::Component::SafePointer<Inspector> (this)] (int result)
        {
            if (safe == nullptr || result != 1)
                return;
            const auto name = box->getTextEditorContents ("name").trim(), subName = box->getTextEditorContents ("sub").trim();
            if (name.isEmpty())
                return;
            safe->page.addListValue (isStyle, name, subName);
        }), true);
    }

    void showKeyMenu (const Row& row, juce::Rectangle<int> area)
    {
        juce::PopupMenu menu;
        const auto& names = pr7s::mediaKeyNames();
        menu.addSectionHeader ("MAJOR");
        for (int i = 0; i < names.size(); ++i)
        {
            if (i == 12)
            {
                menu.addColumnBreak();
                menu.addSectionHeader ("MINOR");
            }
            menu.addItem (i + 1, names[i], true, names[i] == row.value);
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area),
                            [safe = juce::Component::SafePointer<Inspector> (this), id = row.id] (int chosen)
                            {
                                if (safe != nullptr && chosen > 0)
                                    safe->page.setAttribute (id, pr7s::mediaKeyNames()[chosen - 1]);
                            });
    }

    void askText (const Row& row, const juce::String& title, const juce::String& message)
    {
        auto* box = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("value", row.value == mixedText ? juce::String() : row.value, {});
        box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, id = row.id, safe = juce::Component::SafePointer<Inspector> (this)] (int result)
        {
            if (safe != nullptr && result == 1)
                safe->page.setAttribute (id, box->getTextEditorContents ("value").replaceCharacter (',', id.kind == A::comment ? ',' : '.'));
        }), true);
    }

    // The manual's Edit Character dialog: each pair of opposites, one side or neither.
    class CharacterEditor final : public juce::Component
    {
    public:
        explicit CharacterEditor (MediaBayPage& p) : page (p)
        {
            const auto& pairs = pr7s::characterPairs();
            for (int i = 0; i < (int) pairs.size(); ++i)
                for (int side = -1; side <= 1; ++side)
                {
                    auto* key = keys.add (new juce::TextButton (side < 0 ? pairs[(size_t) i].left.toUpperCase()
                                                                : side > 0 ? pairs[(size_t) i].right.toUpperCase() : juce::String ("-")));
                    styleKey (*key);
                    key->onClick = [this, i, side] { page.setCharacter (i, side); refresh(); };
                    addAndMakeVisible (key);
                }
            setSize (360, (int) pairs.size() * 32 + 16);
            refresh();
        }

        void refresh()
        {
            // The files' common sides (several files that differ: none lit).
            const auto text = page.getAttributeText ({ A::character, {} });
            const auto mixed = text == mixedText;
            const auto sides = juce::StringArray::fromTokens (mixed ? juce::String() : text, ",", "");
            const auto has = [&] (const juce::String& word)
            {
                for (const auto& s : sides)
                    if (s.trim() == word)
                        return true;
                return false;
            };
            const auto& pairs = pr7s::characterPairs();
            for (int i = 0; i < (int) pairs.size(); ++i)
            {
                const auto left = has (pairs[(size_t) i].left), right = has (pairs[(size_t) i].right);
                keys[i * 3]->setToggleState (left, juce::dontSendNotification);
                keys[i * 3 + 1]->setToggleState (! mixed && ! left && ! right, juce::dontSendNotification);
                keys[i * 3 + 2]->setToggleState (right, juce::dontSendNotification);
            }
        }

        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff141015)); }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            for (int i = 0; i < keys.size() / 3; ++i)
            {
                auto row = area.removeFromTop (32).reduced (0, 3);
                const auto w = (row.getWidth() - 50) / 2;
                keys[i * 3]->setBounds (row.removeFromLeft (w));
                keys[i * 3 + 2]->setBounds (row.removeFromRight (w));
                keys[i * 3 + 1]->setBounds (row.reduced (6, 0));
            }
        }

    private:
        MediaBayPage& page;
        juce::OwnedArray<juce::TextButton> keys;
    };

    void showCharacterEditor (juce::Rectangle<int> area)
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<CharacterEditor> (page), area, nullptr);
    }

    MediaBayPage& page;
    std::vector<Row> rows;
    int hover = -1;
};

//==============================================================================
MediaBayPage::MediaBayPage (pr7s::Studio& s)
    : studio (s), database (s.getMediaBay())
{
    using pr7s::MediaType;
    // As last time.
    if (auto* settings = studio.getSettings())
    {
        const auto types = juce::StringArray::fromTokens (settings->getValue ("mediaBayTypes"), ",", "");
        for (const auto& t : types)
            for (const auto type : { MediaType::audio, MediaType::midi, MediaType::trackPreset, MediaType::song, MediaType::pluginPreset })
                if (t == pr7s::mediaTypeName (type))
                    query.types.insert (type);
        query.minimumRating = juce::jlimit (0, 5, settings->getIntValue ("mediaBayRating", 0));
        query.subfolders = settings->getBoolValue ("mediaBaySubfolders", true);
        autoPlay = settings->getBoolValue ("mediaBayAutoPlay", true);
        cycle = settings->getBoolValue ("mediaBayCycle", false);
        filtersShown = settings->getBoolValue ("mediaBayFilters", true);
        inspectorShown = settings->getBoolValue ("mediaBayInspector", true);
        dynamic = settings->getBoolValue ("mediaBayDynamic", false);
    }
    // The filter's columns: CATEGORY, SUB CATEGORY, STYLE, CHARACTER at first (or as last time).
    {
        using A = pr7s::MediaAttribute;
        const pr7s::AttributeId defaults[] { { A::category, {} }, { A::subCategory, {} }, { A::style, {} }, { A::character, {} } };
        const auto saved = studio.getSettings() != nullptr ? juce::StringArray::fromTokens (studio.getSettings()->getValue ("mediaBayFilterColumns"), "|", "")
                                                           : juce::StringArray();
        for (int i = 0; i < numFilterColumns; ++i)
        {
            auto id = i < saved.size() ? attributeFromToken (saved[i]) : defaults[i];
            if (id.kind == A::user && database.findUserAttribute (id.user) == nullptr)
                id = defaults[i];
            query.attributes.push_back ({ id, {} });
        }
    }

    search.setTextToShowWhenEmpty ("SEARCH: drum and (kick or snare) -808", ParagonColours::textDim);
    search.setFont (interFont (14.0f));
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b171c));
    search.setColour (juce::TextEditor::textColourId, ParagonColours::text);
    search.setColour (juce::TextEditor::outlineColourId, ParagonColours::panelEdge);
    search.setColour (juce::TextEditor::focusedOutlineColourId, ParagonColours::accent);
    search.setTooltip ("Words all found (and, +, or a space); either (or, a comma); none (not, a leading minus); (parentheses); \"a phrase\". "
                       "Looks in the name, the folder, the type, the tempo and the key");
    search.onTextChange = [this] { setSearch (search.getText()); };
    addAndMakeVisible (search);

    for (auto* key : { &audioKey, &midiKey, &presetsKey, &songsKey, &subfoldersKey, &shuffleKey, &resetKey, &rescanKey,
                       &addKey, &removeKey, &playKey, &cycleKey, &autoPlayKey, &useKey, &linkKey, &waitKey, &halfKey, &doubleKey,
                       &filtersKey, &inspectorKey, &dynamicKey, &definedKey, &userKey })
    {
        styleKey (*key);
        addAndMakeVisible (*key);
    }
    const auto typeKey = [this] (juce::TextButton& key, MediaType type, const char* tip)
    {
        key.setTooltip (tip);
        key.onClick = [this, &key, type] { setTypeShown (type, ! key.getToggleState()); };
    };
    typeKey (audioKey, MediaType::audio, "Audio files (WAV, AIFF, FLAC, Ogg, MP3). With no type lit, every type is shown");
    typeKey (midiKey, MediaType::midi, "MIDI files. With no type lit, every type is shown");
    typeKey (presetsKey, MediaType::trackPreset, "Presets: track presets (.pr7track) and VST presets (.vstpreset). With no type lit, every type is shown");
    typeKey (songsKey, MediaType::song, "Songs (.pr7s). With no type lit, every type is shown");
    subfoldersKey.setTooltip ("A location's subfolders too (off: only the files directly in it)");
    subfoldersKey.onClick = [this] { setSubfolders (! query.subfolders); };
    shuffleKey.setTooltip ("The results in a random order");
    shuffleKey.onClick = [this] { shuffle (juce::Random::getSystemRandom().nextInt()); };
    resetKey.setTooltip ("Clear the search, the types and the rating filter");
    resetKey.onClick = [this] { resetFilters(); };
    rescanKey.setTooltip ("Look through the locations again for new, changed or removed files (it waits while the song plays)");
    rescanKey.onClick = [this] { database.rescan(); refreshKeys(); };
    addKey.setTooltip ("A folder to scan, with its subfolders, picked from a file chooser (or tick its box in the tree)");
    addKey.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("A folder for the MediaBay to scan", juce::File::getSpecialLocation (juce::File::userHomeDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<MediaBayPage> (this)] (const juce::FileChooser& c)
                              {
                                  if (safe != nullptr && c.getResult() != juce::File())
                                      safe->addLocation (c.getResult());
                              });
    };
    removeKey.setTooltip ("Refresh Views: the selected folder's subfolders read again, and a rescan");
    removeKey.onClick = [this] { refreshFolder (selectedFolder); };
    playKey.setTooltip ("Hear the selected file (audio over the output; MIDI through the selected track's instrument)");
    playKey.onClick = [this] { studio.isMediaPreviewing() ? stop() : (void) play(); };
    cycleKey.setTooltip ("Audio plays round and round");
    cycleKey.onClick = [this] { setCycle (! cycle); };
    autoPlayKey.setTooltip ("Play each file as it's selected");
    autoPlayKey.onClick = [this] { setAutoPlay (! autoPlay); };
    // In time with the song (0.23.0, the manual's Audio Files Previewer).
    const auto change = [this] (std::function<void (pr7s::Studio::MediaPreviewOptions&)> edit)
    {
        auto options = studio.getMediaPreviewOptions();
        edit (options);
        setPreviewOptions (options);
    };
    linkKey.setTooltip ("A loop stretched to the song's tempo at the playhead (its pitch kept). Not for one-shots or files without a tempo");
    linkKey.onClick = [change] { change ([] (auto& o) { o.linkTempo = ! o.linkTempo; }); };
    waitKey.setTooltip ("Heard only while the song plays, starting and stopping with it. With LINK TEMPO a loop locks to the song's bars "
                        "from the left locator (put it on a bar's start)");
    waitKey.onClick = [change] { change ([] (auto& o) { o.waitForPlay = ! o.waitForPlay; }); };
    halfKey.setTooltip ("Half speed (its pitch kept)");
    halfKey.onClick = [change] { change ([] (auto& o) { o.speed = o.speed < 0.75 ? 1.0 : 0.5; }); };
    doubleKey.setTooltip ("Double speed (its pitch kept)");
    doubleKey.onClick = [change] { change ([] (auto& o) { o.speed = o.speed > 1.5 ? 1.0 : 2.0; }); };
    for (int semis = -12; semis <= 12; ++semis)
        transposeBox.addItem (semis == 0 ? juce::String ("TRANSPOSE 0") : (semis > 0 ? "+" : "") + juce::String (semis) + " SEMITONES", semis + 13);
    transposeBox.setTooltip ("Moves the preview up or down (audio and MIDI)");
    transposeBox.onChange = [this, change] { const auto semis = transposeBox.getSelectedId() - 13; change ([semis] (auto& o) { o.semitones = semis; }); };
    addAndMakeVisible (transposeBox);
    toKeyBox.addItem ("TO KEY: OFF", 1);
    static const char* const roots[] { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };
    for (int pc = 0; pc < 12; ++pc)
        toKeyBox.addItem ("TO KEY: " + juce::String (roots[pc]), pc + 2);
    toKeyBox.setTooltip ("Moves the preview to this root key, the short way (needs the file's key: from its name, its loop info or your tag)");
    toKeyBox.onChange = [this, change] { const auto key = toKeyBox.getSelectedId() - 2; change ([key] (auto& o) { o.toKey = key; }); };
    addAndMakeVisible (toKeyBox);
    useKey.setTooltip ("Into the song (as a double-click): audio or MIDI on a new track at the playhead, a track preset as a new track, a song opened");
    useKey.onClick = [this] { use (getSelectedRow()); };
    filtersKey.setTooltip ("The attribute filter above the results: files by their category, style, character and more");
    filtersKey.onClick = [this] { setFiltersShown (! filtersShown); };
    inspectorKey.setTooltip ("The attribute inspector at the right: the selected files' tags, to see and set");
    inspectorKey.onClick = [this] { setInspectorShown (! inspectorShown); };
    dynamicKey.setTooltip ("Only the attributes the selected files have");
    dynamicKey.onClick = [this] { setInspectorDynamic (true); };
    definedKey.setTooltip ("Every attribute, set or not");
    definedKey.onClick = [this] { setInspectorDynamic (false); };
    userKey.setTooltip ("An attribute of your own (text, a number, or yes/no), for every file");
    userKey.onClick = [this]
    {
        auto* box = new juce::AlertWindow ("Add a user attribute", "Its name, and what it holds:", juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("name", {}, "NAME");
        box->addComboBox ("type", { "Text", "Number", "Yes / No" }, "TYPE");
        box->getComboBoxComponent ("type")->setSelectedItemIndex (0, juce::dontSendNotification);
        box->addTextEditor ("precision", "0", "DECIMALS (numbers)");
        box->addButton ("ADD", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([this, box, safe = juce::Component::SafePointer<MediaBayPage> (this)] (int result)
        {
            if (safe == nullptr || result != 1)
                return;
            pr7s::UserAttribute attribute;
            attribute.name = box->getTextEditorContents ("name").trim();
            const auto type = box->getComboBoxComponent ("type")->getSelectedItemIndex();
            attribute.type = type == 1 ? pr7s::UserAttributeType::number : type == 2 ? pr7s::UserAttributeType::yesNo : pr7s::UserAttributeType::text;
            attribute.precision = box->getTextEditorContents ("precision").getIntValue();
            if (! addUserAttribute (attribute) && attribute.name.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Add a user attribute",
                                                        "\"" + attribute.name + "\" is already an attribute's name.");
        }), true);
    };

    ratingBox.addItem ("ANY RATING", 1);
    for (int stars = 1; stars <= 5; ++stars)
        ratingBox.addItem (juce::String::repeatedString (juce::String::fromUTF8 ("\xe2\x98\x85"), stars) + (stars < 5 ? " OR MORE" : ""), stars + 1);
    ratingBox.setSelectedId (query.minimumRating + 1, juce::dontSendNotification);
    ratingBox.setTooltip ("Only files rated this or better (rate them in the results' first column)");
    ratingBox.onChange = [this] { setMinimumRating (ratingBox.getSelectedId() - 1); };
    addAndMakeVisible (ratingBox);

    level.setRange (0.0, 1.0, 0.01);
    level.setValue (studio.getMediaPreviewLevel(), juce::dontSendNotification);
    level.setColour (juce::Slider::trackColourId, ParagonColours::accent);
    level.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1b171c));
    level.setTooltip ("The preview's level");
    level.onValueChange = [this] { studio.setMediaPreviewLevel ((float) level.getValue()); };
    addAndMakeVisible (level);

    auto& header = table.getHeader();
    const auto flags = juce::TableHeaderComponent::defaultFlags;
    header.addColumn ("RATING", ratingColumn, 96, 80, 120, flags);
    header.addColumn ("NAME", nameColumn, 220, 120, 800, flags);
    header.addColumn ("TYPE", typeColumn, 90, 70, 160, flags);
    header.addColumn ("LENGTH", lengthColumn, 74, 60, 120, flags);
    header.addColumn ("TEMPO", tempoColumn, 66, 50, 100, flags);
    header.addColumn ("KEY", keyColumn, 54, 40, 90, flags);
    header.addColumn ("CATEGORY", categoryColumn, 130, 80, 300, flags);
    header.addColumn ("STYLE", styleColumn, 120, 80, 300, flags);
    header.addColumn ("CHARACTER", characterColumn, 140, 80, 400, flags & ~juce::TableHeaderComponent::sortable);
    header.addColumn ("FORMAT", formatColumn, 110, 70, 180, flags & ~juce::TableHeaderComponent::sortable);
    header.addColumn ("FOLDER", folderColumn, 220, 100, 800, flags);
    header.addColumn ("SIZE", sizeColumn, 76, 60, 120, flags);
    header.addColumn ("DATE", dateColumn, 120, 90, 180, flags);
    header.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xff1b171c));
    header.setColour (juce::TableHeaderComponent::textColourId, ParagonColours::text);
    header.setColour (juce::TableHeaderComponent::outlineColourId, ParagonColours::panelEdge);
    table.setModel (this);
    table.setRowHeight (26);
    table.setMultipleSelectionEnabled (true);
    table.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0c0a0d));
    table.setOutlineThickness (0);
    addAndMakeVisible (table);

    browser = std::make_unique<Browser> (*this);
    addAndMakeVisible (*browser);
    // Where it was left (0.22.2): the folders open then, and the one chosen.
    {
        const auto state = loadBrowserState();
        if (state.has_value())
        {
            browser->restoreOpenKeys (state->first);
            if (state->second != juce::File() && state->second.isDirectory())
            {
                selectedFolder = state->second;
                browser->showSelected (selectedFolder);
            }
        }
    }

    for (int i = 0; i < numFilterColumns; ++i)
    {
        filterColumns[(size_t) i] = std::make_unique<MediaUi::AttributeFilterColumn> (*this, i);
        addChildComponent (*filterColumns[(size_t) i]);
    }
    inspector = std::make_unique<Inspector> (*this);
    inspectorView = std::make_unique<juce::Viewport>();
    inspectorView->setViewedComponent (inspector.get(), false);
    inspectorView->setScrollBarsShown (true, false);
    addChildComponent (*inspectorView);

    setSize (1600, 900);
    refresh();
    refreshKeys();
    startTimerHz (20);
}

MediaBayPage::~MediaBayPage()
{
    studio.stopMediaPreview();
    table.setModel (nullptr);
}

//==============================================================================
void MediaBayPage::saveSettings()
{
    auto* settings = studio.getSettings();
    if (settings == nullptr)
        return;
    juce::StringArray types;
    for (const auto type : query.types)
        types.add (pr7s::mediaTypeName (type));
    settings->setValue ("mediaBayTypes", types.joinIntoString (","));
    settings->setValue ("mediaBayRating", query.minimumRating);
    settings->setValue ("mediaBaySubfolders", query.subfolders);
    settings->setValue ("mediaBayAutoPlay", autoPlay);
    settings->setValue ("mediaBayCycle", cycle);
    settings->setValue ("mediaBayFilters", filtersShown);
    settings->setValue ("mediaBayInspector", inspectorShown);
    settings->setValue ("mediaBayDynamic", dynamic);
    juce::StringArray columns;
    for (const auto& filter : query.attributes)
        columns.add (attributeToken (filter.attribute));
    settings->setValue ("mediaBayFilterColumns", columns.joinIntoString ("|"));
}

void MediaBayPage::refreshKeys()
{
    using pr7s::MediaType;
    audioKey.setToggleState (query.types.count (MediaType::audio) > 0, juce::dontSendNotification);
    midiKey.setToggleState (query.types.count (MediaType::midi) > 0, juce::dontSendNotification);
    presetsKey.setToggleState (query.types.count (MediaType::trackPreset) > 0, juce::dontSendNotification);
    songsKey.setToggleState (query.types.count (MediaType::song) > 0, juce::dontSendNotification);
    subfoldersKey.setToggleState (query.subfolders, juce::dontSendNotification);
    cycleKey.setToggleState (cycle, juce::dontSendNotification);
    autoPlayKey.setToggleState (autoPlay, juce::dontSendNotification);
    {
        const auto& options = studio.getMediaPreviewOptions();
        linkKey.setToggleState (options.linkTempo, juce::dontSendNotification);
        waitKey.setToggleState (options.waitForPlay, juce::dontSendNotification);
        halfKey.setToggleState (options.speed < 0.75, juce::dontSendNotification);
        doubleKey.setToggleState (options.speed > 1.5, juce::dontSendNotification);
        transposeBox.setSelectedId (options.semitones + 13, juce::dontSendNotification);
        toKeyBox.setSelectedId (options.toKey + 2, juce::dontSendNotification);
    }
    const auto playing = studio.isMediaPreviewing();
    playKey.setButtonText (playing ? "STOP" : "PLAY");
    playKey.setToggleState (playing, juce::dontSendNotification);
    removeKey.setEnabled (true);
    useKey.setEnabled (getSelectedRow() >= 0);
    rescanKey.setEnabled (! database.isScanning());
    filtersKey.setToggleState (filtersShown, juce::dontSendNotification);
    inspectorKey.setToggleState (inspectorShown, juce::dontSendNotification);
    dynamicKey.setToggleState (dynamic, juce::dontSendNotification);
    definedKey.setToggleState (! dynamic, juce::dontSendNotification);
    repaint (0, 0, getWidth(), titleHeight);
}

void MediaBayPage::refreshBrowser()
{
    browser->refreshLists();
}

void MediaBayPage::refresh()
{
    // The selected files stay selected when they're still there.
    juce::StringArray selectedPaths;
    const auto rows = table.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
        if (const auto* item = getResult (rows[i]))
            selectedPaths.add (item->file.getFullPathName());

    query.folder = selectedFolder;
    results = database.find (query);
    if (sortOrder)
        database.sort (results, sortOrder->first, sortOrder->second);
    table.updateContent();
    {
        // (Selecting them again isn't a new choice: AUTO PLAY doesn't start again.)
        const juce::ScopedValueSetter<bool> svs (reselecting, true);
        table.deselectAllRows();
        for (int row = 0; row < (int) results.size() && ! selectedPaths.isEmpty(); ++row)
            if (selectedPaths.contains (getResult (row)->file.getFullPathName()))
                table.selectRow (row, false, false);
    }
    table.repaint();
    refreshKeys();
    refreshTags();
}

void MediaBayPage::refreshTags()
{
    for (auto& column : filterColumns)
        if (column != nullptr)
            column->refresh();
    if (inspector != nullptr)
        inspector->rebuild();
}

void MediaBayPage::attributesEdited()
{
    refresh();
}

const pr7s::MediaItem* MediaBayPage::getResult (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) results.size()))
        return nullptr;
    return &database.getItems()[(size_t) results[(size_t) row]];
}

int MediaBayPage::getSelectedRow() const
{
    return table.getSelectedRow();
}

juce::String MediaBayPage::getStatusText() const
{
    juce::String text;
    text << juce::String (results.size()) << (results.size() == 1 ? " FILE" : " FILES");
    if ((int) results.size() != (int) database.getItems().size())
        text << " OF " << juce::String (database.getItems().size());
    if (database.isScanning())
        text << "  -  SCANNING (" << juce::String (database.getFilesSeen()) << " SEEN)";
    else if (database.getLocations().isEmpty())
        text << "  -  TICK A FOLDER'S BOX IN THE FILE BROWSER TO SCAN IT";
    else if (selectedFolder != juce::File() && database.scanState (selectedFolder) == pr7s::MediaDatabase::ScanState::none)
        text << "  -  THIS FOLDER ISN'T SCANNED: TICK ITS BOX";
    return text;
}

//==============================================================================
void MediaBayPage::setSearch (const juce::String& text)
{
    query.text = text;
    if (search.getText() != text)
        search.setText (text, false);
    refresh();
}

void MediaBayPage::setTypeShown (pr7s::MediaType type, bool shown)
{
    // (0.24.0: PRESETS is both kinds: track presets and VST presets.)
    const auto both = type == pr7s::MediaType::trackPreset || type == pr7s::MediaType::pluginPreset;
    for (const auto t : both ? std::vector<pr7s::MediaType> { pr7s::MediaType::trackPreset, pr7s::MediaType::pluginPreset } : std::vector<pr7s::MediaType> { type })
        if (shown)
            query.types.insert (t);
        else
            query.types.erase (t);
    saveSettings();
    refresh();
}

void MediaBayPage::setMinimumRating (int stars)
{
    query.minimumRating = juce::jlimit (0, 5, stars);
    ratingBox.setSelectedId (query.minimumRating + 1, juce::dontSendNotification);
    saveSettings();
    refresh();
}

void MediaBayPage::setSubfolders (bool on)
{
    query.subfolders = on;
    saveSettings();
    refresh();
}

void MediaBayPage::selectLocation (int index)
{
    selectFolder (juce::isPositiveAndBelow (index, database.getLocations().size()) ? juce::File (database.getLocations()[index]) : juce::File());
}

namespace
{
    // Where the browser was left, when there's no settings file (the tests): for this run only.
    std::optional<std::pair<juce::StringArray, juce::File>>& browserMemory()
    {
        static std::optional<std::pair<juce::StringArray, juce::File>> memory;
        return memory;
    }
}

void MediaBayPage::saveBrowserState()
{
    if (browser == nullptr)
        return;
    const auto open = browser->getOpenKeys();
    if (auto* settings = studio.getSettings())
    {
        settings->setValue ("mediaBayTreeOpen", open.joinIntoString ("\n"));
        settings->setValue ("mediaBayFolder", selectedFolder.getFullPathName());
    }
    else
        browserMemory() = std::pair { open, selectedFolder };
}

std::optional<std::pair<juce::StringArray, juce::File>> MediaBayPage::loadBrowserState() const
{
    if (auto* settings = studio.getSettings())
    {
        if (! settings->containsKey ("mediaBayTreeOpen"))
            return std::nullopt;
        auto open = juce::StringArray::fromLines (settings->getValue ("mediaBayTreeOpen"));
        open.removeEmptyStrings();
        const auto folder = settings->getValue ("mediaBayFolder");
        return std::pair { open, folder.isNotEmpty() ? juce::File (folder) : juce::File() };
    }
    return browserMemory();
}

void MediaBayPage::selectFolder (const juce::File& folder)
{
    selectedFolder = folder;
    saveBrowserState();
    refresh();
    repaint (0, 0, getWidth(), titleHeight);
}

void MediaBayPage::setFolderScanned (const juce::File& folder, bool scanned)
{
    database.setIncluded (folder, scanned);
    if (scanned)
        database.rescan();
    refreshBrowser();
    refresh();
}

void MediaBayPage::refreshFolder (const juce::File& folder)
{
    if (folder != juce::File())
        if (auto* item = browser->reveal (folder))
            item->refreshViews();
    refreshBrowser();
    database.rescan();
    refreshKeys();
}

juce::StringArray MediaBayPage::getFavorites() const
{
    auto favorites = studio.getSettings() != nullptr ? juce::StringArray::fromLines (studio.getSettings()->getValue ("mediaRackFavorites")) : localFavorites;
    favorites.removeEmptyStrings();
    return favorites;
}

bool MediaBayPage::addFavorite (const juce::File& folder)
{
    auto favorites = getFavorites();
    if (! folder.isDirectory() || favorites.contains (folder.getFullPathName()))
        return false;
    favorites.add (folder.getFullPathName());
    if (auto* settings = studio.getSettings())
        settings->setValue ("mediaRackFavorites", favorites.joinIntoString ("\n"));
    else
        localFavorites = favorites;
    // (As the rack's: a favorite's files are scanned.)
    if (! database.isIncluded (folder))
        setFolderScanned (folder, true);
    refreshBrowser();
    return true;
}

bool MediaBayPage::revealFolder (const juce::File& folder)
{
    return browser->reveal (folder) != nullptr;
}

juce::StringArray MediaBayPage::getFolderChildren (const juce::File& folder)
{
    juce::StringArray names;
    if (auto* item = browser->reveal (folder))
    {
        item->setOpen (true);
        if (! item->loaded)
            item->load();
        for (int i = 0; i < item->getNumSubItems(); ++i)
            names.add (static_cast<Browser::FolderItem*> (item->getSubItem (i))->label);
    }
    return names;
}

void MediaBayPage::resetFilters()
{
    query.types.clear();
    for (auto& filter : query.attributes)
        filter.values.clear();
    query.minimumRating = 0;
    ratingBox.setSelectedId (1, juce::dontSendNotification);
    sortOrder.reset();
    table.getHeader().setSortColumnId (0, true);
    saveSettings();
    setSearch ({});
}

void MediaBayPage::shuffle (juce::uint32 seed)
{
    sortOrder.reset();
    table.getHeader().setSortColumnId (0, true);
    pr7s::MediaDatabase::shuffle (results, seed);
    table.updateContent();
    table.repaint();
}

void MediaBayPage::sortBy (pr7s::MediaColumn column, bool ascending)
{
    sortOrder = std::pair { column, ascending };
    refresh();
}

void MediaBayPage::selectRow (int row)
{
    table.selectRow (row);
    selectedRowsChanged (row);
}

void MediaBayPage::setRating (int row, int stars)
{
    if (const auto* item = getResult (row))
    {
        database.setRating (item->file, stars);
        if (query.minimumRating > 0)
            refresh();
        else
            table.repaintRow (row);
    }
}

bool MediaBayPage::play()
{
    const auto* item = getResult (getSelectedRow());
    if (item == nullptr)
        return false;
    previewing = item->file;
    const auto ok = studio.previewMedia (item->file, cycle);
    refreshKeys();
    repaint (previewArea);
    return ok;
}

void MediaBayPage::stop()
{
    studio.stopMediaPreview();
    refreshKeys();
    repaint (previewArea);
}

void MediaBayPage::setAutoPlay (bool on)
{
    autoPlay = on;
    saveSettings();
    refreshKeys();
}

void MediaBayPage::setPreviewOptions (const pr7s::Studio::MediaPreviewOptions& options)
{
    const auto wasPlaying = studio.isMediaPreviewing();
    studio.setMediaPreviewOptions (options);
    refreshKeys();
    if (wasPlaying)
        play();   // (heard again with them)
    repaint (previewArea);
}

void MediaBayPage::setCycle (bool on)
{
    cycle = on;
    studio.setMediaPreviewLoop (on);
    saveSettings();
    refreshKeys();
}

void MediaBayPage::use (int row)
{
    if (const auto* item = getResult (row))
    {
        const auto file = item->file;
        stop();
        if (onUse)
            onUse (file);
    }
}

bool MediaBayPage::addLocation (const juce::File& folder)
{
    if (! folder.isDirectory() || database.getLocations().contains (folder.getFullPathName()))
        return false;
    database.setIncluded (folder, true);
    if (! database.getLocations().contains (folder.getFullPathName()))
        database.addLocation (folder);   // (inside a scanned folder: a location of its own all the same, as asked)
    selectedFolder = folder;
    refreshBrowser();
    database.rescan();
    refresh();
    return true;
}

void MediaBayPage::removeSelectedLocation()
{
    if (selectedFolder == juce::File())
        return;
    setFolderScanned (selectedFolder, false);
}

//==============================================================================
juce::Array<juce::File> MediaBayPage::getSelectedFiles() const
{
    juce::Array<juce::File> files;
    const auto rows = table.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
        if (const auto* item = getResult (rows[i]))
            files.add (item->file);
    return files;
}

void MediaBayPage::selectRows (const juce::Array<int>& rows)
{
    table.deselectAllRows();
    for (const auto row : rows)
        table.selectRow (row, false, false);
    selectedRowsChanged (rows.isEmpty() ? -1 : rows.getLast());
}

void MediaBayPage::setAttribute (const pr7s::AttributeId& id, const juce::String& value)
{
    database.setAttribute (getSelectedFiles(), id, value);
    attributesEdited();
}

void MediaBayPage::removeAttribute (const pr7s::AttributeId& id)
{
    database.removeAttribute (getSelectedFiles(), id);
    attributesEdited();
}

void MediaBayPage::setCharacter (int pair, int side)
{
    database.setCharacter (getSelectedFiles(), pair, side);
    attributesEdited();
}

juce::String MediaBayPage::getAttributeText (const pr7s::AttributeId& id) const
{
    const auto files = getSelectedFiles();
    bool first = true;
    juce::String text;
    for (const auto& item : database.getItems())
    {
        if (! files.contains (item.file))
            continue;
        const auto value = pr7s::MediaDatabase::attributeValues (item, id).joinIntoString (", ");
        if (first)
            text = value;
        else if (value != text)
            return mixedText;
        first = false;
    }
    return text;
}

bool MediaBayPage::addUserAttribute (const pr7s::UserAttribute& attribute)
{
    if (! database.addUserAttribute (attribute))
        return false;
    refreshTags();
    return true;
}

void MediaBayPage::deleteUserAttribute (const juce::String& name)
{
    database.removeUserAttribute (name);
    for (int i = 0; i < (int) query.attributes.size(); ++i)
        if (query.attributes[(size_t) i].attribute.kind == pr7s::MediaAttribute::user && query.attributes[(size_t) i].attribute.user.equalsIgnoreCase (name))
            query.attributes[(size_t) i] = { { pr7s::MediaAttribute::category, {} }, {} };
    saveSettings();
    attributesEdited();
}

void MediaBayPage::addListValue (bool style, const juce::String& name, const juce::String& sub)
{
    using A = pr7s::MediaAttribute;
    if (style)
        database.addStyle (name, sub);
    else
        database.addCategory (name, sub);
    // (The list's own spelling when it was there already: "metal" is Metal.)
    auto spelled = name;
    for (const auto& list : style ? database.getStyles() : database.getCategories())
        if (list.name.equalsIgnoreCase (name))
            spelled = list.name;
    const auto files = getSelectedFiles();
    database.setAttribute (files, { style ? A::style : A::category, {} }, spelled);
    if (sub.isNotEmpty())
        database.setAttribute (files, { style ? A::subStyle : A::subCategory, {} }, sub);
    attributesEdited();
}

void MediaBayPage::setInspectorShown (bool shown)
{
    inspectorShown = shown;
    saveSettings();
    refreshKeys();
    resized();
    repaint();
}

void MediaBayPage::setInspectorDynamic (bool on)
{
    dynamic = on;
    saveSettings();
    refreshKeys();
    inspector->rebuild();
}

juce::StringArray MediaBayPage::getInspectorRows() const
{
    return inspector->getRowNames();
}

void MediaBayPage::setFiltersShown (bool shown)
{
    filtersShown = shown;
    saveSettings();
    refreshKeys();
    resized();
    repaint();
}

void MediaBayPage::setFilterColumn (int column, const pr7s::AttributeId& id)
{
    if (! juce::isPositiveAndBelow (column, (int) query.attributes.size()))
        return;
    query.attributes[(size_t) column] = { id, {} };
    saveSettings();
    refresh();
}

pr7s::AttributeId MediaBayPage::getFilterColumn (int column) const
{
    return juce::isPositiveAndBelow (column, (int) query.attributes.size()) ? query.attributes[(size_t) column].attribute : pr7s::AttributeId {};
}

void MediaBayPage::clickFilterValue (int column, const juce::String& value, bool addToOthers)
{
    if (! juce::isPositiveAndBelow (column, (int) query.attributes.size()))
        return;
    auto& values = query.attributes[(size_t) column].values;
    if (addToOthers)
    {
        if (values.contains (value))
            values.removeString (value);
        else
            values.add (value);
    }
    else if (values.size() == 1 && values[0] == value)
        values.clear();
    else
        values = juce::StringArray (value);
    refresh();
}

std::vector<std::pair<juce::String, int>> MediaBayPage::getFilterValues (int column) const
{
    if (! juce::isPositiveAndBelow (column, (int) query.attributes.size()))
        return {};
    auto query1 = query;
    query1.folder = selectedFolder;
    auto values = database.valueCounts (query1, query.attributes[(size_t) column].attribute);
    // A chosen value no file has any more stays in the list (with 0), so it can be unchosen.
    for (const auto& chosen : query.attributes[(size_t) column].values)
        if (std::none_of (values.begin(), values.end(), [&] (const auto& v) { return v.first == chosen; }))
            values.push_back ({ chosen, 0 });
    return values;
}

juce::StringArray MediaBayPage::getFilterSelection (int column) const
{
    return juce::isPositiveAndBelow (column, (int) query.attributes.size()) ? query.attributes[(size_t) column].values : juce::StringArray();
}

juce::String MediaBayPage::dragDescriptionFor (const juce::StringArray& paths)
{
    return "mediabay:" + paths.joinIntoString ("\n");
}

juce::StringArray MediaBayPage::filesFromDragDescription (const juce::String& description)
{
    if (! description.startsWith ("mediabay:"))
        return {};
    juce::StringArray paths;
    paths.addLines (description.fromFirstOccurrenceOf ("mediabay:", false, false));
    paths.removeEmptyStrings();
    return paths;
}

void MediaBayPage::timerCallback()
{
    if (database.takeScanResults())
    {
        browser->repaint();
        refresh();
    }
    else if (database.isScanning() && ++scanFrames % 10 == 0)
        browser->repaint();   // (the folders being scanned in the accent colour)
    const auto playing = studio.isMediaPreviewing();
    if ((playKey.getButtonText() == "STOP") != playing || database.isScanning())
        refreshKeys();
    if (playing)
        repaint (waveArea.expanded (2));
}

//==============================================================================
int MediaBayPage::getNumRows()
{
    return (int) results.size();
}

void MediaBayPage::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    if (selected)
        g.fillAll (ParagonColours::accent.withAlpha (0.22f));
    else if (row % 2 == 1)
        g.fillAll (juce::Colour (0xff120f13));
    if (const auto* item = getResult (row); item != nullptr && item->file == previewing && studio.isMediaPreviewing())
    {
        g.setColour (ParagonColours::accent);
        g.fillRect (0, 0, 3, height);
    }
    juce::ignoreUnused (width);
}

void MediaBayPage::paintCell (juce::Graphics& g, int row, int column, int width, int height, bool selected)
{
    const auto* item = getResult (row);
    if (item == nullptr)
        return;
    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);
    g.setColour (selected ? ParagonColours::text : ParagonColours::text.withAlpha (0.9f));
    g.setFont (interFont (12.5f));
    switch (column)
    {
        case ratingColumn: drawStars (g, area.toFloat(), item->rating); return;
        case nameColumn:
        {
            g.setColour (typeColour (item->type));
            g.fillRoundedRectangle (area.removeFromLeft (4).withSizeKeepingCentre (4, 14).toFloat(), 1.5f);
            area.removeFromLeft (6);
            g.setColour (ParagonColours::text);
            g.drawText (item->name, area, juce::Justification::centredLeft, true);
            return;
        }
        case typeColumn: g.drawText (pr7s::mediaTypeName (item->type), area, juce::Justification::centredLeft, true); return;
        case lengthColumn:
            g.drawText (item->type == pr7s::MediaType::midi && item->beats > 0.0 ? juce::String (item->beats / 4.0, 1) + " bars" : lengthText (item->seconds),
                        area, juce::Justification::centredLeft, true);
            return;
        case tempoColumn:
        {
            const auto bpm = item->tempo();
            g.drawText (bpm > 0.0 ? juce::String (bpm, bpm == std::round (bpm) ? 0 : 1) : juce::String(), area, juce::Justification::centredLeft, true);
            return;
        }
        case keyColumn: g.drawText (item->keyName(), area, juce::Justification::centredLeft, true); return;
        case formatColumn: g.drawText (formatText (*item), area, juce::Justification::centredLeft, true); return;
        case categoryColumn: g.drawText (joinedPair (item->tags.category, item->tags.subCategory), area, juce::Justification::centredLeft, true); return;
        case styleColumn: g.drawText (joinedPair (item->tags.style, item->tags.subStyle), area, juce::Justification::centredLeft, true); return;
        case characterColumn:
            g.setColour (ParagonColours::textDim);
            g.drawText (item->tags.character.joinIntoString (", "), area, juce::Justification::centredLeft, true);
            return;
        case folderColumn:
            g.setColour (ParagonColours::textDim);
            g.drawText (item->file.getParentDirectory().getFullPathName(), area, juce::Justification::centredLeft, true);
            return;
        case sizeColumn: g.drawText (sizeText (item->size), area, juce::Justification::centredLeft, true); return;
        case dateColumn: g.drawText (item->modified.formatted ("%Y-%m-%d %H:%M"), area, juce::Justification::centredLeft, true); return;
        default: return;
    }
}

void MediaBayPage::cellClicked (int row, int column, const juce::MouseEvent& event)
{
    if (column != ratingColumn)
        return;
    const auto width = (float) table.getHeader().getColumnWidth (ratingColumn) - 12.0f;
    const auto star = juce::jlimit (1, 5, (int) ((event.position.x - 6.0f) / (width / 5.0f)) + 1);
    const auto* item = getResult (row);
    // Clicking the stars it already has takes them all away.
    setRating (row, item != nullptr && item->rating == star ? 0 : star);
}

void MediaBayPage::cellDoubleClicked (int row, int column, const juce::MouseEvent&)
{
    if (column != ratingColumn)
        use (row);
}

void MediaBayPage::selectedRowsChanged (int lastRowSelected)
{
    refreshKeys();
    if (inspector != nullptr)
        inspector->rebuild();
    repaint (previewArea);
    if (autoPlay && ! reselecting && lastRowSelected >= 0 && table.getNumSelectedRows() == 1)
        play();
}

void MediaBayPage::sortOrderChanged (int columnId, bool forwards)
{
    if (const auto column = mediaColumnOf (columnId))
        sortBy (*column, forwards);
}

juce::var MediaBayPage::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    juce::StringArray paths;
    for (int i = 0; i < rows.size(); ++i)
        if (const auto* item = getResult (rows[i]))
            paths.add (item->file.getFullPathName());
    return paths.isEmpty() ? juce::var() : juce::var (dragDescriptionFor (paths));
}

juce::String MediaBayPage::getCellTooltip (int row, int column)
{
    if (column == ratingColumn)
        return "Click a star to rate (click the same star again for none)";
    if (const auto* item = getResult (row))
        return item->file.getFullPathName() + "\nDouble-click: into the song. Drag onto a track.";
    return {};
}

//==============================================================================
void MediaBayPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a0d));
    g.setColour (ParagonColours::accent);
    g.fillRect (0, 0, getWidth(), 3);
    auto title = getLocalBounds().removeFromTop (titleHeight).reduced (gap, 6);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (22.0f));
    g.drawText ("MEDIABAY", title.removeFromLeft (150), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText (getStatusText(), title.withTrimmedRight (110), juce::Justification::centredLeft, true);

    // The FILE BROWSER's heading.
    const auto left = juce::Rectangle<int> (gap, titleHeight + toolbarHeight, locationsWidth, 28);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText ("FILE BROWSER", left, juce::Justification::centredLeft, false);

    if (filtersShown && ! filtersArea.isEmpty())
    {
        g.setColour (juce::Colour (0xff141015));
        g.fillRoundedRectangle (filtersArea.toFloat(), 5.0f);
        g.setColour (ParagonColours::panelEdge);
        g.drawRoundedRectangle (filtersArea.toFloat().reduced (0.5f), 5.0f, 1.0f);
    }
    if (inspectorShown && ! inspectorArea.isEmpty())
    {
        g.setColour (juce::Colour (0xff141015));
        g.fillRoundedRectangle (inspectorArea.toFloat(), 5.0f);
        g.setColour (ParagonColours::panelEdge);
        g.drawRoundedRectangle (inspectorArea.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (ParagonColours::text);
        g.setFont (ParagonLookAndFeel::titleFont (15.0f));
        g.drawText ("ATTRIBUTE INSPECTOR", inspectorArea.reduced (12, 0).removeFromTop (34), juce::Justification::centredLeft, false);
    }

    paintPreviewer (g, previewArea);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (13.0f));
    g.drawText ("LEVEL", levelLabel, juce::Justification::centredRight, false);
}

void MediaBayPage::paintPreviewer (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;
    g.setColour (juce::Colour (0xff141015));
    g.fillRoundedRectangle (area.toFloat(), 5.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 5.0f, 1.0f);
    auto inner = area.reduced (12, 8);
    auto top = inner.removeFromTop (24);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (15.0f));
    const auto* item = getResult (getSelectedRow());
    g.drawText (item != nullptr ? "PREVIEWER  -  " + item->name.toUpperCase() : juce::String ("PREVIEWER  -  SELECT A FILE"), top, juce::Justification::centredLeft, true);

    // The sound's shape (audio being heard) or a plain bar, the playhead over it.
    const auto wave = waveArea.toFloat();
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRoundedRectangle (wave, 3.0f);
    const auto sound = studio.getMediaPreviewSound();
    const auto heardIsAudio = item != nullptr && item->file == previewing && item->type == pr7s::MediaType::audio && sound != nullptr;
    if (heardIsAudio && ! sound->peaks.empty())
    {
        juce::Path shape;
        const auto n = (int) sound->peaks.size();
        const auto columns = juce::jmax (1, (int) wave.getWidth());
        for (int x = 0; x < columns; ++x)
        {
            const auto from = (size_t) ((double) x / columns * n), to = (size_t) juce::jmax ((double) from + 1, (double) (x + 1) / columns * n);
            float peak = 0.0f;
            for (auto i = from; i < juce::jmin (to, (size_t) n); ++i)
                peak = juce::jmax (peak, sound->peaks[i]);
            const auto h = juce::jmin (1.0f, peak) * wave.getHeight() * 0.48f;
            shape.addRectangle (wave.getX() + (float) x, wave.getCentreY() - h, 1.0f, juce::jmax (1.0f, h * 2.0f));
        }
        g.setColour (pr7s::FamilyColours::orange.withAlpha (0.8f));
        g.fillPath (shape);
    }
    else if (item != nullptr)
    {
        g.setColour (ParagonColours::textDim);
        g.setFont (interFont (12.0f));
        const auto waiting = item->file == previewing && studio.getEngine().isPreviewWaiting() && studio.isMediaPreviewing();
        const juce::String text = waiting ? "WAITING FOR THE SONG TO PLAY (WAIT FOR PLAY is on)"
                        : item->type == pr7s::MediaType::midi ? "MIDI: plays through the selected track's instrument"
                        : item->type == pr7s::MediaType::audio ? "Audio: PLAY to hear it"
                        : item->type == pr7s::MediaType::pluginPreset ? "VST preset for " + item->presetPlugin + ": USE IT for a track with it, or drag it onto a track"
                        : "No preview for this type: USE IT or drag it onto the song";
        g.drawText (text, waveArea.reduced (10, 0), juce::Justification::centredLeft, true);
    }
    if (item != nullptr && item->file == previewing && studio.isMediaPreviewing())
    {
        const auto x = wave.getX() + (float) studio.getMediaPreviewProgress() * wave.getWidth();
        g.setColour (juce::Colour (0xfff4eeea));
        g.fillRect (x, wave.getY(), 1.5f, wave.getHeight());
    }
}

void MediaBayPage::resized()
{
    auto area = getLocalBounds();
    auto title = area.removeFromTop (titleHeight).reduced (gap, 8);
    rescanKey.setBounds (title.removeFromRight (96));
    title.removeFromRight (16);
    inspectorKey.setBounds (title.removeFromRight (100));
    title.removeFromRight (4);
    filtersKey.setBounds (title.removeFromRight (84));

    auto bar = area.removeFromTop (toolbarHeight).reduced (gap, 4);
    search.setBounds (bar.removeFromLeft (juce::jmin (380, bar.getWidth() / 3)));
    bar.removeFromLeft (10);
    for (auto* key : { &audioKey, &midiKey, &presetsKey, &songsKey })
    {
        key->setBounds (bar.removeFromLeft (key == &presetsKey ? 84 : 70));
        bar.removeFromLeft (4);
    }
    bar.removeFromLeft (8);
    ratingBox.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (8);
    subfoldersKey.setBounds (bar.removeFromLeft (100));
    resetKey.setBounds (bar.removeFromRight (70));
    bar.removeFromRight (4);
    shuffleKey.setBounds (bar.removeFromRight (84));

    area.removeFromBottom (gap);
    previewArea = area.removeFromBottom (previewerHeight).reduced (gap, 0);
    area.removeFromBottom (gap);
    {
        auto inner = previewArea.reduced (12, 8);
        inner.removeFromTop (28);
        auto keys = inner.removeFromBottom (28);
        playKey.setBounds (keys.removeFromLeft (80));
        keys.removeFromLeft (6);
        cycleKey.setBounds (keys.removeFromLeft (76));
        keys.removeFromLeft (6);
        autoPlayKey.setBounds (keys.removeFromLeft (96));
        keys.removeFromLeft (14);
        linkKey.setBounds (keys.removeFromLeft (100));
        keys.removeFromLeft (6);
        waitKey.setBounds (keys.removeFromLeft (118));
        keys.removeFromLeft (6);
        halfKey.setBounds (keys.removeFromLeft (52));
        keys.removeFromLeft (4);
        doubleKey.setBounds (keys.removeFromLeft (44));
        keys.removeFromLeft (10);
        transposeBox.setBounds (keys.removeFromLeft (150));
        keys.removeFromLeft (6);
        toKeyBox.setBounds (keys.removeFromLeft (150));
        keys.removeFromLeft (14);
        useKey.setBounds (keys.removeFromRight (90));
        keys.removeFromRight (14);
        level.setBounds (keys.removeFromRight (juce::jmin (220, keys.getWidth())).reduced (0, 6));
        keys.removeFromRight (8);
        levelLabel = keys.removeFromRight (50);
        inner.removeFromBottom (6);
        waveArea = inner;
    }

    auto left = area.removeFromLeft (locationsWidth + gap).withTrimmedLeft (gap);
    area.removeFromLeft (gap);
    left.removeFromTop (30);
    auto leftKeys = left.removeFromBottom (28);
    removeKey.setBounds (leftKeys.removeFromRight (90));
    leftKeys.removeFromRight (6);
    addKey.setBounds (leftKeys);
    left.removeFromBottom (6);
    browser->setBounds (left);

    area = area.withTrimmedRight (gap).withTrimmedTop (2);
    inspectorArea = {};
    inspectorView->setVisible (inspectorShown);
    for (auto* key : { &dynamicKey, &definedKey, &userKey })
        key->setVisible (inspectorShown);
    if (inspectorShown)
    {
        inspectorArea = area.removeFromRight (inspectorWidth);
        area.removeFromRight (gap);
        auto inner = inspectorArea.reduced (8, 0);
        inner.removeFromTop (34);
        auto modes = inner.removeFromTop (26);
        dynamicKey.setBounds (modes.removeFromLeft (modes.getWidth() / 2 - 2));
        definedKey.setBounds (modes.removeFromRight (modes.getWidth() - 4));
        inner.removeFromTop (4);
        userKey.setBounds (inner.removeFromBottom (28).withTrimmedBottom (0));
        inner.removeFromBottom (8);
        inspectorView->setBounds (inner);
        inspector->setSize (inner.getWidth() - inspectorView->getScrollBarThickness(), inspector->getHeight());
        inspector->rebuild();
    }
    filtersArea = {};
    for (auto& column : filterColumns)
        column->setVisible (filtersShown);
    if (filtersShown)
    {
        filtersArea = area.removeFromTop (filtersHeight);
        area.removeFromTop (gap);
        auto inner = filtersArea.reduced (8);
        const auto w = (inner.getWidth() - (numFilterColumns - 1) * 8) / numFilterColumns;
        for (int i = 0; i < numFilterColumns; ++i)
        {
            filterColumns[(size_t) i]->setBounds (inner.removeFromLeft (w));
            inner.removeFromLeft (8);
        }
    }
    table.setBounds (area);
}

//==============================================================================
MediaBayWindow::MediaBayWindow (pr7s::Studio& studio)
    : juce::DocumentWindow ("Paragon PR7-S: MediaBay", juce::Colour (0xff0c0a0d), juce::DocumentWindow::closeButton)
{
    page = new MediaBayPage (studio);
    setContentOwned (page, true);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (1200, 640, 7680, 2160);
}

void MediaBayWindow::closeButtonPressed()
{
    page->stop();
    if (onClose)
        onClose();
}

bool MediaBayPage::isFolderOpenInTree (const juce::File& folder)
{
    // (Any item for it that's open, under any heading; nothing is opened to look.)
    bool open = false;
    std::function<void (juce::TreeViewItem&)> walk = [&] (juce::TreeViewItem& item)
    {
        for (int i = 0; i < item.getNumSubItems(); ++i)
        {
            auto* sub = item.getSubItem (i);
            if (auto* folderItem = dynamic_cast<Browser::FolderItem*> (sub); folderItem != nullptr && folderItem->folder == folder && sub->isOpen())
                open = true;
            walk (*sub);
        }
    };
    walk (*browser->getRootItem());
    return open;
}
