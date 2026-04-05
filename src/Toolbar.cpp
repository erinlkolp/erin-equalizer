#include "Toolbar.h"

Toolbar::Toolbar(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
    // Bypass toggle
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2a5a2a));
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff8ce8a8));
    bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e3a));
    bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6a6a9a));
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "masterBypass", bypassButton);

    // Analyzer toggle
    analyzerButton.setClickingTogglesState(true);
    analyzerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1e3a5f));
    analyzerButton.setColour(juce::TextButton::textColourOnId, juce::Colour(EqConstants::accentColor));
    analyzerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e3a));
    analyzerButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6a6a9a));
    addAndMakeVisible(analyzerButton);
    analyzerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "analyzerOn", analyzerButton);

    // Reset button
    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e3a));
    resetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6a6a9a));
    resetButton.onClick = [this]()
    {
        if (onResetAll)
            onResetAll();
    };
    addAndMakeVisible(resetButton);

    // Preset selector
    presetSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e1e3a));
    presetSelector.setColour(juce::ComboBox::textColourId, juce::Colour(0xff8a8aba));
    presetSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3a6a));
    presetSelector.addItem("Flat", 1);
    presetSelector.addItem("Low Cut", 2);
    presetSelector.addItem("High Cut", 3);
    presetSelector.addItem("Smiley Curve", 4);
    presetSelector.addItem("Vocal Presence", 5);
    presetSelector.addItem("Bass Boost", 6);
    presetSelector.addSeparator();
    presetSelector.addItem("Save Preset...", 100);
    presetSelector.setSelectedId(1, juce::dontSendNotification);
    presetSelector.onChange = [this]()
    {
        const int id = presetSelector.getSelectedId();
        if (id == 100 && onSavePreset)
        {
            onSavePreset();
            return;
        }
        if (onPresetSelected)
            onPresetSelected(presetSelector.getText());
    };
    addAndMakeVisible(presetSelector);

    // Output gain knob
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    outputGainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(EqConstants::accentColor));
    outputGainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff3a3a6a));
    outputGainSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(EqConstants::textMidColor));
    outputGainSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "outputGain", outputGainSlider);

    outputGainLabel.setText("OUT", juce::dontSendNotification);
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5a5a8a));
    outputGainLabel.setFont(10.0f);
    addAndMakeVisible(outputGainLabel);
}

void Toolbar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(EqConstants::toolbarBgColor));
    g.setColour(juce::Colour(EqConstants::borderColor));
    g.drawLine(0.0f, static_cast<float>(getHeight()), static_cast<float>(getWidth()),
               static_cast<float>(getHeight()), 1.0f);

    // Logo
    g.setColour(juce::Colour(EqConstants::accentColor));
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText("ERIN EQ", 12, 0, 80, getHeight(), juce::Justification::centredLeft);

    // Separator
    g.setColour(juce::Colour(EqConstants::borderColor));
    g.drawLine(96.0f, 8.0f, 96.0f, static_cast<float>(getHeight() - 8), 1.0f);
}

void Toolbar::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromLeft(96); // logo space

    const int buttonW = 70;
    const int buttonH = 24;
    const int pad = 6;
    const int y = (area.getHeight() - buttonH) / 2;

    bypassButton.setBounds(area.getX() + pad, y, buttonW, buttonH);
    analyzerButton.setBounds(bypassButton.getRight() + pad, y, buttonW + 10, buttonH);
    resetButton.setBounds(analyzerButton.getRight() + pad, y, 55, buttonH);

    // Right-aligned controls
    const int rightX = area.getRight();
    outputGainSlider.setBounds(rightX - 130, y - 2, 130, buttonH + 4);
    outputGainLabel.setBounds(outputGainSlider.getX() - 28, y, 28, buttonH);
    presetSelector.setBounds(outputGainLabel.getX() - 140, y, 130, buttonH);
}
