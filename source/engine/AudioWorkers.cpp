#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "AudioWorkers.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include "SessionLog.h"

#include <thread>

#if JUCE_INTEL
 #include <immintrin.h>
#endif

namespace pr7s
{
    namespace
    {
        thread_local bool onHelper = false;

        // A pause inside a spin: lets the core's other hyper-thread run, and costs less power.
        inline void relax() noexcept
        {
           #if JUCE_INTEL
            _mm_pause();
           #elif JUCE_ARM && (JUCE_GCC || JUCE_CLANG)
            __asm__ __volatile__ ("yield");
           #else
            std::this_thread::yield();
           #endif
        }

       #if defined (_WIN32)
        // Windows: the system's Pro Audio scheduling (MMCSS), as audio threads take it: the helper
        // runs as the audio thread does, and only it (JUCE's realtime thread would raise the whole
        // program, the screen too, to the realtime priority class). Loaded by hand: avrt.dll.
        struct ProAudioScheduling
        {
            ProAudioScheduling()
            {
                if (auto* avrt = LoadLibraryW (L"avrt.dll"))
                {
                    using Set = HANDLE (WINAPI*) (LPCWSTR, LPDWORD);
                    revert = reinterpret_cast<Revert> (reinterpret_cast<void*> (GetProcAddress (avrt, "AvRevertMmThreadCharacteristics")));
                    if (auto set = reinterpret_cast<Set> (reinterpret_cast<void*> (GetProcAddress (avrt, "AvSetMmThreadCharacteristicsW"))))
                    {
                        DWORD index = 0;
                        handle = set (L"Pro Audio", &index);
                    }
                }
                if (handle == nullptr)
                    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);   // (no MMCSS: this thread alone, as high as it goes)
            }
            ~ProAudioScheduling()
            {
                if (handle != nullptr && revert != nullptr)
                    revert (handle);
            }
            using Revert = BOOL (WINAPI*) (HANDLE);
            HANDLE handle = nullptr;
            Revert revert = nullptr;
        };
       #endif

        // How long a helper keeps looking for the next wave of work before it sleeps: a block's
        // waves come microseconds apart; the next block, milliseconds later.
        constexpr double spinSeconds = 0.0002;
    }

    class AudioWorkers::Helper final : public juce::Thread
    {
    public:
        Helper (AudioWorkers& o, int number) : juce::Thread ("PR7-S audio " + juce::String (number)), owner (o) {}
        void run() override { owner.helperLoop (*this); }

    private:
        AudioWorkers& owner;
    };

    AudioWorkers::AudioWorkers() = default;

    AudioWorkers::~AudioWorkers()
    {
        setHelpers (0);
    }

    int AudioWorkers::helpersForThisComputer()
    {
        return juce::jlimit (0, 15, juce::SystemStats::getNumPhysicalCpus() - 1);
    }

    bool AudioWorkers::isHelperThread() noexcept
    {
        return onHelper;
    }

    void AudioWorkers::setHelpers (int count)
    {
        count = juce::jlimit (0, 15, count);
        if (count == (int) helpers.size())
            return;
        // All of them stopped, then the number wanted started (realtime priority, as the audio thread's).
        quitting = true;
        wake.fetch_add (1, std::memory_order_release);
        wake.notify_all();
        for (auto& helper : helpers)
            helper->stopThread (2000);
        helpers.clear();
        quitting = false;
        for (int i = 0; i < count; ++i)
        {
            auto helper = std::make_unique<Helper> (*this, i + 1);
           #if defined (_WIN32)
            helper->startThread (juce::Thread::Priority::highest);   // (then Pro Audio scheduling, in the thread itself)
           #else
            if (! helper->startRealtimeThread (juce::Thread::RealtimeOptions{}.withPriority (10)))
                helper->startThread (juce::Thread::Priority::highest);
           #endif
            helpers.push_back (std::move (helper));
        }
    }

    void AudioWorkers::start (int count, void* task, Invoke invoke)
    {
        const auto generation = (state.load (std::memory_order_relaxed) >> 32) + 1;
        auto& job = jobs[generation & 1];
        job.task.store (task, std::memory_order_relaxed);
        job.invoke.store (invoke, std::memory_order_relaxed);
        job.count.store (count, std::memory_order_relaxed);
        done.store (0, std::memory_order_relaxed);
        state.store (generation << 32, std::memory_order_release);   // (publishes the job)
        wake.fetch_add (1, std::memory_order_release);
        wake.notify_all();
    }

    void AudioWorkers::work()
    {
        for (;;)
        {
            auto now = state.load (std::memory_order_acquire);
            const auto& job = jobs[(now >> 32) & 1];
            const auto index = (int) (now & 0xffffffffu);
            if (index >= job.count.load (std::memory_order_relaxed))
                return;
            auto* task = job.task.load (std::memory_order_relaxed);
            auto* invoke = job.invoke.load (std::memory_order_relaxed);
            // Taken only if the job is still the one read (its generation) and the task still the next.
            if (! state.compare_exchange_weak (now, now + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                continue;
            invoke (task, index);
            done.fetch_add (1, std::memory_order_release);
        }
    }

    void AudioWorkers::finish (int count)
    {
        for (int spins = 0; done.load (std::memory_order_acquire) < count; ++spins)
        {
            if (spins < 4096)
                relax();
            else
                std::this_thread::yield();
        }
    }

    void AudioWorkers::helperLoop (Helper& self)
    {
        onHelper = true;
        SessionLog::markAudioThread();
       #if defined (_WIN32)
        const ProAudioScheduling scheduling;
       #endif
        const juce::ScopedNoDenormals noDenormals;   // (as the audio thread: tiny values are zero)
        const auto spinTicks = juce::Time::secondsToHighResolutionTicks (spinSeconds);
        auto seen = wake.load (std::memory_order_acquire);
        while (! quitting.load() && ! self.threadShouldExit())
        {
            work();
            auto woken = false;
            for (const auto until = juce::Time::getHighResolutionTicks() + spinTicks; juce::Time::getHighResolutionTicks() < until;)
            {
                if (wake.load (std::memory_order_acquire) != seen)
                {
                    woken = true;
                    break;
                }
                for (int i = 0; i < 32; ++i)
                    relax();
            }
            if (! woken)
                wake.wait (seen, std::memory_order_acquire);
            seen = wake.load (std::memory_order_acquire);
        }
    }
}
