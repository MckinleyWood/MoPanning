#include "MainComponent.h"

//=============================================================================
MainComponent::MainComponent(MainController& c)
    : controller(c),
      visualiser(controller),
      settings(controller)
{
    addAndMakeVisible(visualiser);
    addAndMakeVisible(settings);
    addAndMakeVisible(viewButton);

    viewButton.onClick = [this] { toggleView(); };
    settings.setVisible(false); // start in Focus mode
    setSize(1200, 750);
}

//=============================================================================
/* Since the child components visualiser and settings do all the 
drawing, we don't need a paint() function here. */

void MainComponent::resized()
{
    /* This is all just ChatGPT, will change to nice layout */
    auto r = getLocalBounds();
    auto btn = r.removeFromTop(28).removeFromRight(28);
    viewButton.setBounds(btn);

    if (viewMode == ViewMode::Focus)
    {
        visualiser.setBounds(r);
        settings.setVisible(false);
    }
    else // ViewMode == Split
    {
        const int sidebarW = 260;
        auto right = r.removeFromRight(sidebarW);
        settings.setBounds(right.reduced(6));
        visualiser.setBounds(r);
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