#pragma once
#include <JuceHeader.h>
#include "MainController.h"
#include "GLVisualizer.h"
#include "SettingsComponent.h"

//=============================================================================
/*  This is the top-level UI container. It holds the GLVisualizer 
    (OpenGL canvas), SettingsComponent (sidebar), and a MainController& 
    controller reference. It is responsible for	passing user actions to 
    the controller and switching between Focus (full visualizer) and 
    Split (sidebar visible) views.
*/
class MainComponent final : public juce::Component,
                            public juce::ApplicationCommandTarget,
                            public juce::MenuBarModel
{
public:
    //=========================================================================
    enum CommandIDs 
    { 
        cmdToggleSettings   = 0x2000,
        cmdOpenFile         = 0x2001,
        cmdPlayPause        = 0x2002,
    };
    enum class ViewMode { Focus, Split };

    //=========================================================================
    explicit MainComponent(MainController&, juce::ApplicationCommandManager&);

    //=========================================================================
    void resized() override;

private:
    //=========================================================================
    void toggleView();
    void launchOpenDialog();

    //=========================================================================
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID id,
                        juce::ApplicationCommandInfo& info) override;
    bool perform (const InvocationInfo& info) override;
    ApplicationCommandTarget* getNextCommandTarget() override;

    //=========================================================================
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int index,
                                    const juce::String&) override;
    void menuItemSelected(int /*menuID*/,
                          int /*topLevelIndex*/) override {}

    //=========================================================================
    MainController& controller;
    juce::ApplicationCommandManager& commandManager;
    GLVisualizer visualizer;
    SettingsComponent settings;
    ViewMode viewMode { ViewMode::Focus };

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
