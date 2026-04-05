#include "LevelMeter.h"
#include "EqConstants.h"

LevelMeter::LevelMeter(const juce::String& label)
    : label(label)
{
    startTimerHz(30);
}

void LevelMeter::setLevel(float newLevel)
{
    currentLevel = newLevel;
}

void LevelMeter::timerCallback()
{
    if (currentLevel >= peakLevel)
    {
        peakLevel = currentLevel;
        peakHoldCounter = peakHoldFrames;
    }
    else if (peakHoldCounter > 0)
    {
        --peakHoldCounter;
    }
    else
    {
        peakLevel *= decayRate;
    }

    repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    const float labelH = 14.0f;
    const float meterTop = labelH + 4.0f;
    const float meterHeight = h - meterTop - 2.0f;
    const float meterX = (w - 10.0f) * 0.5f;
    const float meterW = 10.0f;

    g.setColour(juce::Colour(EqConstants::textDimColor));
    g.setFont(9.0f);
    g.drawText(label, 0, 0, static_cast<int>(w), static_cast<int>(labelH),
               juce::Justification::centred);

    g.setColour(juce::Colour(0xff111122));
    g.fillRoundedRectangle(meterX, meterTop, meterW, meterHeight, 2.0f);

    const float levelDb = currentLevel > 0.0001f
                              ? std::clamp(20.0f * std::log10(currentLevel), -60.0f, 0.0f)
                              : -60.0f;
    const float normalizedLevel = (levelDb + 60.0f) / 60.0f;
    const float barHeight = normalizedLevel * meterHeight;

    if (barHeight > 0.0f)
    {
        juce::ColourGradient gradient(
            juce::Colour(0xff2a8a4a), meterX, meterTop + meterHeight,
            juce::Colour(0xffe84a4a), meterX, meterTop, false);
        gradient.addColour(0.6, juce::Colour(0xff4ae88a));
        gradient.addColour(0.85, juce::Colour(0xffe8e84a));

        g.setGradientFill(gradient);
        g.fillRoundedRectangle(meterX, meterTop + meterHeight - barHeight,
                               meterW, barHeight, 2.0f);
    }

    if (peakLevel > 0.0001f)
    {
        const float peakDb = std::clamp(20.0f * std::log10(peakLevel), -60.0f, 0.0f);
        const float peakNorm = (peakDb + 60.0f) / 60.0f;
        const float peakY = meterTop + meterHeight - (peakNorm * meterHeight);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillRect(meterX, peakY, meterW, 1.5f);
    }
}
