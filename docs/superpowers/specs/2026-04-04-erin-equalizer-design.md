# Erin Equalizer — Design Specification

A 31-band 1/3 octave graphic equalizer plugin (VST3 + Standalone) for Windows, Linux, and macOS.

## Technology Stack

- **Language:** C++
- **Framework:** JUCE 7.x (fetched via CMake `FetchContent`)
- **Build system:** CMake with `juce_add_plugin()`
- **Plugin formats:** VST3, Standalone
- **Platforms:** Windows, Linux, macOS

## Architecture

Component-based design with thin JUCE shells delegating to focused modules.

### DSP Layer

**PluginProcessor** — JUCE `AudioProcessor` subclass. Owns 31 `AudioParameterFloat` values (one per band), an output gain parameter, and a bypass parameter. Routes audio buffers to `EqProcessor`. Handles state serialization via `getStateInformation` / `setStateInformation` using `juce::ValueTree`.

**EqProcessor** — Core DSP engine. Owns 31 `BiquadFilter` instances in a serial (cascaded) chain. Responsibilities:
- Processes stereo audio through the filter chain (each channel processed independently with identical coefficients)
- Parameter smoothing via `juce::SmoothedValue` (~20ms ramp) to prevent zipper noise
- Per-band bypass (bypassed bands are skipped entirely, zero CPU cost)
- Output gain applied after the filter chain
- Coefficient updates per-block (not per-sample) for efficiency
- Feeds post-EQ signal into a FIFO ring buffer for spectrum analysis

**BiquadFilter** — Single second-order IIR peaking EQ filter using the Audio EQ Cookbook (Robert Bristow-Johnson) formulas. Each instance is configured with:
- Center frequency (fixed per band, from ISO 1/3 octave series)
- Q factor: ~4.318 (fixed, derived from 1/3 octave bandwidth)
- Gain: -15 dB to +15 dB, snapped to 1 dB increments
- Coefficients recalculated only when gain changes

### UI Layer

**PluginEditor** — JUCE `AudioProcessorEditor` subclass. Fixed size 900x500 px. Lays out child components and wires parameter attachments.

**BandComponent** — Reusable vertical fader widget, instantiated 31 times. Each displays:
- Vertical fader with grab handle
- Frequency label below
- Fader snaps to 1 dB increments (31 positions: -15 to +15)
- Double-click to reset to 0 dB
- Right-click for bypass/solo context menu

**SpectrumAnalyzer** — FFT computation and rendering:
- 4096-point FFT using `juce::dsp::FFT`
- Hann window applied before transform
- Computed on UI thread timer (~30 fps), not on audio thread
- Rendered as semi-transparent cyan gradient overlay on the fader area
- Magnitude bins smoothed with exponential decay for visual stability
- Toggleable on/off (off by default)

**LevelMeter** — Vertical bar meter component (x2: input and output):
- Green-yellow-red gradient coloring
- Peak hold with ~1.5s decay
- Peak computed on audio thread via max-abs per block
- Read atomically by UI for display

**Toolbar** — Top bar component containing:
- Plugin logo ("ERIN EQ")
- Master bypass button
- Spectrum analyzer toggle button
- Reset all bands button
- Preset dropdown selector
- Output gain knob with dB readout

## Band Frequencies

Standard ISO 266 1/3 octave center frequencies (31 bands):

```
20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500,
630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
10000, 12500, 16000, 20000
```

## UI Layout

Fixed 900x500 px window. Dark Pro visual style (dark background, gradient-lit fader bars, subtle glow effects).

### Layout Zones

| Zone | Position | Height/Width | Contents |
|------|----------|-------------|----------|
| Top toolbar | Top | 44 px tall | Logo, bypass, analyzer toggle, reset, preset dropdown, output gain |
| dB scale | Left edge | 28 px wide | +15 to -15 dB labels |
| Fader area | Center | Fills remaining space | 31 vertical faders, 0 dB center line, spectrum overlay |
| Frequency labels | Below faders | ~28 px tall | Angled frequency labels |
| I/O meters | Right edge | 40 px wide | Input + output vertical bar meters |
| Status bar | Bottom | 24 px tall | Hovered band info, plugin version |

### Interactions

- Drag fader handles vertically to adjust gain (snaps to 1 dB)
- Double-click a fader to reset to 0 dB
- Right-click a band for bypass/solo context menu
- Hover a band to see frequency and gain in status bar

## Preset System

**Built-in presets:** Flat, Low Cut, High Cut, Smiley Curve, Vocal Presence, Bass Boost

**User presets:** Save/load via preset dropdown. Stored as XML files in platform-standard user data directory (`juce::File::getSpecialLocation`).

**State persistence:** Full plugin state (31 band gains, 31 per-band bypass states, output gain, master bypass state, analyzer on/off) serialized via `juce::ValueTree` XML. Used by DAW for project save/restore and by preset system for file I/O.

## File Structure

```
erin-equalizer/
├── CMakeLists.txt
├── src/
│   ├── PluginProcessor.h / .cpp     # JUCE shell: parameters, state, audio routing
│   ├── PluginEditor.h / .cpp        # JUCE shell: layout, wiring
│   ├── EqProcessor.h / .cpp         # 31-band biquad chain, smoothing, bypass
│   ├── BiquadFilter.h / .cpp        # Single peaking EQ biquad
│   ├── BandComponent.h / .cpp       # Reusable vertical fader widget
│   ├── SpectrumAnalyzer.h / .cpp    # FFT computation + overlay rendering
│   ├── LevelMeter.h / .cpp          # Vertical bar meter component
│   ├── Toolbar.h / .cpp             # Top bar: bypass, presets, analyzer toggle, gain
│   └── EqConstants.h                # Band frequencies, Q value, gain range, colors
└── presets/
    └── factory/                     # Built-in preset XML files
```

## Build

```bash
cmake -B build
cmake --build build
```

JUCE fetched automatically via CMake `FetchContent` (pinned version). Produces VST3 plugin (`.vst3`) and standalone executable.
