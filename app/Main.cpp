/* Main.cpp

This file handles the app initialization and shutdown things.
*/

#include <JuceHeader.h>
#include "MainComponent.h"

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
    void initialise(const juce::String& commandLine) override
    {
        // App initialization code goes here.
        juce::ignoreUnused (commandLine);

        commandManager = std::make_unique<juce::ApplicationCommandManager>();
        controller = std::make_unique<MainController>();

       #if JUCE_MAC
        auto mainComponent = std::make_unique<MainComponent>(*controller, 
                                                             *commandManager);
        juce::PopupMenu appleExtras;
        appleExtras.addCommandItem(&*commandManager,
                                   MainComponent::cmdToggleSettings);
        juce::MenuBarModel::setMacMainMenu(mainComponent.get(), &appleExtras);
        mainWindow = std::make_unique<MainWindow>(getApplicationName(),
                                                  std::move(mainComponent));
       #else
        // Windows/Linux: we can still build inside the window as usual
        mainWindow = std::make_unique<MainWindow>(
            getApplicationName(),
            std::make_unique<MainComponent>(*controller, *commandManager));
       #endif
    }

    void shutdown() override
    {
        // App shutdown code goes here.
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
       #endif

        mainWindow = nullptr; // Deletes the window
        controller = nullptr; // Controller should be destroyed after window
        commandManager = nullptr;
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

    juce::ApplicationCommandManager& getCommandManager() 
    { 
        return *commandManager; 
    }

    //=========================================================================
    /* This class implements the desktop window that contains an 
    instance of our MainComponent class. */
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name,          
                            std::unique_ptr<MainComponent>  mc)
            : DocumentWindow(
                name, 
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                    .findColour(backgroundColourId),
                allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(mc.release(), true);

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
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    std::unique_ptr<MainController> controller;
    std::unique_ptr<MainWindow> mainWindow;
};

//=============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(GuiAppApplication)
