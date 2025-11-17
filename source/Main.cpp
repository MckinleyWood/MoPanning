/*=============================================================================

    This file is part of the MoPanning audio visuaization tool.
    Copyright (C) 2025 Owen Ohlson and Mckinley Wood

    This program is free software: you can redistribute it and/or modify 
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of the 
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but 
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
    Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public 
    License along with this program. If not, see 
    <https://www.gnu.org/licenses/>.

=============================================================================*/

/*  Main.cpp

    This file handles the app initialization and shutdown, including 
    creating/destroying the main window and component, the main 
    controller, and the command manager.
*/

#include <JuceHeader.h>
#include "MainController.h"
#include "MainComponent.h"

//=============================================================================
class GuiAppApplication final : public juce::JUCEApplication
{
public:
    //=========================================================================
    GuiAppApplication() = default;

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
    /*  This is function is called to initialize the application. It 
        creates the command manager, main controller, and main window,
        and initialises the menu bar (mac) or builds the
        window menu (win/linux).
    */
    void initialise(const juce::String& commandLine) override
    {
        // DBG("MoPanning Starting up!");
        juce::ignoreUnused(commandLine);

        commandManager = std::make_unique<juce::ApplicationCommandManager>();
        controller = std::make_unique<MainController>();
        mainComponent = std::make_unique<MainComponent>(*controller, 
                                                        *commandManager);

        mainWindow = std::make_unique<MainWindow>(getApplicationName(),
                                                  std::move(mainComponent),
                                                  *commandManager);

        mainWindow->getContentComponent()->grabKeyboardFocus();

        controller->startAudio();
    }

    /*  This is called to shut down the application. */
    void shutdown() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
       #endif

        mainWindow = nullptr; // Deletes the window
        controller = nullptr; // Controller should be destroyed after window
        commandManager = nullptr;
    }

    //=========================================================================
    /*  This is called when the app is being asked to quit. */
    void systemRequestedQuit() override
    {
        controller->stopRecording();
        quit();
    }

    /*  When another instance of the app is launched while this one is 
        running, this method is invoked, and the commandLine parameter 
        tells you what the other instance's command-line arguments were. 
    */
    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }

    juce::ApplicationCommandManager& getCommandManager() 
    { 
        return *commandManager; 
    }

    //=========================================================================
    /*  This class implements the desktop window that contains an 
        instance of our MainComponent class.
    */
    class MainWindow final  : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name,          
                            std::unique_ptr<MainComponent> mc,
                            juce::ApplicationCommandManager& cm)
            : DocumentWindow(name, juce::Colours::black, allButtons)
        {
            MainComponent* mcPtr = mc.get();

            setContentOwned(mc.release(), true);

            cm.registerAllCommandsForTarget(mcPtr);
            cm.setFirstCommandTarget(mcPtr);
            cm.getKeyMappings()->resetToDefaultMappings();
            addKeyListener(cm.getKeyMappings());

            // Set up the menu bar
           #if JUCE_MAC
            setUsingNativeTitleBar(true);
            juce::PopupMenu appMenu; // The application menu - "MoPanning"
            appMenu.addCommandItem(&cm, MainComponent::cmdToggleSettings);
            juce::MenuBarModel::setMacMainMenu(mcPtr, &appMenu);
           #else
            setUsingNativeTitleBar(true);
            auto bar = std::make_unique<juce::MenuBarComponent>(mcPtr);
            setMenuBarComponent(bar.release());
           #endif

            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
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
    //=========================================================================
    /* unique_ptrs for all of our objects. These need to be initialized
    in initialise(). */
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    std::unique_ptr<MainController> controller;
    std::unique_ptr<MainComponent> mainComponent;
    std::unique_ptr<MainWindow> mainWindow;
};

//=============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(GuiAppApplication)
