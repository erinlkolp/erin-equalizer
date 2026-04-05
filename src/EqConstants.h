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

    constexpr float bandQ = 4.318f;
    constexpr float minGainDb = -15.0f;
    constexpr float maxGainDb = 15.0f;
    constexpr float gainStepDb = 1.0f;
    constexpr float defaultGainDb = 0.0f;
    constexpr float smoothingTimeMs = 20.0f;

    constexpr int windowWidth = 900;
    constexpr int windowHeight = 500;
    constexpr int toolbarHeight = 44;
    constexpr int dbScaleWidth = 28;
    constexpr int meterWidth = 40;
    constexpr int freqLabelHeight = 28;
    constexpr int statusBarHeight = 24;

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
