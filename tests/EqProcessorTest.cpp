#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "EqProcessor.h"
#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
    void fillWithSine(juce::AudioBuffer<float>& buffer, float freqHz, double sampleRate,
                      int startSample = 0)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float phase = 2.0f * static_cast<float>(std::numbers::pi) * freqHz
                                    * static_cast<float>(startSample + i)
                                    / static_cast<float>(sampleRate);
                data[i] = std::sin(phase);
            }
        }
    }

    float measureRms(const juce::AudioBuffer<float>& buffer, int channel)
    {
        double sum = 0.0;
        const auto* data = buffer.getReadPointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            sum += static_cast<double>(data[i]) * data[i];
        return static_cast<float>(std::sqrt(sum / buffer.getNumSamples()));
    }
}

TEST_CASE("EqProcessor: flat EQ passes signal unchanged", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer(2, blockSize);

    for (int block = 0; block < 20; ++block)
    {
        fillWithSine(buffer, 1000.0f, sampleRate, block * blockSize);
        eq.process(buffer);
    }

    juce::AudioBuffer<float> inputRef(2, blockSize);
    fillWithSine(inputRef, 1000.0f, sampleRate, 20 * blockSize);
    fillWithSine(buffer, 1000.0f, sampleRate, 20 * blockSize);
    eq.process(buffer);

    const float inputRms = measureRms(inputRef, 0);
    const float outputRms = measureRms(buffer, 0);
    const float gainDb = 20.0f * std::log10(outputRms / inputRms);

    REQUIRE_THAT(gainDb, WithinAbs(0.0, 0.5));
}

TEST_CASE("EqProcessor: band boost increases level at that frequency", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);
    eq.setBandGain(17, 10.0f);

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer(2, blockSize);

    for (int block = 0; block < 40; ++block)
    {
        fillWithSine(buffer, 1000.0f, sampleRate, block * blockSize);
        eq.process(buffer);
    }

    juce::AudioBuffer<float> inputRef(2, blockSize);
    fillWithSine(inputRef, 1000.0f, sampleRate, 40 * blockSize);
    fillWithSine(buffer, 1000.0f, sampleRate, 40 * blockSize);
    eq.process(buffer);

    const float inputRms = measureRms(inputRef, 0);
    const float outputRms = measureRms(buffer, 0);
    const float gainDb = 20.0f * std::log10(outputRms / inputRms);

    REQUIRE(gainDb > 8.0f);
    REQUIRE(gainDb < 12.0f);
}

TEST_CASE("EqProcessor: bypassed band has no effect", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);
    eq.setBandGain(17, 15.0f);
    eq.setBandBypassed(17, true);

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer(2, blockSize);

    for (int block = 0; block < 20; ++block)
    {
        fillWithSine(buffer, 1000.0f, sampleRate, block * blockSize);
        eq.process(buffer);
    }

    juce::AudioBuffer<float> inputRef(2, blockSize);
    fillWithSine(inputRef, 1000.0f, sampleRate, 20 * blockSize);
    fillWithSine(buffer, 1000.0f, sampleRate, 20 * blockSize);
    eq.process(buffer);

    const float inputRms = measureRms(inputRef, 0);
    const float outputRms = measureRms(buffer, 0);
    const float gainDb = 20.0f * std::log10(outputRms / inputRms);

    REQUIRE_THAT(gainDb, WithinAbs(0.0, 0.5));
}

TEST_CASE("EqProcessor: output gain scales signal", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);
    eq.setOutputGain(-6.0f);

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer(2, blockSize);

    for (int block = 0; block < 20; ++block)
    {
        fillWithSine(buffer, 1000.0f, sampleRate, block * blockSize);
        eq.process(buffer);
    }

    juce::AudioBuffer<float> inputRef(2, blockSize);
    fillWithSine(inputRef, 1000.0f, sampleRate, 20 * blockSize);
    fillWithSine(buffer, 1000.0f, sampleRate, 20 * blockSize);
    eq.process(buffer);

    const float inputRms = measureRms(inputRef, 0);
    const float outputRms = measureRms(buffer, 0);
    const float gainDb = 20.0f * std::log10(outputRms / inputRms);

    REQUIRE_THAT(gainDb, WithinAbs(-6.0, 1.0));
}

TEST_CASE("EqProcessor: gain snaps to 1 dB increments", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);

    eq.setBandGain(0, 3.7f);
    REQUIRE_THAT(eq.getBandGain(0), WithinAbs(4.0, 0.01));

    eq.setBandGain(0, -2.3f);
    REQUIRE_THAT(eq.getBandGain(0), WithinAbs(-2.0, 0.01));

    eq.setBandGain(0, 0.5f);
    REQUIRE_THAT(eq.getBandGain(0), WithinAbs(1.0, 0.01));
}

TEST_CASE("EqProcessor: level metering reports non-zero for signal", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    fillWithSine(buffer, 1000.0f, 44100.0);
    eq.process(buffer);

    REQUIRE(eq.inputLevelLeft.load() > 0.0f);
    REQUIRE(eq.outputLevelLeft.load() > 0.0f);
}
