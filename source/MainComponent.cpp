#include "MainComponent.h"

//=============================================================================
MainComponent::MainComponent(MainController& mc,                 
                             juce::ApplicationCommandManager& cm)
    : controller(mc),
      commandManager(cm),
      visualiser(controller),
      settings(controller)
{
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);

   #if ! JUCE_MAC
    // Windows/Linux: add a menubar component
    addAndMakeVisible(menuBar.get());
   #endif

    addAndMakeVisible(visualiser);
    addAndMakeVisible(settings);

    settings.setVisible(false); // start in Focus mode
    setSize(1200, 750);
}

//=============================================================================
/* Since the child components visualiser and settings do all the 
drawing, we don't need a paint() function here. */

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Set the bounds of the visualizer and the settings panel
    if (viewMode == ViewMode::Focus)
    {
        visualiser.setBounds(bounds);
        settings.setVisible(false);
    }
    else // ViewMode == Split
    {
        const int sidebarW = 260;
        auto right = bounds.removeFromRight(sidebarW);
        settings.setBounds(right);
        visualiser.setBounds(bounds);
        settings.setVisible(true);
    }
}

//=============================================================================
void MainComponent::toggleView()
{
    viewMode = (viewMode == ViewMode::Focus ? ViewMode::Split
                                            : ViewMode::Focus);

    resized(); // Force update view
}

//=============================================================================
void MainComponent::getAllCommands(juce::Array<juce::CommandID>& commands) 
{ 
    commands.add(cmdToggleSettings);
}

void MainComponent::getCommandInfo(juce::CommandID id,
                                   juce::ApplicationCommandInfo& info)
{
    if (id == cmdToggleSettings)
    {
        info.setInfo("Settings...", // menu text
                     "Show the settings sidebar", // tooltip / description
                     "Application", // menu category (optional)
                     0);
        info.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
    }
}

bool MainComponent::perform(const InvocationInfo& info)
{
    if (info.commandID == cmdToggleSettings)
    {
        toggleView();
        return true;
    }
    return false;
}

ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

//=============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
   #if JUCE_MAC
    return { "File", "Help" };
   #else
    return { "MarPanning", "File", "Help" };
   #endif
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelIndex,
                                               const juce::String&)
{
    juce::ignoreUnused(topLevelIndex);
    juce::PopupMenu m;

   #if JUCE_MAC
    /* Add commands to the Mac menu bar here. Index 0 = File, 1 = Help.
    For apple menu ("MarPanning"), you have to add it in Main.cpp. */
   #else
    /* Add commands to Windows / Linux menu bars here. 
    Indexes are 0 = MarPanning, 1 = File, 2 = Help. */
    if (topLevelIndex == 0)
        m.addCommandItem(&commandManager, cmdToggleSettings);
   #endif

    return m;
}