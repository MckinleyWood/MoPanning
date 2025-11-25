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

#pragma once
#include <JuceHeader.h>
#include "MainController.h"
#include "GLVisualizer2.h"
#include "SettingsComponent.h"
/* #include "GridComponent.h" */

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
    void paint(juce::Graphics&) override;

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
    std::unique_ptr<GLVisualizer2> visualizer;
    std::unique_ptr<SettingsComponent> settings;
    /* std::unique_ptr<GridComponent> grid; */
    ViewMode viewMode { ViewMode::Focus };
    /* std::unique_ptr<juce::ParameterAttachment> gridAttachment; */

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};