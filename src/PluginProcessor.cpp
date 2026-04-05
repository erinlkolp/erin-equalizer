#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::String ErinEqProcessor::getBandParamId(int bandIndex)
{
    return "band" + juce::String(bandIndex);
}

juce::String ErinEqProcessor::getBandBypassParamId(int bandIndex)
{
    return "band" + juce::String(bandIndex) + "_bypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout ErinEqProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        const float freq = EqConstants::bandFrequencies[static_cast<size_t>(i)];
        juce::String label;
        if (freq >= 1000.0f)
            label = juce::String(freq / 1000.0f, (freq >= 10000.0f) ? 0 : 1) + "k";
        else
            label = juce::String(freq, (freq == std::floor(freq)) ? 0 : 1);

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{getBandParamId(i), 1},
            label + " Hz",
            juce::NormalisableRange<float>(EqConstants::minGainDb,
                                           EqConstants::maxGainDb,
                                           EqConstants::gainStepDb),
            EqConstants::defaultGainDb));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{getBandBypassParamId(i), 1},
            label + " Hz Bypass",
            false));
    }

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1},
        "Output Gain",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"masterBypass", 1},
        "Master Bypass",
        false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"analyzerOn", 1},
        "Analyzer",
        false));

    return layout;
}

ErinEqProcessor::ErinEqProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ErinEqState", createParameterLayout())
{
}

void ErinEqProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    eqProcessor.prepare(sampleRate, samplesPerBlock);
    syncParametersToEqProcessor();
}

void ErinEqProcessor::releaseResources()
{
    eqProcessor.reset();
}

void ErinEqProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const bool bypassed = apvts.getRawParameterValue("masterBypass")->load() > 0.5f;
    if (bypassed)
        return;

    syncParametersToEqProcessor();
    eqProcessor.process(buffer);
}

void ErinEqProcessor::syncParametersToEqProcessor()
{
    for (int i = 0; i < EqConstants::numBands; ++i)
    {
        const float gain = apvts.getRawParameterValue(getBandParamId(i))->load();
        eqProcessor.setBandGain(i, gain);

        const bool bypass = apvts.getRawParameterValue(getBandBypassParamId(i))->load() > 0.5f;
        eqProcessor.setBandBypassed(i, bypass);
    }

    const float outGain = apvts.getRawParameterValue("outputGain")->load();
    eqProcessor.setOutputGain(outGain);
}

void ErinEqProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ErinEqProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* ErinEqProcessor::createEditor()
{
    return new ErinEqEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ErinEqProcessor();
}
