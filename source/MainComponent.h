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
class MainComponent final : public juce::Component
{
public:
    //=========================================================================
    enum class ViewMode { Focus, Split };

    //=========================================================================
    explicit MainComponent(MainController&);

    //=========================================================================
    // void paint (juce::Graphics&) override;
    void resized() override;

private:
    //=========================================================================
    void toggleView();

    //=========================================================================
    MainController& controller;
    GLVisualizer visualiser;
    SettingsComponent settings;
    juce::TextButton viewButton { "*" };
    ViewMode viewMode { ViewMode::Focus };


    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
