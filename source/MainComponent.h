/* MainComponent

This file handles the main window generation and graphics.
*/

#pragma once
#include <JuceHeader.h>
#include "MainController.h"
#include "GLVisualizer.h"
#include "SettingsComponent.h"

//=============================================================================
/* This component lives inside our window, and this is where you should 
put all your controls and content. */
class MainComponent final : public juce::Component,
                            public juce::ApplicationCommandTarget,
                            public juce::MenuBarModel
{
public:
    //=========================================================================
    enum CommandIDs { cmdToggleSettings = 0x2000 };
    enum class ViewMode { Focus, Split };

    //=========================================================================
    explicit MainComponent(MainController&, juce::ApplicationCommandManager&);

    //=========================================================================
    // void paint (juce::Graphics&) override;
    void resized() override;

private:
    //=========================================================================
    void toggleView();

    //=========================================================================
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID id,
                        juce::ApplicationCommandInfo& info) override;
    bool perform (const InvocationInfo& info) override;
    ApplicationCommandTarget* getNextCommandTarget() override;

    //=========================================================================
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index,
                                     const juce::String&) override;
    void menuItemSelected (int /*menuID*/,
                           int /*topLevelIndex*/) override {}

    //=========================================================================
    MainController& controller;
    juce::ApplicationCommandManager& commandManager;
    GLVisualizer visualiser;
    SettingsComponent settings;
    ViewMode viewMode { ViewMode::Focus };

   #if ! JUCE_MAC
    std::unique_ptr<juce::MenuBarComponent> menuBar;
   #endif


    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
