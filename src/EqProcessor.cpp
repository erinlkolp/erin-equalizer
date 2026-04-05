#include "EqProcessor.h"
#include <cmath>

EqProcessor::EqProcessor()
{
    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        bands[static_cast<size_t>(i)].targetGainDb = 0.0f;
        bands[static_cast<size_t>(i)].bypassed = false;
    }
}

void EqProcessor::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        auto& band = bands[static_cast<size_t>(i)];
        band.filterL.reset();
        band.filterR.reset();
        band.filterL.setParameters(EqConstants::bandFrequencies[static_cast<size_t>(i)],
                                   EqConstants::bandQ, band.targetGainDb, sampleRate);
        band.filterR.setParameters(EqConstants::bandFrequencies[static_cast<size_t>(i)],
                                   EqConstants::bandQ, band.targetGainDb, sampleRate);
        band.smoothedGain.reset(sampleRate, EqConstants::smoothingTimeMs / 1000.0);
        band.smoothedGain.setCurrentAndTargetValue(band.targetGainDb);
    }

    smoothedOutputGain.reset(sampleRate, EqConstants::smoothingTimeMs / 1000.0);
    smoothedOutputGain.setCurrentAndTargetValue(
        std::pow(10.0f, outputGainDb / 20.0f));

    fifoWriteIndex.store(0);
    fifoReady.store(false);

    juce::ignoreUnused(samplesPerBlock);
}

void EqProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    if (numChannels >= 1)
        inputLevelLeft.store(buffer.getMagnitude(0, 0, numSamples));
    if (numChannels >= 2)
        inputLevelRight.store(buffer.getMagnitude(1, 0, numSamples));

    for (int b = 0; b < EqConstants::numBands; ++b)
    {
        auto& band = bands[static_cast<size_t>(b)];
        if (band.bypassed)
            continue;
        if (soloedBand >= 0 && b != soloedBand)
            continue;

        const float smoothedGain = band.smoothedGain.skip(numSamples);
        band.filterL.setParameters(EqConstants::bandFrequencies[static_cast<size_t>(b)],
                                   EqConstants::bandQ, smoothedGain, currentSampleRate);
        band.filterR.setParameters(EqConstants::bandFrequencies[static_cast<size_t>(b)],
                                   EqConstants::bandQ, smoothedGain, currentSampleRate);

        if (std::abs(smoothedGain) < 0.01f && !band.smoothedGain.isSmoothing())
            continue;

        auto* left = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            left[i] = band.filterL.processSample(left[i]);

        if (numChannels >= 2)
        {
            auto* right = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i)
                right[i] = band.filterR.processSample(right[i]);
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = smoothedOutputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    if (numChannels >= 1)
        outputLevelLeft.store(buffer.getMagnitude(0, 0, numSamples));
    if (numChannels >= 2)
        outputLevelRight.store(buffer.getMagnitude(1, 0, numSamples));

    pushToFifo(buffer);
}

void EqProcessor::reset()
{
    for (auto& band : bands)
    {
        band.filterL.reset();
        band.filterR.reset();
    }
    inputLevelLeft.store(0.0f);
    inputLevelRight.store(0.0f);
    outputLevelLeft.store(0.0f);
    outputLevelRight.store(0.0f);
}

void EqProcessor::setBandGain(int bandIndex, float gainDb)
{
    if (bandIndex < 0 || bandIndex >= EqConstants::numBands)
        return;
    gainDb = std::round(gainDb);
    gainDb = std::clamp(gainDb, EqConstants::minGainDb, EqConstants::maxGainDb);
    auto& band = bands[static_cast<size_t>(bandIndex)];
    band.targetGainDb = gainDb;
    band.smoothedGain.setTargetValue(gainDb);
}

void EqProcessor::setBandBypassed(int bandIndex, bool bypassed)
{
    if (bandIndex < 0 || bandIndex >= EqConstants::numBands)
        return;
    bands[static_cast<size_t>(bandIndex)].bypassed = bypassed;
}

void EqProcessor::setOutputGain(float gainDb)
{
    outputGainDb = std::clamp(gainDb, -20.0f, 20.0f);
    smoothedOutputGain.setTargetValue(std::pow(10.0f, outputGainDb / 20.0f));
}

float EqProcessor::getBandGain(int bandIndex) const
{
    if (bandIndex < 0 || bandIndex >= EqConstants::numBands)
        return 0.0f;
    return bands[static_cast<size_t>(bandIndex)].targetGainDb;
}

bool EqProcessor::isBandBypassed(int bandIndex) const
{
    if (bandIndex < 0 || bandIndex >= EqConstants::numBands)
        return false;
    return bands[static_cast<size_t>(bandIndex)].bypassed;
}

void EqProcessor::setSoloBand(int bandIndex)
{
    soloedBand = (bandIndex >= 0 && bandIndex < EqConstants::numBands) ? bandIndex : -1;
}

int EqProcessor::getSoloBand() const
{
    return soloedBand;
}

void EqProcessor::pushToFifo(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    for (int i = 0; i < numSamples; ++i)
    {
        float monoSample = buffer.getSample(0, i);
        if (numChannels >= 2)
            monoSample = (monoSample + buffer.getSample(1, i)) * 0.5f;

        const int writeIdx = fifoWriteIndex.load();
        fifoBuffer[static_cast<size_t>(writeIdx)] = monoSample;

        if (writeIdx == fifoSize - 1)
        {
            fifoReady.store(true);
            fifoWriteIndex.store(0);
        }
        else
        {
            fifoWriteIndex.store(writeIdx + 1);
        }
    }
}

bool EqProcessor::pullFifoData(std::array<float, 4096>& dest)
{
    if (!fifoReady.load())
        return false;
    fifoReady.store(false);
    dest = fifoBuffer;
    return true;
}
