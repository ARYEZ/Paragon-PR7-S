#include "ui/HistoryPage.h"

#include "ui/InfoEntry.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const juce::Colour pageColour { 0xff0d0a0e };
    constexpr int columnWidths[] = { 230, 96, 120 };   // action, time, state; details the rest
    constexpr int headerHeight = 24;

    int columnX (int column)
    {
        auto x = 0;
        for (int c = 0; c < column; ++c)
            x += columnWidths[c];
        return x;
    }
}

// The rows and the separator, in the viewport.
struct HistoryPage::List final : public juce::Component
{
    explicit List (HistoryPage& p) : page (p) {}

    int rows() const { return page.getNumRows(); }
    int position() const { return page.getSeparatorRow(); }
    int rowY (int row) const { return row * rowHeight + (row > position() ? separatorHeight : 0); }
    int separatorY() const { return (position() + 1) * rowHeight; }
    // The step whose bottom edge is nearest y (where a dragged separator would go).
    int rowForSeparatorAt (int y) const
    {
        const auto r = (int) std::floor ((double) y / rowHeight + 0.5) - 1;
        return juce::jlimit (0, juce::jmax (0, rows() - 1), r);
    }
    int rowAt (int y) const
    {
        for (int r = 0; r < rows(); ++r)
            if (y >= rowY (r) && y < rowY (r) + rowHeight)
                return r;
        return -1;
    }
    void sizeToRows()
    {
        setSize (juce::jmax (100, getParentWidth()), rows() * rowHeight + separatorHeight);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (pageColour);
        const auto pos = position();
        const auto width = getWidth();
        const auto clip = g.getClipBounds();
        for (int r = 0; r < rows(); ++r)
        {
            const auto y = rowY (r);
            if (y + rowHeight < clip.getY() || y > clip.getBottom())
                continue;
            const auto undone = r > pos;
            g.setColour (r == pos ? pr7s::FamilyColours::pink.withAlpha (0.22f) : (r % 2 == 0 ? juce::Colour (0xff151016) : pageColour));
            g.fillRect (0, y, width, rowHeight);
            for (int c = 0; c < 4; ++c)
            {
                const auto x = columnX (c);
                const auto w = c < 3 ? columnWidths[c] : width - x;
                g.setColour (undone ? ParagonColours::textDim.withAlpha (0.55f)
                                    : c == 2 ? pr7s::FamilyColours::orange.brighter (0.3f) : ParagonColours::text);
                g.setFont (c == 3 ? ParagonLookAndFeel::textFont (14.0f) : ParagonLookAndFeel::titleFont (15.0f));
                g.drawText (page.getCellText (r, c), juce::Rectangle<int> (x + 8, y, w - 12, rowHeight), juce::Justification::centredLeft, true);
            }
        }
        // The separator: a bar in the family's gradient with a grip in its middle.
        if (rows() > 0)
        {
            const auto bar = juce::Rectangle<int> (0, separatorY(), width, separatorHeight).toFloat();
            g.setGradientFill (ParagonLookAndFeel::familyGradient (bar.getTopLeft(), bar.getTopRight()));
            g.fillRect (bar.reduced (0.0f, 1.0f));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            const auto cx = bar.getCentreX(), cy = bar.getCentreY();
            for (int i = -2; i <= 2; ++i)
                g.fillEllipse (cx + (float) i * 8.0f - 1.5f, cy - 1.5f, 3.0f, 3.0f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (nearSeparator (e.y) ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }
    bool nearSeparator (int y) const { return rows() > 0 && y >= separatorY() - 4 && y <= separatorY() + separatorHeight + 4; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = nearSeparator (e.y);
        if (! dragging && e.x < columnWidths[0] / 4)
            if (const auto r = rowAt (e.y); r >= 0)
                page.moveSeparatorTo (r);   // (ours: a click at a row's left edge)
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;
        // Where the bar is now and the mouse's place in the rows as they'd be with it there.
        const auto target = rowForSeparatorAt (e.y < separatorY() ? e.y : e.y - separatorHeight);
        if (target != position())
            page.moveSeparatorTo (target);
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            viewport->autoScroll (e.getEventRelativeTo (viewport).x, e.getEventRelativeTo (viewport).y, 20, 10);
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x >= columnX (3))
            if (const auto r = rowAt (e.y); r >= 0)
                page.beginEdit (r);
    }

    HistoryPage& page;
    bool dragging = false;
};

HistoryPage::HistoryPage (pr7s::Studio& s) : studio (s)
{
    list = std::make_unique<List> (*this);
    viewport.setViewedComponent (list.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    maxLabel.setFont (ParagonLookAndFeel::titleFont (14.0f));
    maxLabel.setColour (juce::Label::textColourId, ParagonColours::textDim);
    maxLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (maxLabel);
    for (const auto n : { 10, 25, 50, 100, 200, 500, 1000 })
        maxSteps.addItem (juce::String (n), n);
    maxSteps.setSelectedId (studio.getMaxUndoSteps(), juce::dontSendNotification);
    if (maxSteps.getSelectedId() == 0)
        maxSteps.setText (juce::String (studio.getMaxUndoSteps()), juce::dontSendNotification);
    maxSteps.setTooltip ("Maximum Undo Steps: how many edits are kept to go back to (the oldest go first)");
    maxSteps.onChange = [this] { if (maxSteps.getSelectedId() > 0) setMaxSteps (maxSteps.getSelectedId()); };
    addAndMakeVisible (maxSteps);
    setWantsKeyboardFocus (true);
    setSize (preferredWidth, preferredHeight);
    refresh();
    startTimerHz (10);
}

HistoryPage::~HistoryPage()
{
    stopTimer();
}

int HistoryPage::getNumRows() const { return (int) studio.getHistory().size(); }
int HistoryPage::getSeparatorRow() const { return studio.getHistoryPosition(); }

juce::String HistoryPage::getCellText (int row, int column) const
{
    const auto& steps = studio.getHistory();
    if (row < 0 || row >= (int) steps.size())
        return {};
    const auto& step = steps[(size_t) row];
    switch (column)
    {
        case 0: return step.action;
        case 1: return step.time.formatted ("%H:%M:%S");
        case 2: return row == 0 ? juce::String ("START") : row <= getSeparatorRow() ? juce::String ("DONE") : juce::String ("UNDONE");
        default: return step.details;
    }
}

juce::Rectangle<int> HistoryPage::getSeparatorBounds() const
{
    return juce::Rectangle<int> (0, list->separatorY(), list->getWidth(), separatorHeight)
               .translated (viewport.getX() - viewport.getViewPositionX(), viewport.getY() - viewport.getViewPositionY());
}

juce::Rectangle<int> HistoryPage::getRowBounds (int row) const
{
    return juce::Rectangle<int> (0, list->rowY (row), list->getWidth(), rowHeight)
               .translated (viewport.getX() - viewport.getViewPositionX(), viewport.getY() - viewport.getViewPositionY());
}

void HistoryPage::moveSeparatorTo (int row)
{
    studio.goToHistory (row);
    refresh();
}

void HistoryPage::setDetails (int row, const juce::String& text)
{
    studio.setHistoryDetails (row, text);
    refresh();
}

void HistoryPage::setMaxSteps (int steps)
{
    studio.setMaxUndoSteps (steps);
    refresh();
}

void HistoryPage::refresh()
{
    seenChange = studio.getChangeVersion();
    const auto rows = getNumRows(), position = getSeparatorRow();
    const auto grew = rows > seenRows;
    list->sizeToRows();
    list->repaint();
    if (grew || position != seenPosition)   // (the separator kept in view)
    {
        const auto top = list->separatorY() - rowHeight, bottom = list->separatorY() + separatorHeight + rowHeight;
        auto y = viewport.getViewPositionY();
        if (bottom > y + viewport.getHeight()) y = bottom - viewport.getHeight();
        if (top < y) y = top;
        viewport.setViewPosition (0, juce::jmax (0, y));
    }
    seenRows = rows;
    seenPosition = position;
}

void HistoryPage::timerCallback()
{
    if (studio.getChangeVersion() != seenChange || getNumRows() != seenRows || getSeparatorRow() != seenPosition)
        refresh();
}

void HistoryPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    auto area = getLocalBounds().reduced (16, 12);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (18.0f));
    g.drawText ("EDIT HISTORY", area.removeFromTop (24), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::textFont (12.0f));
    g.drawText ("DRAG THE BAR UP TO UNDO, DOWN TO REDO  -  DOUBLE-CLICK DETAILS TO TYPE A NOTE",
                getLocalBounds().reduced (16, 12).withTrimmedLeft (170).withTrimmedRight (200).removeFromTop (24), juce::Justification::centredLeft, true);
    // The column titles over the list.
    auto header = juce::Rectangle<int> (viewport.getX(), viewport.getY() - headerHeight, viewport.getWidth(), headerHeight);
    g.setColour (juce::Colour (0xff1c161d));
    g.fillRect (header);
    g.setColour (ParagonColours::textDim);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    const char* titles[] = { "ACTION", "TIME", "STATE", "DETAILS" };
    for (int c = 0; c < 4; ++c)
        g.drawText (titles[c], header.getX() + columnX (c) + 8, header.getY(), c < 3 ? columnWidths[c] - 12 : header.getWidth() - columnX (c) - 12,
                    headerHeight, juce::Justification::centredLeft, false);
}

void HistoryPage::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    auto top = area.removeFromTop (24);
    maxSteps.setBounds (top.removeFromRight (80));
    maxLabel.setBounds (top.removeFromRight (100));
    area.removeFromTop (8 + headerHeight);
    viewport.setBounds (area);
    list->sizeToRows();
    if (editor != nullptr && editRow >= 0)
        editor->setBounds (getRowBounds (editRow).withTrimmedLeft (columnX (3)));
}

void HistoryPage::beginEdit (int row)
{
    editRow = row;
    auto entry = std::make_unique<InfoEntry>();
    entry->setFont (ParagonLookAndFeel::textFont (14.0f));
    entry->setText (getCellText (row, 3), false);
    entry->setSelectAllWhenFocused (true);
    auto finish = [this] (bool apply)
    {
        if (editor == nullptr)
            return;
        const auto text = editor->getText();
        const auto r = editRow;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<HistoryPage> (this)]
        {
            if (safe != nullptr)
            {
                safe->editor.reset();
                safe->editRow = -1;
                safe->refresh();
            }
        });
        if (apply)
            setDetails (r, text);
    };
    entry->onEnter = [finish] (bool) { finish (true); };
    entry->onEscapeKey = [finish] { finish (false); };
    entry->onFocusLost = [finish] { finish (true); };
    editor = std::move (entry);
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
}

bool HistoryPage::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    if (! key.getModifiers().isAnyModifierKeyDown())
    {
        if (code == juce::KeyPress::upKey && getSeparatorRow() > 0)
        {
            moveSeparatorTo (getSeparatorRow() - 1);
            return true;
        }
        if (code == juce::KeyPress::downKey && getSeparatorRow() + 1 < getNumRows())
        {
            moveSeparatorTo (getSeparatorRow() + 1);
            return true;
        }
    }
    const auto handled = onOtherKey != nullptr && onOtherKey (key);
    refresh();
    return handled;
}

//==============================================================================
HistoryWindow::HistoryWindow (pr7s::Studio& s)
    : DocumentWindow ("Paragon PR7-S: edit history", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    page = new HistoryPage (s);
    setContentOwned (page, true);
    setResizable (true, false);
    setResizeLimits (560, 300, 3000, 2000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void HistoryWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
