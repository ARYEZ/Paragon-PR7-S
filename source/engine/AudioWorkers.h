#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <vector>

// Multi-core audio (0.34.5): helper threads that take a share of a block's
// work, so tracks are processed side by side on the computer's cores instead
// of one after another on one.
//
// `run (count, task)` calls task (0) ... task (count - 1), each exactly once,
// spread over the helpers and the calling thread (the audio thread), and
// returns when all are done. The audio thread never waits for a helper to
// start: it takes tasks itself until none are left, then waits only for the
// ones helpers have already begun. So a helper the system is slow to wake
// costs nothing but the help; with no helpers at all, everything runs on the
// calling thread, in order.
//
// Nothing here allocates or locks while running: tasks are claimed with one
// atomic number (the job's generation and the next task's index together, so
// a helper that wakes late for a job long finished can never claim a task of
// the next), helpers are woken through std::atomic's wait/notify (a futex on
// Linux, WaitOnAddress on Windows), and spin briefly first (a block's waves of
// work come microseconds apart).
namespace pr7s
{
    class AudioWorkers
    {
    public:
        AudioWorkers();
        ~AudioWorkers();

        // How many helper threads (0: none, everything on the calling thread). Never while a
        // `run` is going on (the engine changes it under its lock, which the audio thread holds
        // through a block).
        void setHelpers (int count);
        int getHelpers() const noexcept { return (int) helpers.size(); }

        // How many helpers this computer should have: a core each but the audio thread's
        // (physical cores: a hyper-thread shares its core's work), at most 15.
        static int helpersForThisComputer();

        template <typename Task>
        void run (int count, Task& task)
        {
            if (count <= 0)
                return;
            if (helpers.empty() || count == 1)
            {
                for (int i = 0; i < count; ++i)
                    task (i);
                return;
            }
            start (count, &task, [] (void* t, int i) { (*static_cast<Task*> (t)) (i); });
            work();
            finish (count);
        }

        // Each helper thread marks itself as the audio thread's kind (for the session log and the
        // plug-in knob watchers, which leave the audio threads' changes alone).
        static bool isHelperThread() noexcept;

    private:
        using Invoke = void (*) (void*, int);
        struct Job
        {
            std::atomic<void*> task { nullptr };
            std::atomic<Invoke> invoke { nullptr };
            std::atomic<int> count { 0 };
        };
        class Helper;

        void start (int count, void* task, Invoke invoke);
        void work();                       // claims and runs tasks of the job now, until none are left
        void finish (int count);           // waits for the tasks helpers have taken
        void helperLoop (Helper&);

        // generation << 32 | the next task's index
        std::atomic<juce::uint64> state { 0 };
        Job jobs[2];                       // by generation & 1: a job's description never changes while it's current
        std::atomic<int> done { 0 };
        std::atomic<juce::uint32> wake { 0 };
        std::atomic<bool> quitting { false };
        std::vector<std::unique_ptr<Helper>> helpers;

        JUCE_DECLARE_NON_COPYABLE (AudioWorkers)
    };
}
