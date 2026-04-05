#pragma once
#include "EqProcessor.h"
#include "EqConstants.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    SpectrumAnalyzer(EqProcessor& eqProcessor);
    ~SpectrumAnalyzer() override = default;

    void paint(juce::Graphics&) override;
    void setActive(bool shouldBeActive);

private:
    void timerCallback() override;

    EqProcessor& eqProcessor;
    bool active = false;

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numDisplayBins = 256;

    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{
        static_cast<size_t>(fftSize), juce::dsp::WindowingFunction<float>::hann};

    std::array<float, fftSize> fftInput{};
    std::array<float, fftSize * 2> fftOutput{};
    std::array<float, numDisplayBins> smoothedMagnitudes{};

    static constexpr float smoothingFactor = 0.8f;
    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
