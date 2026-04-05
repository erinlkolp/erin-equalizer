#pragma once
#include "PluginProcessor.h"
#include "BandComponent.h"
#include "LevelMeter.h"
#include "Toolbar.h"
#include "EqConstants.h"
#include "SpectrumAnalyzer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class ErinEqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ErinEqEditor(ErinEqProcessor&);
    ~ErinEqEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void resetAllBands();
    void applyPreset(const juce::String& presetName);
    void saveUserPreset();
    void paintDbScale(juce::Graphics& g, juce::Rectangle<int> area);
    void paintStatusBar(juce::Graphics& g, juce::Rectangle<int> area);

    ErinEqProcessor& processorRef;

    Toolbar toolbar;
    std::array<std::unique_ptr<BandComponent>, EqConstants::numBands> bandComponents;
    LevelMeter inputMeter{"IN"};
    LevelMeter outputMeter{"OUT"};
    SpectrumAnalyzer spectrumAnalyzer;

    int hoveredBandIndex = -1;
    float hoveredBandGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErinEqEditor)
};
