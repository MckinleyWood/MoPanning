#include "SettingsComponent.h"
#include "MainController.h"

//=============================================================================
SettingsComponent::SettingsComponent(MainController& c) : controller(c)
{
    content = std::make_unique<SettingsContentComponent>(controller);
    viewport.setViewedComponent(content.get(), true);
    addAndMakeVisible(viewport);
}

//=============================================================================
SettingsComponent::~SettingsComponent() = default;

//=============================================================================
void SettingsComponent::resized() 
{
    viewport.setBounds(getLocalBounds());

    const int titleHeight = 60;
    const int deviceSelectorHeight = content->getDeviceSelectorHeight();
    const int rowHeight = 50;
    const int numSettings = (int)content->getUIObjects().size();
    const int contentHeight = titleHeight + deviceSelectorHeight
                            + rowHeight * numSettings + 20;
    int contentWidth = getWidth();

    // if (contentHeight > getHeight())
        contentWidth = getWidth() - 8; // Leave some space for scrollbar

    if (content)
        content->setSize(contentWidth, contentHeight);
}

//=============================================================================
using sc = SettingsComponent;

sc::SettingsContentComponent::SettingsContentComponent(MainController& c) 
                                                       : controller(c)
{
    using Font = juce::FontOptions;

    // Set the font
    Font normalFont = {13.f, 0};
    Font titleFont = normalFont.withHeight(40).withStyle("Bold");

    // Set up title label
    title.setText("MoPanning", juce::dontSendNotification);
    title.setFont(titleFont);
    title.setJustificationType(juce::Justification::centred);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    // Set up device selector
    deviceSelector = std::make_unique<CustomAudioDeviceSelectorComponent>(
        controller.getDeviceManager(),
        2, 2, 2, 2,
        false, false, true, true);

    deviceSelector.get()->onHeightChanged = [this]()
    {
        if (auto* parent = findParentComponentOfClass<SettingsComponent>())
        {
            parent->resized();
        }
    };

    addAndMakeVisible(deviceSelector.get());

    // Set up parameter controls
    auto parameters = controller.getParameterDescriptors();
    auto& apvts = controller.getAPVTS();
    for (const auto& p : parameters)
    {
        if (p.type == ParameterDescriptor::Float)
        {
            auto* slider = new NonScrollingSlider(p.displayName);
            slider->setRange(p.range.start, p.range.end);
            slider->setValue(p.defaultValue);
            slider->setTextValueSuffix(p.unit);
            addAndMakeVisible(slider);
            uiObjects.push_back(slider);

            auto attachment = std::make_unique<apvts::SliderAttachment>(
                apvts, p.id, *slider);
            sliderAttachments.push_back(std::move(attachment));
            
        }
        else if (p.type == ParameterDescriptor::Choice)
        {
            auto* combo = new juce::ComboBox(p.displayName);
            combo->addItemList(p.choices, 1); // JUCE items start at index 1
            combo->setSelectedItemIndex(static_cast<int>(p.defaultValue));
            addAndMakeVisible(combo);
            uiObjects.push_back(combo);

            auto attachment = std::make_unique<apvts::ComboBoxAttachment>(
                apvts, p.id, *combo);
            comboAttachments.push_back(std::move(attachment));
        }

        auto* label = new juce::Label();
        label->setText(p.displayName, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::left);
        label->setFont(normalFont);
        addAndMakeVisible(label);
        labels.push_back(label);
    }

    initialized = true;
}

sc::SettingsContentComponent::~SettingsContentComponent() = default;

void sc::SettingsContentComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    const int rowHeight = 50;
    const int labelHeight = 14;
    int numSettings = (int)uiObjects.size();

    // Layout the title at the top
    auto titleZone = bounds.removeFromTop(60);
    title.setBounds(titleZone);

    // Layout the device selector below the title
    auto deviceSelectorZone = bounds.removeFromTop(
         getDeviceSelectorHeight());
    deviceSelector->setBounds(deviceSelectorZone);

    // Layout each setting
    // auto settings = getSettings();
    // auto labels = getLabels();

    for (int i = 0; i < numSettings; ++i)
    {
        auto row = bounds.removeFromTop(rowHeight);

        auto labelZone = row.removeFromTop(labelHeight);
        labels[i]->setBounds(labelZone);

        // settings[i]->setBounds(row.reduced(0, 4));
        uiObjects[i]->setBounds(row.reduced(0, 4));
    }
}

void sc::SettingsContentComponent::paint(juce::Graphics& g)
{
    // Paint the background
    g.fillAll(juce::Colours::darkgrey);

    // Paint boxes around all components (for testing)
    // g.setColour(juce::Colours::yellow);
    // g.drawRect(title.getBounds());
    // g.drawRect(deviceSelector->getBounds());
    // for (const auto& comp : uiObjects)
    // {
    //     g.drawRect(comp->getBounds());
    // }
}

//=============================================================================
int SettingsComponent::SettingsContentComponent::getDeviceSelectorHeight() const
{
    return deviceSelector ? deviceSelector->getHeight() - 30 : 0;
}