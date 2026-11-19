#include "GrooveFoldersPage.h"

#include "ParagonLookAndFeel.h"

using namespace pr7s;

namespace
{
    juce::String withSeparator (const juce::File& folder)
    {
        auto p = folder.getFullPathName();
        if (! p.endsWithChar (juce::File::getSeparatorChar()))
            p << juce::File::getSeparatorChar();
        return p;
    }

    void drawStar (juce::Graphics& g, juce::Rectangle<float> box, bool on, bool inherited)
    {
        juce::Path star;
        star.addStar (box.getCentre(), 5, box.getWidth() * 0.22f, box.getWidth() * 0.48f, 0.0f);
        if (on)
        {
            g.setGradientFill (ParagonLookAndFeel::familyGradient (box.getTopLeft(), box.getBottomRight()));
            g.fillPath (star);
        }
        else if (inherited)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.35f));
            g.fillPath (star);
        }
        else
        {
            g.setColour (ParagonColours::textDim.withAlpha (0.7f));
            g.strokePath (star, juce::PathStrokeType (1.2f));
        }
    }
}

class GrooveFoldersPage::FolderItem final : public juce::TreeViewItem
{
public:
    FolderItem (GrooveFoldersPage& p, juce::File f, bool isRoot = false) : page (p), folder (std::move (f)), hiddenRoot (isRoot) {}

    bool mightContainSubItems() override
    {
        return hiddenRoot || ! page.childrenOf (folder).isEmpty();
    }

    juce::String getUniqueName() const override { return hiddenRoot ? juce::String ("root") : folder.getFullPathName(); }

    int getItemHeight() const override { return 28; }

    void itemOpennessChanged (bool open) override
    {
        if (! open || hiddenRoot || getNumSubItems() > 0)
            return;
        for (const auto& name : page.childrenOf (folder))
            addSubItem (new FolderItem (page, folder.getChildFile (name)));
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        if (hiddenRoot)
            return;
        const auto marked = page.isFavourite (folder);
        const auto inherited = ! marked && isInFavouriteFolder (folder.getChildFile ("x"), page.favourites);
        drawStar (g, juce::Rectangle<float> (2.0f, 4.0f, (float) height - 8.0f, (float) height - 8.0f), marked, inherited);
        const auto count = page.countIn (folder);
        auto text = juce::Rectangle<int> (starWidth + 6, 0, width - starWidth - 6, height);
        g.setFont (ParagonLookAndFeel::textFont (16.0f));
        g.setColour (marked ? ParagonColours::text : ParagonColours::text.withAlpha (count > 0 ? 0.9f : 0.45f));
        const auto name = getParentItem() != nullptr && getParentItem()->getParentItem() == nullptr ? folder.getFullPathName() : folder.getFileName();
        g.drawText (name.toUpperCase(), text.removeFromLeft (juce::jmax (40, width - starWidth - 130)), juce::Justification::centredLeft, true);
        g.setFont (ParagonLookAndFeel::textFont (13.0f));
        g.setColour (ParagonColours::textDim);
        g.drawText (juce::String (count) + (count == 1 ? " GROOVE" : " GROOVES") + (inherited ? "  (IN A FAVOURITE)" : ""), text,
                    juce::Justification::centredRight, true);
    }

    // (JUCE's own triangle is near black: lost on the dark panel)
    void paintOpenCloseButton (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour, bool isMouseOver) override
    {
        const auto box = area.reduced (area.getWidth() * 0.3f, area.getHeight() * 0.3f);
        juce::Path arrow;
        if (isOpen())
            arrow.addTriangle (box.getX(), box.getY() + box.getHeight() * 0.2f, box.getRight(), box.getY() + box.getHeight() * 0.2f,
                               box.getCentreX(), box.getBottom() - box.getHeight() * 0.1f);
        else
            arrow.addTriangle (box.getX() + box.getWidth() * 0.2f, box.getY(), box.getX() + box.getWidth() * 0.2f, box.getBottom(),
                               box.getRight() - box.getWidth() * 0.1f, box.getCentreY());
        g.setColour (isMouseOver ? ParagonColours::text : ParagonColours::textDim);
        g.fillPath (arrow);
    }

    void itemClicked (const juce::MouseEvent& e) override
    {
        if (hiddenRoot)
            return;
        if (e.x < starWidth)
            page.toggle (folder);
        else
            setOpen (! isOpen());
    }

    GrooveFoldersPage& page;
    juce::File folder;
    bool hiddenRoot = false;
};

GrooveFoldersPage::GrooveFoldersPage (Studio& s) : studio (s)
{
    tree.setRootItemVisible (false);
    tree.setDefaultOpenness (false);
    tree.setIndentSize (18);
    tree.setColour (juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (tree);
    clearKey.setWantsKeyboardFocus (false);
    clearKey.setTooltip ("No folder a favourite: a section's grooves are half Toontrack's and a quarter your other packs' again, "
                         "the favourites' quarter shared between them");
    clearKey.onClick = [this]
    {
        studio.setGrooveFavouriteFolders ({});
        favourites = studio.getGrooveFavouriteFolders();
        tree.repaint();
        refreshSummary();
    };
    addAndMakeVisible (clearKey);
    summary.setFont (ParagonLookAndFeel::textFont (14.0f));
    summary.setColour (juce::Label::textColourId, ParagonColours::textDim);
    addAndMakeVisible (summary);
    refresh();
    startTimerHz (2);
}

void GrooveFoldersPage::timerCallback()
{
    if (! isShowing())
        return;
    if (const auto now = studio.getGrooveFavouriteFolders(); now != favourites)
    {
        favourites = now;
        tree.repaint();
        refreshSummary();
    }
}

GrooveFoldersPage::~GrooveFoldersPage()
{
    tree.setRootItem (nullptr);
}

// (0.179.0) Both ends found by halving: a folder holding 100,000 grooves was walked one by one to its end, on every
// row drawn. Every path starting with the prefix (which ends in a separator) sorts at or after it and before the
// prefix with that separator one character higher.
std::pair<size_t, size_t> GrooveFoldersPage::rangeOf (const juce::String& prefix) const
{
    const auto from = std::lower_bound (paths.begin(), paths.end(), prefix);
    auto past = prefix.dropLastCharacters (1);
    past += juce::String::charToString ((juce::juce_wchar) (prefix.getLastCharacter() + 1));
    const auto to = std::lower_bound (from, paths.end(), past);
    return { (size_t) std::distance (paths.begin(), from), (size_t) std::distance (paths.begin(), to) };
}

int GrooveFoldersPage::countIn (const juce::File& folder) const
{
    const auto key = folder.getFullPathName();
    if (const auto found = counts.find (key); found != counts.end())
        return found->second;
    const auto [from, to] = rangeOf (withSeparator (folder));
    return counts[key] = (int) (to - from);
}

juce::StringArray GrooveFoldersPage::childrenOf (const juce::File& folder) const
{
    const auto key = folder.getFullPathName();
    if (const auto found = children.find (key); found != children.end())
        return found->second;
    const auto prefix = withSeparator (folder);
    auto [from, to] = rangeOf (prefix);
    juce::StringArray names;
    // (0.179.0) A subfolder's name taken from its first groove, then the rest of that subfolder jumped over.
    while (from < to)
    {
        const auto rest = paths[from].substring (prefix.length());
        const auto cut = rest.indexOfChar (juce::File::getSeparatorChar());
        if (cut <= 0)
        {
            ++from;   // (a groove right in the folder)
            continue;
        }
        const auto name = rest.substring (0, cut);
        names.addIfNotAlreadyThere (name);
        const auto [_, next] = rangeOf (prefix + name + juce::File::getSeparatorChar());
        from = juce::jmax (from + 1, next);
    }
    names.sortNatural();
    return children[key] = names;
}

bool GrooveFoldersPage::isFavourite (const juce::File& folder) const
{
    for (const auto& f : favourites)
        if (juce::File (f) == folder)
            return true;
    return false;
}

void GrooveFoldersPage::toggle (const juce::File& folder)
{
    studio.setGrooveFolderFavourite (folder, ! studio.isGrooveFolderFavourite (folder));
    favourites = studio.getGrooveFavouriteFolders();
    tree.repaint();
    refreshSummary();
}

juce::String GrooveFoldersPage::getSummary() const
{
    return summary.getText();
}

void GrooveFoldersPage::refreshSummary()
{
    const auto& folders = favourites;
    auto grooves = 0;
    for (const auto& f : folders)
        if (! isInFavouriteFolder (juce::File (f), folders))   // (one inside another counted with it)
            grooves += countIn (juce::File (f));
    summary.setText (folders.isEmpty() ? juce::String ("NO FAVOURITE FOLDERS YET: CLICK A STAR")
                                       : juce::String (folders.size()) + (folders.size() == 1 ? " FAVOURITE FOLDER, " : " FAVOURITE FOLDERS, ")
                                             + juce::String (grooves) + (grooves == 1 ? " GROOVE IN THEM" : " GROOVES IN THEM"),
                     juce::dontSendNotification);
}

void GrooveFoldersPage::refresh()
{
    std::unique_ptr<juce::XmlElement> open;
    if (root != nullptr)
        open = tree.getOpennessState (false);
    paths.clear();
    counts.clear();
    children.clear();
    favourites = studio.getGrooveFavouriteFolders();
    const auto context = studio.getComposerContext (0.0);
    paths.reserve (context.libraryGrooves.size());
    for (const auto& g : context.libraryGrooves)
        paths.push_back (g.file.getFullPathName());
    std::sort (paths.begin(), paths.end());
    tree.setRootItem (nullptr);
    root = std::make_unique<FolderItem> (*this, juce::File(), true);
    for (const auto& r : studio.getGrooveFolderRoots())
        root->addSubItem (new FolderItem (*this, r));
    tree.setRootItem (root.get());
    root->setOpen (true);
    if (open != nullptr)
        tree.restoreOpennessState (*open, false);
    refreshSummary();
}

void GrooveFoldersPage::paint (juce::Graphics& g)
{
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("GROOVE FOLDERS", titleArea.toFloat(), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (13.0f));
    g.drawFittedText ("CLICK A STAR TO MARK A FOLDER (AND EVERYTHING INSIDE IT) AS A FAVOURITE. EACH SECTION'S 200 LIBRARY GROOVES: 50 FROM YOUR "
                      "FAVOURITE FOLDERS, 100 FROM THE REST OF TOONTRACK, 50 FROM YOUR OTHER PACKS. AUTOMATIC AND THE FILLS PICK FAVOURITES FIRST.",
                      noteArea, juce::Justification::topLeft, 3);
    if (studio.getGrooveFolderRoots().empty())
        g.drawText ("NO GROOVE LIBRARY FOUND: LINK YOUR GROOVE FOLDERS IN PR7-D (OR ADD THEM TO THE MEDIABAY)", tree.getBounds().toFloat(),
                    juce::Justification::centred, true);
}

void GrooveFoldersPage::resized()
{
    auto area = getLocalBounds();
    titleArea = area.removeFromTop (26);
    noteArea = area.removeFromTop (44);
    auto bottom = area.removeFromBottom (34);
    clearKey.setBounds (bottom.removeFromRight (220));
    bottom.removeFromRight (12);
    summary.setBounds (bottom);
    area.removeFromBottom (8);
    tree.setBounds (area);
}
