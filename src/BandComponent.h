#pragma once
#include "EqConstants.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class BandComponent : public juce::Component
{
public:
    BandComponent(int bandIndex, juce::AudioProcessorValueTreeState& apvts);
    ~BandComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    int getBandIndex() const { return bandIndex; }
    float getCurrentGainDb() const;

    std::function<void(int bandIndex, float gainDb)> onHover;
    std::function<void(int bandIndex)> onSolo;

private:
    float yPositionToGainDb(float y) const;
    float gainDbToYPosition(float gainDb) const;

    int bandIndex;
    juce::String freqLabel;

    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Slider hiddenSlider;

    bool isHovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandComponent)
};
