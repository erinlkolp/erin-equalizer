#pragma once
#include <cmath>
#include <numbers>

class BiquadFilter
{
public:
    BiquadFilter() = default;

    void setParameters(float frequency, float q, float gainDb, double sampleRate)
    {
        if (frequency == currentFreq && q == currentQ
            && gainDb == currentGainDb && sampleRate == currentSampleRate)
            return;

        currentFreq = frequency;
        currentQ = q;
        currentGainDb = gainDb;
        currentSampleRate = sampleRate;

        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * static_cast<float>(std::numbers::pi) * frequency
                         / static_cast<float>(sampleRate);
        const float sinW0 = std::sin(w0);
        const float cosW0 = std::cos(w0);
        const float alpha = sinW0 / (2.0f * q);

        const float b0 = 1.0f + alpha * A;
        const float b1 = -2.0f * cosW0;
        const float b2 = 1.0f - alpha * A;
        const float a0 = 1.0f + alpha / A;
        const float a1 = -2.0f * cosW0;
        const float a2 = 1.0f - alpha / A;

        coeffB0 = b0 / a0;
        coeffB1 = b1 / a0;
        coeffB2 = b2 / a0;
        coeffA1 = a1 / a0;
        coeffA2 = a2 / a0;
    }

    float processSample(float input)
    {
        const float output = coeffB0 * input + coeffB1 * z1 + coeffB2 * z2
                             - coeffA1 * w1 - coeffA2 * w2;

        z2 = z1;
        z1 = input;
        w2 = w1;
        w1 = output;

        return output;
    }

    void reset()
    {
        z1 = z2 = w1 = w2 = 0.0f;
    }

private:
    float coeffB0 = 1.0f, coeffB1 = 0.0f, coeffB2 = 0.0f;
    float coeffA1 = 0.0f, coeffA2 = 0.0f;

    float z1 = 0.0f, z2 = 0.0f;
    float w1 = 0.0f, w2 = 0.0f;

    float currentFreq = 0.0f, currentQ = 0.0f;
    float currentGainDb = 0.0f;
    double currentSampleRate = 0.0;
};
