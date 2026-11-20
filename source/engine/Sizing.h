#pragma once

#include "Project.h"
#include "TempoMap.h"

// (0.187.0) The Object Selection tool's three ways of sizing a part (the manual's Normal Sizing, Sizing
// Moves Contents, Sizing Applies Time Stretch), and the four ways a warped clip can be stretched. Plain
// functions on parts: no screen, no engine. Normal Sizing is the trim the arrangement always did
// (ArrangeView's Drag::resize / Drag::trimStart).
namespace pr7s
{
    enum class Sizing { normal, movesContents, timeStretch };
    const char* sizingName (Sizing);   // "NORMAL SIZING", "SIZING MOVES CONTENTS", "SIZING APPLIES TIME STRETCH"
    Sizing nextSizing (Sizing);         // normal -> moves contents -> time stretch -> normal (1 pressed again)

    // How a warped clip's audio is stretched (`Part::stretchMode`; our own names and engine: Signalsmith
    // Stretch, an open one, so not the sound of any other program's).
    namespace StretchModes
    {
        constexpr int clean = 0, voice = 1, light = 2, tape = 3, count = 4;
        const char* name (int mode);          // "CLEAN", "VOICE", "LIGHT", "TAPE"
        const char* description (int mode);   // a line for the menu
    }

    // Sizing Moves Contents at the left edge ("the event is resized, and the content follows"): the start
    // to `newStart` with its content carried along (a MIDI part's notes and controllers keep their places
    // from its start; an audio clip's offset is kept, so its sound starts at its new start; a warped clip's
    // markers go with it). The end stays where it was. At the right edge the content stays with the start,
    // as Normal Sizing does.
    void moveStartWithContents (Part&, double newStart, const TempoMap&);
    // Sizing Applies Time Stretch: the part to [newStart, newStart + newLength), what's in it stretched or
    // squeezed to fit. MIDI: notes (starts and lengths) and controllers scaled by new / old length. Audio:
    // warp markers (made at its edges if it had none) scaled, so the clip is heard faster or slower, its
    // file untouched (Flatten writes one); its fades scaled too.
    void stretchPartTo (Part&, double newStart, double newLength, const TempoMap&);
}
