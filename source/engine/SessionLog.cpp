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
 #pragma comment (lib, "dbghelp.lib")
#endif

#include "SessionLog.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <csignal>
#include <exception>
#include <thread>

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

        juce::String stamp()
        {
            return juce::Time::getCurrentTime().formatted ("%H:%M:%S.") + juce::String (juce::Time::getCurrentTime().getMilliseconds()).paddedLeft ('0', 3);
        }

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

        juce::String threadName()
        {
            if (onAudioThread)
                return "the audio thread (the sound: instruments and effects play here)";
            if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && mm->isThisTheMessageThread())
                return "the screen's thread (the window, clicks and keys)";
            if (auto* t = juce::Thread::getCurrentThread())
                return "the studio's thread \"" + t->getThreadName() + "\"";
            return "another thread (a driver's or a plug-in's own)";
        }

       #if defined (_WIN32)
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

        bool symbolsReady = false;
        void initSymbols()
        {
            if (symbolsReady)
                return;
            symbolsReady = true;
            SymSetOptions (SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
            // The app's folder first: its .pdb is beside it (the names and lines of the studio's own code).
            const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getFullPathName();
            SymInitialize (GetCurrentProcess(), folder.toRawUTF8(), TRUE);
        }

        // "Kontakt 8.vst3 + 0x1a2b3c: SomeFunction + 0x12 (Engine.cpp:345)"
        juce::String describeAddress (DWORD64 address)
        {
            const auto process = GetCurrentProcess();
            juce::String text;
            HMODULE module = nullptr;
            if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    reinterpret_cast<LPCWSTR> (address), &module) && module != nullptr)
            {
                wchar_t path[MAX_PATH] {};
                GetModuleFileNameW (module, path, MAX_PATH);
                text << juce::File (juce::String (path)).getFileName()
                     << " + 0x" << juce::String::toHexString ((juce::int64) (address - (DWORD64) module));
            }
            else
            {
                text << "0x" << juce::String::toHexString ((juce::int64) address) << " (in no file)";
            }
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

        // The freeze watch's copy of the screen thread's stack (walked after the thread runs again).
        struct StackCopy
        {
            DWORD64 base = 0;
            size_t size = 0;
            std::vector<unsigned char> data;
        };
        StackCopy* stackCopy = nullptr;

        BOOL CALLBACK readMemory (HANDLE process, DWORD64 address, PVOID buffer, DWORD size, LPDWORD read)
        {
            if (stackCopy != nullptr && address >= stackCopy->base && address + size <= stackCopy->base + stackCopy->size)
            {
                std::memcpy (buffer, stackCopy->data.data() + (address - stackCopy->base), size);
                *read = size;
                return TRUE;
            }
            SIZE_T got = 0;
            const auto ok = ReadProcessMemory (process, reinterpret_cast<LPCVOID> (address), buffer, size, &got);
            *read = (DWORD) got;
            return ok;
        }

        juce::String walkStack (HANDLE thread, const CONTEXT& start, bool fromCopy)
        {
            initSymbols();
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

        HANDLE screenThread = nullptr;

        juce::String screenThreadStack()
        {
            // Stopped for a moment while its registers and stack are copied (nothing
            // allocated meanwhile: it may hold the memory lock), then walked.
            static StackCopy copy;
            if (copy.data.empty())
                copy.data.resize (512 * 1024);
            if (screenThread == nullptr || SuspendThread (screenThread) == (DWORD) -1)
                return "  (couldn't look at it)\r\n";
            CONTEXT context {};
            context.ContextFlags = CONTEXT_FULL;
            auto ok = GetThreadContext (screenThread, &context) != 0;
            if (ok)
            {
               #if defined (_M_X64)
                const DWORD64 sp = context.Rsp;
               #elif defined (_M_ARM64)
                const DWORD64 sp = context.Sp;
               #else
                const DWORD64 sp = context.Esp;
               #endif
                MEMORY_BASIC_INFORMATION info {};
                ok = VirtualQuery (reinterpret_cast<LPCVOID> (sp), &info, sizeof (info)) != 0;
                if (ok)
                {
                    const auto end = (DWORD64) info.BaseAddress + (DWORD64) info.RegionSize;
                    copy.base = sp;
                    copy.size = (size_t) juce::jmin<DWORD64> (end - sp, (DWORD64) copy.data.size());
                    std::memcpy (copy.data.data(), reinterpret_cast<const void*> (sp), copy.size);
                }
            }
            ResumeThread (screenThread);
            if (! ok)
                return "  (couldn't look at it)\r\n";
            stackCopy = &copy;
            auto text = walkStack (screenThread, context, true);
            stackCopy = nullptr;
            return text;
        }

        juce::String hereStack()
        {
            CONTEXT context {};
            RtlCaptureContext (&context);
            return walkStack (GetCurrentThread(), context, false);
        }
       #else
        juce::String hereStack()
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
        struct FreezeWatch final : juce::Thread
        {
            explicit FreezeWatch (double s) : juce::Thread ("PR7-S freeze watch"), limit (s) {}
            void run() override
            {
                auto frozen = false;
                auto frozenSince = 0.0;
                while (! threadShouldExit())
                {
                    wait (500);
                    const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                    const auto last = lastBeat.load();
                    if (last <= 0.0)
                        continue;
                    if (! frozen && now - last > limit)
                    {
                        frozen = true;
                        frozenSince = last;
                        juce::String text;
                        text << "\r\n" << stamp() << "  FROZEN: the screen hasn't answered for " << juce::String (now - last, 0) << " seconds.\r\n"
                             << "  What its thread is doing:\r\n";
                       #if defined (_WIN32)
                        text << screenThreadStack();
                       #else
                        text << "  (only shown on Windows)\r\n";
                       #endif
                        writeRaw (text, false);
                    }
                    else if (frozen && last > frozenSince)
                    {
                        writeRaw (stamp() + "  The screen answered again after " + juce::String (last - frozenSince, 1) + " seconds.", false);
                        frozen = false;
                    }
                }
            }
            const double limit;
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
        writeRaw (stamp() + "  " + line, false);
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

    void markAudioThread (bool on) noexcept { onAudioThread = on; }
    bool isAudioThread() noexcept { return onAudioThread; }

    juce::String describeCrash (const juce::String& what, void* info)
    {
        juce::String text;
        text << "\r\n" << stamp() << "  CRASH: " << what << "\r\n"
             << "  On " << threadName() << ".\r\n";
       #if defined (_WIN32)
        if (auto* pointers = static_cast<EXCEPTION_POINTERS*> (info); pointers != nullptr && pointers->ExceptionRecord != nullptr)
        {
            initSymbols();
            const auto* record = pointers->ExceptionRecord;
            text << "  " << exceptionName (record->ExceptionCode) << "\r\n";
            if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
            {
                const auto kind = record->ExceptionInformation[0];
                text << "  It tried to " << (kind == 0 ? "read" : kind == 1 ? "write" : "run code at") << " address 0x"
                     << juce::String::toHexString ((juce::int64) record->ExceptionInformation[1])
                     << (record->ExceptionInformation[1] < 0x10000 ? " (nothing there: a pointer that was empty)" : "") << "\r\n";
            }
            text << "  Where: " << describeAddress ((DWORD64) record->ExceptionAddress) << "\r\n"
                 << "  The calls that led there (the innermost first):\r\n"
                 << walkStack (GetCurrentThread(), *pointers->ContextRecord, false);
            return text;
        }
       #else
        juce::ignoreUnused (info);
       #endif
        text << "  The calls that led there (the innermost first):\r\n" << hereStack();
        return text;
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

    void startFreezeWatch (double seconds)
    {
       #if defined (_WIN32)
        if (screenThread == nullptr)
            screenThread = OpenThread (THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
       #endif
        stopFreezeWatch();
        watch = std::make_unique<FreezeWatch> (seconds);
        beat();
        watch->startThread (juce::Thread::Priority::low);
    }

    void beat()
    {
        if (watch != nullptr)
            watch->lastBeat = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    }

    void stopFreezeWatch()
    {
        if (watch != nullptr)
        {
            watch->stopThread (2000);
            watch = nullptr;
        }
    }
}
