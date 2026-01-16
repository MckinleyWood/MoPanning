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

//=============================================================================
class EpicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EpicLookAndFeel() 
    {
        juce::Colour epicBackGround = Colour::fromRGB(30, 30, 30);
        juce::Colour epicText = Colours::linen;

        // General background colour
        setColour(ResizableWindow::backgroundColourId, epicBackGround);

        // Combo box colours
        setColour(ComboBox::backgroundColourId, epicBackGround);
        setColour(ComboBox::outlineColourId, epicText);
        setColour(ComboBox::arrowColourId, epicText);
        
        // Slider colours
        setColour(Slider::backgroundColourId, Colours::darkgrey);
        setColour(Slider::thumbColourId, Colours::lightgrey);
        setColour(Slider::trackColourId, Colours::grey);
        
        setColour(Slider::textBoxTextColourId, Colours::black);
        setColour(Slider::textBoxBackgroundColourId, Colours::lightgrey);
        setColour(Slider::textBoxOutlineColourId, Colours::grey);
        setColour(Slider::textBoxHighlightColourId, Colours::lightblue);

        // Button colours
        setColour (TextButton::buttonColourId, Colours::darkgrey);
        setColour(TextButton::textColourOnId, epicText);
        setColour(TextButton::textColourOffId, epicText);
        setColour(TextButton::buttonOnColourId, Colour::fromRGB(65, 65, 65));

        setColour(ToggleButton::textColourId, epicText);
        setColour(ToggleButton::tickColourId, epicText);

        // List box colours
        setColour(ListBox::backgroundColourId, Colours::darkgrey);
        setColour(ListBox::textColourId, epicText);
        setColour(ListBox::outlineColourId, Colours::lightgrey);
        setColour(ScrollBar::thumbColourId, Colours::lightgrey);
        setColour(ScrollBar::trackColourId, Colour::fromRGB(65, 65, 65));

        // Pop-up menu colours
        setColour(PopupMenu::textColourId, epicText);
        setColour(PopupMenu::backgroundColourId, epicBackGround);
        setColour(PopupMenu::headerTextColourId, epicText);
        setColour(PopupMenu::highlightedTextColourId, epicText);
        setColour(PopupMenu::highlightedBackgroundColourId, Colours::grey);

        // Dialog window colours
        setColour(DialogWindow::backgroundColourId, epicBackGround);

        // Alert window colours
        setColour(AlertWindow::backgroundColourId, epicBackGround);
        setColour(AlertWindow::textColourId, epicText);
    }

    void drawToggleButton (juce::Graphics& g,
                       juce::ToggleButton& button,
                       bool isHighlighted,
                       bool isDown) override
    {
        // Only special-case record buttons
        if (button.getButtonText() != "Record")
        {
            LookAndFeel_V4::drawToggleButton(g, button, isHighlighted, isDown);
            return;
        }

        auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
        auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto circle = bounds.withSizeKeepingCentre(diameter, diameter);

        // Outer ring
        g.setColour(juce::Colours::white);
        g.fillEllipse(circle);

        // Inner record light
        g.setColour(button.getToggleState()
                        ? juce::Colours::red
                        : juce::Colours::darkred);

        g.fillEllipse(circle.reduced(1.0f));

        // Hover ring
        if (isHighlighted)
        {
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.drawEllipse(circle, 2.0f);
        }
    }

private:

};
