#pragma once

#include <juce_core/juce_core.h>

namespace pr7s
{
    // (0.80.1, the owner's freeze) A plug-in's file kept in memory until the studio closes, once one
    // of its instances has loaded. When the last instance of a plug-in goes (a track disabled,
    // removed, rendered in place), the plug-in host frees its file, and Windows then runs the
    // plug-in's own clean-up while it holds the loader lock (every thread loading or freeing a
    // file waits). The owner's log: the PR7 plug-ins' clean-up started the graphics card there and
    // the NVIDIA driver waited for good. Kept loaded, the file is never freed in a session (the
    // studio ends its process without unloading anything), so that clean-up never runs under the
    // lock. Windows only (elsewhere it does nothing). Returns how many files it newly kept.
    int keepPluginLoaded (const juce::String& pluginPath);
}
