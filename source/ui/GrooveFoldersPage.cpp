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
        const auto marked = page.studio.isGrooveFolderFavourite (folder);
        const auto inherited = ! marked && isInFavouriteFolder (folder.getChildFile ("x"), page.studio.getGrooveFavouriteFolders());
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
        tree.repaint();
        refreshSummary();
    };
    addAndMakeVisible (clearKey);
    summary.setFont (ParagonLookAndFeel::textFont (14.0f));
    summary.setColour (juce::Label::textColourId, ParagonColours::textDim);
    addAndMakeVisible (summary);
    refresh();
}

GrooveFoldersPage::~GrooveFoldersPage()
{
    tree.setRootItem (nullptr);
}

std::pair<size_t, size_t> GrooveFoldersPage::rangeOf (const juce::String& prefix) const
{
    const auto from = std::lower_bound (paths.begin(), paths.end(), prefix);
    auto to = from;
    while (to != paths.end() && to->startsWith (prefix))
        ++to;
    return { (size_t) std::distance (paths.begin(), from), (size_t) std::distance (paths.begin(), to) };
}

int GrooveFoldersPage::countIn (const juce::File& folder) const
{
    const auto [from, to] = rangeOf (withSeparator (folder));
    return (int) (to - from);
}

juce::StringArray GrooveFoldersPage::childrenOf (const juce::File& folder) const
{
    const auto prefix = withSeparator (folder);
    const auto [from, to] = rangeOf (prefix);
    juce::StringArray names;
    for (auto i = from; i < to; ++i)
    {
        const auto rest = paths[i].substring (prefix.length());
        const auto cut = rest.indexOfChar (juce::File::getSeparatorChar());
        if (cut > 0)
        {
            const auto name = rest.substring (0, cut);
            if (names.isEmpty() || names[names.size() - 1] != name)
                names.addIfNotAlreadyThere (name);
        }
    }
    names.sortNatural();
    return names;
}

void GrooveFoldersPage::toggle (const juce::File& folder)
{
    studio.setGrooveFolderFavourite (folder, ! studio.isGrooveFolderFavourite (folder));
    tree.repaint();
    refreshSummary();
}

juce::String GrooveFoldersPage::getSummary() const
{
    return summary.getText();
}

void GrooveFoldersPage::refreshSummary()
{
    const auto folders = studio.getGrooveFavouriteFolders();
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
    const auto context = studio.getComposerContext (0.0);
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
