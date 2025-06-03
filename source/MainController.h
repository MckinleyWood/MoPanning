/* MainController

This file handles the control logic and owns the audio engine, 
transport, and parameters, and supplies data the the other peices. All
communication between parts of the program must run through here.

Right now it is just a stub and doesn't do anything.
*/

#pragma once
#include <JuceHeader.h>

//=============================================================================
class MainController
{
public:
    //=========================================================================
    MainController();
    ~MainController();

    //=========================================================================


private:
    //=========================================================================

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainController)
};