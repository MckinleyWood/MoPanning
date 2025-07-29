#include "MainComponent.h"

//=============================================================================
MainComponent::MainComponent(MainController& mc,                 
                             juce::ApplicationCommandManager& cm)
    : controller(mc),
      commandManager(cm),
      visualizer(controller),
      settings(controller)
{
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);

    controller.registerVisualizer(&visualizer);

    addAndMakeVisible(visualizer);
    addAndMakeVisible(settings);

    settings.setVisible(false); // Since we start in Focus mode
    setSize(1200, 750);
}

//=============================================================================
/* Since the child components visualizer and settings do all the 
drawing, we don't need a paint() function here. */

/*  This is called every time the window is resized, and is where we set
    the bounds of the subcomponents (visualizer and settings panel).
*/
void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    if (viewMode == ViewMode::Focus)
    {
        visualizer.setBounds(bounds);
        settings.setVisible(false);
    }
    else // ViewMode == Split
    {
        const int sidebarW = 300;
        auto right = bounds.removeFromRight(sidebarW);
        settings.setBounds(right);
        visualizer.setBounds(bounds);
        settings.setVisible(true);
    }
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
    static juce::File lastDir (juce::File::getSpecialLocation (
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
    if (id == cmdToggleSettings)
    {
        info.setInfo("Settings...", "Show the settings sidebar", 
                     "MoPanning",  0);
        info.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
    }
    if (id == cmdOpenFile)
    {
        info.setInfo("Open...", "Load an audio file", "File", 0);
        info.addDefaultKeypress('O', juce::ModifierKeys::commandModifier);
    }
    if (id == cmdPlayPause)
    {
        info.setInfo("Play / Pause", 
                     "Play or pause the currently loaded audio file", 
                     "File", 0);
        info.addDefaultKeypress(' ', juce::ModifierKeys::noModifiers);
    }
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
    /* Add commands to the Mac menu bar here. Index 0 = File, 1 = Help.
    For apple menu ("MoPanning"), you have to add it in Main.cpp. */
    if (topLevelIndex == 0)
    {
        m.addCommandItem(&commandManager, cmdOpenFile);
        m.addCommandItem(&commandManager, cmdPlayPause);
    }
   #else
    /* Add commands to Windows / Linux menu bars here. 
    Indexes are 0 = MoPanning, 1 = File, 2 = Help. */
    if (topLevelIndex == 0)
        m.addCommandItem(&commandManager, cmdToggleSettings);
    if (topLevelIndex == 1)
    {
        m.addCommandItem(&commandManager, cmdOpenFile);
        m.addCommandItem(&commandManager, cmdPlayPause);
    }
   #endif

    return m;
}