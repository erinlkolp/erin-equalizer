#pragma once
#include "EqProcessor.h"
#include "EqConstants.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class ErinEqProcessor : public juce::AudioProcessor
{
public:
    ErinEqProcessor();
    ~ErinEqProcessor() override = default;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    EqProcessor& getEqProcessor() { return eqProcessor; }
    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

    static juce::String getBandParamId(int bandIndex);
    static juce::String getBandBypassParamId(int bandIndex);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncParametersToEqProcessor();

    EqProcessor eqProcessor;
    juce::AudioProcessorValueTreeState apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErinEqProcessor)
};
