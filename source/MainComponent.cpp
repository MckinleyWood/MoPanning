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

#include "MainComponent.h"

//=============================================================================
MainComponent::MainComponent(MainController& mc,                 
                             juce::ApplicationCommandManager& cm)
    : controller(mc),
      commandManager(cm)
{
    visualizer = std::make_unique<GLVisualizer>(controller);
    settings = std::make_unique<SettingsComponent>(controller);
    grid = std::make_unique<GridComponent>(controller);    

    jassert(visualizer != nullptr && settings != nullptr && grid != nullptr);
    
    addAndMakeVisible(visualizer.get());
    addChildComponent(settings.get());    
    addChildComponent(grid.get());

    controller.registerVisualizer(visualizer.get());
    controller.registerGrid(grid.get());
    grid->setAlwaysOnTop(true);

    controller.setDefaultParameters();

    setSize(1200, 750);
}

//=============================================================================
/*  This is called every time the window is resized, and is where we set
    the bounds of the subcomponents (visualizer and settings panel).
*/
void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    if (viewMode == ViewMode::Focus)
    {
        visualizer->setBounds(bounds);
        settings->setVisible(false);
        grid->setBounds(bounds);
    }
    else // viewMode == Split
    {
        const int sidebarW = 300;
        auto right = bounds.removeFromRight(sidebarW);
        settings->setBounds(right);
        visualizer->setBounds(bounds);
        grid->setBounds(bounds);
        settings->setVisible(true);
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey); // Clear background to grey
}

//=============================================================================
/*  This switches between the Focus (full window visualizer) and Split
    (settings panel on sidebar) views. 
*/
void MainComponent::toggleView()
{
    viewMode = (viewMode == ViewMode::Focus ? ViewMode::Split
                                            : ViewMode::Focus);

    resized(); // Forces a redraw of the subcomponents
}

/*  This launches an asynchronous dialog window that allows the user to
    choose an audio file to load and play back. 
*/
void MainComponent::launchOpenDialog()
{
    // Keep a “last directory” so the chooser re-opens in the same place
    static juce::File lastDir(juce::File::getSpecialLocation(
                                juce::File::userDocumentsDirectory));

    // Only highlight audio files we support
    const juce::String filters = "*.wav;*.aiff;*.mp3;*.flac;*.m4a;*.ogg";

    // Initialize file chooser object
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select an audio file to open...",
        lastDir, filters, /* useNativeDialog */ true);

    /* Asynchronous dialog window - the lambda function is called once 
    open or cancel is pressed. */
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode // Flags
      | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            juce::ignoreUnused (chooser);
            auto file = fc.getResult();

            if (file.existsAsFile())
            {
                lastDir = file.getParentDirectory();
                controller.loadFile(file); // Give file to controller to open
            }
        });
}

//=============================================================================
void MainComponent::getAllCommands(juce::Array<juce::CommandID>& commands) 
{ 
    commands.add(cmdToggleSettings);
    commands.add(cmdOpenFile);
    commands.add(cmdPlayPause);
}

/*  This returns info about the command asscociated with id, including
    which key triggers it (default keypress). 
*/
void MainComponent::getCommandInfo(juce::CommandID id,
                                   juce::ApplicationCommandInfo& info)
{
    juce::String shortName, description, category;
    int key = 0;
    juce::ModifierKeys modifiers;
    
    if (id == cmdToggleSettings)
    {
        shortName = "Settings...";
        description = "Show the settings sidebar";
        category = "MoPanning";
        key = ',';
        modifiers = juce::ModifierKeys::commandModifier;
    }
    else if (id == cmdOpenFile)
    {
        shortName = "Open...";
        description = "Load an audio file";
        category = "File";
        key = 'O';
        modifiers = juce::ModifierKeys::commandModifier;
    }
    else if (id == cmdPlayPause)
    {
        shortName = "Play / Pause";
        description = "Play or pause the currently loaded audio file";
        category = "File";
        key = ' ';
        modifiers = juce::ModifierKeys::noModifiers;
    }
    else
    {
        jassertfalse; // Unknown command ID!
        return;
    }

    info.setInfo(shortName, description, category, 0);
    info.addDefaultKeypress(key, modifiers);
}

/*  This is called whenever a command is executed, and is where we set 
    the functionality of each command. 
*/
bool MainComponent::perform(const InvocationInfo& info)
{
    if (info.commandID == cmdToggleSettings)
    {
        toggleView();
        return true;
    }
    if (info.commandID == cmdOpenFile)
    {
        launchOpenDialog();
        return true;
    }
    if (info.commandID == cmdPlayPause)
    {
        controller.togglePlayback();
        return true;
    }
    return false;
}

ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

//=============================================================================
/* This returns the names of each of the menu bar fields. */
juce::StringArray MainComponent::getMenuBarNames()
{
   #if JUCE_MAC
    return { "File", "Help" };
   #else
    return { "MoPanning", "File", "Help" };
   #endif
}

/*  This adds commands to the menu bar based on their index. The index
    is different for mac vs. win/linux because the "MoPanning" field is
    treated differently on mac. 
*/
juce::PopupMenu MainComponent::getMenuForIndex(int topLevelIndex,
                                               const juce::String&)
{
    juce::ignoreUnused(topLevelIndex);

    juce::PopupMenu m;

   #if JUCE_MAC
    // Add commands to the Mac menu bar here. Index 0 = File, 1 = Help.
    // For apple menu ("MoPanning"), you have to add it in Main.cpp.
    if (topLevelIndex == 0)
    {
        m.addCommandItem(&commandManager, cmdOpenFile);
        m.addCommandItem(&commandManager, cmdPlayPause);
    }
   #else
    // Add commands to Windows / Linux menu bars here. 
    // Indexes are 0 = MoPanning, 1 = File, 2 = Help.
    if (topLevelIndex == 0)
    {
        m.addCommandItem(&commandManager, cmdToggleSettings);
    }
    else if (topLevelIndex == 1)
    {
        m.addCommandItem(&commandManager, cmdOpenFile);
        m.addCommandItem(&commandManager, cmdPlayPause);
    }
   #endif

    return m;
}