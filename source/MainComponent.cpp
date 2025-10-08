#include "MainComponent.h"

//=============================================================================
MainComponent::MainComponent(MainController& mc,                 
                             juce::ApplicationCommandManager& cm)
    : controller(mc),
      commandManager(cm)
{
    std::cout << "MainComponent constructor entered\n"; std::cout.flush();
    DBG("MainComponent constructor");

    visualizer = std::make_unique<GLVisualizer>(controller);
    addAndMakeVisible(visualizer.get());
    settings = std::make_unique<SettingsComponent>(controller);
    addAndMakeVisible(settings.get());
    grid = std::make_unique<GridComponent>(controller);
    addAndMakeVisible(grid.get());
    grid->setBounds(getLocalBounds());

    DBG("Grid pointer is " + juce::String(grid ? "valid" : "null"));




    grid->setAlwaysOnTop(true);


    controller.registerVisualizer(visualizer.get());
    controller.setDefaultParameters();

    if (settings != nullptr)
        settings->setVisible(false);

    setSize(1200, 750);

    // Defer the attachment setup until after *this* is fully constructed.
    // callAsync will run on the Message Thread shortly after the constructor returns.
    DBG("Scheduling initGridAttachment() via callAsync");
    juce::MessageManager::callAsync([this] { 
        DBG("Inside initGridAttachment lambda");
        initGridAttachment(); 
    });
}

//=============================================================================
/*  This is called every time the window is resized, and is where we set
    the bounds of the subcomponents (visualizer and settings panel).
*/
void MainComponent::resized()
{
    if (grid) grid->setBounds(getLocalBounds());
    if (visualizer) visualizer->setBounds(getLocalBounds());

    if (settings != nullptr)
    {
        auto bounds = getLocalBounds();

        if (viewMode == ViewMode::Focus)
        {
            visualizer->setBounds(bounds);
            settings->setVisible(false);
            grid->setBounds(bounds);
            gridToggle.setBounds(10, 10, 120, 24); // top-left corner
        }
        else // viewMode == Split
        {
            const int sidebarW = 300;
            auto right = bounds.removeFromRight(sidebarW);
            settings->setBounds(right);
            visualizer->setBounds(bounds);
            grid->setBounds(bounds);
            gridToggle.setBounds(10, 10, 120, 24);
            settings->setVisible(true);
        }
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
    // DBG("commandID = " << info.commandID);
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

void MainComponent::initGridAttachment()
{
    DBG("Entered initGridAttachment()");

    // Guard: make sure APVTS and parameter exist
    auto& apvts = controller.getAPVTS();         // safe to call now
    if (auto* param = apvts.getParameter("showGrid"))
    {
        // create the attachment (lambda captures `this` — safe now)
        gridAttachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newValue)
            {
                bool visible = newValue > 0.5f;
                juce::MessageManager::callAsync([this, visible]
                {
                    gridToggle.setToggleState(visible, juce::dontSendNotification);
                    grid->setVisible(visible);
                });
            },
            nullptr // undo manager optional
        );

        // Initial sync
        bool visible = param->getValue() > 0.5f;
        gridToggle.setToggleState(visible, juce::dontSendNotification);
        grid->setVisible(visible);
    }
    else
    {
        DBG("Warning: showGrid parameter not found at initGridAttachment()");
    }
}
