# Erin Equalizer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a 31-band 1/3 octave graphic equalizer as a VST3 plugin and standalone app for Windows, Linux, and macOS.

**Architecture:** Component-based with thin JUCE shells. DSP layer (BiquadFilter → EqProcessor → PluginProcessor) processes audio through 31 serial IIR biquad filters. UI layer (BandComponent, LevelMeter, Toolbar, SpectrumAnalyzer → PluginEditor) communicates with DSP through JUCE's `AudioParameterFloat` system.

**Tech Stack:** C++17, JUCE 7.x (CMake FetchContent), Catch2 v3 (testing), CMake 3.22+

---

## File Map

| File | Responsibility |
|------|---------------|
| `CMakeLists.txt` | Build config: JUCE + Catch2 fetch, plugin + test targets |
| `src/EqConstants.h` | Band frequencies, Q factor, gain range, UI color constants |
| `src/BiquadFilter.h` | Single peaking EQ biquad — header-only, no JUCE dependency |
| `src/EqProcessor.h` / `.cpp` | 31-band filter chain, parameter smoothing, bypass, output gain, FIFO |
| `src/PluginProcessor.h` / `.cpp` | JUCE AudioProcessor shell: parameters, state serialization, audio routing |
| `src/PluginEditor.h` / `.cpp` | JUCE AudioProcessorEditor shell: layout and child component wiring |
| `src/BandComponent.h` / `.cpp` | Reusable vertical fader widget (x31) |
| `src/LevelMeter.h` / `.cpp` | Vertical bar meter with peak hold |
| `src/Toolbar.h` / `.cpp` | Top bar: bypass, analyzer toggle, reset, presets, output gain |
| `src/SpectrumAnalyzer.h` / `.cpp` | FFT computation (FIFO consumer) + semi-transparent overlay renderer |
| `tests/BiquadFilterTest.cpp` | BiquadFilter unit tests — sine tone through filter, measure gain |
| `tests/EqProcessorTest.cpp` | EqProcessor integration tests — multi-band processing, bypass, output gain |
| `presets/factory/Flat.xml` | Factory preset: all bands 0 dB |
| `presets/factory/LowCut.xml` | Factory preset |
| `presets/factory/HighCut.xml` | Factory preset |
| `presets/factory/SmileyCurve.xml` | Factory preset |
| `presets/factory/VocalPresence.xml` | Factory preset |
| `presets/factory/BassBoost.xml` | Factory preset |

---

### Task 1: Project Scaffolding

Set up the CMake build system with JUCE and Catch2 via FetchContent, and a minimal plugin skeleton that compiles and runs as standalone.

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/PluginProcessor.h`
- Create: `src/PluginProcessor.cpp`
- Create: `src/PluginEditor.h`
- Create: `src/PluginEditor.cpp`

- [ ] **Step 1: Create CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(ErinEqualizer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

juce_add_plugin(ErinEqualizer
    COMPANY_NAME "Erin Audio"
    PLUGIN_MANUFACTURER_CODE Erin
    PLUGIN_CODE EqGr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Erin Equalizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_include_directories(ErinEqualizer PRIVATE src)

target_compile_definitions(ErinEqualizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ErinEqualizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Test target
add_executable(ErinEqualizerTests
    tests/BiquadFilterTest.cpp
)

target_include_directories(ErinEqualizerTests PRIVATE src)

target_link_libraries(ErinEqualizerTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_dsp
)

include(CTest)
include(Catch)
catch_discover_tests(ErinEqualizerTests)
```

- [ ] **Step 2: Create minimal PluginProcessor**

`src/PluginProcessor.h`:
```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ErinEqProcessor : public juce::AudioProcessor
{
public:
    ErinEqProcessor();
    ~ErinEqProcessor() override = default;

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

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErinEqProcessor)
};
```

`src/PluginProcessor.cpp`:
```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

ErinEqProcessor::ErinEqProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ErinEqProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}
void ErinEqProcessor::releaseResources() {}

void ErinEqProcessor::processBlock(juce::AudioBuffer<float>& /*buffer*/, juce::MidiBuffer& /*midiMessages*/)
{
}

juce::AudioProcessorEditor* ErinEqProcessor::createEditor()
{
    return new ErinEqEditor(*this);
}

void ErinEqProcessor::getStateInformation(juce::MemoryBlock& /*destData*/) {}
void ErinEqProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ErinEqProcessor();
}
```

- [ ] **Step 3: Create minimal PluginEditor**

`src/PluginEditor.h`:
```cpp
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ErinEqEditor : public juce::AudioProcessorEditor
{
public:
    explicit ErinEqEditor(ErinEqProcessor&);
    ~ErinEqEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ErinEqProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErinEqEditor)
};
```

`src/PluginEditor.cpp`:
```cpp
#include "PluginEditor.h"

ErinEqEditor::ErinEqEditor(ErinEqProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(900, 500);
    setResizable(false, false);
}

void ErinEqEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d0d1a));
    g.setColour(juce::Colour(0xff7cb3e8));
    g.setFont(20.0f);
    g.drawText("Erin Equalizer", getLocalBounds(), juce::Justification::centred);
}

void ErinEqEditor::resized() {}
```

- [ ] **Step 4: Create placeholder test file**

`tests/BiquadFilterTest.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Placeholder compiles", "[scaffold]")
{
    REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ErinEqualizer_Standalone
cmake --build build --target ErinEqualizerTests
cd build && ctest --output-on-failure
```

Expected: Plugin builds successfully. Standalone launches with a dark window showing "Erin Equalizer". Test passes.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/ tests/
git commit -m "feat: scaffold JUCE project with CMake, Catch2, and minimal plugin skeleton"
```

---

### Task 2: EqConstants and BiquadFilter

Implement the constants header and the core biquad filter with full test coverage. BiquadFilter is header-only, pure C++ (no JUCE dependency) for easy testing.

**Files:**
- Create: `src/EqConstants.h`
- Create: `src/BiquadFilter.h`
- Modify: `tests/BiquadFilterTest.cpp`

- [ ] **Step 1: Create EqConstants.h**

```cpp
#pragma once
#include <array>
#include <cstdint>

namespace EqConstants
{
    constexpr int numBands = 31;

    constexpr std::array<float, numBands> bandFrequencies = {
        20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f,
        125.0f, 160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f,
        630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f, 2000.0f, 2500.0f,
        3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f,
        12500.0f, 16000.0f, 20000.0f
    };

    constexpr float bandQ = 4.318f; // 1/3 octave bandwidth
    constexpr float minGainDb = -15.0f;
    constexpr float maxGainDb = 15.0f;
    constexpr float gainStepDb = 1.0f;
    constexpr float defaultGainDb = 0.0f;
    constexpr float smoothingTimeMs = 20.0f;

    // UI constants
    constexpr int windowWidth = 900;
    constexpr int windowHeight = 500;
    constexpr int toolbarHeight = 44;
    constexpr int dbScaleWidth = 28;
    constexpr int meterWidth = 40;
    constexpr int freqLabelHeight = 28;
    constexpr int statusBarHeight = 24;

    // UI colors (ARGB)
    constexpr uint32_t bgColor = 0xff0d0d1a;
    constexpr uint32_t toolbarBgColor = 0xff1a1a2e;
    constexpr uint32_t accentColor = 0xff7cb3e8;
    constexpr uint32_t faderGlowStart = 0xff00c9ff;
    constexpr uint32_t faderGlowEnd = 0xff92fe9d;
    constexpr uint32_t textDimColor = 0xff3a3a6a;
    constexpr uint32_t textMidColor = 0xff5a8ab5;
    constexpr uint32_t borderColor = 0xff2a2a4a;
    constexpr uint32_t zeroLineColor = 0xff2a2a4a;
    constexpr uint32_t spectrumColor = 0xff00c9ff;
}
```

- [ ] **Step 2: Create BiquadFilter.h**

Header-only implementation using Audio EQ Cookbook peaking EQ formula:

```cpp
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

        // Normalize by a0
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

    float z1 = 0.0f, z2 = 0.0f; // input delay line
    float w1 = 0.0f, w2 = 0.0f; // output delay line

    float currentFreq = 0.0f, currentQ = 0.0f;
    float currentGainDb = 0.0f;
    double currentSampleRate = 0.0;
};
```

- [ ] **Step 3: Write BiquadFilter tests**

Replace `tests/BiquadFilterTest.cpp`:

```cpp
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
    // Generate a sine tone at the given frequency, process through filter, measure RMS of output
    float measureFilterGainDb(BiquadFilter& filter, float toneFreqHz, double sampleRate,
                              int numSamples = 8192)
    {
        // Let the filter settle for a few cycles first
        const int settleSamples = 4096;
        for (int i = 0; i < settleSamples; ++i)
        {
            const float phase = 2.0f * static_cast<float>(std::numbers::pi) * toneFreqHz
                                * static_cast<float>(i) / static_cast<float>(sampleRate);
            filter.processSample(std::sin(phase));
        }

        // Measure RMS of input and output over numSamples
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

    // Tone at 100 Hz should be largely unaffected by a 1 kHz boost
    const float gainDb = measureFilterGainDb(filter, 100.0f, 44100.0);
    REQUIRE(std::abs(gainDb) < 2.0f);
}

TEST_CASE("BiquadFilter: reset clears state", "[biquad]")
{
    BiquadFilter filter;
    filter.setParameters(1000.0f, EqConstants::bandQ, 6.0f, 44100.0);

    // Process some audio
    for (int i = 0; i < 100; ++i)
        filter.processSample(1.0f);

    filter.reset();

    // After reset, processing silence should produce silence
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cmake --build build --target ErinEqualizerTests
cd build && ctest --output-on-failure
```

Expected: All 6 test cases pass.

- [ ] **Step 5: Commit**

```bash
git add src/EqConstants.h src/BiquadFilter.h tests/BiquadFilterTest.cpp
git commit -m "feat: add EqConstants and BiquadFilter with unit tests"
```

---

### Task 3: EqProcessor

Implement the 31-band filter chain with parameter smoothing, per-band bypass, output gain, and a FIFO ring buffer for spectrum analysis.

**Files:**
- Create: `src/EqProcessor.h`
- Create: `src/EqProcessor.cpp`
- Create: `tests/EqProcessorTest.cpp`
- Modify: `CMakeLists.txt` (add EqProcessor.cpp to plugin sources, EqProcessorTest.cpp to test sources)

- [ ] **Step 1: Create EqProcessor header**

`src/EqProcessor.h`:
```cpp
#pragma once
#include "BiquadFilter.h"
#include "EqConstants.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class EqProcessor
{
public:
    EqProcessor();

    void prepare(double sampleRate, int samplesPerBlock);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();

    void setBandGain(int bandIndex, float gainDb);
    void setBandBypassed(int bandIndex, bool bypassed);
    void setOutputGain(float gainDb);
    void setSoloBand(int bandIndex); // -1 to clear solo

    float getBandGain(int bandIndex) const;
    bool isBandBypassed(int bandIndex) const;
    int getSoloBand() const;

    // Spectrum FIFO: audio thread pushes, UI thread pulls
    bool pullFifoData(std::array<float, 4096>& dest);

    // Level metering: audio thread writes, UI thread reads
    std::atomic<float> inputLevelLeft{0.0f};
    std::atomic<float> inputLevelRight{0.0f};
    std::atomic<float> outputLevelLeft{0.0f};
    std::atomic<float> outputLevelRight{0.0f};

private:
    void pushToFifo(const juce::AudioBuffer<float>& buffer);

    struct Band
    {
        BiquadFilter filterL;
        BiquadFilter filterR;
        juce::SmoothedValue<float> smoothedGain{0.0f};
        float targetGainDb = 0.0f;
        bool bypassed = false;
    };

    std::array<Band, EqConstants::numBands> bands;
    juce::SmoothedValue<float> smoothedOutputGain{1.0f};
    float outputGainDb = 0.0f;
    double currentSampleRate = 44100.0;
    int soloedBand = -1; // -1 = no solo

    // Spectrum FIFO
    static constexpr int fifoSize = 4096;
    std::array<float, fifoSize> fifoBuffer{};
    std::atomic<int> fifoWriteIndex{0};
    std::atomic<bool> fifoReady{false};
};
```

- [ ] **Step 2: Implement EqProcessor**

`src/EqProcessor.cpp`:
```cpp
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

    // Measure input levels
    if (numChannels >= 1)
        inputLevelLeft.store(buffer.getMagnitude(0, 0, numSamples));
    if (numChannels >= 2)
        inputLevelRight.store(buffer.getMagnitude(1, 0, numSamples));

    // Process each band
    for (int b = 0; b < EqConstants::numBands; ++b)
    {
        auto& band = bands[static_cast<size_t>(b)];
        if (band.bypassed)
            continue;
        if (soloedBand >= 0 && b != soloedBand)
            continue;

        // Update coefficients if gain is still smoothing
        const float smoothedGain = band.smoothedGain.skip(numSamples);
        band.filterL.setParameters(EqConstants::bandFrequencies[static_cast<size_t>(b)],
                                   EqConstants::bandQ, smoothedGain, currentSampleRate);
        band.filterR.setParameters(EqConstants::bandFrequencies[static_cast<size_t>(b)],
                                   EqConstants::bandQ, smoothedGain, currentSampleRate);

        // Skip processing if gain is essentially 0 dB and not smoothing
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

    // Apply output gain
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = smoothedOutputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    // Measure output levels
    if (numChannels >= 1)
        outputLevelLeft.store(buffer.getMagnitude(0, 0, numSamples));
    if (numChannels >= 2)
        outputLevelRight.store(buffer.getMagnitude(1, 0, numSamples));

    // Push to spectrum FIFO (mono mix of output)
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

    // Snap to 1 dB increments
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
```

- [ ] **Step 3: Add EqProcessor to CMake sources**

In `CMakeLists.txt`, update the `target_sources` for the plugin:
```cmake
target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/EqProcessor.cpp
)
```

And add the test file and JUCE link for the test target:
```cmake
add_executable(ErinEqualizerTests
    tests/BiquadFilterTest.cpp
    tests/EqProcessorTest.cpp
)

target_include_directories(ErinEqualizerTests PRIVATE src)

target_link_libraries(ErinEqualizerTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_dsp
)

target_compile_definitions(ErinEqualizerTests PRIVATE
    JUCE_STANDALONE_APPLICATION=0
)
```

- [ ] **Step 4: Write EqProcessor tests**

`tests/EqProcessorTest.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "EqProcessor.h"
#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
    // Fill buffer with a sine tone
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

    // Process a few blocks to let filters settle
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer(2, blockSize);

    for (int block = 0; block < 20; ++block)
    {
        fillWithSine(buffer, 1000.0f, sampleRate, block * blockSize);
        eq.process(buffer);
    }

    // Measure final block
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
    eq.setBandGain(17, 10.0f); // Band 17 = 1000 Hz, +10 dB

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer(2, blockSize);

    // Let it settle
    for (int block = 0; block < 40; ++block)
    {
        fillWithSine(buffer, 1000.0f, sampleRate, block * blockSize);
        eq.process(buffer);
    }

    // Measure
    juce::AudioBuffer<float> inputRef(2, blockSize);
    fillWithSine(inputRef, 1000.0f, sampleRate, 40 * blockSize);
    fillWithSine(buffer, 1000.0f, sampleRate, 40 * blockSize);
    eq.process(buffer);

    const float inputRms = measureRms(inputRef, 0);
    const float outputRms = measureRms(buffer, 0);
    const float gainDb = 20.0f * std::log10(outputRms / inputRms);

    REQUIRE(gainDb > 8.0f);  // Should be close to 10 dB
    REQUIRE(gainDb < 12.0f);
}

TEST_CASE("EqProcessor: bypassed band has no effect", "[eqprocessor]")
{
    EqProcessor eq;
    eq.prepare(44100.0, 512);
    eq.setBandGain(17, 15.0f);    // +15 dB at 1 kHz
    eq.setBandBypassed(17, true);  // But bypassed

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
    eq.setOutputGain(-6.0f); // -6 dB output

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
```

- [ ] **Step 5: Build and run tests**

Run:
```bash
cmake --build build --target ErinEqualizerTests
cd build && ctest --output-on-failure
```

Expected: All EqProcessor and BiquadFilter tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/EqProcessor.h src/EqProcessor.cpp tests/EqProcessorTest.cpp CMakeLists.txt
git commit -m "feat: add EqProcessor with 31-band filter chain, smoothing, bypass, and FIFO"
```

---

### Task 4: PluginProcessor Integration

Wire the JUCE AudioProcessor to EqProcessor with 31 band parameters, output gain, bypass, and state serialization.

**Files:**
- Modify: `src/PluginProcessor.h`
- Modify: `src/PluginProcessor.cpp`

- [ ] **Step 1: Update PluginProcessor header**

Replace `src/PluginProcessor.h`:
```cpp
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
```

- [ ] **Step 2: Implement PluginProcessor**

Replace `src/PluginProcessor.cpp`:
```cpp
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

    // Check master bypass
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ErinEqProcessor();
}
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
```

Expected: Builds successfully. Standalone app launches with the placeholder UI.

- [ ] **Step 4: Commit**

```bash
git add src/PluginProcessor.h src/PluginProcessor.cpp
git commit -m "feat: wire PluginProcessor to EqProcessor with APVTS parameters and state"
```

---

### Task 5: BandComponent

Build the reusable vertical fader widget with 1 dB snap, double-click reset, and frequency label.

**Files:**
- Create: `src/BandComponent.h`
- Create: `src/BandComponent.cpp`
- Modify: `CMakeLists.txt` (add source)

- [ ] **Step 1: Create BandComponent header**

`src/BandComponent.h`:
```cpp
#pragma once
#include "EqConstants.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class BandComponent : public juce::Component
{
public:
    BandComponent(int bandIndex, juce::AudioProcessorValueTreeState& apvts);
    ~BandComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    int getBandIndex() const { return bandIndex; }
    float getCurrentGainDb() const;

    std::function<void(int bandIndex, float gainDb)> onHover;
    std::function<void(int bandIndex)> onSolo;

private:
    float yPositionToGainDb(float y) const;
    float gainDbToYPosition(float gainDb) const;

    int bandIndex;
    juce::String freqLabel;

    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Slider hiddenSlider; // invisible slider for parameter attachment

    bool isHovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandComponent)
};
```

- [ ] **Step 2: Implement BandComponent**

`src/BandComponent.cpp`:
```cpp
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
        onHover(bandIndex, -999.0f); // sentinel: no hover
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Update `target_sources`:
```cmake
target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/EqProcessor.cpp
    src/BandComponent.cpp
)
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
```

Expected: Builds successfully.

- [ ] **Step 5: Commit**

```bash
git add src/BandComponent.h src/BandComponent.cpp CMakeLists.txt
git commit -m "feat: add BandComponent vertical fader with 1dB snap, reset, and bypass menu"
```

---

### Task 6: LevelMeter

Build the vertical bar meter component with peak hold and gradient coloring.

**Files:**
- Create: `src/LevelMeter.h`
- Create: `src/LevelMeter.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create LevelMeter header**

`src/LevelMeter.h`:
```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter(const juce::String& label);
    ~LevelMeter() override = default;

    void paint(juce::Graphics&) override;
    void setLevel(float newLevel);

private:
    void timerCallback() override;

    juce::String label;
    float currentLevel = 0.0f;
    float peakLevel = 0.0f;
    float peakDecay = 0.0f;
    int peakHoldCounter = 0;

    static constexpr int peakHoldFrames = 45; // ~1.5s at 30fps
    static constexpr float decayRate = 0.92f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
```

- [ ] **Step 2: Implement LevelMeter**

`src/LevelMeter.cpp`:
```cpp
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
    // Peak hold logic
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

    // Label
    g.setColour(juce::Colour(EqConstants::textDimColor));
    g.setFont(9.0f);
    g.drawText(label, 0, 0, static_cast<int>(w), static_cast<int>(labelH),
               juce::Justification::centred);

    // Meter background
    g.setColour(juce::Colour(0xff111122));
    g.fillRoundedRectangle(meterX, meterTop, meterW, meterHeight, 2.0f);

    // Convert level to dB for display (clamp to -60..0 dB range)
    const float levelDb = currentLevel > 0.0001f
                              ? std::clamp(20.0f * std::log10(currentLevel), -60.0f, 0.0f)
                              : -60.0f;
    const float normalizedLevel = (levelDb + 60.0f) / 60.0f;
    const float barHeight = normalizedLevel * meterHeight;

    // Gradient fill
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

    // Peak indicator
    if (peakLevel > 0.0001f)
    {
        const float peakDb = std::clamp(20.0f * std::log10(peakLevel), -60.0f, 0.0f);
        const float peakNorm = (peakDb + 60.0f) / 60.0f;
        const float peakY = meterTop + meterHeight - (peakNorm * meterHeight);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillRect(meterX, peakY, meterW, 1.5f);
    }
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Update `target_sources`:
```cmake
target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/EqProcessor.cpp
    src/BandComponent.cpp
    src/LevelMeter.cpp
)
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
```

Expected: Builds successfully.

- [ ] **Step 5: Commit**

```bash
git add src/LevelMeter.h src/LevelMeter.cpp CMakeLists.txt
git commit -m "feat: add LevelMeter component with peak hold and gradient display"
```

---

### Task 7: Toolbar

Build the top toolbar with bypass, analyzer toggle, reset, preset selector, and output gain knob.

**Files:**
- Create: `src/Toolbar.h`
- Create: `src/Toolbar.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create Toolbar header**

`src/Toolbar.h`:
```cpp
#pragma once
#include "EqConstants.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class Toolbar : public juce::Component
{
public:
    Toolbar(juce::AudioProcessorValueTreeState& apvts);
    ~Toolbar() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onResetAll;
    std::function<void(const juce::String&)> onPresetSelected;
    std::function<void()> onSavePreset;

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton bypassButton{"BYPASS"};
    juce::TextButton analyzerButton{"ANALYZER"};
    juce::TextButton resetButton{"RESET"};
    juce::ComboBox presetSelector;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyzerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Toolbar)
};
```

- [ ] **Step 2: Implement Toolbar**

`src/Toolbar.cpp`:
```cpp
#include "Toolbar.h"

Toolbar::Toolbar(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
    // Bypass toggle
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2a5a2a));
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff8ce8a8));
    bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e3a));
    bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6a6a9a));
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "masterBypass", bypassButton);

    // Analyzer toggle
    analyzerButton.setClickingTogglesState(true);
    analyzerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1e3a5f));
    analyzerButton.setColour(juce::TextButton::textColourOnId, juce::Colour(EqConstants::accentColor));
    analyzerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e3a));
    analyzerButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6a6a9a));
    addAndMakeVisible(analyzerButton);
    analyzerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "analyzerOn", analyzerButton);

    // Reset button
    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e1e3a));
    resetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6a6a9a));
    resetButton.onClick = [this]()
    {
        if (onResetAll)
            onResetAll();
    };
    addAndMakeVisible(resetButton);

    // Preset selector
    presetSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e1e3a));
    presetSelector.setColour(juce::ComboBox::textColourId, juce::Colour(0xff8a8aba));
    presetSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3a6a));
    presetSelector.addItem("Flat", 1);
    presetSelector.addItem("Low Cut", 2);
    presetSelector.addItem("High Cut", 3);
    presetSelector.addItem("Smiley Curve", 4);
    presetSelector.addItem("Vocal Presence", 5);
    presetSelector.addItem("Bass Boost", 6);
    presetSelector.addSeparator();
    presetSelector.addItem("Save Preset...", 100);
    presetSelector.setSelectedId(1, juce::dontSendNotification);
    presetSelector.onChange = [this]()
    {
        const int id = presetSelector.getSelectedId();
        if (id == 100 && onSavePreset)
        {
            onSavePreset();
            return;
        }
        if (onPresetSelected)
            onPresetSelected(presetSelector.getText());
    };
    addAndMakeVisible(presetSelector);

    // Output gain knob
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    outputGainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(EqConstants::accentColor));
    outputGainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff3a3a6a));
    outputGainSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(EqConstants::textMidColor));
    outputGainSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "outputGain", outputGainSlider);

    outputGainLabel.setText("OUT", juce::dontSendNotification);
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5a5a8a));
    outputGainLabel.setFont(10.0f);
    addAndMakeVisible(outputGainLabel);
}

void Toolbar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(EqConstants::toolbarBgColor));
    g.setColour(juce::Colour(EqConstants::borderColor));
    g.drawLine(0.0f, static_cast<float>(getHeight()), static_cast<float>(getWidth()),
               static_cast<float>(getHeight()), 1.0f);

    // Logo
    g.setColour(juce::Colour(EqConstants::accentColor));
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText("ERIN EQ", 12, 0, 80, getHeight(), juce::Justification::centredLeft);

    // Separator
    g.setColour(juce::Colour(EqConstants::borderColor));
    g.drawLine(96.0f, 8.0f, 96.0f, static_cast<float>(getHeight() - 8), 1.0f);
}

void Toolbar::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromLeft(96); // logo space

    const int buttonW = 70;
    const int buttonH = 24;
    const int pad = 6;
    const int y = (area.getHeight() - buttonH) / 2;

    bypassButton.setBounds(area.getX() + pad, y, buttonW, buttonH);
    analyzerButton.setBounds(bypassButton.getRight() + pad, y, buttonW + 10, buttonH);
    resetButton.setBounds(analyzerButton.getRight() + pad, y, 55, buttonH);

    // Right-aligned controls
    const int rightX = area.getRight();
    outputGainSlider.setBounds(rightX - 130, y - 2, 130, buttonH + 4);
    outputGainLabel.setBounds(outputGainSlider.getX() - 28, y, 28, buttonH);
    presetSelector.setBounds(outputGainLabel.getX() - 140, y, 130, buttonH);
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Update `target_sources`:
```cmake
target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/EqProcessor.cpp
    src/BandComponent.cpp
    src/LevelMeter.cpp
    src/Toolbar.cpp
)
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
```

Expected: Builds successfully.

- [ ] **Step 5: Commit**

```bash
git add src/Toolbar.h src/Toolbar.cpp CMakeLists.txt
git commit -m "feat: add Toolbar with bypass, analyzer toggle, reset, presets, output gain"
```

---

### Task 8: PluginEditor Layout

Wire all components together in the editor with the full layout: toolbar, fader area, meters, and status bar.

**Files:**
- Modify: `src/PluginEditor.h`
- Modify: `src/PluginEditor.cpp`

- [ ] **Step 1: Update PluginEditor header**

Replace `src/PluginEditor.h`:
```cpp
#pragma once
#include "PluginProcessor.h"
#include "BandComponent.h"
#include "LevelMeter.h"
#include "Toolbar.h"
#include "EqConstants.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class ErinEqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ErinEqEditor(ErinEqProcessor&);
    ~ErinEqEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void resetAllBands();
    void applyPreset(const juce::String& presetName);
    void paintDbScale(juce::Graphics& g, juce::Rectangle<int> area);
    void paintStatusBar(juce::Graphics& g, juce::Rectangle<int> area);

    ErinEqProcessor& processorRef;

    Toolbar toolbar;
    std::array<std::unique_ptr<BandComponent>, EqConstants::numBands> bandComponents;
    LevelMeter inputMeter{"IN"};
    LevelMeter outputMeter{"OUT"};

    int hoveredBandIndex = -1;
    float hoveredBandGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErinEqEditor)
};
```

- [ ] **Step 2: Implement PluginEditor**

Replace `src/PluginEditor.cpp`:
```cpp
#include "PluginEditor.h"

ErinEqEditor::ErinEqEditor(ErinEqProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      toolbar(p.getApvts())
{
    setSize(EqConstants::windowWidth, EqConstants::windowHeight);
    setResizable(false, false);

    addAndMakeVisible(toolbar);
    toolbar.onResetAll = [this]() { resetAllBands(); };
    toolbar.onPresetSelected = [this](const juce::String& name) { applyPreset(name); };

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
            eq.setSoloBand(eq.getSoloBand() == idx ? -1 : idx); // toggle
        };

        addAndMakeVisible(*bandComponents[static_cast<size_t>(i)]);
    }

    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    startTimerHz(30);
}

void ErinEqEditor::timerCallback()
{
    auto& eq = processorRef.getEqProcessor();
    inputMeter.setLevel(std::max(eq.inputLevelLeft.load(), eq.inputLevelRight.load()));
    outputMeter.setLevel(std::max(eq.outputLevelLeft.load(), eq.outputLevelRight.load()));
}

void ErinEqEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(EqConstants::bgColor));

    // dB scale
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

    // Status bar
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
    // Reset first
    resetAllBands();

    // Apply preset gains: array of 31 floats indexed by band
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
    // "Flat" uses the reset default (all zeros)

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
```

- [ ] **Step 3: Build and verify visually**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
./build/ErinEqualizer_artefacts/Release/Standalone/ErinEqualizer
```

Expected: Standalone launches with the full UI — toolbar at top, 31 faders in the center, meters on the right, status bar at bottom. Faders should be draggable, double-click resets to 0 dB, toolbar buttons are functional.

- [ ] **Step 4: Commit**

```bash
git add src/PluginEditor.h src/PluginEditor.cpp
git commit -m "feat: wire PluginEditor with toolbar, 31 band faders, meters, and status bar"
```

---

### Task 9: SpectrumAnalyzer

Implement the FFT-based spectrum analyzer as a toggleable overlay on the fader area.

**Files:**
- Create: `src/SpectrumAnalyzer.h`
- Create: `src/SpectrumAnalyzer.cpp`
- Modify: `src/PluginEditor.h`
- Modify: `src/PluginEditor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create SpectrumAnalyzer header**

`src/SpectrumAnalyzer.h`:
```cpp
#pragma once
#include "EqProcessor.h"
#include "EqConstants.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    SpectrumAnalyzer(EqProcessor& eqProcessor);
    ~SpectrumAnalyzer() override = default;

    void paint(juce::Graphics&) override;
    void setActive(bool shouldBeActive);

private:
    void timerCallback() override;

    EqProcessor& eqProcessor;
    bool active = false;

    static constexpr int fftOrder = 12; // 2^12 = 4096
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numDisplayBins = 256;

    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{
        static_cast<size_t>(fftSize), juce::dsp::WindowingFunction<float>::hann};

    std::array<float, fftSize> fftInput{};
    std::array<float, fftSize * 2> fftOutput{};
    std::array<float, numDisplayBins> smoothedMagnitudes{};

    static constexpr float smoothingFactor = 0.8f;
    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
```

- [ ] **Step 2: Implement SpectrumAnalyzer**

`src/SpectrumAnalyzer.cpp`:
```cpp
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

    // Copy and apply window
    fftInput = fifoData;
    window.multiplyWithWindowingTable(fftInput.data(), static_cast<size_t>(fftSize));

    // Perform FFT
    std::copy(fftInput.begin(), fftInput.end(), fftOutput.begin());
    std::fill(fftOutput.begin() + fftSize, fftOutput.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform(fftOutput.data());

    // Map FFT bins to display bins (log-spaced)
    for (int i = 0; i < numDisplayBins; ++i)
    {
        // Map display bin to frequency range (20 Hz - 20 kHz, log scale)
        const float normPos = static_cast<float>(i) / static_cast<float>(numDisplayBins);
        const float freq = 20.0f * std::pow(1000.0f, normPos); // 20 Hz to 20 kHz

        // Map frequency to FFT bin
        // Assume sample rate of 44100 for bin mapping (close enough for display)
        const float binFloat = freq * static_cast<float>(fftSize) / 44100.0f;
        const int bin = std::clamp(static_cast<int>(binFloat), 0, fftSize / 2 - 1);

        const float magnitude = fftOutput[static_cast<size_t>(bin)];
        const float magnitudeDb = magnitude > 0.0f
                                      ? std::clamp(20.0f * std::log10(magnitude), minDb, maxDb)
                                      : minDb;
        const float normalized = (magnitudeDb - minDb) / (maxDb - minDb);

        // Exponential smoothing
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
        if (i == 0)
            spectrumPath.lineTo(x, y);
        else
            spectrumPath.lineTo(x, y);
    }

    spectrumPath.lineTo(w, h);
    spectrumPath.closeSubPath();

    juce::ColourGradient gradient(
        juce::Colour(EqConstants::spectrumColor).withAlpha(0.0f), 0.0f, h,
        juce::Colour(EqConstants::spectrumColor).withAlpha(0.25f), 0.0f, 0.0f, false);

    g.setGradientFill(gradient);
    g.fillPath(spectrumPath);

    // Outline
    g.setColour(juce::Colour(EqConstants::spectrumColor).withAlpha(0.3f));
    g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
}
```

- [ ] **Step 3: Integrate into PluginEditor**

Add to `src/PluginEditor.h`, in the private section after existing members:
```cpp
#include "SpectrumAnalyzer.h"

// Add as member:
SpectrumAnalyzer spectrumAnalyzer;
```

Update `src/PluginEditor.cpp` constructor — add after `addAndMakeVisible(outputMeter)`:
```cpp
spectrumAnalyzer.setActive(false);
addAndMakeVisible(spectrumAnalyzer);
```

In `resized()`, add after band fader layout (before the closing brace):
```cpp
// Spectrum overlay covers the fader area
spectrumAnalyzer.setBounds(area);
```

Add a timer check in `timerCallback()` to sync analyzer state:
```cpp
const bool analyzerOn = processorRef.getApvts().getRawParameterValue("analyzerOn")->load() > 0.5f;
spectrumAnalyzer.setActive(analyzerOn);
```

- [ ] **Step 4: Add to CMakeLists.txt**

Update `target_sources`:
```cmake
target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/EqProcessor.cpp
    src/BandComponent.cpp
    src/LevelMeter.cpp
    src/Toolbar.cpp
    src/SpectrumAnalyzer.cpp
)
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
```

Expected: Builds. Clicking "ANALYZER" in the toolbar shows/hides a cyan spectrum overlay behind the faders.

- [ ] **Step 6: Commit**

```bash
git add src/SpectrumAnalyzer.h src/SpectrumAnalyzer.cpp src/PluginEditor.h src/PluginEditor.cpp CMakeLists.txt
git commit -m "feat: add toggleable spectrum analyzer overlay with FFT display"
```

---

### Task 10: Preset System

Implement factory preset XML files and user preset save/load via platform user data directory.

**Files:**
- Create: `presets/factory/Flat.xml`
- Create: `presets/factory/LowCut.xml`
- Create: `presets/factory/HighCut.xml`
- Create: `presets/factory/SmileyCurve.xml`
- Create: `presets/factory/VocalPresence.xml`
- Create: `presets/factory/BassBoost.xml`
- Modify: `src/Toolbar.h`
- Modify: `src/Toolbar.cpp`
- Modify: `src/PluginEditor.cpp`

- [ ] **Step 1: Create factory preset XML files**

Each preset is a `juce::ValueTree` XML. The preset files encode band gains. Example `presets/factory/Flat.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ErinEqPreset name="Flat">
  <Bands>
    <Band index="0" gain="0"/>
    <Band index="1" gain="0"/>
    <Band index="2" gain="0"/>
    <Band index="3" gain="0"/>
    <Band index="4" gain="0"/>
    <Band index="5" gain="0"/>
    <Band index="6" gain="0"/>
    <Band index="7" gain="0"/>
    <Band index="8" gain="0"/>
    <Band index="9" gain="0"/>
    <Band index="10" gain="0"/>
    <Band index="11" gain="0"/>
    <Band index="12" gain="0"/>
    <Band index="13" gain="0"/>
    <Band index="14" gain="0"/>
    <Band index="15" gain="0"/>
    <Band index="16" gain="0"/>
    <Band index="17" gain="0"/>
    <Band index="18" gain="0"/>
    <Band index="19" gain="0"/>
    <Band index="20" gain="0"/>
    <Band index="21" gain="0"/>
    <Band index="22" gain="0"/>
    <Band index="23" gain="0"/>
    <Band index="24" gain="0"/>
    <Band index="25" gain="0"/>
    <Band index="26" gain="0"/>
    <Band index="27" gain="0"/>
    <Band index="28" gain="0"/>
    <Band index="29" gain="0"/>
    <Band index="30" gain="0"/>
  </Bands>
  <OutputGain value="0"/>
</ErinEqPreset>
```

`presets/factory/LowCut.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ErinEqPreset name="Low Cut">
  <Bands>
    <Band index="0" gain="-15"/>
    <Band index="1" gain="-14"/>
    <Band index="2" gain="-12"/>
    <Band index="3" gain="-10"/>
    <Band index="4" gain="-7"/>
    <Band index="5" gain="-4"/>
    <Band index="6" gain="-2"/>
    <Band index="7" gain="0"/>
    <Band index="8" gain="0"/>
    <Band index="9" gain="0"/>
    <Band index="10" gain="0"/>
    <Band index="11" gain="0"/>
    <Band index="12" gain="0"/>
    <Band index="13" gain="0"/>
    <Band index="14" gain="0"/>
    <Band index="15" gain="0"/>
    <Band index="16" gain="0"/>
    <Band index="17" gain="0"/>
    <Band index="18" gain="0"/>
    <Band index="19" gain="0"/>
    <Band index="20" gain="0"/>
    <Band index="21" gain="0"/>
    <Band index="22" gain="0"/>
    <Band index="23" gain="0"/>
    <Band index="24" gain="0"/>
    <Band index="25" gain="0"/>
    <Band index="26" gain="0"/>
    <Band index="27" gain="0"/>
    <Band index="28" gain="0"/>
    <Band index="29" gain="0"/>
    <Band index="30" gain="0"/>
  </Bands>
  <OutputGain value="0"/>
</ErinEqPreset>
```

`presets/factory/HighCut.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ErinEqPreset name="High Cut">
  <Bands>
    <Band index="0" gain="0"/>
    <Band index="1" gain="0"/>
    <Band index="2" gain="0"/>
    <Band index="3" gain="0"/>
    <Band index="4" gain="0"/>
    <Band index="5" gain="0"/>
    <Band index="6" gain="0"/>
    <Band index="7" gain="0"/>
    <Band index="8" gain="0"/>
    <Band index="9" gain="0"/>
    <Band index="10" gain="0"/>
    <Band index="11" gain="0"/>
    <Band index="12" gain="0"/>
    <Band index="13" gain="0"/>
    <Band index="14" gain="0"/>
    <Band index="15" gain="0"/>
    <Band index="16" gain="0"/>
    <Band index="17" gain="0"/>
    <Band index="18" gain="0"/>
    <Band index="19" gain="0"/>
    <Band index="20" gain="0"/>
    <Band index="21" gain="0"/>
    <Band index="22" gain="0"/>
    <Band index="23" gain="0"/>
    <Band index="24" gain="-2"/>
    <Band index="25" gain="-4"/>
    <Band index="26" gain="-7"/>
    <Band index="27" gain="-10"/>
    <Band index="28" gain="-12"/>
    <Band index="29" gain="-14"/>
    <Band index="30" gain="-15"/>
  </Bands>
  <OutputGain value="0"/>
</ErinEqPreset>
```

`presets/factory/SmileyCurve.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ErinEqPreset name="Smiley Curve">
  <Bands>
    <Band index="0" gain="6"/>
    <Band index="1" gain="5"/>
    <Band index="2" gain="5"/>
    <Band index="3" gain="4"/>
    <Band index="4" gain="3"/>
    <Band index="5" gain="3"/>
    <Band index="6" gain="2"/>
    <Band index="7" gain="1"/>
    <Band index="8" gain="0"/>
    <Band index="9" gain="0"/>
    <Band index="10" gain="-1"/>
    <Band index="11" gain="-2"/>
    <Band index="12" gain="-3"/>
    <Band index="13" gain="-3"/>
    <Band index="14" gain="-3"/>
    <Band index="15" gain="-3"/>
    <Band index="16" gain="-3"/>
    <Band index="17" gain="-2"/>
    <Band index="18" gain="-1"/>
    <Band index="19" gain="0"/>
    <Band index="20" gain="0"/>
    <Band index="21" gain="1"/>
    <Band index="22" gain="2"/>
    <Band index="23" gain="3"/>
    <Band index="24" gain="3"/>
    <Band index="25" gain="4"/>
    <Band index="26" gain="4"/>
    <Band index="27" gain="5"/>
    <Band index="28" gain="5"/>
    <Band index="29" gain="5"/>
    <Band index="30" gain="6"/>
  </Bands>
  <OutputGain value="0"/>
</ErinEqPreset>
```

`presets/factory/VocalPresence.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ErinEqPreset name="Vocal Presence">
  <Bands>
    <Band index="0" gain="-2"/>
    <Band index="1" gain="-2"/>
    <Band index="2" gain="-1"/>
    <Band index="3" gain="-1"/>
    <Band index="4" gain="0"/>
    <Band index="5" gain="0"/>
    <Band index="6" gain="0"/>
    <Band index="7" gain="0"/>
    <Band index="8" gain="0"/>
    <Band index="9" gain="0"/>
    <Band index="10" gain="0"/>
    <Band index="11" gain="0"/>
    <Band index="12" gain="1"/>
    <Band index="13" gain="2"/>
    <Band index="14" gain="3"/>
    <Band index="15" gain="4"/>
    <Band index="16" gain="5"/>
    <Band index="17" gain="5"/>
    <Band index="18" gain="4"/>
    <Band index="19" gain="3"/>
    <Band index="20" gain="2"/>
    <Band index="21" gain="1"/>
    <Band index="22" gain="0"/>
    <Band index="23" gain="0"/>
    <Band index="24" gain="0"/>
    <Band index="25" gain="0"/>
    <Band index="26" gain="-1"/>
    <Band index="27" gain="-1"/>
    <Band index="28" gain="-2"/>
    <Band index="29" gain="-2"/>
    <Band index="30" gain="-2"/>
  </Bands>
  <OutputGain value="0"/>
</ErinEqPreset>
```

`presets/factory/BassBoost.xml`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<ErinEqPreset name="Bass Boost">
  <Bands>
    <Band index="0" gain="8"/>
    <Band index="1" gain="7"/>
    <Band index="2" gain="7"/>
    <Band index="3" gain="6"/>
    <Band index="4" gain="5"/>
    <Band index="5" gain="4"/>
    <Band index="6" gain="3"/>
    <Band index="7" gain="2"/>
    <Band index="8" gain="1"/>
    <Band index="9" gain="0"/>
    <Band index="10" gain="0"/>
    <Band index="11" gain="0"/>
    <Band index="12" gain="0"/>
    <Band index="13" gain="0"/>
    <Band index="14" gain="0"/>
    <Band index="15" gain="0"/>
    <Band index="16" gain="0"/>
    <Band index="17" gain="0"/>
    <Band index="18" gain="0"/>
    <Band index="19" gain="0"/>
    <Band index="20" gain="0"/>
    <Band index="21" gain="0"/>
    <Band index="22" gain="0"/>
    <Band index="23" gain="0"/>
    <Band index="24" gain="0"/>
    <Band index="25" gain="0"/>
    <Band index="26" gain="0"/>
    <Band index="27" gain="0"/>
    <Band index="28" gain="0"/>
    <Band index="29" gain="0"/>
    <Band index="30" gain="0"/>
  </Bands>
  <OutputGain value="0"/>
</ErinEqPreset>
```

- [ ] **Step 2: Add user preset save functionality to PluginEditor**

In `src/PluginEditor.cpp`, add a `saveUserPreset` method implementation. First add the declaration in `src/PluginEditor.h` private section:
```cpp
void saveUserPreset();
```

Wire it in the constructor after setting `toolbar.onPresetSelected`:
```cpp
toolbar.onSavePreset = [this]() { saveUserPreset(); };
```

Implement in `src/PluginEditor.cpp`:
```cpp
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
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cmake --build build --target ErinEqualizer_Standalone
```

Expected: Builds. Preset dropdown loads factory presets. "Save Preset..." opens a file dialog.

- [ ] **Step 4: Run all tests**

Run:
```bash
cmake --build build --target ErinEqualizerTests
cd build && ctest --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add presets/ src/PluginEditor.h src/PluginEditor.cpp
git commit -m "feat: add factory presets and user preset save/load"
```

---

### Task 11: Final Integration and Polish

Run the full build for all targets, verify everything works end-to-end, add `.gitignore`.

**Files:**
- Create: `.gitignore`

- [ ] **Step 1: Create .gitignore**

```
build/
.superpowers/
*.user
.cache/
CMakeUserPresets.json
```

- [ ] **Step 2: Full build — all targets**

Run:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Expected: Both `ErinEqualizer_VST3`, `ErinEqualizer_Standalone`, and `ErinEqualizerTests` build successfully.

- [ ] **Step 3: Run all tests**

Run:
```bash
cd build && ctest --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 4: Verify standalone launches**

Run:
```bash
./build/ErinEqualizer_artefacts/Release/Standalone/ErinEqualizer
```

Expected: Full UI with all 31 bands, toolbar, meters, spectrum analyzer toggle working.

- [ ] **Step 5: Verify VST3 output exists**

Run:
```bash
ls -la build/ErinEqualizer_artefacts/Release/VST3/
```

Expected: `Erin Equalizer.vst3` directory exists.

- [ ] **Step 6: Commit**

```bash
git add .gitignore
git commit -m "chore: add .gitignore and verify full build"
```
