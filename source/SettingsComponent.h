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

using apvts = juce::AudioProcessorValueTreeState;

class MainController;

// Helper for finding child components of device selector
static juce::Component* findAudioPanelRecursively (juce::Component* parent)
{
    if (parent == nullptr)
        return nullptr;

    // Case A: Is this a TabbedComponent?
    if (auto* tabs = dynamic_cast<juce::TabbedComponent*>(parent))
    {
        if (auto* content = tabs->getTabContentComponent(0))
            return content;

        return nullptr;
    }

    // Case B: Panel is the *direct* child of AudioDeviceSelectorComponent
    // (internal AudioDeviceSettingsPanel)
    if (parent->getNumChildComponents() == 1)
    {
        return parent->getChildComponent(0);
    }

    // Fallback: recursively search children
    for (int i = 0; i < parent->getNumChildComponents(); ++i)
        if (auto* found = findAudioPanelRecursively(parent->getChildComponent(i)))
            return found;

    return nullptr;
}

//=============================================================================
class CustomAudioDeviceSelectorComponent 
    : public juce::AudioDeviceSelectorComponent
{
public:
    std::function<void()> onHeightChanged;
    std::function<void()> onInputTypeChanged;

    using juce::AudioDeviceSelectorComponent::AudioDeviceSelectorComponent;

    void resized() override
    {
        auto oldHeight = getHeight();
        juce::AudioDeviceSelectorComponent::resized();
        auto newHeight = getHeight();

        if (oldHeight != newHeight && onHeightChanged)
            onHeightChanged();
    }

    void setDeviceControls(bool streaming)
    {
        if (auto* panel = findAudioPanelRecursively(this))
        {
            // DBG("Number of child components: " << panel->getNumChildComponents());

            // for (int i = 0; i < panel->getNumChildComponents(); ++i)
            // {
            //     juce::Component* child = panel->getChildComponent(i);
            //     DBG("  child[" << i << "] class = " << typeid(*child).name());
            // }

            // Output elements
            const int outputButtonIndex       = 0;
            const int outputDropdownIndex     = 1;
            const int outputLabelIndex        = 2;
            const int outputListBoxIndex      = 7;
            const int outputListBoxLabelIndex = 8;

            // Input elements
            const int inputButtonIndex        = 3;
            const int inputDropdownIndex      = 4;
            const int inputLabelIndex         = 5;
            const int inputMeterIndex         = 6;
            const int inputListBoxIndex       = 13;

            auto trySetVisible = [panel](int index, bool vis)
            {
                if (index < panel->getNumChildComponents())
                    panel->getChildComponent(index)->setVisible(vis);
            };

            // Get the input dropdown
            if (auto* inputBox = dynamic_cast<juce::ComboBox*>(panel->getChildComponent(inputDropdownIndex)))
            {
                // Center justification
                inputBox->setJustificationType(juce::Justification::centred);

                if (streaming == false)
                {
                    // Currently selected input for later
                    selected = inputBox->getSelectedId();
                    // selectedName = inputBox->getSelectedIdAsValue();
                    
                    // inputBox->setSelectedId(-1);
                    
                    // Set the displayed text without affecting the underlying item list
                    selectedText = inputBox->getText();
                    inputBox->setText("File", juce::dontSendNotification);
                    
                    // Lock the box so the user cannot change it
                    inputBox->setEnabled(false);

                    // trySetVisible(inputListBoxIndex, false);

                }
                else 
                {
                    inputBox->setEnabled(true);

                    inputBox->setText(selectedText, juce::dontSendNotification);
                    // int numItems = inputBox->getNumItems();
                    // if (numItems > 0)
                    //     inputBox->setSelectedId(selected);
                }
            }

            if (auto* outputBox = dynamic_cast<juce::ComboBox*>(panel->getChildComponent(outputDropdownIndex)))
            {
                // Center justification
                outputBox->setJustificationType(juce::Justification::centred);

                int x = outputBox->getX();
                int y = outputBox->getY();
                int w = outputBox->getWidth();
                int h = outputBox->getHeight();

                outputBox->setBounds(x - 50 , y, w, h);
            }


            // trySetVisible(inputButtonIndex,   shouldBeVisible);
            // trySetVisible(inputDropdownIndex, shouldBeVisible);
            // trySetVisible(inputLabelIndex,    shouldBeVisible);
            // trySetVisible(inputMeterIndex,    shouldBeVisible);

            panel->resized();
        }

        resized();
        if (onHeightChanged) onHeightChanged();
    }

private:
    juce::PropertyPanel* findPropertyPanel()
    {
        for (auto* child : getChildren())
            if (auto* panel = dynamic_cast<juce::PropertyPanel*>(child))
                return panel;

        return nullptr;
    }

    int selected = 0;
    String selectedText;
};

class epicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    epicLookAndFeel() 
    {
        // Combo box colors
        setColour(ComboBox::backgroundColourId, Colours::darkgrey);
        setColour(ComboBox::outlineColourId, Colours::linen);
        setColour(ComboBox::arrowColourId, Colours::linen);
        
        // Slider colors
        setColour(Slider::backgroundColourId, Colours::darkgrey);
        setColour(Slider::thumbColourId, Colours::lightgrey);
        setColour(Slider::trackColourId, Colours::grey);
        
        setColour(Slider::textBoxTextColourId, Colours::black);
        setColour(Slider::textBoxBackgroundColourId, Colours::lightgrey);
        setColour(Slider::textBoxOutlineColourId, Colours::grey);
        setColour(Slider::textBoxHighlightColourId, Colours::lightblue);

        // Button colors
        setColour (TextButton::buttonColourId, Colours::darkgrey);
        setColour(TextButton::textColourOnId, Colours::linen);
        setColour(TextButton::textColourOffId, Colours::linen);
        setColour(TextButton::buttonOnColourId, Colour::fromRGB(65, 65, 65));

        setColour(ToggleButton::textColourId, Colours::linen);
        setColour(ToggleButton::tickColourId, Colours::linen);

        // List box colors
        setColour(ListBox::backgroundColourId, Colours::darkgrey);
        setColour(ListBox::textColourId, Colours::linen);
        setColour(ListBox::outlineColourId, Colours::lightgrey);
        setColour(ScrollBar::thumbColourId, Colours::lightgrey);
        setColour(ScrollBar::trackColourId, Colour::fromRGB(65, 65, 65));

        // Pop-up menu colors
        setColour(PopupMenu::textColourId, Colours::linen);
        setColour(PopupMenu::backgroundColourId, Colours::darkgrey);
        setColour(PopupMenu::headerTextColourId, Colours::linen);
        setColour(PopupMenu::highlightedTextColourId, Colours::linen);
        setColour(PopupMenu::highlightedBackgroundColourId, Colours::grey);
    }

    // void drawMenuBarBackground (Graphics& g, int width, int height,
    //                             bool, MenuBarComponent& menuBar) override
    // {
    //     menuBar.setColour(TextButton::buttonColourId, Colours::lightgreen);

    //     auto colour = menuBar.findColour (TextButton::buttonColourId).withAlpha (0.4f);

    //     Rectangle<int> r (width, height);

    //     g.setColour (colour.contrasting (0.15f));
    //     g.fillRect  (r.removeFromTop (1));
    //     g.fillRect  (r.removeFromBottom (1));

    //     g.setGradientFill (ColourGradient::vertical (colour, 0, colour.darker (0.2f), (float) height));
    //     g.fillRect (r);
    // }

    // void drawMenuBarItem (Graphics& g, int width, int height,
    //                     int itemIndex, const String& itemText,
    //                     bool isMouseOverItem, bool isMenuOpen,
    //                     bool /*isMouseOverBar*/, MenuBarComponent& menuBar) override
    // {
    //     menuBar.setColour(TextButton::textColourOffId, Colours::red);
    //     menuBar.setColour(TextButton::textColourOnId, Colours::purple);
    //     menuBar.setColour(TextButton::buttonOnColourId, Colours::pink);


    //     if (! menuBar.isEnabled())
    //     {
    //         g.setColour (menuBar.findColour (TextButton::textColourOffId)
    //                             .withMultipliedAlpha (0.5f));
    //     }
    //     else if (isMenuOpen || isMouseOverItem)
    //     {
    //         g.fillAll   (menuBar.findColour (TextButton::buttonOnColourId));
    //         g.setColour (menuBar.findColour (TextButton::textColourOnId));
    //     }
    //     else
    //     {
    //         g.setColour (menuBar.findColour (TextButton::textColourOffId));
    //     }

    //     g.setFont (getMenuBarFont (menuBar, itemIndex, itemText));
    //     g.drawFittedText (itemText, 0, 0, width, height, Justification::centred, 1);
    // }

    // void drawComboBox(juce::Graphics& g, int width, int height,
    //                 bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH,
    //                 ComboBox& box) override
    // {
    //     // box.setColour(ComboBox::backgroundColourId, Colours::darkgrey);
    //     // box.setColour(ComboBox::outlineColourId, Colours::linen);
    //     // box.setColour(ComboBox::arrowColourId, Colours::linen);

    //     auto cornerSize = box.findParentComponentOfClass<ChoicePropertyComponent>() != nullptr ? 0.0f : 3.0f;
    //     Rectangle<int> boxBounds (0, 0, width, height);

    //     g.setColour (box.findColour (ComboBox::backgroundColourId));
    //     g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

    //     g.setColour (box.findColour (ComboBox::outlineColourId));
    //     g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

    //     Rectangle<int> arrowZone (width - 30, 0, 20, height);
    //     Path path;
    //     path.startNewSubPath ((float) arrowZone.getX() + 3.0f, (float) arrowZone.getCentreY() - 2.0f);
    //     path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 3.0f);
    //     path.lineTo ((float) arrowZone.getRight() - 3.0f, (float) arrowZone.getCentreY() - 2.0f);

    //     g.setColour (box.findColour (ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 0.9f : 0.2f)));
    //     g.strokePath (path, PathStrokeType (2.0f));
    // }

    // void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    //                 float sliderPos, float minSliderPos, float maxSliderPos,
    //                 const Slider::SliderStyle style, Slider& slider) override
    // {
    //     // slider.setColour(Slider::backgroundColourId, Colours::darkgrey);
    //     // slider.setColour(Slider::thumbColourId, Colours::lightgrey);
    //     // slider.setColour(Slider::trackColourId, Colours::grey);

    //     // slider.setColour(Slider::textBoxTextColourId, Colours::black);
    //     // slider.setColour(Slider::textBoxBackgroundColourId, Colours::lightgrey);
    //     // slider.setColour(Slider::textBoxOutlineColourId, Colours::grey);
    //     // slider.setColour(Slider::textBoxHighlightColourId, Colours::lightblue);

    //     if (slider.isBar())
    //     {
    //         g.setColour (slider.findColour (Slider::trackColourId));
    //         g.fillRect (slider.isHorizontal() ? Rectangle<float> (static_cast<float> (x), (float) y + 0.5f, sliderPos - (float) x, (float) height - 1.0f)
    //                                         : Rectangle<float> ((float) x + 0.5f, sliderPos, (float) width - 1.0f, (float) y + ((float) height - sliderPos)));

    //         drawLinearSliderOutline (g, x, y, width, height, style, slider);
    //     }
    //     else
    //     {
    //         auto isTwoVal   = (style == Slider::SliderStyle::TwoValueVertical   || style == Slider::SliderStyle::TwoValueHorizontal);
    //         auto isThreeVal = (style == Slider::SliderStyle::ThreeValueVertical || style == Slider::SliderStyle::ThreeValueHorizontal);

    //         auto trackWidth = jmin (6.0f, slider.isHorizontal() ? (float) height * 0.25f : (float) width * 0.25f);

    //         Point<float> startPoint (slider.isHorizontal() ? (float) x : (float) x + (float) width * 0.5f,
    //                                 slider.isHorizontal() ? (float) y + (float) height * 0.5f : (float) (height + y));

    //         Point<float> endPoint (slider.isHorizontal() ? (float) (width + x) : startPoint.x,
    //                             slider.isHorizontal() ? startPoint.y : (float) y);

    //         Path backgroundTrack;
    //         backgroundTrack.startNewSubPath (startPoint);
    //         backgroundTrack.lineTo (endPoint);
    //         g.setColour (slider.findColour (Slider::backgroundColourId));
    //         g.strokePath (backgroundTrack, { trackWidth, PathStrokeType::curved, PathStrokeType::rounded });

    //         Path valueTrack;
    //         Point<float> minPoint, maxPoint, thumbPoint;

    //         if (isTwoVal || isThreeVal)
    //         {
    //             minPoint = { slider.isHorizontal() ? minSliderPos : (float) width * 0.5f,
    //                         slider.isHorizontal() ? (float) height * 0.5f : minSliderPos };

    //             if (isThreeVal)
    //                 thumbPoint = { slider.isHorizontal() ? sliderPos : (float) width * 0.5f,
    //                             slider.isHorizontal() ? (float) height * 0.5f : sliderPos };

    //             maxPoint = { slider.isHorizontal() ? maxSliderPos : (float) width * 0.5f,
    //                         slider.isHorizontal() ? (float) height * 0.5f : maxSliderPos };
    //         }
    //         else
    //         {
    //             auto kx = slider.isHorizontal() ? sliderPos : ((float) x + (float) width * 0.5f);
    //             auto ky = slider.isHorizontal() ? ((float) y + (float) height * 0.5f) : sliderPos;

    //             minPoint = startPoint;
    //             maxPoint = { kx, ky };
    //         }

    //         auto thumbWidth = getSliderThumbRadius (slider);

    //         valueTrack.startNewSubPath (minPoint);
    //         valueTrack.lineTo (isThreeVal ? thumbPoint : maxPoint);
    //         g.setColour (slider.findColour (Slider::trackColourId));
    //         g.strokePath (valueTrack, { trackWidth, PathStrokeType::curved, PathStrokeType::rounded });

    //         if (! isTwoVal)
    //         {
    //             g.setColour (slider.findColour (Slider::thumbColourId));
    //             g.fillEllipse (Rectangle<float> (static_cast<float> (thumbWidth), static_cast<float> (thumbWidth)).withCentre (isThreeVal ? thumbPoint : maxPoint));
    //         }

    //         if (isTwoVal || isThreeVal)
    //         {
    //             auto sr = jmin (trackWidth, (slider.isHorizontal() ? (float) height : (float) width) * 0.4f);
    //             auto pointerColour = slider.findColour (Slider::thumbColourId);

    //             if (slider.isHorizontal())
    //             {
    //                 drawPointer (g, minSliderPos - sr,
    //                             jmax (0.0f, (float) y + (float) height * 0.5f - trackWidth * 2.0f),
    //                             trackWidth * 2.0f, pointerColour, 2);

    //                 drawPointer (g, maxSliderPos - trackWidth,
    //                             jmin ((float) (y + height) - trackWidth * 2.0f, (float) y + (float) height * 0.5f),
    //                             trackWidth * 2.0f, pointerColour, 4);
    //             }
    //             else
    //             {
    //                 drawPointer (g, jmax (0.0f, (float) x + (float) width * 0.5f - trackWidth * 2.0f),
    //                             minSliderPos - trackWidth,
    //                             trackWidth * 2.0f, pointerColour, 1);

    //                 drawPointer (g, jmin ((float) (x + width) - trackWidth * 2.0f, (float) x + (float) width * 0.5f), maxSliderPos - sr,
    //                             trackWidth * 2.0f, pointerColour, 3);
    //             }
    //         }

    //         if (slider.isBar())
    //             drawLinearSliderOutline (g, x, y, width, height, style, slider);
    //     }
    // }

    // void drawButtonBackground (Graphics& g,
    //                             Button& button,
    //                             const Colour& backgroundColour,
    //                             bool shouldDrawButtonAsHighlighted,
    //                             bool shouldDrawButtonAsDown) override
    // {
    //     // button.setColour(ComboBox::backgroundColourId, Colours::darkgrey);
    //     // button.setColour(ComboBox::outlineColourId, Colours::linen);
    //     // button.setColour(ComboBox::arrowColourId, Colours::linen);

    //     auto cornerSize = 6.0f;
    //     auto bounds = button.getLocalBounds().toFloat().reduced (0.5f, 0.5f);

    //     auto baseColour = backgroundColour.withMultipliedSaturation (button.hasKeyboardFocus (true) ? 1.3f : 0.9f)
    //                                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

    //     if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
    //         baseColour = baseColour.contrasting (shouldDrawButtonAsDown ? 0.2f : 0.05f);

    //     g.setColour (baseColour);

    //     auto flatOnLeft   = button.isConnectedOnLeft();
    //     auto flatOnRight  = button.isConnectedOnRight();
    //     auto flatOnTop    = button.isConnectedOnTop();
    //     auto flatOnBottom = button.isConnectedOnBottom();

    //     if (flatOnLeft || flatOnRight || flatOnTop || flatOnBottom)
    //     {
    //         Path path;
    //         path.addRoundedRectangle (bounds.getX(), bounds.getY(),
    //                                 bounds.getWidth(), bounds.getHeight(),
    //                                 cornerSize, cornerSize,
    //                                 ! (flatOnLeft  || flatOnTop),
    //                                 ! (flatOnRight || flatOnTop),
    //                                 ! (flatOnLeft  || flatOnBottom),
    //                                 ! (flatOnRight || flatOnBottom));

    //         g.fillPath (path);

    //         g.setColour (button.findColour (ComboBox::outlineColourId));
    //         g.strokePath (path, PathStrokeType (1.0f));
    //     }
    //     else
    //     {
    //         g.fillRoundedRectangle (bounds, cornerSize);

    //         g.setColour (button.findColour (ComboBox::outlineColourId));
    //         g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    //     }
    // }

    // void drawToggleButton (Graphics& g, ToggleButton& button,
    //                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    // {

    //     // button.setColour(ToggleButton::textColourId, Colours::linen);
    //     // button.setColour(ToggleButton::tickColourId, Colours::linen);

    //     auto fontSize = jmin (15.0f, (float) button.getHeight() * 0.75f);
    //     auto tickWidth = fontSize * 1.1f;

    //     drawTickBox (g, button, 4.0f, ((float) button.getHeight() - tickWidth) * 0.5f,
    //                 tickWidth, tickWidth,
    //                 button.getToggleState(),
    //                 button.isEnabled(),
    //                 shouldDrawButtonAsHighlighted,
    //                 shouldDrawButtonAsDown);

    //     g.setColour (button.findColour (ToggleButton::textColourId));
    //     g.setFont (fontSize);

    //     if (! button.isEnabled())
    //         g.setOpacity (0.5f);

    //     g.drawFittedText (button.getButtonText(),
    //                     button.getLocalBounds().withTrimmedLeft (roundToInt (tickWidth) + 10)
    //                                             .withTrimmedRight (2),
    //                     Justification::centredLeft, 10);
    // }

private:

};

//=============================================================================
class PageComponent : public juce::Component
{
public:
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::unique_ptr<CustomAudioDeviceSelectorComponent> deviceSelector;
    std::unique_ptr<ComboBox> inputTypeCombo;
    std::unique_ptr<juce::Label> inputTypeLabel;

    void resized() override
    {
        jassert (controls.size() == labels.size());

        auto bounds = getLocalBounds();

        // Input type
        if (inputTypeCombo != nullptr && inputTypeLabel != nullptr)
        {
            auto w = inputTypeCombo->getWidth();
            auto h = inputTypeCombo->getHeight();

            auto zone = bounds.removeFromTop(25);
            auto comboZone = zone.removeFromRight(200);
            auto comboLabelZone = zone.removeFromLeft(150);

            inputTypeCombo->setBounds(comboZone);
            inputTypeLabel->setBounds(comboLabelZone);
            inputTypeCombo->setSize(w, h);
        }

        // Device selector and input type
        if (deviceSelector != nullptr)
        {
            auto h = deviceSelector->getHeight();
            deviceSelector->setBounds(bounds.removeFromTop(h));
        }

        int j = 0;
        juce::Rectangle<int> vSliderArea;
        juce::Rectangle<int> vSliderLabelArea;

        // Lay out controls
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto* label = labels[i].get();
            auto* ctrl = controls[i].get();

            // Skip items that are hidden
            if (!label->isVisible() || !ctrl->isVisible())
                continue;

            // Combos
            if (auto* control = dynamic_cast<ComboBox*>(ctrl))
            {
                // setBounds will change size, so store width and height
                auto w = ctrl->getWidth();
                auto h = ctrl->getHeight();

                auto zone = bounds.removeFromTop(25);

                auto comboZone = zone.removeFromRight(200);
                auto comboLabelZone = zone.removeFromLeft(150);

                label->setBounds(comboLabelZone);

                ctrl->setBounds(comboZone);
                
                ctrl->setSize(w, h);
            }

            // Sliders
            else if (auto* slider = dynamic_cast<Slider*>(ctrl))
            {
                // Vertical gain sliders
                if (slider->getSliderStyle() == Slider::SliderStyle::LinearVertical)
                {
                    if (j == 0)
                    {
                        vSliderArea = bounds.removeFromTop(150);
                        vSliderLabelArea = bounds.removeFromTop(25);
                        ++j;
                    }

                    int gainSpacing = ctrl->getWidth();

                    label->setBounds(vSliderLabelArea.removeFromLeft(gainSpacing));
                    label->setJustificationType(juce::Justification::centred);

                    ctrl->setBounds(vSliderArea.removeFromLeft(gainSpacing));
                    ctrl->setSize(gainSpacing, 150);
                }

                // Horizontal sliders
                else
                {
                    auto zone = bounds.removeFromTop(25);

                    auto sliderZone = zone.removeFromRight(200);
                    auto sliderLabelZone = zone.removeFromLeft(150);

                    label->setBounds(sliderLabelZone);

                    ctrl->setBounds(sliderZone);
                    ctrl->setSize(200, 25);
                }
            }

            bounds.removeFromTop(12);
        }
    }
};

//=============================================================================
/*  This is the component for the settings widget.
*/
class SettingsComponent : public juce::Component
{
public:
    //=========================================================================
    explicit SettingsComponent(MainController&);
    ~SettingsComponent() override;

    //=========================================================================
    
    void resized(void) override;

    int oldDeviceSelectorHeight = 0;

    void updateParamVisibility(int numTracksIn, bool threeDimIn);

    std::unordered_map<juce::String, juce::Component*> parameterComponentMap;
    std::unordered_map<juce::String, juce::Label*> parameterLabelMap;
    int numTracks = 1;
    int dim = 1;

    epicLookAndFeel epicLookAndFeel;

private:
    //=========================================================================
    MainController& controller;

    // Title label subcomponent
    juce::Label title;

    // Record button
    std::unique_ptr<juce::ToggleButton> recordButton;

    // Label pointers
    std::vector<std::unique_ptr<juce::Label>> labels;

    // Attachment pointers
    std::vector<std::unique_ptr<apvts::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<apvts::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<apvts::ButtonAttachment>> buttonAttachments;

    bool initialized = false;

    std::unique_ptr<juce::TabbedComponent> tabs;
    std::unique_ptr<PageComponent> ioPage;
    std::unique_ptr<PageComponent> visualPage;
    std::unique_ptr<PageComponent> analysisPage;
    std::unique_ptr<PageComponent> colorsPage;

    //=========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};

//=============================================================================
class SettingsWindow : public juce::DocumentWindow
{
public:
    SettingsWindow(MainController& controller)
        : DocumentWindow("Settings",
                         juce::Colour::fromRGB(30, 30, 30),
                         DocumentWindow::allButtons,
                         true)   // addToDesktop
    {
        setUsingNativeTitleBar(true);

        // Create the settings component
        settings = std::make_unique<SettingsComponent>(controller);

        setResizable(true, true);
        setContentOwned(settings.get(), true);

        centreWithSize(400, 600);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    std::unique_ptr<SettingsComponent> settings;
};