#include "BandComponent.h"
#include "PluginProcessor.h"

BandComponent::BandComponent(int bandIndex, juce::AudioProcessorValueTreeState& apvts)
    : bandIndex(bandIndex), apvts(apvts)
{
    const float freq = EqConstants::bandFrequencies[static_cast<size_t>(bandIndex)];
    if (freq >= 1000.0f)
        freqLabel = juce::String(freq / 1000.0f, (freq >= 10000.0f) ? 0 : 1) + "k";
    else
        freqLabel = juce::String(freq, (freq == std::floor(freq)) ? 0 : 1);

    hiddenSlider.setRange(EqConstants::minGainDb, EqConstants::maxGainDb, EqConstants::gainStepDb);
    hiddenSlider.setValue(EqConstants::defaultGainDb);
    hiddenSlider.setVisible(false);
    addChildComponent(hiddenSlider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ErinEqProcessor::getBandParamId(bandIndex), hiddenSlider);
}

float BandComponent::getCurrentGainDb() const
{
    return static_cast<float>(hiddenSlider.getValue());
}

float BandComponent::yPositionToGainDb(float y) const
{
    const float faderTop = 0.0f;
    const float faderBottom = static_cast<float>(getHeight() - EqConstants::freqLabelHeight);
    const float faderHeight = faderBottom - faderTop;

    const float normalized = 1.0f - ((y - faderTop) / faderHeight);
    const float gainDb = EqConstants::minGainDb
                         + normalized * (EqConstants::maxGainDb - EqConstants::minGainDb);

    return std::round(std::clamp(gainDb, EqConstants::minGainDb, EqConstants::maxGainDb));
}

float BandComponent::gainDbToYPosition(float gainDb) const
{
    const float faderTop = 0.0f;
    const float faderBottom = static_cast<float>(getHeight() - EqConstants::freqLabelHeight);
    const float faderHeight = faderBottom - faderTop;

    const float normalized = (gainDb - EqConstants::minGainDb)
                             / (EqConstants::maxGainDb - EqConstants::minGainDb);

    return faderBottom - (normalized * faderHeight);
}

void BandComponent::paint(juce::Graphics& g)
{
    const float w = static_cast<float>(getWidth());
    const float faderBottom = static_cast<float>(getHeight() - EqConstants::freqLabelHeight);
    const float zeroY = gainDbToYPosition(0.0f);
    const float currentY = gainDbToYPosition(getCurrentGainDb());

    // Fader track
    const float trackX = w * 0.5f;
    g.setColour(juce::Colour(EqConstants::borderColor));
    g.drawLine(trackX, 0.0f, trackX, faderBottom, 1.0f);

    // 0 dB tick
    g.setColour(juce::Colour(EqConstants::zeroLineColor));
    g.drawLine(0.0f, zeroY, w, zeroY, 0.5f);

    // Fader bar (from zero to current position)
    if (std::abs(getCurrentGainDb()) > 0.1f)
    {
        juce::ColourGradient gradient(
            juce::Colour(EqConstants::faderGlowStart), trackX, faderBottom,
            juce::Colour(EqConstants::faderGlowEnd), trackX, 0.0f, false);

        const float barTop = std::min(zeroY, currentY);
        const float barBottom = std::max(zeroY, currentY);
        const float barWidth = w * 0.35f;

        g.setGradientFill(gradient);
        g.setOpacity(isHovered ? 0.9f : 0.7f);
        g.fillRect(trackX - barWidth * 0.5f, barTop, barWidth, barBottom - barTop);
    }

    // Fader handle
    const float handleW = w * 0.7f;
    const float handleH = 5.0f;
    g.setColour(isHovered ? juce::Colours::white : juce::Colour(0xffcccccc));
    g.fillRoundedRectangle(trackX - handleW * 0.5f, currentY - handleH * 0.5f,
                           handleW, handleH, 1.5f);

    // Glow on handle
    if (isHovered)
    {
        g.setColour(juce::Colour(EqConstants::faderGlowStart).withAlpha(0.3f));
        g.fillRoundedRectangle(trackX - handleW * 0.5f - 1.0f, currentY - handleH * 0.5f - 1.0f,
                               handleW + 2.0f, handleH + 2.0f, 2.0f);
    }

    // Frequency label
    g.setColour(juce::Colour(isHovered ? EqConstants::accentColor : EqConstants::textDimColor));
    g.setFont(8.0f);

    auto labelArea = juce::Rectangle<float>(0.0f, faderBottom + 2.0f, w,
                                            static_cast<float>(EqConstants::freqLabelHeight));

    juce::AffineTransform rotation = juce::AffineTransform::rotation(
        -0.785f, labelArea.getCentreX(), labelArea.getCentreY());

    g.addTransform(rotation);
    g.drawText(freqLabel, labelArea, juce::Justification::centred, false);
    g.addTransform(rotation.inverted());
}

void BandComponent::resized()
{
}

void BandComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        const juce::String bypassId = ErinEqProcessor::getBandBypassParamId(bandIndex);
        const bool isBypassed = apvts.getRawParameterValue(bypassId)->load() > 0.5f;

        menu.addItem(1, "Bypass", true, isBypassed);
        menu.addItem(2, "Solo", true, false);
        menu.showMenuAsync(juce::PopupMenu::Options(), [this, bypassId, isBypassed](int result)
        {
            if (result == 1)
            {
                if (auto* param = apvts.getParameter(bypassId))
                    param->setValueNotifyingHost(isBypassed ? 0.0f : 1.0f);
            }
            else if (result == 2 && onSolo)
            {
                onSolo(bandIndex);
            }
        });
        return;
    }

    const float newGain = yPositionToGainDb(static_cast<float>(e.y));
    hiddenSlider.setValue(newGain, juce::sendNotificationSync);
    repaint();
}

void BandComponent::mouseDrag(const juce::MouseEvent& e)
{
    const float newGain = yPositionToGainDb(static_cast<float>(e.y));
    hiddenSlider.setValue(newGain, juce::sendNotificationSync);
    repaint();

    if (onHover)
        onHover(bandIndex, getCurrentGainDb());
}

void BandComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    hiddenSlider.setValue(0.0, juce::sendNotificationSync);
    repaint();
}

void BandComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
    if (onHover)
        onHover(bandIndex, getCurrentGainDb());
}

void BandComponent::mouseExit(const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
    if (onHover)
        onHover(bandIndex, -999.0f);
}
