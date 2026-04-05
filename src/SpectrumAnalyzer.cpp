#include "SpectrumAnalyzer.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer(EqProcessor& eqProcessor)
    : eqProcessor(eqProcessor)
{
    smoothedMagnitudes.fill(0.0f);
    setInterceptsMouseClicks(false, false);
}

void SpectrumAnalyzer::setActive(bool shouldBeActive)
{
    active = shouldBeActive;
    if (active && !isTimerRunning())
        startTimerHz(30);
    else if (!active && isTimerRunning())
    {
        stopTimer();
        smoothedMagnitudes.fill(0.0f);
        repaint();
    }
}

void SpectrumAnalyzer::timerCallback()
{
    std::array<float, fftSize> fifoData{};
    if (!eqProcessor.pullFifoData(fifoData))
        return;

    fftInput = fifoData;
    window.multiplyWithWindowingTable(fftInput.data(), static_cast<size_t>(fftSize));

    std::copy(fftInput.begin(), fftInput.end(), fftOutput.begin());
    std::fill(fftOutput.begin() + fftSize, fftOutput.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform(fftOutput.data());

    for (int i = 0; i < numDisplayBins; ++i)
    {
        const float normPos = static_cast<float>(i) / static_cast<float>(numDisplayBins);
        const float freq = 20.0f * std::pow(1000.0f, normPos);

        const float binFloat = freq * static_cast<float>(fftSize) / 44100.0f;
        const int bin = std::clamp(static_cast<int>(binFloat), 0, fftSize / 2 - 1);

        const float magnitude = fftOutput[static_cast<size_t>(bin)];
        const float magnitudeDb = magnitude > 0.0f
                                      ? std::clamp(20.0f * std::log10(magnitude), minDb, maxDb)
                                      : minDb;
        const float normalized = (magnitudeDb - minDb) / (maxDb - minDb);

        smoothedMagnitudes[static_cast<size_t>(i)] =
            smoothingFactor * smoothedMagnitudes[static_cast<size_t>(i)]
            + (1.0f - smoothingFactor) * normalized;
    }

    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    if (!active)
        return;

    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());

    juce::Path spectrumPath;
    spectrumPath.startNewSubPath(0.0f, h);

    for (int i = 0; i < numDisplayBins; ++i)
    {
        const float x = (static_cast<float>(i) / static_cast<float>(numDisplayBins)) * w;
        const float y = h - smoothedMagnitudes[static_cast<size_t>(i)] * h;
        spectrumPath.lineTo(x, y);
    }

    spectrumPath.lineTo(w, h);
    spectrumPath.closeSubPath();

    juce::ColourGradient gradient(
        juce::Colour(EqConstants::spectrumColor).withAlpha(0.0f), 0.0f, h,
        juce::Colour(EqConstants::spectrumColor).withAlpha(0.25f), 0.0f, 0.0f, false);

    g.setGradientFill(gradient);
    g.fillPath(spectrumPath);

    g.setColour(juce::Colour(EqConstants::spectrumColor).withAlpha(0.3f));
    g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
}
