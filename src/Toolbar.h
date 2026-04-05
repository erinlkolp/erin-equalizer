#pragma once
#include "EqConstants.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class Toolbar : public juce::Component
{
public:
    Toolbar(juce::AudioProcessorValueTreeState& apvts);
    ~Toolbar() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onResetAll;
    std::function<void(const juce::String&)> onPresetSelected;
    std::function<void()> onSavePreset;

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton bypassButton{"BYPASS"};
    juce::TextButton analyzerButton{"ANALYZER"};
    juce::TextButton resetButton{"RESET"};
    juce::ComboBox presetSelector;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyzerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Toolbar)
};
