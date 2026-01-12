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
#include "Utils.h"

class WelcomeComponent : public juce::Component
{
public:
    WelcomeComponent()
    {
        addAndMakeVisible(text);
        text.setText(
            "Welcome to MoPanning!",
            juce::dontSendNotification);

        text.setReadOnly(true);

        addAndMakeVisible(okButton);
        okButton.setButtonText("Got it!");
        okButton.onClick = [this]
        {
            if(auto* w = findParentComponentOfClass<juce::DialogWindow>())
                w->exitModalState(0);
        };

        setSize(400, 300);
    }

    // void paint(juce::Graphics& g) override
    // {
    //     g.fillAll(juce::Colours::darkgrey);
    // }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        text.setBounds(area.removeFromTop(200));
        okButton.setBounds(area.removeFromBottom(30));
    }

private:
    juce::TextEditor text;
    juce::TextButton okButton;
};


struct WelcomeWindow
{
    static void show()
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new WelcomeComponent());
        options.dialogTitle = "Welcome to MoPanning";
        options.dialogBackgroundColour = juce::Colours::darkgrey;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    }
};