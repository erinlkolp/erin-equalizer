#include "PluginEditor.h"

ErinEqEditor::ErinEqEditor(ErinEqProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      toolbar(p.getApvts()),
      spectrumAnalyzer(p.getEqProcessor())
{
    setSize(EqConstants::windowWidth, EqConstants::windowHeight);
    setResizable(false, false);

    addAndMakeVisible(toolbar);
    toolbar.onResetAll = [this]() { resetAllBands(); };
    toolbar.onPresetSelected = [this](const juce::String& name) { applyPreset(name); };
    toolbar.onSavePreset = [this]() { saveUserPreset(); };

    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        bandComponents[static_cast<size_t>(i)] =
            std::make_unique<BandComponent>(i, p.getApvts());

        bandComponents[static_cast<size_t>(i)]->onHover = [this](int idx, float gain)
        {
            hoveredBandIndex = (gain > -998.0f) ? idx : -1;
            hoveredBandGain = gain;
            repaint(0, getHeight() - EqConstants::statusBarHeight,
                    getWidth(), EqConstants::statusBarHeight);
        };

        bandComponents[static_cast<size_t>(i)]->onSolo = [this](int idx)
        {
            auto& eq = processorRef.getEqProcessor();
            eq.setSoloBand(eq.getSoloBand() == idx ? -1 : idx);
        };

        addAndMakeVisible(*bandComponents[static_cast<size_t>(i)]);
    }

    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    spectrumAnalyzer.setActive(false);
    addAndMakeVisible(spectrumAnalyzer);

    startTimerHz(30);
}

void ErinEqEditor::timerCallback()
{
    auto& eq = processorRef.getEqProcessor();
    inputMeter.setLevel(std::max(eq.inputLevelLeft.load(), eq.inputLevelRight.load()));
    outputMeter.setLevel(std::max(eq.outputLevelLeft.load(), eq.outputLevelRight.load()));

    const bool analyzerOn = processorRef.getApvts().getRawParameterValue("analyzerOn")->load() > 0.5f;
    spectrumAnalyzer.setActive(analyzerOn);
}

void ErinEqEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(EqConstants::bgColor));

    const int faderTop = EqConstants::toolbarHeight;
    const int faderBottom = getHeight() - EqConstants::statusBarHeight;
    paintDbScale(g, {0, faderTop, EqConstants::dbScaleWidth, faderBottom - faderTop});

    // 0 dB center line across fader area
    const float zeroY = static_cast<float>(faderTop)
                        + static_cast<float>(faderBottom - faderTop
                              - EqConstants::freqLabelHeight) * 0.5f;
    g.setColour(juce::Colour(EqConstants::zeroLineColor));
    g.drawLine(static_cast<float>(EqConstants::dbScaleWidth), zeroY,
               static_cast<float>(getWidth() - EqConstants::meterWidth), zeroY, 0.5f);

    paintStatusBar(g, {0, getHeight() - EqConstants::statusBarHeight,
                       getWidth(), EqConstants::statusBarHeight});
}

void ErinEqEditor::paintDbScale(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(8.0f);
    const float faderH = static_cast<float>(area.getHeight() - EqConstants::freqLabelHeight);

    const float dbValues[] = {15, 10, 5, 0, -5, -10, -15};
    for (float db : dbValues)
    {
        const float normalized = (db - EqConstants::minGainDb)
                                 / (EqConstants::maxGainDb - EqConstants::minGainDb);
        const float y = static_cast<float>(area.getY())
                        + faderH * (1.0f - normalized);

        g.setColour(db == 0.0f ? juce::Colour(0xff4a4a6a) : juce::Colour(EqConstants::textDimColor));
        juce::String text = (db > 0 ? "+" : "") + juce::String(static_cast<int>(db));
        g.drawText(text, area.getX(), static_cast<int>(y) - 5,
                   area.getWidth() - 2, 10, juce::Justification::centredRight);
    }
}

void ErinEqEditor::paintStatusBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(0xff0a0a16));
    g.fillRect(area);
    g.setColour(juce::Colour(0xff1a1a2e));
    g.drawLine(static_cast<float>(area.getX()), static_cast<float>(area.getY()),
               static_cast<float>(area.getRight()), static_cast<float>(area.getY()), 1.0f);

    g.setFont(9.0f);
    g.setColour(juce::Colour(0xff2a2a5a));

    if (hoveredBandIndex >= 0 && hoveredBandIndex < EqConstants::numBands)
    {
        const float freq = EqConstants::bandFrequencies[static_cast<size_t>(hoveredBandIndex)];
        juce::String freqStr;
        if (freq >= 1000.0f)
            freqStr = juce::String(freq / 1000.0f, 1) + " kHz";
        else
            freqStr = juce::String(freq, (freq == std::floor(freq)) ? 0 : 1) + " Hz";

        juce::String gainStr = (hoveredBandGain >= 0 ? "+" : "")
                               + juce::String(hoveredBandGain, 0) + " dB";

        g.drawText("Hover: " + freqStr + " | " + gainStr,
                   area.reduced(12, 0), juce::Justification::centredLeft);
    }

    g.drawText("Erin Equalizer v1.0",
               area.reduced(12, 0), juce::Justification::centredRight);
}

void ErinEqEditor::resized()
{
    auto area = getLocalBounds();

    toolbar.setBounds(area.removeFromTop(EqConstants::toolbarHeight));
    area.removeFromBottom(EqConstants::statusBarHeight);

    // Right meters
    auto meterArea = area.removeFromRight(EqConstants::meterWidth);
    const int meterW = EqConstants::meterWidth / 2;
    inputMeter.setBounds(meterArea.getX(), meterArea.getY(),
                         meterW, meterArea.getHeight());
    outputMeter.setBounds(meterArea.getX() + meterW, meterArea.getY(),
                          meterW, meterArea.getHeight());

    // Left dB scale (just painting, no component)
    area.removeFromLeft(EqConstants::dbScaleWidth);

    // Band faders
    const int bandAreaWidth = area.getWidth();
    const float bandWidth = static_cast<float>(bandAreaWidth) / EqConstants::numBands;

    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        const int x = area.getX() + static_cast<int>(bandWidth * static_cast<float>(i));
        const int w = static_cast<int>(bandWidth * static_cast<float>(i + 1))
                      - static_cast<int>(bandWidth * static_cast<float>(i));
        bandComponents[static_cast<size_t>(i)]->setBounds(x, area.getY(), w, area.getHeight());
    }

    // Spectrum overlay covers the fader area
    spectrumAnalyzer.setBounds(area);
}

void ErinEqEditor::resetAllBands()
{
    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        const juce::String paramId = ErinEqProcessor::getBandParamId(i);
        if (auto* param = processorRef.getApvts().getParameter(paramId))
            param->setValueNotifyingHost(param->getDefaultValue());

        const juce::String bypassId = ErinEqProcessor::getBandBypassParamId(i);
        if (auto* param = processorRef.getApvts().getParameter(bypassId))
            param->setValueNotifyingHost(0.0f);
    }

    if (auto* param = processorRef.getApvts().getParameter("outputGain"))
        param->setValueNotifyingHost(param->getDefaultValue());
}

void ErinEqEditor::applyPreset(const juce::String& presetName)
{
    resetAllBands();

    std::array<float, EqConstants::numBands> gains{};

    if (presetName == "Low Cut")
    {
        gains = {-15,-14,-12,-10,-7,-4,-2, 0, 0, 0, 0, 0, 0, 0, 0,
                  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }
    else if (presetName == "High Cut")
    {
        gains = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-4,-7,-10,-12,-14,-15};
    }
    else if (presetName == "Smiley Curve")
    {
        gains = {6, 5, 5, 4, 3, 3, 2, 1, 0, 0,-1,-2,-3,-3,-3,
                -3,-3,-2,-1, 0, 0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 6};
    }
    else if (presetName == "Vocal Presence")
    {
        gains = {-2,-2,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3,
                  4, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,-1,-1,-2,-2,-2};
    }
    else if (presetName == "Bass Boost")
    {
        gains = {8, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }

    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        if (gains[static_cast<size_t>(i)] != 0.0f)
        {
            const juce::String paramId = ErinEqProcessor::getBandParamId(i);
            if (auto* param = processorRef.getApvts().getParameter(paramId))
            {
                const float normalized = param->convertTo0to1(gains[static_cast<size_t>(i)]);
                param->setValueNotifyingHost(normalized);
            }
        }
    }
}

void ErinEqEditor::saveUserPreset()
{
    auto presetDir = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("Erin Audio")
        .getChildFile("Erin Equalizer")
        .getChildFile("Presets");

    presetDir.createDirectory();

    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Preset", presetDir, "*.xml");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        auto preset = std::make_unique<juce::XmlElement>("ErinEqPreset");
        preset->setAttribute("name", file.getFileNameWithoutExtension());

        auto* bandsXml = preset->createNewChildElement("Bands");
        for (int i = 0; i < EqConstants::numBands; ++i)
        {
            auto* bandXml = bandsXml->createNewChildElement("Band");
            bandXml->setAttribute("index", i);
            const float gain = processorRef.getApvts().getRawParameterValue(
                ErinEqProcessor::getBandParamId(i))->load();
            bandXml->setAttribute("gain", static_cast<int>(gain));
        }

        auto* outXml = preset->createNewChildElement("OutputGain");
        outXml->setAttribute("value",
            static_cast<int>(processorRef.getApvts().getRawParameterValue("outputGain")->load()));

        preset->writeTo(file);
    });
}
