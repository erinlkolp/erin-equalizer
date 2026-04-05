#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter(const juce::String& label);
    ~LevelMeter() override = default;

    void paint(juce::Graphics&) override;
    void setLevel(float newLevel);

private:
    void timerCallback() override;

    juce::String label;
    float currentLevel = 0.0f;
    float peakLevel = 0.0f;
    float peakDecay = 0.0f;
    int peakHoldCounter = 0;

    static constexpr int peakHoldFrames = 45;
    static constexpr float decayRate = 0.92f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
