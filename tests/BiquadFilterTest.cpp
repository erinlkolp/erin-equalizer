#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "BiquadFilter.h"
#include "EqConstants.h"
#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    float measureFilterGainDb(BiquadFilter& filter, float toneFreqHz, double sampleRate,
                              int numSamples = 8192)
    {
        const int settleSamples = 4096;
        for (int i = 0; i < settleSamples; ++i)
        {
            const float phase = 2.0f * static_cast<float>(std::numbers::pi) * toneFreqHz
                                * static_cast<float>(i) / static_cast<float>(sampleRate);
            filter.processSample(std::sin(phase));
        }

        double inputRms = 0.0;
        double outputRms = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const int sampleIdx = settleSamples + i;
            const float phase = 2.0f * static_cast<float>(std::numbers::pi) * toneFreqHz
                                * static_cast<float>(sampleIdx) / static_cast<float>(sampleRate);
            const float input = std::sin(phase);
            const float output = filter.processSample(input);

            inputRms += static_cast<double>(input) * input;
            outputRms += static_cast<double>(output) * output;
        }

        inputRms = std::sqrt(inputRms / numSamples);
        outputRms = std::sqrt(outputRms / numSamples);

        return 20.0f * static_cast<float>(std::log10(outputRms / inputRms));
    }
}

TEST_CASE("BiquadFilter: 0 dB gain passes signal unchanged", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(1000.0f, EqConstants::bandQ, 0.0f, 44100.0);
    const float gainDb = measureFilterGainDb(filter, 1000.0f, 44100.0);
    REQUIRE_THAT(gainDb, WithinAbs(0.0, 0.1));
}

TEST_CASE("BiquadFilter: +6 dB boost at center frequency", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(1000.0f, EqConstants::bandQ, 6.0f, 44100.0);
    const float gainDb = measureFilterGainDb(filter, 1000.0f, 44100.0);
    REQUIRE_THAT(gainDb, WithinAbs(6.0, 0.5));
}

TEST_CASE("BiquadFilter: -12 dB cut at center frequency", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(1000.0f, EqConstants::bandQ, -12.0f, 44100.0);
    const float gainDb = measureFilterGainDb(filter, 1000.0f, 44100.0);
    REQUIRE_THAT(gainDb, WithinAbs(-12.0, 0.5));
}

TEST_CASE("BiquadFilter: minimal effect far from center frequency", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(1000.0f, EqConstants::bandQ, 15.0f, 44100.0);
    const float gainDb = measureFilterGainDb(filter, 100.0f, 44100.0);
    REQUIRE(std::abs(gainDb) < 2.0f);
}

TEST_CASE("BiquadFilter: reset clears state", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(1000.0f, EqConstants::bandQ, 6.0f, 44100.0);
    for (int i = 0; i < 100; ++i)
        filter.processSample(1.0f);
    filter.reset();
    const float output = filter.processSample(0.0f);
    REQUIRE_THAT(output, WithinAbs(0.0, 1e-6));
}

TEST_CASE("BiquadFilter: works at 48 kHz", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(4000.0f, EqConstants::bandQ, 10.0f, 48000.0);
    const float gainDb = measureFilterGainDb(filter, 4000.0f, 48000.0);
    REQUIRE_THAT(gainDb, WithinAbs(10.0, 0.5));
}
