#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Studio.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

// The Paragon PR7-S app: one window holding the studio.
class ParagonStudioApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Paragon PR7-S"; }
    const juce::String getApplicationVersion() override { return PR7S_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        studio = std::make_unique<pr7s::Studio> (true);
        window = std::make_unique<MainWindow> (*this, *studio);

        // A song given on the command line (double-clicked in Explorer) opens.
        if (const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.unquoted().trim());
            commandLine.isNotEmpty() && file.existsAsFile())
        {
            juce::String problems;
            studio->open (file, problems);
        }
    }

    void shutdown() override
    {
        window = nullptr;
        studio = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr)
            window->closeButtonPressed();
        else
            quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

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
            setResizeLimits (1100, 680, 10000, 10000);
            centreWithSize (1500, 900);
            setVisible (true);
            view->grabKeyboardFocus();
        }

        void closeButtonPressed() override
        {
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
                                                       if (result == 1)
                                                           view->saveProject ([this] (bool saved) { if (saved) app.quit(); });
                                                       else if (result == 2)
                                                           app.quit();
                                                   }));
        }

    private:
        ParagonStudioApplication& app;
    };

    ParagonLookAndFeel lookAndFeel;
    std::unique_ptr<pr7s::Studio> studio;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (ParagonStudioApplication)
