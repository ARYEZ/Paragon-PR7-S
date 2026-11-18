#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// (0.83.4) A window of the studio's own that takes a MIDI part dragged out of the arrangement (the owner: drag
// a part into the composer and it builds that section around it). The arrangement asks while it drags a part:
// over one of these, the part isn't moved or dragged out as a file; let go over it, it's handed over by id
// (all inside the studio: no file drag through Windows).
class PartDropTarget
{
public:
    virtual ~PartDropTarget() = default;
    virtual bool isInterestedInPart (int partId) = 0;
    virtual void partDragHover (bool over) = 0;    // lit while a part it wants is held over it
    virtual void partDropped (int partId) = 0;

    // The one under a point on the screen (the component there or one of its parents), if any.
    static PartDropTarget* at (juce::Point<float> screen)
    {
        for (auto* c = juce::Desktop::getInstance().findComponentAt (screen.roundToInt()); c != nullptr; c = c->getParentComponent())
            if (auto* t = dynamic_cast<PartDropTarget*> (c))
                return t;
        return nullptr;
    }
};
