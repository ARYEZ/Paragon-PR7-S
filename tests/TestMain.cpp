#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

#if JUCE_LINUX
 #include <X11/Xlib.h>

namespace
{
    // On a real Linux desktop the window manager registers the standard
    // window names (WM_PROTOCOLS, _NET_WM_STATE...) with the display. The
    // tests' virtual display has no window manager, and JUCE uses those names
    // when it opens a window (the key editor's own window), which then fails.
    // So the tests register them, as a window manager would, on a connection
    // kept open for the whole run (the display forgets them once nothing is
    // connected).
    void registerWindowManagerNames()
    {
        static Display* display = XOpenDisplay (nullptr);
        if (display == nullptr)
            return;
        for (const auto* name : { "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "_NET_WM_PING", "WM_CHANGE_STATE", "WM_STATE",
                                  "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_STATE", "_NET_WM_STATE_HIDDEN",
                                  "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_SKIP_TASKBAR",
                                  "_NET_WM_STATE_ABOVE", "_NET_WM_ALLOWED_ACTIONS", "_NET_WM_ACTION_RESIZE", "_NET_WM_ACTION_FULLSCREEN",
                                  "_NET_WM_ACTION_MINIMIZE", "_NET_WM_ACTION_CLOSE", "_MOTIF_WM_HINTS", "_NET_FRAME_EXTENTS", "_NET_WORKAREA" })
            XInternAtom (display, name, False);
        XSync (display, False);
    }
}
#endif

int main (int argc, char* argv[])
{
   #if JUCE_LINUX
    registerWindowManagerNames();
   #endif
    // JUCE's windows, timers and plugin classes expect the message manager
    // to exist, as it does in the running app.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    return Catch::Session().run (argc, argv);
}
