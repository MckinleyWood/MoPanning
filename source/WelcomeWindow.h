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
#include "EpicLookAndFeel.h"
#include "Utils.h"

class WelcomeComponent : public juce::Component
{
public:
    WelcomeComponent()
    {
        setLookAndFeel(&epicLookAndFeel);

        using Font = juce::FontOptions;

        // Set the fonts
        Font normalFont = {14.f, 0};
        Font titleFont = normalFont.withHeight(30.0f).withStyle("Bold");

        // Set up title label
        title.setFont(titleFont);
        title.setText("Welcome to MoPanning!", juce::dontSendNotification);
        addAndMakeVisible(title);

        // Set up text label
        text.setFont(normalFont);
        text.setText(
            "Thank you for installing MoPanning, A perception-based real-time "
            "visualization program for stereo audio including music. To learn "
            "more about what MoPanning is and how to use it, check out our "
            "                         and our                     . "
            "Otherwise, select 'MoPanning -> Settings...' from the menu bar "
            "to open the settings window and get started!",
            juce::dontSendNotification);
        addAndMakeVisible(text);

        // Set up links
        youtubeLink.setFont(normalFont.withUnderline(true), false);
        youtubeLink.setButtonText("YouTube video");
        youtubeLink.setURL(juce::URL("https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
        addAndMakeVisible(youtubeLink);

        githubLink.setFont(normalFont.withUnderline(true), false);
        githubLink.setButtonText("GitHub page");
        githubLink.setURL(juce::URL("https://github.com/MckinleyWood/MoPanning"));
        addAndMakeVisible(githubLink);

        okButton.setButtonText("Got it!");
        okButton.onClick = [this]
        {
            if(auto* w = findParentComponentOfClass<juce::DialogWindow>())
                w->exitModalState(0);
        };
        addAndMakeVisible(okButton);

        setSize(350, 250);
    }

    ~WelcomeComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::DialogWindow::backgroundColourId));

        // paintOverChildren(g);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Lay out the title at the top
        auto titleZone = bounds.removeFromTop(30);
        title.setBounds(titleZone);

        // Lay out the text below the title
        auto textZone = bounds.removeFromTop(98);
        text.setBounds(textZone);

        // Lay out the links within the text zone
        youtubeLink.setBounds(textZone.withTrimmedTop(47)
                                      .withTrimmedBottom(33)
                                      .withTrimmedRight(125)
                                      .withTrimmedLeft(113));

        githubLink.setBounds(textZone.withTrimmedTop(47)
                                     .withTrimmedBottom(33)
                                     .withTrimmedRight(10)
                                     .withTrimmedLeft(245));

        okButton.setBounds(bounds.removeFromBottom(30));
    }

private:
    void paintOverChildren(juce::Graphics& g) override
    {
        // g.setColour(juce::Colours::yellow.withAlpha(0.9f));

        // for (auto* child : getChildren())
        // {
        //     if (child->isVisible())
        //         g.drawRect(child->getBounds(), 2);
        // }
    }


    EpicLookAndFeel epicLookAndFeel;
    juce::Label title;
    juce::Label text;
    juce::HyperlinkButton youtubeLink;
    juce::HyperlinkButton githubLink;
    juce::TextButton okButton;
};


struct WelcomeWindow
{
    static void show()
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new WelcomeComponent());
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    }
};