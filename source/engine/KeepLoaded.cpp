#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <psapi.h>
 #pragma comment (lib, "psapi.lib")
#endif

#include "KeepLoaded.h"
#include "SessionLog.h"

#include <iterator>
#include <set>
#include <vector>

namespace pr7s
{
    int keepPluginLoaded (const juce::String& pluginPath)
    {
       #if defined (_WIN32)
        // The plug-in's file is the .vst3 itself, or inside a .vst3 folder (Contents\x86_64-win\..):
        // every loaded module at that path or under it is kept.
        const auto root = juce::File (pluginPath).getFullPathName();
        if (root.isEmpty())
            return 0;
        static std::set<juce::String> kept;   // (message thread only: plug-ins are made there)
        std::vector<HMODULE> modules (1024);
        DWORD needed = 0;
        if (! EnumProcessModules (GetCurrentProcess(), modules.data(), (DWORD) (modules.size() * sizeof (HMODULE)), &needed))
            return 0;
        modules.resize (juce::jmin ((size_t) needed / sizeof (HMODULE), modules.size()));
        int newlyKept = 0;
        for (auto* module : modules)
        {
            wchar_t name[MAX_PATH * 2] = {};
            if (GetModuleFileNameExW (GetCurrentProcess(), module, name, (DWORD) std::size (name)) == 0)
                continue;
            const juce::String path (name);
            if (! (path.equalsIgnoreCase (root) || path.startsWithIgnoreCase (root + "\\")))
                continue;
            if (kept.count (path.toLowerCase()) > 0)
                continue;
            HMODULE pinned = nullptr;
            if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_PIN, name, &pinned))
            {
                kept.insert (path.toLowerCase());
                ++newlyKept;
                SessionLog::note ("  Kept in memory until the studio closes: " + path);
            }
        }
        return newlyKept;
       #else
        juce::ignoreUnused (pluginPath);
        return 0;
       #endif
    }
}
