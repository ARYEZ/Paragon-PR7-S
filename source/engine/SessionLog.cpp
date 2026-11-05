#if defined (_WIN32)
 // (Windows' own headers first, without their min/max macros, then JUCE's.)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <dbghelp.h>
 #include <psapi.h>
 #include <tlhelp32.h>
 #pragma comment (lib, "dbghelp.lib")
 #pragma comment (lib, "psapi.lib")
#endif

#include "SessionLog.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstring>
#include <exception>
#include <map>
#include <thread>
#include <vector>

#if ! defined (_WIN32)
 #include <execinfo.h>
 #include <unistd.h>
#endif

namespace pr7s::SessionLog
{
    namespace
    {
        constexpr auto closedLine = "Closed normally.";

        struct State
        {
            juce::CriticalSection lock;
            std::unique_ptr<juce::FileOutputStream> out;
            juce::File file;
            juce::Time started;
        };
        State& state()
        {
            static State s;   // (lives to the end: a crash late in shutdown can still write)
            return s;
        }

        thread_local bool onAudioThread = false;
        std::atomic<bool> crashing { false };
        std::atomic<bool> closingNow { false };

        juce::String stamp()
        {
            return juce::Time::getCurrentTime().formatted ("%H:%M:%S.") + juce::String (juce::Time::getCurrentTime().getMilliseconds()).paddedLeft ('0', 3);
        }

        double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }

        void writeRaw (const juce::String& text, bool fromCrash)
        {
            auto& s = state();
            // A crash mustn't wait for ever on a lock another thread holds (it may be the one that crashed holding it).
            auto locked = s.lock.tryEnter();
            for (int i = 0; ! locked && fromCrash && i < 100; ++i)
            {
                juce::Thread::sleep (10);
                locked = s.lock.tryEnter();
            }
            if (! locked && ! fromCrash)
            {
                s.lock.enter();
                locked = true;
            }
            if (s.out != nullptr)
            {
                s.out->writeText (text, false, false, "\r\n");
                s.out->flush();
            }
            if (locked)
                s.lock.exit();
        }

        // The threads that run the sound (the device's callback, the helper cores): marked so a
        // crash or a freeze report can name them. Ids only (no allocation: the audio thread marks itself).
        constexpr int audioIdCount = 32;
        std::atomic<juce::uint64> audioIds[audioIdCount] {};

        juce::uint64 thisThreadKey() noexcept
        {
           #if defined (_WIN32)
            return (juce::uint64) GetCurrentThreadId();
           #else
            return (juce::uint64) (juce::pointer_sized_int) juce::Thread::getCurrentThreadId() + 1;
           #endif
        }

        bool isScreenThread()
        {
            auto* mm = juce::MessageManager::getInstanceWithoutCreating();
            return mm != nullptr && mm->isThisTheMessageThread();
        }

        juce::String threadName()
        {
            if (onAudioThread)
                return "the audio thread (the sound: instruments and effects play here)";
            if (isScreenThread())
                return "the screen's thread (the window, clicks and keys)";
            if (auto* t = juce::Thread::getCurrentThread())
                return "the studio's thread \"" + t->getThreadName() + "\"";
            return "another thread (a driver's or a plug-in's own)";
        }

        //======================================================================
        // (0.52.1) Steps under way: fixed slots, so a report can read them while the thread that
        // began them is stuck (no lock to wait for, nothing it could be holding).
        struct StepSlot
        {
            std::atomic<juce::uint64> thread { 0 };   // 0: free
            std::atomic<double> started { 0.0 };
            char text[240] {};
            char where[96] {};
        };
        constexpr int stepSlotCount = 48;
        StepSlot stepSlots[stepSlotCount];

        void copyText (char* into, size_t size, const juce::String& text)
        {
            const auto utf8 = text.toRawUTF8();
            auto length = std::strlen (utf8);
            if (length >= size)
                length = size - 1;
            while (length > 0 && (((unsigned char) utf8[length]) & 0xc0) == 0x80)
                --length;   // (not halfway through a character)
            std::memcpy (into, utf8, length);
            into[length] = 0;
        }

        // A long job (a render): beaten as it moves.
        std::atomic<int> jobDepth { 0 };
        std::atomic<double> lastJobBeat { 0.0 };
        char jobName[200] {};

       #if defined (_WIN32)
        //======================================================================
        // The modules (the studio, Windows' own, every plug-in's .vst3, drivers) and where each
        // lies in memory. Read through the process's own records (psapi: no loader lock), kept
        // up to date by the watch while all is well, so a report never needs to ask Windows then:
        // a plug-in stuck loading or unloading holds the loader lock, and asking for a module's
        // name the usual way (GetModuleHandleEx / GetModuleFileName) or loading symbols for the
        // whole process (SymInitialize's "invade") waits for that lock for ever. Until 0.52.1 the
        // FROZEN entry did both before writing a word: the hangs the owner saw left nothing.
        struct Module
        {
            DWORD64 base = 0, size = 0;
            juce::String path, name;
        };
        juce::CriticalSection moduleLock;
        std::vector<Module> modules;   // sorted by base

        std::vector<Module> readModules()
        {
            std::vector<Module> found;
            const auto process = GetCurrentProcess();
            std::vector<HMODULE> handles (2048);
            DWORD needed = 0;
            if (! EnumProcessModules (process, handles.data(), (DWORD) (handles.size() * sizeof (HMODULE)), &needed))
                return found;
            handles.resize (juce::jmin (handles.size(), (size_t) needed / sizeof (HMODULE)));
            for (auto* handle : handles)
            {
                MODULEINFO info {};
                if (! GetModuleInformation (process, handle, &info, sizeof (info)))
                    continue;
                wchar_t path[MAX_PATH] {};
                GetModuleFileNameExW (process, handle, path, MAX_PATH);
                Module m;
                m.base = (DWORD64) info.lpBaseOfDll;
                m.size = (DWORD64) info.SizeOfImage;
                m.path = juce::String (path);
                m.name = juce::File (m.path).getFileName();
                found.push_back (m);
            }
            std::sort (found.begin(), found.end(), [] (const Module& a, const Module& b) { return a.base < b.base; });
            return found;
        }

        const Module* moduleAt (const std::vector<Module>& list, DWORD64 address)
        {
            auto it = std::upper_bound (list.begin(), list.end(), address, [] (DWORD64 a, const Module& m) { return a < m.base; });
            if (it == list.begin())
                return nullptr;
            --it;
            return address < it->base + it->size ? &*it : nullptr;
        }

        std::vector<Module> knownModules()
        {
            const juce::ScopedLock sl (moduleLock);
            return modules;
        }

        //======================================================================
        // Symbols (dbghelp: one caller at a time). Each module registered from the list above
        // (SymInitialize without "invade": no loader lock), its names read lazily from the disk.
        juce::CriticalSection dbgLock;
        bool symbolsReady = false;
        std::map<DWORD64, juce::String> symbolModules;   // base -> path, as dbghelp knows them

        void syncSymbols (const std::vector<Module>& list)   // (dbgLock held)
        {
            const auto process = GetCurrentProcess();
            if (! symbolsReady)
            {
                symbolsReady = true;
                SymSetOptions (SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
                // The app's folder first: its .pdb is beside it (the names and lines of the studio's own code).
                const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getFullPathName();
                SymInitializeW (process, folder.toWideCharPointer(), FALSE);
            }
            for (const auto& m : list)
            {
                auto known = symbolModules.find (m.base);
                if (known != symbolModules.end() && known->second == m.path)
                    continue;
                if (known != symbolModules.end())
                    SymUnloadModule64 (process, m.base);   // (another module where an unloaded one was)
                SymLoadModuleExW (process, nullptr, m.path.toWideCharPointer(), nullptr, m.base, (DWORD) m.size, nullptr, 0);
                symbolModules[m.base] = m.path;
            }
        }

        // The list read again and given to dbghelp (the watch, while the screen answers).
        void refreshModules()
        {
            auto list = readModules();
            if (list.empty())
                return;
            {
                const juce::ScopedLock sl (moduleLock);
                modules = list;
            }
            if (dbgLock.tryEnter())
            {
                syncSymbols (list);
                dbgLock.exit();
            }
        }

        // For a crash: the list as it is now (a plug-in may have loaded since the watch looked).
        void initSymbols()
        {
            refreshModules();
        }

        juce::String exceptionName (DWORD code)
        {
            switch (code)
            {
                case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS VIOLATION (memory that wasn't there or wasn't its to use)";
                case EXCEPTION_STACK_OVERFLOW:        return "STACK OVERFLOW (calls within calls without end)";
                case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL INSTRUCTION";
                case EXCEPTION_PRIV_INSTRUCTION:      return "PRIVILEGED INSTRUCTION";
                case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "DIVIDE BY ZERO";
                case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLOATING-POINT DIVIDE BY ZERO";
                case EXCEPTION_FLT_INVALID_OPERATION: return "FLOATING-POINT INVALID OPERATION";
                case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY BOUNDS EXCEEDED";
                case EXCEPTION_DATATYPE_MISALIGNMENT: return "MISALIGNED DATA";
                case EXCEPTION_IN_PAGE_ERROR:         return "IN-PAGE ERROR (a file it read from went away)";
                case EXCEPTION_BREAKPOINT:            return "BREAKPOINT";
                case 0xE06D7363:                      return "C++ EXCEPTION NOT CAUGHT";
                case 0xC0000409:                      return "STACK BUFFER OVERRUN (or a fast fail)";
                case 0xC0000374:                      return "HEAP CORRUPTION";
                default:                              return "EXCEPTION 0x" + juce::String::toHexString ((juce::int64) code).toUpperCase();
            }
        }

        // "Kontakt 8.vst3 + 0x1a2b3c", from the module list alone.
        juce::String placeOf (const std::vector<Module>& list, DWORD64 address)
        {
            if (const auto* m = moduleAt (list, address))
                return m->name + " + 0x" + juce::String::toHexString ((juce::int64) (address - m->base));
            return "0x" + juce::String::toHexString ((juce::int64) address) + " (in no file)";
        }

        // "Kontakt 8.vst3 + 0x1a2b3c: SomeFunction + 0x12 (Engine.cpp:345)". (dbgLock held)
        juce::String describeAddress (DWORD64 address)
        {
            const auto process = GetCurrentProcess();
            const auto list = knownModules();
            juce::String text = placeOf (list, address);
            alignas (SYMBOL_INFO) char buffer[sizeof (SYMBOL_INFO) + 512] {};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*> (buffer);
            symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
            symbol->MaxNameLen = 511;
            DWORD64 displacement = 0;
            if (SymFromAddr (process, address, &displacement, symbol))
                text << ": " << juce::String (symbol->Name) << " + 0x" << juce::String::toHexString ((juce::int64) displacement);
            IMAGEHLP_LINE64 line {};
            line.SizeOfStruct = sizeof (line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64 (process, address, &lineDisplacement, &line) && line.FileName != nullptr)
                text << " (" << juce::File (juce::String (line.FileName)).getFileName() << ":" << (int) line.LineNumber << ")";
            return text;
        }

        // A thread's stack, copied while it was stopped (walked after it runs again).
        struct StackCopy
        {
            DWORD64 base = 0;
            size_t size = 0;
            const unsigned char* data = nullptr;
        };
        const StackCopy* stackCopy = nullptr;

        BOOL CALLBACK readMemory (HANDLE process, DWORD64 address, PVOID buffer, DWORD size, LPDWORD read)
        {
            if (stackCopy != nullptr && address >= stackCopy->base && address + size <= stackCopy->base + stackCopy->size)
            {
                std::memcpy (buffer, stackCopy->data + (address - stackCopy->base), size);
                *read = size;
                return TRUE;
            }
            SIZE_T got = 0;
            const auto ok = ReadProcessMemory (process, reinterpret_cast<LPCVOID> (address), buffer, size, &got);
            *read = (DWORD) got;
            return ok;
        }

        // (dbgLock held)
        juce::String walkStack (HANDLE thread, const CONTEXT& start, bool fromCopy)
        {
            CONTEXT context = start;   // (StackWalk64 changes it as it goes)
            STACKFRAME64 frame {};
            DWORD machine = 0;
           #if defined (_M_X64)
            machine = IMAGE_FILE_MACHINE_AMD64;
            frame.AddrPC.Offset = context.Rip;
            frame.AddrFrame.Offset = context.Rbp;
            frame.AddrStack.Offset = context.Rsp;
           #elif defined (_M_ARM64)
            machine = IMAGE_FILE_MACHINE_ARM64;
            frame.AddrPC.Offset = context.Pc;
            frame.AddrFrame.Offset = context.Fp;
            frame.AddrStack.Offset = context.Sp;
           #else
            machine = IMAGE_FILE_MACHINE_I386;
            frame.AddrPC.Offset = context.Eip;
            frame.AddrFrame.Offset = context.Ebp;
            frame.AddrStack.Offset = context.Esp;
           #endif
            frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
            juce::String text;
            for (int i = 0; i < 64; ++i)
            {
                if (! StackWalk64 (machine, GetCurrentProcess(), thread, &frame, &context,
                                   fromCopy ? readMemory : nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
                    || frame.AddrPC.Offset == 0)
                    break;
                text << "  " << juce::String (i).paddedLeft (' ', 2) << "  " << describeAddress (frame.AddrPC.Offset) << "\r\n";
            }
            return text.isEmpty() ? juce::String ("  (the calls couldn't be read)\r\n") : text;
        }

        //======================================================================
        // Every thread of the studio (its own, Windows', drivers', plug-ins'), each stopped for a
        // moment while its registers and stack are copied (nothing allocated meanwhile: it may hold
        // the memory lock), then let go at once.
        DWORD screenThreadId = 0;
        using GetDescription = HRESULT (WINAPI*) (HANDLE, PWSTR*);
        GetDescription getThreadDescription = nullptr;   // (Windows 10 1607 on; found at start)

        struct Captured
        {
            DWORD id = 0;
            HANDLE handle = nullptr;
            CONTEXT context {};
            std::vector<unsigned char> data;
            StackCopy copy;
            bool ok = false;
            juce::String label;
        };

        DWORD64 stackPointer (const CONTEXT& c)
        {
           #if defined (_M_X64)
            return c.Rsp;
           #elif defined (_M_ARM64)
            return c.Sp;
           #else
            return c.Esp;
           #endif
        }

        DWORD64 programCounter (const CONTEXT& c)
        {
           #if defined (_M_X64)
            return c.Rip;
           #elif defined (_M_ARM64)
            return c.Pc;
           #else
            return c.Eip;
           #endif
        }

        bool isAudioId (DWORD id)
        {
            for (auto& a : audioIds)
                if (a.load() == (juce::uint64) id)
                    return true;
            return false;
        }

        std::vector<std::unique_ptr<Captured>> captureThreads()
        {
            std::vector<DWORD> ids;
            const auto self = GetCurrentThreadId(), process = GetCurrentProcessId();
            const auto snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);
            if (snapshot != INVALID_HANDLE_VALUE)
            {
                THREADENTRY32 entry {};
                entry.dwSize = sizeof (entry);
                for (auto more = Thread32First (snapshot, &entry); more; more = Thread32Next (snapshot, &entry))
                    if (entry.th32OwnerProcessID == process && entry.th32ThreadID != self)
                        ids.push_back (entry.th32ThreadID);
                CloseHandle (snapshot);
            }
            // The screen's first, then the sound's, then the rest; at most 160.
            std::stable_sort (ids.begin(), ids.end(), [] (DWORD a, DWORD b)
            {
                const auto rank = [] (DWORD id) { return id == screenThreadId ? 0 : isAudioId (id) ? 1 : 2; };
                return rank (a) < rank (b);
            });
            if (ids.size() > 160)
                ids.resize (160);
            std::vector<std::unique_ptr<Captured>> threads;
            for (const auto id : ids)
            {
                auto c = std::make_unique<Captured>();
                c->id = id;
                c->data.resize (id == screenThreadId ? 512 * 1024 : 96 * 1024);   // (made before anything is stopped)
                threads.push_back (std::move (c));
            }
            for (auto& c : threads)
            {
                c->handle = OpenThread (THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION,
                                        FALSE, c->id);
                if (c->handle == nullptr)
                    continue;
                if (SuspendThread (c->handle) == (DWORD) -1)
                    continue;
                c->context.ContextFlags = CONTEXT_FULL;
                if (GetThreadContext (c->handle, &c->context) != 0)
                {
                    const auto sp = stackPointer (c->context);
                    MEMORY_BASIC_INFORMATION info {};
                    if (VirtualQuery (reinterpret_cast<LPCVOID> (sp), &info, sizeof (info)) != 0)
                    {
                        const auto end = (DWORD64) info.BaseAddress + (DWORD64) info.RegionSize;
                        c->copy.base = sp;
                        c->copy.size = (size_t) juce::jmin<DWORD64> (end - sp, (DWORD64) c->data.size());
                        std::memcpy (c->data.data(), reinterpret_cast<const void*> (sp), c->copy.size);
                        c->copy.data = c->data.data();
                        c->ok = true;
                    }
                }
                ResumeThread (c->handle);
            }
            // Names (after they all run again: these allocate).
            for (auto& c : threads)
            {
                juce::String described;
                if (getThreadDescription != nullptr && c->handle != nullptr)
                {
                    PWSTR name = nullptr;
                    if (SUCCEEDED (getThreadDescription (c->handle, &name)) && name != nullptr)
                    {
                        described = juce::String (name);
                        LocalFree (name);
                    }
                }
                c->label = c->id == screenThreadId ? "THE SCREEN'S THREAD (the window, clicks and keys)"
                         : isAudioId (c->id) ? "an audio thread (instruments and effects play here)"
                         : described.isNotEmpty() ? "thread \"" + described + "\""
                         : juce::String ("a thread");
                c->label << " [" << (int) c->id << "]";
            }
            return threads;
        }

        // Each thread: where it is (module + offset) and the modules found on its stack, from the
        // module list alone (nothing that could wait).
        juce::String rawThreadReport (const std::vector<std::unique_ptr<Captured>>& threads)
        {
            const auto list = knownModules();
            juce::String text;
            text << "  Every thread (" << (int) threads.size() << "): where it is, and the modules found on its stack (innermost first):\r\n";
            for (const auto& c : threads)
            {
                text << "  - " << c->label << ": ";
                if (! c->ok)
                {
                    text << "(couldn't look at it)\r\n";
                    continue;
                }
                text << "at " << placeOf (list, programCounter (c->context)) << "\r\n";
                juce::StringArray seen, places;
                const auto words = c->copy.size / sizeof (DWORD64);
                for (size_t i = 0; i < words && places.size() < 12; ++i)
                {
                    DWORD64 value = 0;
                    std::memcpy (&value, c->copy.data + i * sizeof (DWORD64), sizeof (value));
                    if (const auto* m = moduleAt (list, value))
                    {
                        seen.addIfNotAlreadyThere (m->name);
                        places.add (placeOf (list, value));
                    }
                }
                for (size_t i = 0; i < words; ++i)   // (every module, beyond the first dozen places)
                {
                    DWORD64 value = 0;
                    std::memcpy (&value, c->copy.data + i * sizeof (DWORD64), sizeof (value));
                    if (const auto* m = moduleAt (list, value))
                        seen.addIfNotAlreadyThere (m->name);
                }
                if (! seen.isEmpty())
                    text << "      modules: " << seen.joinIntoString (", ") << "\r\n"
                         << "      places:  " << places.joinIntoString (", ") << "\r\n";
            }
            return text;
        }
       #else
        juce::String hereStackPosix()
        {
            void* frames[64];
            const auto count = backtrace (frames, 64);
            juce::String text;
            if (auto** names = backtrace_symbols (frames, count))
            {
                for (int i = 0; i < count; ++i)
                    text << "  " << juce::String (i).paddedLeft (' ', 2) << "  " << names[i] << "\r\n";
                ::free (names);
            }
            return text;
        }
       #endif

        void crashed (const juce::String& what, void* info)
        {
            if (crashing.exchange (true))
                return;   // (a crash while writing the last one)
            writeRaw (describeCrash (what, info), true);
        }

        //======================================================================
        // A FROZEN or STUCK report: the headline and the steps under way written at once (whatever
        // follows may not finish), then every thread, then (last, slowest) their calls by name.
        void report (const juce::String& headline)
        {
            juce::String text;
            text << "\r\n" << stamp() << "  " << headline << "\r\n"
                 << "  Under way:\r\n" << stepsUnderWay();
            writeRaw (text, false);
           #if defined (_WIN32)
            const auto threads = captureThreads();
            writeRaw (rawThreadReport (threads), false);
            writeRaw ("  Their calls by name (if this stops part way, looking them up got stuck too: what's above is what counts):", false);
            if (dbgLock.tryEnter())
            {
                for (const auto& c : threads)
                {
                    if (! c->ok || c->handle == nullptr)
                        continue;
                    stackCopy = &c->copy;
                    const auto calls = walkStack (c->handle, c->context, true);
                    stackCopy = nullptr;
                    writeRaw ("  " + c->label + ":\r\n" + calls, false);
                }
                dbgLock.exit();
            }
            else
            {
                writeRaw ("  (the names were busy: a crash being written)", false);
            }
            for (const auto& c : threads)
                if (c->handle != nullptr)
                    CloseHandle (c->handle);
           #else
            writeRaw ("  (each thread's calls are only shown on Windows)\r\n", false);
           #endif
        }

        //======================================================================
        struct FreezeWatch final : juce::Thread
        {
            FreezeWatch (double s, double job) : juce::Thread ("PR7-S freeze watch"), limit (s), jobLimit (job) {}
            void run() override
            {
                auto frozen = false, stuck = false;
                auto frozenSince = 0.0, stuckSince = 0.0, lastRefresh = -1.0e9;
                while (! threadShouldExit())
                {
                   #if defined (_WIN32)
                    const auto now = nowSeconds();
                    if (! frozen && ! stuck && now - lastRefresh > 5.0)
                    {
                        refreshModules();   // (while all answers: ready for a report)
                        lastRefresh = now;
                    }
                   #else
                    juce::ignoreUnused (lastRefresh);
                   #endif
                    wait (250);
                    const auto at = nowSeconds();
                    const auto last = lastBeat.load();
                    if (last > 0.0)
                    {
                        if (! frozen && at - last > limit.load())
                        {
                            frozen = true;
                            frozenSince = last;
                            report ("FROZEN: the screen hasn't answered for " + juce::String (at - last, 0) + " seconds"
                                    + (closingNow.load() ? juce::String (" (while closing)") : juce::String()) + ".");
                        }
                        else if (frozen && last > frozenSince)
                        {
                            writeRaw (stamp() + "  The screen answered again after " + juce::String (last - frozenSince, 1) + " seconds.", false);
                            frozen = false;
                        }
                    }
                    if (jobDepth.load() > 0)
                    {
                        const auto moved = lastJobBeat.load();
                        if (! stuck && at - moved > jobLimit)
                        {
                            stuck = true;
                            stuckSince = moved;
                            report ("STUCK: " + juce::String (juce::CharPointer_UTF8 (jobName)) + " hasn't moved for "
                                    + juce::String (at - moved, 0) + " seconds (the screen still answers).");
                        }
                        else if (stuck && moved > stuckSince)
                        {
                            writeRaw (stamp() + "  It moved again after " + juce::String (moved - stuckSince, 1) + " seconds.", false);
                            stuck = false;
                        }
                    }
                    else
                        stuck = false;
                }
            }
            std::atomic<double> limit;
            const double jobLimit;
            std::atomic<double> lastBeat { 0.0 };
        };
        std::unique_ptr<FreezeWatch> watch;
    }

    //==========================================================================
    juce::File getFolder()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Paragon PR7-S";
        options.filenameSuffix = ".settings";
        options.folderName = "Paragon PR7-S";
        options.osxLibrarySubFolder = "Application Support";
        return options.getDefaultFile().getParentDirectory().getChildFile ("Logs");
    }

    juce::File getFile()
    {
        const juce::ScopedLock sl (state().lock);
        return state().file;
    }

    bool closedNormally (const juce::File& log)
    {
        return log.loadFileAsString().trimEnd().endsWith (closedLine);
    }

    bool recordsCrash (const juce::File& log)
    {
        return log.loadFileAsString().contains ("CRASH:");
    }

    juce::File start (const juce::String& header, juce::File folder, int keep)
    {
        if (folder == juce::File())
            folder = getFolder();
        folder.createDirectory();
        auto logs = folder.findChildFiles (juce::File::findFiles, false, "PR7-S *.log");
        std::sort (logs.begin(), logs.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
        juce::File unfinished;
        if (! logs.isEmpty() && ! closedNormally (logs.getLast()))
            unfinished = logs.getLast();
        for (int i = 0; i < logs.size() - (keep - 1); ++i)
            logs.getReference (i).deleteFile();

        closingNow = false;
        auto& s = state();
        const juce::ScopedLock sl (s.lock);
        s.started = juce::Time::getCurrentTime();
        s.file = folder.getNonexistentChildFile ("PR7-S " + s.started.formatted ("%Y-%m-%d %H-%M-%S"), ".log", false);
        s.out = std::make_unique<juce::FileOutputStream> (s.file);
        if (! s.out->openedOk())
        {
            s.out = nullptr;
            return unfinished;
        }
        juce::String text;
        text << header << "\r\n"
             << "Started " << s.started.formatted ("%Y-%m-%d %H:%M:%S") << "\r\n"
             << "System: " << juce::SystemStats::getOperatingSystemName() << (juce::SystemStats::isOperatingSystem64Bit() ? " (64-bit)" : "") << "\r\n"
             << "Processor: " << juce::SystemStats::getCpuModel().trim() << ", " << juce::SystemStats::getNumPhysicalCpus() << " cores ("
             << juce::SystemStats::getNumCpus() << " threads)\r\n"
             << "Memory: " << juce::SystemStats::getMemorySizeInMegabytes() << " MB\r\n"
             << "The app: " << juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName() << "\r\n"
             << "\r\n";
        s.out->writeText (text, false, false, nullptr);
        s.out->flush();
        return unfinished;
    }

    void note (const juce::String& line)
    {
        if (crashing.load())
            return;
        writeRaw (stamp() + "  " + line + "\n", false);   // (one line each: 0.16.0 ran them together)
    }

    void stop()
    {
        stopFreezeWatch();
        auto& s = state();
        const juce::ScopedLock sl (s.lock);
        if (s.out == nullptr)
            return;
        const auto minutes = (juce::Time::getCurrentTime() - s.started).inMinutes();
        s.out->writeText (stamp() + "  After " + juce::String (minutes, 0) + " minutes. " + closedLine + "\r\n", false, false, nullptr);
        s.out->flush();
        s.out = nullptr;
    }

    //==========================================================================
    Step::Step (const juce::String& what) : started (nowSeconds()), text (what)
    {
        note ("Starting: " + what);
        juce::String where = isScreenThread() ? juce::String ("the screen's thread")
                           : onAudioThread ? juce::String ("an audio thread")
                           : juce::Thread::getCurrentThread() != nullptr ? "thread \"" + juce::Thread::getCurrentThread()->getThreadName() + "\""
                           : juce::String ("a thread");
        for (int i = 0; i < stepSlotCount; ++i)
        {
            auto& s = stepSlots[i];
            juce::uint64 free = 0;
            // Claimed with a marker first, filled, then marked as this thread's (a reader sees it whole).
            if (s.thread.compare_exchange_strong (free, ~(juce::uint64) 0))
            {
                copyText (s.text, sizeof (s.text), what);
                copyText (s.where, sizeof (s.where), where);
                s.started = started;
                s.thread.store (thisThreadKey(), std::memory_order_release);
                slot = i;
                break;
            }
        }
    }

    Step::~Step()
    {
        if (slot >= 0)
            stepSlots[slot].thread.store (0, std::memory_order_release);
        note ("  done: " + text + " (" + juce::String (juce::roundToInt ((nowSeconds() - started) * 1000.0)) + " ms)");
    }

    juce::String stepsUnderWay()
    {
        struct Line { double started; juce::String text; };
        std::vector<Line> lines;
        const auto now = nowSeconds();
        for (auto& s : stepSlots)
        {
            const auto owner = s.thread.load (std::memory_order_acquire);
            if (owner == 0 || owner == ~(juce::uint64) 0)
                continue;
            char text[sizeof (s.text)], where[sizeof (s.where)];
            std::memcpy (text, s.text, sizeof (text));
            std::memcpy (where, s.where, sizeof (where));
            text[sizeof (text) - 1] = where[sizeof (where) - 1] = 0;
            const auto started = s.started.load();
            lines.push_back ({ started, "  - " + juce::String (juce::CharPointer_UTF8 (where)) + ", for " + juce::String (now - started, 1) + " s: "
                                        + juce::String (juce::CharPointer_UTF8 (text)) });
        }
        if (jobDepth.load() > 0)
            lines.push_back ({ now, "  - the job " + juce::String (juce::CharPointer_UTF8 (jobName)) + ", last moved "
                                    + juce::String (now - lastJobBeat.load(), 1) + " s ago" });
        if (lines.empty())
            return "  (nothing marked: it stopped between steps)\r\n";
        std::sort (lines.begin(), lines.end(), [] (const Line& a, const Line& b) { return a.started < b.started; });
        juce::String text;
        for (const auto& l : lines)
            text << l.text << "\r\n";
        return text;
    }

    Job::Job (const juce::String& what)
    {
        copyText (jobName, sizeof (jobName), what);
        jobBeat();
        ++jobDepth;
    }

    Job::~Job() { --jobDepth; }

    void jobBeat() noexcept { lastJobBeat.store (nowSeconds()); }

    void closing (double seconds)
    {
        note ("Closing.");
        closingNow = true;
        beat();
        if (watch != nullptr)
            watch->limit = seconds;
    }

    //==========================================================================
    void markAudioThread (bool on) noexcept
    {
        if (on == onAudioThread)
            return;
        onAudioThread = on;
        const auto key = thisThreadKey();
        for (auto& a : audioIds)
        {
            juce::uint64 expected = on ? 0 : key;
            if (a.compare_exchange_strong (expected, on ? key : 0))
                break;
        }
    }

    bool isAudioThread() noexcept { return onAudioThread; }

    juce::String describeCrash (const juce::String& what, void* info)
    {
        juce::String text;
        text << "\r\n" << stamp() << "  CRASH: " << what << "\r\n"
             << "  On " << threadName() << ".\r\n";
       #if defined (_WIN32)
        if (auto* pointers = static_cast<EXCEPTION_POINTERS*> (info); pointers != nullptr && pointers->ExceptionRecord != nullptr)
        {
            const auto* record = pointers->ExceptionRecord;
            text << "  " << exceptionName (record->ExceptionCode) << "\r\n";
            if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
            {
                const auto kind = record->ExceptionInformation[0];
                text << "  It tried to " << (kind == 0 ? "read" : kind == 1 ? "write" : "run code at") << " address 0x"
                     << juce::String::toHexString ((juce::int64) record->ExceptionInformation[1])
                     << (record->ExceptionInformation[1] < 0x10000 ? " (nothing there: a pointer that was empty)" : "") << "\r\n";
            }
            // dbghelp one caller at a time (the freeze watch may be using it: wait a little, not for ever).
            auto locked = dbgLock.tryEnter();
            for (int i = 0; ! locked && i < 200; ++i)
            {
                juce::Thread::sleep (10);
                locked = dbgLock.tryEnter();
            }
            initSymbols();
            if (locked && symbolsReady)
                syncSymbols (knownModules());
            text << "  Where: " << (locked ? describeAddress ((DWORD64) record->ExceptionAddress)
                                           : placeOf (knownModules(), (DWORD64) record->ExceptionAddress)) << "\r\n"
                 << "  The calls that led there (the innermost first):\r\n"
                 << (locked ? walkStack (GetCurrentThread(), *pointers->ContextRecord, false) : juce::String ("  (busy: not read)\r\n"));
            if (locked)
                dbgLock.exit();
            return text;
        }
        text << "  The calls that led there (the innermost first):\r\n";
        if (dbgLock.tryEnter())
        {
            initSymbols();
            CONTEXT context {};
            RtlCaptureContext (&context);
            text << walkStack (GetCurrentThread(), context, false);
            dbgLock.exit();
        }
        return text;
       #else
        juce::ignoreUnused (info);
        text << "  The calls that led there (the innermost first):\r\n" << hereStackPosix();
        return text;
       #endif
    }

    void installCrashHandler()
    {
        // Crashes (JUCE: Windows' unhandled-exception filter; signals elsewhere).
        juce::SystemStats::setApplicationCrashHandler ([] (void* info)
        {
           #if defined (_WIN32)
            crashed ("the studio stopped on an error it couldn't recover from", info);
           #else
            crashed ("the studio stopped on a signal (" + juce::String (info != nullptr ? (int) (juce::pointer_sized_int) info : 0) + ")", nullptr);
           #endif
        });
        // A C++ error nothing caught, or abort() (a failed check, a pure virtual call).
        std::set_terminate ([]
        {
            juce::String what = "std::terminate (an error nothing caught)";
            if (auto current = std::current_exception())
            {
                try { std::rethrow_exception (current); }
                catch (const std::exception& e) { what << ": " << e.what(); }
                catch (...) {}
            }
            crashed (what, nullptr);
            std::_Exit (3);
        });
       #if defined (_WIN32)
        std::signal (SIGABRT, [] (int) { crashed ("abort() (the program ended itself: a failed check)", nullptr); std::_Exit (3); });
        _set_purecall_handler ([] { crashed ("a pure virtual function was called (an object used after it was deleted)", nullptr); std::_Exit (3); });
       #endif
    }

    void startFreezeWatch (double seconds, double jobSeconds)
    {
       #if defined (_WIN32)
        // The screen's thread is the one starting the watch. GetThreadDescription found now (not
        // while something may be stuck holding the loader lock).
        screenThreadId = GetCurrentThreadId();
        if (getThreadDescription == nullptr)
            if (auto* kernel = GetModuleHandleW (L"kernel32.dll"))
                getThreadDescription = reinterpret_cast<GetDescription> (reinterpret_cast<void*> (GetProcAddress (kernel, "GetThreadDescription")));
       #endif
        stopFreezeWatch();
        closingNow = false;
        watch = std::make_unique<FreezeWatch> (seconds, jobSeconds);
        beat();
        watch->startThread (juce::Thread::Priority::low);
    }

    void beat()
    {
        if (watch != nullptr)
            watch->lastBeat = nowSeconds();
    }

    void stopFreezeWatch()
    {
        if (watch != nullptr)
        {
            watch->stopThread (5000);
            watch = nullptr;
        }
    }
}
