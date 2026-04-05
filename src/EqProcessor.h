#pragma once
#include "BiquadFilter.h"
#include "EqConstants.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class EqProcessor
{
public:
    EqProcessor();

    void prepare(double sampleRate, int samplesPerBlock);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();

    void setBandGain(int bandIndex, float gainDb);
    void setBandBypassed(int bandIndex, bool bypassed);
    void setOutputGain(float gainDb);
    void setSoloBand(int bandIndex); // -1 to clear solo

    float getBandGain(int bandIndex) const;
    bool isBandBypassed(int bandIndex) const;
    int getSoloBand() const;

    bool pullFifoData(std::array<float, 4096>& dest);

    std::atomic<float> inputLevelLeft{0.0f};
    std::atomic<float> inputLevelRight{0.0f};
    std::atomic<float> outputLevelLeft{0.0f};
    std::atomic<float> outputLevelRight{0.0f};

private:
    void pushToFifo(const juce::AudioBuffer<float>& buffer);

    struct Band
    {
        BiquadFilter filterL;
        BiquadFilter filterR;
        juce::SmoothedValue<float> smoothedGain{0.0f};
        float targetGainDb = 0.0f;
        bool bypassed = false;
    };

    std::array<Band, EqConstants::numBands> bands;
    juce::SmoothedValue<float> smoothedOutputGain{1.0f};
    float outputGainDb = 0.0f;
    double currentSampleRate = 44100.0;
    int soloedBand = -1;

    static constexpr int fifoSize = 4096;
    std::array<float, fifoSize> fifoBuffer{};
    std::atomic<int> fifoWriteIndex{0};
    std::atomic<bool> fifoReady{false};
};
