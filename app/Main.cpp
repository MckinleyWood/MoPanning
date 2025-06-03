/* Main.cpp

This file handles the app initialization and shutdown things.
*/


#include "MainComponent.h"
#include <JuceHeader.h>

//=============================================================================
class GuiAppApplication final : public juce::JUCEApplication
{
public:
    //=========================================================================
    GuiAppApplication() {}

    const juce::String getApplicationName() override 
    { 
        return ProjectInfo::projectName; 
    }

    const juce::String getApplicationVersion() override
    { 
        return ProjectInfo::versionString; 
    }

    bool moreThanOneInstanceAllowed() override
    { 
        return true; 
    }

    //=========================================================================
    void initialise (const juce::String& commandLine) override
    {
        // App initialization code goes here.
        juce::ignoreUnused (commandLine);

        controller  = std::make_unique<MainController>();
        mainWindow.reset(new MainWindow(getApplicationName(),
                                        *controller));
    }


    void shutdown() override
    {
        // App shutdown code goes here.

        mainWindow = nullptr; // Deletes the window
        controller = nullptr; // Controller should be destroyed after window
    }

    //=========================================================================
    /* Called when the app is being asked to quit. */
    void systemRequestedQuit() override
    {
        quit();
    }


    /* When another instance of the app is launched while this one is 
    running, this method is invoked, and the commandLine parameter tells
    you what the other instance's command-line arguments were. */
    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }


    //=========================================================================
    /* This class implements the desktop window that contains an 
    instance of our MainComponent class. */
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name, MainController& c)
            : DocumentWindow(
                name, 
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                    .findColour(backgroundColourId),
                allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(c), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
           #endif

            setVisible(true);
        }

        /* This is called when the user tries to close this window. */
        void closeButtonPressed() override
        {
            getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods 
        - the base class uses a lot of them, so by overriding you might 
        break its functionality. It's best to do all your work in your 
        content component instead, but if you really have to override 
        any DocumentWindow methods, make sure your subclass also calls 
        the superclass's method. */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainController> controller;
    std::unique_ptr<MainWindow> mainWindow;
};

//=============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (GuiAppApplication)
