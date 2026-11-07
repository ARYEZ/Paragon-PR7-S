#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/SessionLog.h"
#include "engine/Studio.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Windows.h"

// The Paragon PR7-S app: one window holding the studio.
class ParagonStudioApplication final : public juce::JUCEApplication,
                                       private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Paragon PR7-S"; }
    const juce::String getApplicationVersion() override { return PR7S_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        // The session log first (0.9.14): a crash from here on is written down.
        const auto unfinished = pr7s::SessionLog::start ("Paragon PR7-S " + getApplicationVersion());
        pr7s::SessionLog::installCrashHandler();
        pr7s::SessionLog::startFreezeWatch (10.0);
        startTimer (250);   // the screen's heartbeat for the freeze watch

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        pr7s::clearDragOutFolder();   // (0.53.1: last session's dragged-out parts)
        studio = std::make_unique<pr7s::Studio> (true);
        window = std::make_unique<MainWindow> (*this, *studio);
        if (studio->getSoftwareDrawing())   // (0.60.0)
        {
            pr7s::SessionLog::note ("Drawing: with the processor (SETTINGS)");
            Windows::applyDrawing (true);
        }

        // A song given on the command line (double-clicked in Explorer) opens.
        auto opened = false;
        if (const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.unquoted().trim());
            commandLine.isNotEmpty() && file.existsAsFile())
        {
            juce::String problems;
            opened = studio->open (file, problems);
        }
        // The Hub at start (0.12.0), unless a song was opened by double-clicking it.
        if (! opened && studio->getLibrary().opensAtStart())
            if (auto* view = dynamic_cast<MainView*> (window->getContentComponent()))
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (view)] { if (safe != nullptr) safe->openHub(); });
        pr7s::SessionLog::note ("Ready.");

        // Last time it didn't close as it should: offer its log.
        if (unfinished.existsAsFile())
            juce::MessageManager::callAsync ([unfinished] { reportUnfinished (unfinished); });
    }

    static void reportUnfinished (const juce::File& log)
    {
        const auto crashed = pr7s::SessionLog::recordsCrash (log);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
            crashed ? "PR7-S crashed last time" : "PR7-S didn't close normally last time",
            (crashed ? juce::String ("It wrote down where it crashed and what it was doing before: ")
                     : juce::String ("It may have frozen and been ended, or the computer went off. What it was doing last is written down: "))
                + log.getFileName() + ".\n\nIt's plain text (Notepad opens it). Send it over and the cause can be found.",
            "Show the log", "OK", nullptr,
            juce::ModalCallbackFunction::create ([log] (int result) { if (result == 1) log.revealToUser(); }));
    }

    void shutdown() override
    {
        // (0.52.1) The watch carries on while closing (30 s, plug-ins unloading can take a while):
        // a hang here used to leave nothing in the log.
        pr7s::SessionLog::closing (30.0);
        stopTimer();
        {
            const pr7s::SessionLog::Step step ("Closing the window");
            window = nullptr;
        }
        {
            const pr7s::SessionLog::Step step ("Closing the studio (the audio device, every plug-in unloaded)");
            studio = nullptr;
        }
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        pr7s::SessionLog::stop();
        // (0.52.2, the owner's report: still in Task Manager after closing) Everything is saved, every
        // plug-in unloaded and the log closed: the process ends here, not left waiting on Windows going
        // through each plug-in's file once more as it exits (a plug-in that won't let go kept it alive).
        pr7s::SessionLog::endProcessNow (0);
    }

    void unhandledException (const std::exception* e, const juce::String& sourceFile, int lineNumber) override
    {
        pr7s::SessionLog::note ("An error nothing handled" + (e != nullptr ? ": " + juce::String (e->what()) : juce::String())
                                + " (" + sourceFile + ":" + juce::String (lineNumber) + ")");
    }

    void systemRequestedQuit() override
    {
        if (studio != nullptr && studio->isRendering())
            return;   // (a render runs: quitting waits until it's done)
        if (window != nullptr)
            window->closeButtonPressed();
        else
            quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

    void timerCallback() override
    {
        pr7s::SessionLog::beat();
        // (0.60.0) Windows opened since follow SETTINGS' drawing (the processor or the graphics card). Since 0.73.4 every
        // tick (a quarter second; menus and boxes are switched as they're made, this catches the rest).
        if (studio != nullptr)
            Windows::applyDrawing (studio->getSoftwareDrawing());
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (ParagonStudioApplication& appToUse, pr7s::Studio& studio)
            : DocumentWindow ("Paragon PR7-S", ParagonColours::background, DocumentWindow::allButtons), app (appToUse)
        {
            setUsingNativeTitleBar (true);
            auto* view = new MainView (studio);
            view->onTitleChange = [this] (const juce::String& title) { setName (title); };
            setContentOwned (view, false);
            setResizable (true, true);
            setResizeLimits (1100, 680, 32768, 16384);   // (wide enough for three 4K screens side by side)
            // Where it was left last time (across several screens too), else the middle of the main one.
            const auto saved = studio.getSettings() != nullptr ? studio.getSettings()->getValue ("mainWindow") : juce::String();
            if (saved.isEmpty() || ! restoreWindowStateFromString (saved))
                centreWithSize (1500, 900);
            setVisible (true);
            view->grabKeyboardFocus();
        }

        void closeButtonPressed() override
        {
            if (app.studio != nullptr && app.studio->isRendering())
                return;   // (a render runs: closing waits until it's done)
            auto* view = dynamic_cast<MainView*> (getContentComponent());
            auto& studio = *app.studio;
            if (view == nullptr || ! studio.isModified())
            {
                app.quit();
                return;
            }
            // Unsaved changes: save, don't save, or carry on.
            juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save the song?",
                                                   "This song has changes that aren't saved.", "Save", "Don't save", "Cancel", nullptr,
                                                   juce::ModalCallbackFunction::create ([this, view] (int result)
                                                   {
                                                       if (result == 1)   // (after the box has gone: a chooser inside its answer closes with it on Windows)
                                                           juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainView> (view), this]
                                                           {
                                                               if (safe != nullptr)
                                                                   safe->saveProject ([this] (bool saved) { if (saved) app.quit(); });
                                                           });
                                                       else if (result == 2)
                                                           app.quit();
                                                   }));
        }

        ~MainWindow() override
        {
            if (auto* settings = app.studio != nullptr ? app.studio->getSettings() : nullptr)
            {
                settings->setValue ("mainWindow", getWindowStateAsString());
                settings->saveIfNeeded();
            }
        }

    private:
        ParagonStudioApplication& app;
    };

    ParagonLookAndFeel lookAndFeel;
    std::unique_ptr<pr7s::Studio> studio;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (ParagonStudioApplication)
