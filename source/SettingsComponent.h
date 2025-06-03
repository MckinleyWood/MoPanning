/* SettingsComponent

This file handles the settings widget GUI.
*/

#pragma once
#include <JuceHeader.h>

class MainController;

//=============================================================================
class SettingsComponent : public juce::Component
{
public:
    //=========================================================================
    explicit SettingsComponent(MainController&);
    ~SettingsComponent() override;

    //=========================================================================
    void paint(juce::Graphics&) override;
    void resized(void) override;

private:
    //=========================================================================
    MainController& controller;
    
    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};