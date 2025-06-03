#pragma once

/* CMake builds don't use an AppConfig.h, so it's safe to include juce 
module headers directly. */
#include <JuceHeader.h>

//======================================================================
/* This component lives inside our window, and this is where you should 
put all your controls and content. */
class MainComponent final : public juce::Component
{
public:
    //==================================================================
    MainComponent();

    //==================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==================================================================
    // Private member variables go here...

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
