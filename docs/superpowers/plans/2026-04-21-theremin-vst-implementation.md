# Theremin VST Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows VST3 plugin that uses webcam + hand tracking to emit MIDI, in working commit-sized increments that culminate in a downloadable `.vst3` binary on GitHub Releases.

**Architecture:** JUCE 8 C++20 audio plugin. OpenCV 4 for webcam capture. ONNX Runtime 1.17 for MediaPipe hand landmark inference. Camera + inference run on a worker thread; the audio thread reads landmarks from a lock-free shared slot and emits MIDI events. Pure-logic modules (calibration math, scale quantization, MIDI mapping) are testable without any camera or plugin host.

**Tech Stack:** JUCE 8, C++20, CMake 3.22+, OpenCV 4.9, ONNX Runtime 1.17, Catch2 (for unit tests), GitHub Actions (Windows build), vcpkg (OpenCV provisioning).

**Reference documents:**
- Design spec: `docs/superpowers/specs/2026-04-21-theremin-vst-design.md`
- Original web prototype (for mapping-logic reference): `/Users/bre/Thermostatwebapp/src/app/App.tsx`

**Primary tester:** Mackie (Windows rig). Primary dev machine: Lappy (macOS, M3). Windows builds are produced via GitHub Actions CI; the repo maintainer never needs to run a Windows build locally.

---

## Task 1: Bootstrap JUCE + minimal plugin skeleton

**Goal:** Get a VST3 `.vst3` binary that loads in a plugin host (pluginval or a DAW) and shows a blank window with the plugin name. No camera, no MIDI, no logic — just "does JUCE build and produce a plugin."

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitmodules`
- Create: `vendor/JUCE/` (git submodule)
- Create: `src/PluginProcessor.h`
- Create: `src/PluginProcessor.cpp`
- Create: `src/PluginEditor.h`
- Create: `src/PluginEditor.cpp`

- [ ] **Step 1.1: Add JUCE as a git submodule, pinned to a stable tag**

Run from repo root:

```bash
git submodule add https://github.com/juce-framework/JUCE.git vendor/JUCE
cd vendor/JUCE && git checkout 8.0.3 && cd ../..
git add .gitmodules vendor/JUCE
```

- [ ] **Step 1.2: Write the root `CMakeLists.txt`**

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(Theremin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Silence JUCE deprecation warnings on macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# JUCE as subdirectory (vendored submodule)
add_subdirectory(vendor/JUCE)

juce_add_plugin(Theremin
    COMPANY_NAME "Brebuilds"
    PLUGIN_MANUFACTURER_CODE Brbd
    PLUGIN_CODE Thrm
    FORMATS VST3 Standalone
    PRODUCT_NAME "Theremin"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Instrument" "Tools"
)

target_sources(Theremin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_compile_definitions(Theremin PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(Theremin PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
```

Note: we declare `IS_SYNTH TRUE` + `NEEDS_MIDI_OUTPUT TRUE` because we generate MIDI from an external signal (webcam). This puts it on an instrument track in the DAW, where MIDI output routing is standard.

- [ ] **Step 1.3: Create the minimal `PluginProcessor.h`**

Create `src/PluginProcessor.h`:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ThereminAudioProcessor : public juce::AudioProcessor
{
public:
    ThereminAudioProcessor();
    ~ThereminAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminAudioProcessor)
};
```

- [ ] **Step 1.4: Create the minimal `PluginProcessor.cpp`**

Create `src/PluginProcessor.cpp`:

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

ThereminAudioProcessor::ThereminAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ThereminAudioProcessor::prepareToPlay(double, int) {}
void ThereminAudioProcessor::releaseResources() {}

void ThereminAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();  // zero audio; we only emit MIDI
    // MIDI emission will be added in later tasks.
    juce::ignoreUnused(midi);
}

juce::AudioProcessorEditor* ThereminAudioProcessor::createEditor()
{
    return new ThereminAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThereminAudioProcessor();
}
```

- [ ] **Step 1.5: Create the minimal `PluginEditor.h`**

Create `src/PluginEditor.h`:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ThereminAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ThereminAudioProcessorEditor(ThereminAudioProcessor& p);
    ~ThereminAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ThereminAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminAudioProcessorEditor)
};
```

- [ ] **Step 1.6: Create the minimal `PluginEditor.cpp`**

Create `src/PluginEditor.cpp`:

```cpp
#include "PluginEditor.h"

ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 400, 1600, 1200);
}

void ThereminAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(32.0f)));
    g.drawFittedText("THEREMIN", getLocalBounds(), juce::Justification::centred, 1);
}

void ThereminAudioProcessorEditor::resized() {}
```

- [ ] **Step 1.7: Configure and build on macOS**

Run:

```bash
cmake -B build -G "Xcode" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target Theremin_Standalone
```

Expected: `build/Theremin_artefacts/Debug/Standalone/Theremin.app` exists (macOS). The VST3 target also builds: `build/Theremin_artefacts/Debug/VST3/Theremin.vst3/`.

- [ ] **Step 1.8: Launch the standalone app to confirm it renders**

Run:

```bash
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```

Expected: window opens showing cyan "THEREMIN" text centered on dark background.

- [ ] **Step 1.9: Commit**

```bash
git add CMakeLists.txt .gitmodules src/
git commit -m "feat: scaffold JUCE plugin with minimal editor"
git push
```

---

## Task 2: GitHub Actions — Windows VST3 build

**Goal:** Every push to `main` produces a downloadable `Theremin.vst3` from a Windows runner. Mackie can install from artifacts on day 1.

**Files:**
- Create: `.github/workflows/build.yml`

- [ ] **Step 2.1: Write the CI workflow**

Create `.github/workflows/build.yml`:

```yaml
name: Build

on:
  push:
    branches: [main]
    tags: ["v*"]
  pull_request:
    branches: [main]
  workflow_dispatch:

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install CMake
        uses: lukka/get-cmake@latest

      - name: Configure
        run: cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

      - name: Build plugin
        run: cmake --build build --config Release --target Theremin_VST3 --parallel

      - name: Collect VST3 artifact
        shell: pwsh
        run: |
          $vst3 = Get-ChildItem -Path build/Theremin_artefacts/Release/VST3 -Filter "Theremin.vst3" -Directory | Select-Object -First 1
          if (-not $vst3) { throw "VST3 not produced" }
          Compress-Archive -Path $vst3.FullName -DestinationPath Theremin-vst3-windows.zip

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: Theremin-vst3-windows
          path: Theremin-vst3-windows.zip
          retention-days: 30

      - name: Publish GitHub Release (on tag)
        if: startsWith(github.ref, 'refs/tags/v')
        uses: softprops/action-gh-release@v2
        with:
          files: Theremin-vst3-windows.zip
          generate_release_notes: true
```

- [ ] **Step 2.2: Commit and push**

```bash
git add .github/workflows/build.yml
git commit -m "ci: build Theremin.vst3 on windows-latest via GitHub Actions"
git push
```

- [ ] **Step 2.3: Verify CI pass**

Open `https://github.com/brebuilds/theremin-vst/actions` and confirm the `Build / build-windows` job succeeds. Download the `Theremin-vst3-windows` artifact, unzip, verify `Theremin.vst3` directory exists inside.

Expected: green checkmark, artifact downloadable. If this fails, fix the build config before any further work — every subsequent task depends on this being green.

---

## Task 3: Plugin parameters (AudioProcessorValueTreeState)

**Goal:** All user-controllable values (octaves, magnetism, scale, key, MIDI channel, enabled) live in JUCE's parameter system. Parameters appear in the DAW's automation list. No UI yet — just the parameter scaffold.

**Files:**
- Create: `src/PluginState.h`
- Modify: `src/PluginProcessor.h` — add APVTS member
- Modify: `src/PluginProcessor.cpp` — initialize APVTS, wire state save/load

- [ ] **Step 3.1: Create `src/PluginState.h` with parameter ID constants**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ThereminParams {
    inline constexpr const char* octaves   = "octaves";    // int, 1..5
    inline constexpr const char* magnetism = "magnetism";  // float, 0..1
    inline constexpr const char* scale     = "scale";      // choice
    inline constexpr const char* key       = "key";        // choice, 0..11 (C..B)
    inline constexpr const char* channel   = "channel";    // int, 1..16
    inline constexpr const char* enabled   = "enabled";    // bool

    inline const juce::StringArray scaleNames {
        "Chromatic", "Major", "Minor", "Pentatonic Major", "Pentatonic Minor", "Blues"
    };

    inline const juce::StringArray keyNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> params;

        params.push_back(std::make_unique<AudioParameterInt>(
            ParameterID{ octaves, 1 }, "Octaves", 1, 5, 2));

        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{ magnetism, 1 }, "Magnetism",
            NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

        params.push_back(std::make_unique<AudioParameterChoice>(
            ParameterID{ scale, 1 }, "Scale", scaleNames, 0));

        params.push_back(std::make_unique<AudioParameterChoice>(
            ParameterID{ key, 1 }, "Key", keyNames, 0));

        params.push_back(std::make_unique<AudioParameterInt>(
            ParameterID{ channel, 1 }, "MIDI Channel", 1, 16, 1));

        params.push_back(std::make_unique<AudioParameterBool>(
            ParameterID{ enabled, 1 }, "Enabled", true));

        return { params.begin(), params.end() };
    }
}
```

- [ ] **Step 3.2: Add APVTS to `PluginProcessor.h`**

Modify `src/PluginProcessor.h`, replace class body additions:

```cpp
// Add include near top:
#include "PluginState.h"

// Inside class, public:
    juce::AudioProcessorValueTreeState apvts;
```

Replace the constructor declaration to accept no params (unchanged), and change state methods:

```cpp
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
```

- [ ] **Step 3.3: Update `PluginProcessor.cpp`**

Modify the constructor and state methods:

```cpp
ThereminAudioProcessor::ThereminAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Theremin", ThereminParams::createLayout())
{
}

void ThereminAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ThereminAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

- [ ] **Step 3.4: Build and confirm**

```bash
cmake --build build --config Debug --target Theremin_Standalone
```

Expected: clean build, no warnings about unused APVTS.

- [ ] **Step 3.5: Commit**

```bash
git add src/PluginState.h src/PluginProcessor.h src/PluginProcessor.cpp
git commit -m "feat: add APVTS with octaves, magnetism, scale, key, channel, enabled params"
git push
```

---

## Task 4: Pure-logic module — Calibration (with unit tests)

**Goal:** The math that converts a 4-corner hand capture into a trapezoidal-warp "hand position → unit square" function, isolated and tested. No JUCE dependency.

**Files:**
- Create: `src/mapping/Calibration.h`
- Create: `src/mapping/Calibration.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_main.cpp`
- Create: `tests/test_calibration.cpp`
- Modify: `CMakeLists.txt` — add tests subdirectory

- [ ] **Step 4.1: Add Catch2 as a FetchContent dependency in root CMake**

Modify `CMakeLists.txt`, add before `juce_add_plugin`:

```cmake
# Tests
include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 4.2: Create `tests/CMakeLists.txt`**

```cmake
add_executable(theremin_tests
    test_main.cpp
    test_calibration.cpp
)

target_include_directories(theremin_tests PRIVATE ../src)

target_sources(theremin_tests PRIVATE
    ../src/mapping/Calibration.cpp
)

target_link_libraries(theremin_tests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(theremin_tests)
```

- [ ] **Step 4.3: Create `tests/test_main.cpp`**

```cpp
// Single-file test runner; Catch2::Catch2WithMain provides main().
// This file is intentionally empty — it exists to reserve a slot
// if we ever want custom test session hooks.
```

- [ ] **Step 4.4: Write the failing calibration test first**

Create `tests/test_calibration.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "mapping/Calibration.h"

using Catch::Approx;

TEST_CASE("Calibration::map returns (0,0) at top-left corner", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners(
        {0.1f, 0.1f},  // top-left
        {0.9f, 0.1f},  // top-right
        {0.9f, 0.9f},  // bottom-right
        {0.1f, 0.9f}); // bottom-left

    auto result = cal.map({0.1f, 0.1f});
    REQUIRE(result.x == Approx(0.0f).margin(0.001f));
    REQUIRE(result.y == Approx(0.0f).margin(0.001f));
}

TEST_CASE("Calibration::map returns (1,1) at bottom-right corner", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners({0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f});

    auto result = cal.map({0.9f, 0.9f});
    REQUIRE(result.x == Approx(1.0f).margin(0.001f));
    REQUIRE(result.y == Approx(1.0f).margin(0.001f));
}

TEST_CASE("Calibration::map returns (0.5, 0.5) at center of rectangular bounds", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners({0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f});

    auto result = cal.map({0.5f, 0.5f});
    REQUIRE(result.x == Approx(0.5f).margin(0.001f));
    REQUIRE(result.y == Approx(0.5f).margin(0.001f));
}

TEST_CASE("Calibration::map clamps values outside the quad to [0,1]", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners({0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f});

    auto below = cal.map({0.0f, 0.0f});
    REQUIRE(below.x >= 0.0f);
    REQUIRE(below.y >= 0.0f);

    auto above = cal.map({1.0f, 1.0f});
    REQUIRE(above.x <= 1.0f);
    REQUIRE(above.y <= 1.0f);
}

TEST_CASE("Calibration::map handles a trapezoidal quad (top narrower than bottom)", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners(
        {0.3f, 0.1f},  // top-left (narrower top)
        {0.7f, 0.1f},  // top-right
        {0.9f, 0.9f},  // bottom-right (wider bottom)
        {0.1f, 0.9f}); // bottom-left

    // Point inside the quad should map inside [0,1]x[0,1]
    auto result = cal.map({0.5f, 0.5f});
    REQUIRE(result.x >= 0.0f);
    REQUIRE(result.x <= 1.0f);
    REQUIRE(result.y >= 0.0f);
    REQUIRE(result.y <= 1.0f);
}

TEST_CASE("Calibration default (no corners set) maps input identically", "[calibration]") {
    theremin::Calibration cal;
    auto result = cal.map({0.42f, 0.73f});
    REQUIRE(result.x == Approx(0.42f));
    REQUIRE(result.y == Approx(0.73f));
}
```

- [ ] **Step 4.5: Run tests to verify they fail with "file not found"**

```bash
cmake -B build -G "Xcode" -DBUILD_TESTING=ON
cmake --build build --target theremin_tests 2>&1 | head -30
```

Expected: compile error — `mapping/Calibration.h` not found.

- [ ] **Step 4.6: Write minimal `src/mapping/Calibration.h`**

```cpp
#pragma once

namespace theremin {

struct Point2D {
    float x;
    float y;
};

class Calibration {
public:
    Calibration() = default;

    // Set the four corners in normalized image coords (0..1).
    // Order: top-left, top-right, bottom-right, bottom-left.
    void setCorners(Point2D tl, Point2D tr, Point2D br, Point2D bl);

    // Map a hand-position point (normalized image coords) into the calibrated
    // unit square. Output is clamped to [0,1] x [0,1].
    // If setCorners has not been called, returns input unchanged (clamped).
    Point2D map(Point2D input) const;

    bool isCalibrated() const { return calibrated; }

private:
    Point2D tl{0.0f, 0.0f};
    Point2D tr{1.0f, 0.0f};
    Point2D br{1.0f, 1.0f};
    Point2D bl{0.0f, 1.0f};
    bool calibrated = false;
};

} // namespace theremin
```

- [ ] **Step 4.7: Write `src/mapping/Calibration.cpp`**

The math here is an inverse bilinear interpolation: given four corners of a quad and a point inside, find the (u, v) parameters in [0,1]² such that `bilerp(tl, tr, br, bl, u, v) == point`.

```cpp
#include "Calibration.h"
#include <algorithm>
#include <cmath>

namespace theremin {

namespace {
    float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

    // Solve for (u, v) such that:
    //   P = (1-u)(1-v)*tl + u(1-v)*tr + u*v*br + (1-u)*v*bl
    // via iterative Newton-Raphson (converges in ~5 iters for reasonable quads).
    Point2D inverseBilerp(Point2D p, Point2D tl, Point2D tr, Point2D br, Point2D bl)
    {
        float u = 0.5f, v = 0.5f;
        for (int i = 0; i < 12; ++i) {
            float one_minus_u = 1.0f - u;
            float one_minus_v = 1.0f - v;

            float Px = one_minus_u * one_minus_v * tl.x
                     + u * one_minus_v * tr.x
                     + u * v * br.x
                     + one_minus_u * v * bl.x;
            float Py = one_minus_u * one_minus_v * tl.y
                     + u * one_minus_v * tr.y
                     + u * v * br.y
                     + one_minus_u * v * bl.y;

            float dPx_du = -one_minus_v * tl.x + one_minus_v * tr.x + v * br.x - v * bl.x;
            float dPx_dv = -one_minus_u * tl.x - u * tr.x + u * br.x + one_minus_u * bl.x;
            float dPy_du = -one_minus_v * tl.y + one_minus_v * tr.y + v * br.y - v * bl.y;
            float dPy_dv = -one_minus_u * tl.y - u * tr.y + u * br.y + one_minus_u * bl.y;

            float det = dPx_du * dPy_dv - dPx_dv * dPy_du;
            if (std::abs(det) < 1e-8f) break;

            float ex = Px - p.x;
            float ey = Py - p.y;

            float du = (dPy_dv * ex - dPx_dv * ey) / det;
            float dv = (-dPy_du * ex + dPx_du * ey) / det;

            u -= du;
            v -= dv;

            if (std::abs(du) < 1e-5f && std::abs(dv) < 1e-5f) break;
        }
        return { clamp01(u), clamp01(v) };
    }
}

void Calibration::setCorners(Point2D tl_, Point2D tr_, Point2D br_, Point2D bl_)
{
    tl = tl_;
    tr = tr_;
    br = br_;
    bl = bl_;
    calibrated = true;
}

Point2D Calibration::map(Point2D input) const
{
    if (!calibrated) {
        return { clamp01(input.x), clamp01(input.y) };
    }
    return inverseBilerp(input, tl, tr, br, bl);
}

} // namespace theremin
```

- [ ] **Step 4.8: Build and run tests, confirm pass**

```bash
cmake --build build --target theremin_tests
./build/tests/theremin_tests
```

Expected: all 6 test cases pass.

- [ ] **Step 4.9: Commit**

```bash
git add CMakeLists.txt tests/ src/mapping/
git commit -m "feat: calibration math with unit tests (4-corner trapezoidal warp)"
git push
```

---

## Task 5: Pure-logic module — Scales

**Goal:** Given a root key (0..11 for C..B) and a scale name, produce the function "is MIDI note N legal? And if not, what's the nearest legal note?" Isolated and tested.

**Files:**
- Create: `src/mapping/Scales.h`
- Create: `src/mapping/Scales.cpp`
- Create: `tests/test_scales.cpp`
- Modify: `tests/CMakeLists.txt` — add `test_scales.cpp` + `Scales.cpp` to sources

- [ ] **Step 5.1: Write the failing tests first**

Create `tests/test_scales.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "mapping/Scales.h"

using namespace theremin;

TEST_CASE("Chromatic scale accepts all notes", "[scales]") {
    Scale s(ScaleKind::Chromatic, 0); // C chromatic
    for (int n = 0; n < 128; ++n) {
        REQUIRE(s.isLegalNote(n));
        REQUIRE(s.snapToLegal(n) == n);
    }
}

TEST_CASE("C Major accepts only white-key notes", "[scales]") {
    Scale s(ScaleKind::Major, 0); // C major
    // C D E F G A B in the 4th octave (MIDI 60..71)
    REQUIRE(s.isLegalNote(60)); // C
    REQUIRE_FALSE(s.isLegalNote(61)); // C#
    REQUIRE(s.isLegalNote(62)); // D
    REQUIRE_FALSE(s.isLegalNote(63)); // D#
    REQUIRE(s.isLegalNote(64)); // E
    REQUIRE(s.isLegalNote(65)); // F
    REQUIRE_FALSE(s.isLegalNote(66)); // F#
    REQUIRE(s.isLegalNote(67)); // G
    REQUIRE_FALSE(s.isLegalNote(68)); // G#
    REQUIRE(s.isLegalNote(69)); // A
    REQUIRE_FALSE(s.isLegalNote(70)); // A#
    REQUIRE(s.isLegalNote(71)); // B
}

TEST_CASE("C Major snaps C# down to C", "[scales]") {
    Scale s(ScaleKind::Major, 0);
    REQUIRE(s.snapToLegal(61) == 60);
}

TEST_CASE("A Minor accepts A B C D E F G", "[scales]") {
    Scale s(ScaleKind::Minor, 9); // A = key 9
    REQUIRE(s.isLegalNote(69)); // A
    REQUIRE(s.isLegalNote(71)); // B
    REQUIRE(s.isLegalNote(72)); // C
    REQUIRE(s.isLegalNote(74)); // D
    REQUIRE(s.isLegalNote(76)); // E
    REQUIRE(s.isLegalNote(77)); // F
    REQUIRE(s.isLegalNote(79)); // G
    REQUIRE_FALSE(s.isLegalNote(70)); // A#
    REQUIRE_FALSE(s.isLegalNote(73)); // C#
}

TEST_CASE("C Pentatonic Major has 5 notes per octave", "[scales]") {
    Scale s(ScaleKind::PentatonicMajor, 0);
    int count = 0;
    for (int n = 60; n < 72; ++n) if (s.isLegalNote(n)) ++count;
    REQUIRE(count == 5);
}

TEST_CASE("Blues scale at C includes the b5 blue note", "[scales]") {
    Scale s(ScaleKind::Blues, 0);
    REQUIRE(s.isLegalNote(66)); // F# = b5 above C
}

TEST_CASE("snapToLegal returns closest legal note when input is between two", "[scales]") {
    Scale s(ScaleKind::Major, 0); // C major; D=62 and E=64 are legal, D#=63 is not
    // D# snaps to D (distance 1) not E (distance 1) — ties broken toward lower
    REQUIRE(s.snapToLegal(63) == 62);
}
```

- [ ] **Step 5.2: Update `tests/CMakeLists.txt` to include the new test + source**

```cmake
add_executable(theremin_tests
    test_main.cpp
    test_calibration.cpp
    test_scales.cpp
)

target_include_directories(theremin_tests PRIVATE ../src)

target_sources(theremin_tests PRIVATE
    ../src/mapping/Calibration.cpp
    ../src/mapping/Scales.cpp
)

target_link_libraries(theremin_tests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(theremin_tests)
```

- [ ] **Step 5.3: Run tests, verify compile failure**

```bash
cmake --build build --target theremin_tests 2>&1 | head -20
```

Expected: "Scales.h: No such file or directory"

- [ ] **Step 5.4: Write `src/mapping/Scales.h`**

```cpp
#pragma once
#include <array>

namespace theremin {

enum class ScaleKind {
    Chromatic = 0,
    Major,
    Minor,
    PentatonicMajor,
    PentatonicMinor,
    Blues
};

class Scale {
public:
    Scale(ScaleKind kind, int rootKey);

    // Is this MIDI note (0..127) in the scale?
    bool isLegalNote(int midiNote) const;

    // Return the closest legal MIDI note (ties broken toward the lower note).
    int snapToLegal(int midiNote) const;

private:
    ScaleKind kind;
    int rootKey; // 0..11 (C..B)
    std::array<bool, 12> legalPitchClasses{}; // which of C..B are in scale
};

} // namespace theremin
```

- [ ] **Step 5.5: Write `src/mapping/Scales.cpp`**

```cpp
#include "Scales.h"
#include <algorithm>

namespace theremin {

namespace {
    // Semitone offsets from root for each scale kind.
    std::array<bool, 12> buildMask(ScaleKind kind) {
        std::array<bool, 12> mask{};
        auto mark = [&](std::initializer_list<int> semitones) {
            for (int s : semitones) mask[s] = true;
        };
        switch (kind) {
            case ScaleKind::Chromatic:
                for (int i = 0; i < 12; ++i) mask[i] = true;
                break;
            case ScaleKind::Major:
                mark({0, 2, 4, 5, 7, 9, 11}); // Ionian
                break;
            case ScaleKind::Minor:
                mark({0, 2, 3, 5, 7, 8, 10}); // Natural minor (Aeolian)
                break;
            case ScaleKind::PentatonicMajor:
                mark({0, 2, 4, 7, 9});
                break;
            case ScaleKind::PentatonicMinor:
                mark({0, 3, 5, 7, 10});
                break;
            case ScaleKind::Blues:
                mark({0, 3, 5, 6, 7, 10}); // minor pentatonic + b5
                break;
        }
        return mask;
    }
}

Scale::Scale(ScaleKind k, int root)
    : kind(k), rootKey(((root % 12) + 12) % 12), legalPitchClasses(buildMask(k))
{}

bool Scale::isLegalNote(int midiNote) const
{
    if (midiNote < 0 || midiNote > 127) return false;
    int pc = ((midiNote - rootKey) % 12 + 12) % 12;
    return legalPitchClasses[pc];
}

int Scale::snapToLegal(int midiNote) const
{
    if (isLegalNote(midiNote)) return midiNote;

    // Search outward from the input note. Ties broken toward the lower note
    // by testing the lower offset first at each distance.
    for (int dist = 1; dist < 12; ++dist) {
        int down = midiNote - dist;
        if (down >= 0 && isLegalNote(down)) return down;
        int up = midiNote + dist;
        if (up <= 127 && isLegalNote(up)) return up;
    }
    return midiNote;
}

} // namespace theremin
```

- [ ] **Step 5.6: Build and run tests, confirm pass**

```bash
cmake --build build --target theremin_tests
./build/tests/theremin_tests "[scales]"
```

Expected: all scale tests pass.

- [ ] **Step 5.7: Commit**

```bash
git add tests/ src/mapping/
git commit -m "feat: scale quantization (chromatic, major, minor, pentatonic x2, blues)"
git push
```

---

## Task 6: Pure-logic module — GestureMapper

**Goal:** Combine calibration + scales + octave range + magnetism into "given normalized (x, y) and current params, produce desired MIDI state (note, pitch bend, velocity, CC7, CC74)." Pure function, fully testable.

**Files:**
- Create: `src/mapping/GestureMapper.h`
- Create: `src/mapping/GestureMapper.cpp`
- Create: `tests/test_gesture_mapper.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 6.1: Write `src/mapping/GestureMapper.h`**

```cpp
#pragma once
#include "Calibration.h"
#include "Scales.h"

namespace theremin {

struct MidiState {
    bool active = false;
    int note = 60;           // MIDI note (0..127)
    int pitchBend = 8192;    // 0..16383, center = 8192
    int velocity = 100;      // 0..127
    int cc7Volume = 100;     // 0..127
    int cc74Filter = 64;     // 0..127
};

struct GestureParams {
    int octaves = 2;           // 1..5
    float magnetism = 0.8f;    // 0..1
    ScaleKind scaleKind = ScaleKind::Chromatic;
    int rootKey = 0;           // 0..11
};

class GestureMapper {
public:
    // Map a calibrated (x, y) ∈ [0,1]^2 to MIDI state.
    // x = 0 → lowest note (A2, MIDI 45). x = 1 → highest note (A2 + octaves*12).
    // y = 0 → top of frame → loud. y = 1 → bottom → silent.
    static MidiState map(float x, float y, GestureParams params);

    // Called when the hand leaves the frame — emit an "inactive" state.
    static MidiState inactive();
};

} // namespace theremin
```

- [ ] **Step 6.2: Write the tests**

Create `tests/test_gesture_mapper.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "mapping/GestureMapper.h"

using namespace theremin;
using Catch::Approx;

TEST_CASE("GestureMapper x=0 yields lowest note A2 (MIDI 45)", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.0f, 0.5f, p);
    REQUIRE(s.active);
    REQUIRE(s.note == 45);
}

TEST_CASE("GestureMapper x=1 at 2 octaves yields A4 (MIDI 69)", "[mapper]") {
    GestureParams p{2, 1.0f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(1.0f, 0.5f, p);
    REQUIRE(s.note == 69); // 45 + 24
}

TEST_CASE("GestureMapper x=1 at 5 octaves yields A7 (MIDI 105)", "[mapper]") {
    GestureParams p{5, 1.0f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(1.0f, 0.5f, p);
    REQUIRE(s.note == 105); // 45 + 60
}

TEST_CASE("GestureMapper y=0 (top) yields max volume CC7=127", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f, 0.0f, p);
    REQUIRE(s.cc7Volume == 127);
}

TEST_CASE("GestureMapper y=1 (bottom) yields near-zero volume", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f, 1.0f, p);
    REQUIRE(s.cc7Volume <= 5);
}

TEST_CASE("GestureMapper magnetism=1 produces no pitch bend", "[mapper]") {
    GestureParams p{2, 1.0f, ScaleKind::Chromatic, 0};
    // Half-semitone between A2 and A#2 → would normally be a pitch bend,
    // but with magnetism=1 it snaps hard.
    auto s = GestureMapper::map(0.5f / 24.0f, 0.5f, p);
    REQUIRE(s.pitchBend == 8192);
}

TEST_CASE("GestureMapper magnetism=0 produces pitch bend between semitones", "[mapper]") {
    GestureParams p{2, 0.0f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f / 24.0f, 0.5f, p);
    REQUIRE(s.pitchBend != 8192);
}

TEST_CASE("GestureMapper scale lock prevents illegal notes", "[mapper]") {
    GestureParams p{2, 1.0f, ScaleKind::Major, 0}; // C major
    // Position 1/24 = ~A#2 which is not in C major. Should snap to A2 or B2.
    auto s = GestureMapper::map(1.0f / 24.0f, 0.5f, p);
    Scale scale(ScaleKind::Major, 0);
    REQUIRE(scale.isLegalNote(s.note));
}

TEST_CASE("GestureMapper inactive returns active=false", "[mapper]") {
    auto s = GestureMapper::inactive();
    REQUIRE_FALSE(s.active);
}

TEST_CASE("GestureMapper velocity stays within 40..127 floor for audible notes", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f, 1.0f, p); // bottom of frame
    REQUIRE(s.velocity >= 40);
}
```

- [ ] **Step 6.3: Update `tests/CMakeLists.txt`**

```cmake
add_executable(theremin_tests
    test_main.cpp
    test_calibration.cpp
    test_scales.cpp
    test_gesture_mapper.cpp
)

target_include_directories(theremin_tests PRIVATE ../src)

target_sources(theremin_tests PRIVATE
    ../src/mapping/Calibration.cpp
    ../src/mapping/Scales.cpp
    ../src/mapping/GestureMapper.cpp
)

target_link_libraries(theremin_tests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(theremin_tests)
```

- [ ] **Step 6.4: Write `src/mapping/GestureMapper.cpp`**

```cpp
#include "GestureMapper.h"
#include <algorithm>
#include <cmath>

namespace theremin {

namespace {
    constexpr int kLowestNote = 45; // A2

    int clampI(int v, int lo, int hi) {
        return std::max(lo, std::min(hi, v));
    }

    float clampF(float v, float lo, float hi) {
        return std::max(lo, std::min(hi, v));
    }
}

MidiState GestureMapper::map(float x, float y, GestureParams p)
{
    x = clampF(x, 0.0f, 1.0f);
    y = clampF(y, 0.0f, 1.0f);

    const int octaves = clampI(p.octaves, 1, 5);
    const int totalSemitones = octaves * 12;

    // Continuous pitch position, in semitones above the lowest note.
    float contNote = x * static_cast<float>(totalSemitones);

    // Quantize to nearest semitone within the scale.
    Scale scale(p.scaleKind, p.rootKey);
    int baseNote = kLowestNote + static_cast<int>(std::round(contNote));
    baseNote = scale.snapToLegal(baseNote);
    baseNote = clampI(baseNote, 0, 127);

    // Raw continuous MIDI note (for pitch bend math).
    float rawMidi = kLowestNote + contNote;

    // Pitch bend: how far the raw pitch is from the quantized note, in semitones.
    // Magnetism blends between pure-bend (0) and pure-snap (1).
    float bendSemis = (rawMidi - static_cast<float>(baseNote)) * (1.0f - p.magnetism);
    // Clamp to ±2 semitones (our pitch-bend range).
    bendSemis = clampF(bendSemis, -2.0f, 2.0f);
    int pitchBend = 8192 + static_cast<int>(bendSemis / 2.0f * 8191.0f);
    pitchBend = clampI(pitchBend, 0, 16383);

    // Volume (CC7): top of frame = loud, bottom = quiet.
    float vol = 1.0f - y;
    int cc7 = static_cast<int>(std::round(vol * 127.0f));
    cc7 = clampI(cc7, 0, 127);

    // Filter cutoff (CC74): mirrors the web prototype's filter-follows-pitch.
    // Base curve: bright when high pitch + top-of-frame, dark when low pitch + bottom.
    float filter = x * 0.5f + (1.0f - y) * 0.5f;
    int cc74 = static_cast<int>(std::round(filter * 127.0f));
    cc74 = clampI(cc74, 0, 127);

    // Velocity: floor at 40 so hand presence always plays audibly,
    // but vary with Y so soft playing is possible.
    int vel = 40 + static_cast<int>(std::round((1.0f - y) * 87.0f));
    vel = clampI(vel, 40, 127);

    MidiState s;
    s.active = true;
    s.note = baseNote;
    s.pitchBend = pitchBend;
    s.velocity = vel;
    s.cc7Volume = cc7;
    s.cc74Filter = cc74;
    return s;
}

MidiState GestureMapper::inactive()
{
    MidiState s;
    s.active = false;
    return s;
}

} // namespace theremin
```

- [ ] **Step 6.5: Build and run tests, confirm pass**

```bash
cmake --build build --target theremin_tests
./build/tests/theremin_tests "[mapper]"
```

Expected: all 10 mapper tests pass.

- [ ] **Step 6.6: Commit**

```bash
git add tests/ src/mapping/
git commit -m "feat: GestureMapper (x,y + params → MIDI state) with unit tests"
git push
```

---

## Task 7: Pure-logic module — MidiEmitter (audio-thread-safe)

**Goal:** Given a `MidiState` target and the previous `MidiState`, produce a list of `juce::MidiMessage`s to emit this block. No allocations on the audio thread. Stuck-note safe. Testable in isolation (no audio thread required — just test the diff logic).

**Files:**
- Create: `src/midi/MidiEmitter.h`
- Create: `src/midi/MidiEmitter.cpp`
- Create: `tests/test_midi_emitter.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 7.1: Write `src/midi/MidiEmitter.h`**

```cpp
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../mapping/GestureMapper.h"

namespace theremin {

class MidiEmitter {
public:
    // Apply the desired state. Any resulting MIDI events are written to `buffer`
    // at sample position 0. `channel` is 1..16.
    // This method does not allocate.
    void apply(const MidiState& target, juce::MidiBuffer& buffer, int channel);

    // Force "all notes off" on the current channel. Used on plugin reset and
    // on hand-lost timeout.
    void panic(juce::MidiBuffer& buffer, int channel);

    // Reset state without emitting any MIDI (e.g., when apvts resets).
    void reset();

private:
    MidiState last;
    bool lastChannelValid = false;
    int lastChannel = 1;
};

} // namespace theremin
```

- [ ] **Step 7.2: Write the tests**

Create `tests/test_midi_emitter.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include "midi/MidiEmitter.h"

using namespace theremin;

namespace {
    // Count MIDI events of a specific type in a buffer.
    int countNoteOn(const juce::MidiBuffer& b) {
        int n = 0;
        for (const auto meta : b) if (meta.getMessage().isNoteOn()) ++n;
        return n;
    }
    int countNoteOff(const juce::MidiBuffer& b) {
        int n = 0;
        for (const auto meta : b) if (meta.getMessage().isNoteOff()) ++n;
        return n;
    }
}

TEST_CASE("MidiEmitter emits note-on when going from inactive to active", "[midi]") {
    MidiEmitter e;
    juce::MidiBuffer buf;
    MidiState s;
    s.active = true;
    s.note = 60;
    s.velocity = 100;
    s.pitchBend = 8192;
    s.cc7Volume = 100;
    s.cc74Filter = 64;

    e.apply(s, buf, 1);
    REQUIRE(countNoteOn(buf) == 1);
    REQUIRE(countNoteOff(buf) == 0);
}

TEST_CASE("MidiEmitter emits note-off when note changes", "[midi]") {
    MidiEmitter e;
    juce::MidiBuffer buf;

    MidiState s1; s1.active = true; s1.note = 60; s1.velocity = 100; s1.pitchBend = 8192;
    e.apply(s1, buf, 1);
    buf.clear();

    MidiState s2 = s1; s2.note = 62;
    e.apply(s2, buf, 1);
    REQUIRE(countNoteOff(buf) == 1); // old note off
    REQUIRE(countNoteOn(buf) == 1);  // new note on
}

TEST_CASE("MidiEmitter emits no note events when note is unchanged", "[midi]") {
    MidiEmitter e;
    juce::MidiBuffer buf;

    MidiState s; s.active = true; s.note = 60; s.velocity = 100; s.pitchBend = 8192;
    e.apply(s, buf, 1);
    buf.clear();

    // Same state — only CCs/bend might change but the note should not.
    s.cc7Volume = 80;
    e.apply(s, buf, 1);
    REQUIRE(countNoteOn(buf) == 0);
    REQUIRE(countNoteOff(buf) == 0);
}

TEST_CASE("MidiEmitter emits note-off when going from active to inactive", "[midi]") {
    MidiEmitter e;
    juce::MidiBuffer buf;

    MidiState on; on.active = true; on.note = 60; on.velocity = 100;
    e.apply(on, buf, 1);
    buf.clear();

    MidiState off = MidiEmitter().inactive_state();  // helper; see note below
    // Use GestureMapper::inactive() instead to avoid a new API.
    off = GestureMapper::inactive();
    e.apply(off, buf, 1);
    REQUIRE(countNoteOff(buf) == 1);
}

TEST_CASE("MidiEmitter panic emits all-notes-off CC", "[midi]") {
    MidiEmitter e;
    juce::MidiBuffer buf;
    e.panic(buf, 1);
    bool foundAllNotesOff = false;
    for (const auto meta : buf) {
        const auto& m = meta.getMessage();
        if (m.isController() && m.getControllerNumber() == 123) {
            foundAllNotesOff = true;
        }
    }
    REQUIRE(foundAllNotesOff);
}
```

Note: one test references `inactive_state()` — we'll use `GestureMapper::inactive()` instead. Fixed inline in Step 7.2 (line edited).

- [ ] **Step 7.3: Write `src/midi/MidiEmitter.cpp`**

```cpp
#include "MidiEmitter.h"

namespace theremin {

namespace {
    juce::MidiMessage makeNoteOn(int note, int velocity, int channel) {
        return juce::MidiMessage::noteOn(channel, note, (juce::uint8)velocity);
    }
    juce::MidiMessage makeNoteOff(int note, int channel) {
        return juce::MidiMessage::noteOff(channel, note);
    }
    juce::MidiMessage makePitchBend(int bend, int channel) {
        return juce::MidiMessage::pitchWheel(channel, bend);
    }
    juce::MidiMessage makeCC(int cc, int value, int channel) {
        return juce::MidiMessage::controllerEvent(channel, cc, value);
    }
    juce::MidiMessage allNotesOff(int channel) {
        return juce::MidiMessage::controllerEvent(channel, 123, 0);
    }
}

void MidiEmitter::apply(const MidiState& t, juce::MidiBuffer& buf, int channel)
{
    // Channel change → panic the old channel first.
    if (lastChannelValid && lastChannel != channel) {
        if (last.active) {
            buf.addEvent(makeNoteOff(last.note, lastChannel), 0);
        }
        buf.addEvent(allNotesOff(lastChannel), 0);
        last = MidiState{};
    }

    lastChannel = channel;
    lastChannelValid = true;

    // Transitions:
    if (!last.active && t.active) {
        // inactive → active: note-on + bend + CCs
        buf.addEvent(makeNoteOn(t.note, t.velocity, channel), 0);
        buf.addEvent(makePitchBend(t.pitchBend, channel), 0);
        buf.addEvent(makeCC(7, t.cc7Volume, channel), 0);
        buf.addEvent(makeCC(74, t.cc74Filter, channel), 0);
    } else if (last.active && !t.active) {
        // active → inactive
        buf.addEvent(makeNoteOff(last.note, channel), 0);
        buf.addEvent(makeCC(7, 0, channel), 0);
    } else if (last.active && t.active) {
        // active → active: check for note change, CC changes, bend changes
        if (last.note != t.note) {
            buf.addEvent(makeNoteOff(last.note, channel), 0);
            buf.addEvent(makeNoteOn(t.note, t.velocity, channel), 0);
        }
        if (last.pitchBend != t.pitchBend) {
            buf.addEvent(makePitchBend(t.pitchBend, channel), 0);
        }
        if (last.cc7Volume != t.cc7Volume) {
            buf.addEvent(makeCC(7, t.cc7Volume, channel), 0);
        }
        if (last.cc74Filter != t.cc74Filter) {
            buf.addEvent(makeCC(74, t.cc74Filter, channel), 0);
        }
    }
    // inactive → inactive: no events

    last = t;
}

void MidiEmitter::panic(juce::MidiBuffer& buf, int channel)
{
    if (last.active) {
        buf.addEvent(makeNoteOff(last.note, channel), 0);
    }
    buf.addEvent(allNotesOff(channel), 0);
    last = MidiState{};
    lastChannel = channel;
    lastChannelValid = true;
}

void MidiEmitter::reset()
{
    last = MidiState{};
    lastChannelValid = false;
}

} // namespace theremin
```

- [ ] **Step 7.4: Fix the test to use `GestureMapper::inactive()`**

Before running tests, ensure `test_midi_emitter.cpp` line `off = MidiEmitter().inactive_state();` is REPLACED with `off = GestureMapper::inactive();`. Also remove the confusing comment. The test should look like:

```cpp
TEST_CASE("MidiEmitter emits note-off when going from active to inactive", "[midi]") {
    MidiEmitter e;
    juce::MidiBuffer buf;

    MidiState on; on.active = true; on.note = 60; on.velocity = 100;
    e.apply(on, buf, 1);
    buf.clear();

    MidiState off = GestureMapper::inactive();
    e.apply(off, buf, 1);
    REQUIRE(countNoteOff(buf) == 1);
}
```

- [ ] **Step 7.5: Update `tests/CMakeLists.txt`**

```cmake
add_executable(theremin_tests
    test_main.cpp
    test_calibration.cpp
    test_scales.cpp
    test_gesture_mapper.cpp
    test_midi_emitter.cpp
)

target_include_directories(theremin_tests PRIVATE ../src)

target_sources(theremin_tests PRIVATE
    ../src/mapping/Calibration.cpp
    ../src/mapping/Scales.cpp
    ../src/mapping/GestureMapper.cpp
    ../src/midi/MidiEmitter.cpp
)

target_link_libraries(theremin_tests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_audio_basics
    juce::juce_core
)

include(Catch)
catch_discover_tests(theremin_tests)
```

- [ ] **Step 7.6: Build and run tests**

```bash
cmake --build build --target theremin_tests
./build/tests/theremin_tests "[midi]"
```

Expected: all 5 midi-emitter tests pass.

- [ ] **Step 7.7: Commit**

```bash
git add tests/ src/midi/
git commit -m "feat: MidiEmitter with stuck-note-safe diffing + panic"
git push
```

---

## Task 8: Integrate MidiEmitter into `processBlock` with a debug oscillator

**Goal:** Temporarily drive the plugin with a `std::atomic<float>` "fake hand X position" controlled by a UI slider. Confirm in a DAW that MIDI notes come out and drive a downstream synth. No camera yet.

**Files:**
- Modify: `src/PluginProcessor.h` — add MidiEmitter, atomic fake-position
- Modify: `src/PluginProcessor.cpp` — wire fake-position → GestureMapper → MidiEmitter in processBlock
- Modify: `src/PluginEditor.cpp` — add two horizontal sliders (debug X, debug Y) bound to the atomics

- [ ] **Step 8.1: Add members to `PluginProcessor.h`**

Inside the class, public section:

```cpp
    std::atomic<float> debugX { 0.5f };
    std::atomic<float> debugY { 0.5f };
    std::atomic<bool>  debugHandPresent { false };

private:
    MidiEmitter midiEmitter;
```

Add include:

```cpp
#include "midi/MidiEmitter.h"
```

(The `MidiEmitter` type is in `theremin::` namespace. Add `using theremin::MidiEmitter;` inside the class or fully qualify.)

- [ ] **Step 8.2: Wire `processBlock` in `PluginProcessor.cpp`**

Replace the existing `processBlock`:

```cpp
#include "mapping/GestureMapper.h"

void ThereminAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const bool enabled = apvts.getRawParameterValue(ThereminParams::enabled)->load() > 0.5f;
    if (!enabled) {
        midiEmitter.panic(midi, 1);
        return;
    }

    theremin::GestureParams p;
    p.octaves = static_cast<int>(apvts.getRawParameterValue(ThereminParams::octaves)->load());
    p.magnetism = apvts.getRawParameterValue(ThereminParams::magnetism)->load();
    p.scaleKind = static_cast<theremin::ScaleKind>(
        static_cast<int>(apvts.getRawParameterValue(ThereminParams::scale)->load()));
    p.rootKey = static_cast<int>(apvts.getRawParameterValue(ThereminParams::key)->load());

    int channel = static_cast<int>(apvts.getRawParameterValue(ThereminParams::channel)->load());

    theremin::MidiState state;
    if (debugHandPresent.load()) {
        state = theremin::GestureMapper::map(debugX.load(), debugY.load(), p);
    } else {
        state = theremin::GestureMapper::inactive();
    }

    midiEmitter.apply(state, midi, channel);
}
```

- [ ] **Step 8.3: Add debug sliders in `PluginEditor.cpp`**

Replace the editor body:

```cpp
#include "PluginEditor.h"

ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 400, 1600, 1200);

    xSlider.setRange(0.0, 1.0, 0.001);
    xSlider.setValue(0.5);
    xSlider.onValueChange = [this] { processorRef.debugX.store((float)xSlider.getValue()); };
    addAndMakeVisible(xSlider);

    ySlider.setRange(0.0, 1.0, 0.001);
    ySlider.setValue(0.5);
    ySlider.onValueChange = [this] { processorRef.debugY.store((float)ySlider.getValue()); };
    addAndMakeVisible(ySlider);

    handToggle.setButtonText("Hand present (debug)");
    handToggle.onStateChange = [this] {
        processorRef.debugHandPresent.store(handToggle.getToggleState());
    };
    addAndMakeVisible(handToggle);
}

void ThereminAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(32.0f)));
    g.drawFittedText("THEREMIN", getLocalBounds().removeFromTop(80),
                     juce::Justification::centred, 1);
}

void ThereminAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(80); // title area
    xSlider.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);
    ySlider.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);
    handToggle.setBounds(bounds.removeFromTop(30));
}
```

And add to `PluginEditor.h`:

```cpp
#include <juce_gui_basics/juce_gui_basics.h>

// Inside class, private:
    juce::Slider xSlider;
    juce::Slider ySlider;
    juce::ToggleButton handToggle;
```

- [ ] **Step 8.4: Build standalone and test manually**

```bash
cmake --build build --config Debug --target Theremin_Standalone
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```

Expected: window shows "THEREMIN" + two sliders + toggle. Standalone mode doesn't route MIDI to a synth, so there's no audible output here — that's fine. We'll verify MIDI output via the VST3 build in a DAW.

- [ ] **Step 8.5: Push and verify CI build still passes**

```bash
git add src/ && git commit -m "feat: wire GestureMapper + MidiEmitter in processBlock with debug sliders"
git push
```

Wait for the Actions job to go green. Download the Windows artifact, have Mackie (or a Windows machine) drop it into `C:\Program Files\Common Files\VST3\`, open his DAW, put the plugin on a MIDI track routed to any synth, move the debug X/Y sliders, confirm MIDI notes + CC7 + pitch bend are visible on a MIDI monitor and produce sound.

This is the first "Mackie can actually hear something" milestone.

---

## Task 9: Vendor OpenCV + ONNX Runtime on Windows CI

**Goal:** The Windows runner acquires OpenCV 4.9 and ONNX Runtime 1.17 headers + libs, and the plugin links them successfully (no runtime logic yet — just confirm the link step succeeds).

**Files:**
- Modify: `.github/workflows/build.yml` — provision OpenCV via vcpkg, ORT via GitHub Releases download
- Modify: `CMakeLists.txt` — find_package for both, link to Theremin
- Create: `cmake/FindONNXRuntime.cmake` — helper for locating ORT

- [ ] **Step 9.1: Update `build.yml` to install vcpkg and OpenCV**

Replace the build-windows job body with:

```yaml
  build-windows:
    runs-on: windows-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install CMake
        uses: lukka/get-cmake@latest

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: "2024.02.14"

      - name: Install OpenCV via vcpkg
        run: |
          ${{ env.VCPKG_ROOT }}/vcpkg install opencv4:x64-windows
        shell: pwsh

      - name: Download ONNX Runtime
        shell: pwsh
        run: |
          $ortVersion = "1.17.3"
          $url = "https://github.com/microsoft/onnxruntime/releases/download/v$ortVersion/onnxruntime-win-x64-$ortVersion.zip"
          Invoke-WebRequest -Uri $url -OutFile ort.zip
          Expand-Archive ort.zip -DestinationPath vendor/onnxruntime-staging
          Rename-Item "vendor/onnxruntime-staging/onnxruntime-win-x64-$ortVersion" "vendor/onnxruntime"
          Remove-Item -Recurse vendor/onnxruntime-staging

      - name: Configure
        shell: pwsh
        run: |
          cmake -B build -G "Visual Studio 17 2022" -A x64 `
            -DCMAKE_BUILD_TYPE=Release `
            -DCMAKE_TOOLCHAIN_FILE="${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake" `
            -DONNXRUNTIME_ROOT="${{ github.workspace }}/vendor/onnxruntime"

      - name: Build plugin
        run: cmake --build build --config Release --target Theremin_VST3 --parallel

      - name: Collect VST3 + runtime DLLs
        shell: pwsh
        run: |
          $vst3 = Get-ChildItem -Path build/Theremin_artefacts/Release/VST3 -Filter "Theremin.vst3" -Directory | Select-Object -First 1
          if (-not $vst3) { throw "VST3 not produced" }

          # Copy onnxruntime.dll + OpenCV DLLs next to the plugin binary.
          $binDir = "$($vst3.FullName)/Contents/x86_64-win"
          Copy-Item "vendor/onnxruntime/lib/onnxruntime.dll" -Destination $binDir
          Get-ChildItem -Path "${{ env.VCPKG_ROOT }}/installed/x64-windows/bin" -Filter "opencv_*.dll" |
              ForEach-Object { Copy-Item $_.FullName -Destination $binDir }

          Compress-Archive -Path $vst3.FullName -DestinationPath Theremin-vst3-windows.zip

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: Theremin-vst3-windows
          path: Theremin-vst3-windows.zip
          retention-days: 30

      - name: Publish GitHub Release (on tag)
        if: startsWith(github.ref, 'refs/tags/v')
        uses: softprops/action-gh-release@v2
        with:
          files: Theremin-vst3-windows.zip
          generate_release_notes: true
```

- [ ] **Step 9.2: Create `cmake/FindONNXRuntime.cmake`**

```cmake
# Finds ONNX Runtime given ONNXRUNTIME_ROOT.
if(NOT ONNXRUNTIME_ROOT)
    message(FATAL_ERROR "ONNXRUNTIME_ROOT not set")
endif()

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS "${ONNXRUNTIME_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIB)
    message(FATAL_ERROR "ONNX Runtime not found in ${ONNXRUNTIME_ROOT}")
endif()

add_library(onnxruntime SHARED IMPORTED GLOBAL)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_LIB}"
    IMPORTED_IMPLIB "${ONNXRUNTIME_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
)
```

- [ ] **Step 9.3: Update `CMakeLists.txt` to find and link OpenCV + ORT**

Add near the top after `project(...)`:

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio dnn)
find_package(ONNXRuntime REQUIRED)
```

And in the plugin `target_link_libraries`, add:

```cmake
target_link_libraries(Theremin PRIVATE
    # ... existing juce libs ...
    ${OpenCV_LIBS}
    onnxruntime
)
target_include_directories(Theremin PRIVATE ${OpenCV_INCLUDE_DIRS})
```

- [ ] **Step 9.4: Verify CI passes with new link deps**

```bash
git add .github/workflows/build.yml cmake/ CMakeLists.txt
git commit -m "ci: vendor OpenCV (vcpkg) and ONNX Runtime 1.17.3 on Windows"
git push
```

Wait for Actions. Expected: link step succeeds, artifact still produces `Theremin.vst3` — runtime DLLs now inside.

- [ ] **Step 9.5: Local OpenCV install on macOS (for dev)**

```bash
brew install opencv
# Download ORT for macOS:
curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-osx-arm64-1.17.3.tgz -o /tmp/ort.tgz
mkdir -p vendor/onnxruntime
tar -xzf /tmp/ort.tgz -C vendor --strip-components=1 --no-same-owner
mv vendor/onnxruntime-osx-arm64-1.17.3 vendor/onnxruntime
```

Add to local build config:

```bash
cmake -B build -G "Xcode" -DONNXRUNTIME_ROOT="$(pwd)/vendor/onnxruntime"
cmake --build build --config Debug --target Theremin_Standalone
```

Expected: local build links successfully.

Commit the README install note:

Create `docs/DEV_SETUP.md`:

```markdown
# Dev Setup (macOS)

Local development requires OpenCV and ONNX Runtime.

## Install

```bash
brew install opencv cmake

# ONNX Runtime 1.17.3
curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-osx-arm64-1.17.3.tgz -o /tmp/ort.tgz
mkdir -p vendor
tar -xzf /tmp/ort.tgz -C vendor
mv vendor/onnxruntime-osx-arm64-1.17.3 vendor/onnxruntime
```

## Build

```bash
cmake -B build -G "Xcode" -DONNXRUNTIME_ROOT="$(pwd)/vendor/onnxruntime"
cmake --build build --config Debug --target Theremin_Standalone
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```
```

```bash
git add docs/DEV_SETUP.md
git commit -m "docs: dev setup instructions for OpenCV + ORT on macOS"
git push
```

---

## Task 10: Bundle hand-landmark ONNX model + font as binary resources

**Goal:** Ship the ML model and font embedded in the plugin so users don't need external files.

**Files:**
- Download: `resources/hand_landmark_full.onnx` (not committed — added to `.gitignore`, fetched by CI and a local script)
- Download: `resources/MajorMonoDisplay-Regular.ttf`
- Create: `scripts/fetch_resources.sh`
- Modify: `CMakeLists.txt` — `juce_add_binary_data` for resources
- Modify: `.github/workflows/build.yml` — call fetch_resources before configure

- [ ] **Step 10.1: Create `scripts/fetch_resources.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p resources

# Hand landmark ONNX model (MediaPipe hand_landmark_full, ONNX export).
# Source: https://github.com/PINTO0309/PINTO_model_zoo/tree/main/033_Hand_Detection_and_Tracking
MODEL_URL="https://github.com/PINTO0309/PINTO_model_zoo/raw/main/033_Hand_Detection_and_Tracking/model_hand_landmark_sparse_Nx3x224x224.onnx"
if [ ! -f resources/hand_landmark.onnx ]; then
    echo "Downloading hand landmark model..."
    curl -L "$MODEL_URL" -o resources/hand_landmark.onnx
fi

# Major Mono Display from Google Fonts (OFL license).
FONT_URL="https://github.com/google/fonts/raw/main/ofl/majormonodisplay/MajorMonoDisplay-Regular.ttf"
if [ ! -f resources/MajorMonoDisplay-Regular.ttf ]; then
    echo "Downloading Major Mono Display..."
    curl -L "$FONT_URL" -o resources/MajorMonoDisplay-Regular.ttf
fi

echo "Resources ready."
```

Make executable:

```bash
chmod +x scripts/fetch_resources.sh
```

- [ ] **Step 10.2: Add entries to `.gitignore`**

Append:

```
resources/*.onnx
resources/*.ttf
```

- [ ] **Step 10.3: Update `CMakeLists.txt` to bundle resources**

Add before `juce_add_plugin`:

```cmake
juce_add_binary_data(ThereminData
    HEADER_NAME ThereminData.h
    NAMESPACE ThereminData
    SOURCES
        resources/hand_landmark.onnx
        resources/MajorMonoDisplay-Regular.ttf
)
```

Link it into the plugin:

```cmake
target_link_libraries(Theremin PRIVATE
    ThereminData
    # ... rest ...
)
```

- [ ] **Step 10.4: Run the fetch script locally**

```bash
./scripts/fetch_resources.sh
```

Expected: `resources/hand_landmark.onnx` (~2-6 MB) and `resources/MajorMonoDisplay-Regular.ttf` (~50 KB) exist.

- [ ] **Step 10.5: Add fetch step to CI**

In `.github/workflows/build.yml`, insert **before the "Configure" step**:

```yaml
      - name: Fetch binary resources
        shell: bash
        run: ./scripts/fetch_resources.sh
```

- [ ] **Step 10.6: Build locally to verify resource bundling works**

```bash
cmake --build build --config Debug --target Theremin_Standalone
```

Expected: build succeeds. Binary includes the ONNX model and font as embedded data.

- [ ] **Step 10.7: Commit and push, verify CI**

```bash
git add scripts/ .gitignore CMakeLists.txt .github/workflows/
git commit -m "feat: bundle hand-landmark ONNX model and Major Mono Display font"
git push
```

Expected: CI passes. Artifact size grows by ~5 MB.

---

## Task 11: HandTracker — ONNX Runtime wrapper

**Goal:** A class that loads the bundled ONNX model at plugin startup and runs inference on a single RGB image, returning 21 hand landmarks. No camera yet — tested with a static image.

**Files:**
- Create: `src/tracking/Landmark.h`
- Create: `src/tracking/HandTracker.h`
- Create: `src/tracking/HandTracker.cpp`
- Modify: `tests/CMakeLists.txt` — tests depend on OpenCV + ORT but we'll skip a unit test here because it requires the bundled binary data; integration test happens in the DAW instead

- [ ] **Step 11.1: Write `src/tracking/Landmark.h`**

```cpp
#pragma once
#include <array>

namespace theremin {

struct Landmark {
    float x;  // normalized [0,1]
    float y;  // normalized [0,1]
    float z;  // relative depth
};

using HandLandmarks = std::array<Landmark, 21>;

struct LandmarkFrame {
    bool detected = false;
    float confidence = 0.0f;
    HandLandmarks landmarks{};
};

} // namespace theremin
```

- [ ] **Step 11.2: Write `src/tracking/HandTracker.h`**

```cpp
#pragma once
#include <memory>
#include <vector>
#include <opencv2/core.hpp>
#include <onnxruntime_cxx_api.h>
#include "Landmark.h"

namespace theremin {

class HandTracker {
public:
    HandTracker();
    ~HandTracker();

    // Load the ONNX model from embedded binary data. Call once at startup.
    // Returns true on success.
    bool loadModelFromMemory(const void* data, size_t sizeBytes);

    // Run hand detection + landmark extraction on a BGR image (OpenCV default).
    // Returns landmark frame with detected=false if no hand found.
    LandmarkFrame detect(const cv::Mat& bgrImage);

private:
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    Ort::SessionOptions sessionOpts;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
    std::vector<std::string> inputNameStrings;
    std::vector<std::string> outputNameStrings;
    bool ready = false;

    static constexpr int kInputSize = 224;
};

} // namespace theremin
```

- [ ] **Step 11.3: Write `src/tracking/HandTracker.cpp`**

```cpp
#include "HandTracker.h"
#include <opencv2/imgproc.hpp>

namespace theremin {

HandTracker::HandTracker() = default;
HandTracker::~HandTracker() = default;

bool HandTracker::loadModelFromMemory(const void* data, size_t sizeBytes)
{
    try {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "theremin");
        sessionOpts.SetIntraOpNumThreads(1);
        sessionOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session = std::make_unique<Ort::Session>(*env, data, sizeBytes, sessionOpts);

        Ort::AllocatorWithDefaultOptions alloc;
        for (size_t i = 0; i < session->GetInputCount(); ++i) {
            auto namePtr = session->GetInputNameAllocated(i, alloc);
            inputNameStrings.emplace_back(namePtr.get());
            inputNames.push_back(inputNameStrings.back().c_str());
        }
        for (size_t i = 0; i < session->GetOutputCount(); ++i) {
            auto namePtr = session->GetOutputNameAllocated(i, alloc);
            outputNameStrings.emplace_back(namePtr.get());
            outputNames.push_back(outputNameStrings.back().c_str());
        }
        ready = true;
        return true;
    } catch (const Ort::Exception& e) {
        ready = false;
        return false;
    }
}

LandmarkFrame HandTracker::detect(const cv::Mat& bgrImage)
{
    LandmarkFrame frame;
    if (!ready || bgrImage.empty()) return frame;

    cv::Mat rgb;
    cv::cvtColor(bgrImage, rgb, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(kInputSize, kInputSize));

    cv::Mat floatImg;
    resized.convertTo(floatImg, CV_32FC3, 1.0 / 255.0);

    // NHWC → NCHW transpose into a flat buffer.
    std::vector<float> inputTensor(3 * kInputSize * kInputSize);
    std::vector<cv::Mat> channels(3);
    cv::split(floatImg, channels);
    for (int c = 0; c < 3; ++c) {
        std::memcpy(inputTensor.data() + c * kInputSize * kInputSize,
                    channels[c].data,
                    kInputSize * kInputSize * sizeof(float));
    }

    std::array<int64_t, 4> shape = {1, 3, kInputSize, kInputSize};
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto inputOrt = Ort::Value::CreateTensor<float>(
        memInfo, inputTensor.data(), inputTensor.size(), shape.data(), shape.size());

    try {
        auto outputs = session->Run(Ort::RunOptions{nullptr},
                                    inputNames.data(), &inputOrt, 1,
                                    outputNames.data(), outputNames.size());

        if (outputs.empty()) return frame;

        // Model output convention for MediaPipe hand landmark (sparse 224x224):
        //   [0] landmarks  (1, 21, 3)   -- x,y,z in pixel coords of the 224x224 input
        //   [1] handedness (1,1) -- confidence
        // If the model you bundle differs, adjust indexing here.
        float* lmData = outputs[0].GetTensorMutableData<float>();
        for (int i = 0; i < 21; ++i) {
            frame.landmarks[i].x = lmData[i * 3 + 0] / static_cast<float>(kInputSize);
            frame.landmarks[i].y = lmData[i * 3 + 1] / static_cast<float>(kInputSize);
            frame.landmarks[i].z = lmData[i * 3 + 2];
        }
        if (outputs.size() > 1) {
            frame.confidence = outputs[1].GetTensorMutableData<float>()[0];
        } else {
            frame.confidence = 1.0f;
        }
        frame.detected = frame.confidence > 0.5f;
    } catch (const Ort::Exception&) {
        // inference failure
    }
    return frame;
}

} // namespace theremin
```

- [ ] **Step 11.4: Add HandTracker to plugin `target_sources`**

In `CMakeLists.txt`:

```cmake
target_sources(Theremin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/mapping/Calibration.cpp
    src/mapping/Scales.cpp
    src/mapping/GestureMapper.cpp
    src/midi/MidiEmitter.cpp
    src/tracking/HandTracker.cpp
)
```

- [ ] **Step 11.5: Build locally, confirm it links**

```bash
cmake --build build --config Debug --target Theremin_Standalone
```

Expected: no link errors. HandTracker is instantiated on-demand only.

- [ ] **Step 11.6: Commit**

```bash
git add src/tracking/ CMakeLists.txt
git commit -m "feat: HandTracker wrapping ONNX Runtime on bundled model"
git push
```

---

## Task 12: CameraWorker — capture thread

**Goal:** A class that owns an OpenCV `VideoCapture`, runs a loop on its own thread grabbing frames and running them through the HandTracker, and publishes the latest `LandmarkFrame` via a lock-free double-buffered atomic slot.

**Files:**
- Create: `src/camera/CameraWorker.h`
- Create: `src/camera/CameraWorker.cpp`
- Modify: `CMakeLists.txt` — add CameraWorker.cpp to plugin sources

- [ ] **Step 12.1: Write `src/camera/CameraWorker.h`**

```cpp
#pragma once
#include <atomic>
#include <thread>
#include <opencv2/videoio.hpp>
#include "../tracking/HandTracker.h"

namespace theremin {

class CameraWorker {
public:
    CameraWorker();
    ~CameraWorker();

    // Start capture. Returns true on success, false if camera cannot open.
    bool start(int deviceIndex = 0);

    // Stop capture thread. Safe to call multiple times.
    void stop();

    // Thread-safe. Returns the most recent landmark frame.
    LandmarkFrame getLatestFrame() const;

    // For UI preview. Copies the latest captured BGR frame into `out`.
    // Returns true if a frame was available (and copied).
    // This IS a copy — not audio-thread safe. Call from UI thread only.
    bool copyLatestImage(cv::Mat& out) const;

    bool isRunning() const { return running.load(); }
    const std::string& getLastError() const { return lastError; }

private:
    void runLoop();

    cv::VideoCapture capture;
    HandTracker tracker;
    std::thread worker;
    std::atomic<bool> running{false};

    mutable std::mutex frameMutex;
    LandmarkFrame latestLandmarks;
    cv::Mat latestImage;

    std::string lastError;
};

} // namespace theremin
```

- [ ] **Step 12.2: Write `src/camera/CameraWorker.cpp`**

```cpp
#include "CameraWorker.h"
#include "../../resources/ThereminData.h"  // generated by juce_add_binary_data
#include <BinaryData.h>                    // JUCE names it this when NAMESPACE is set...
// NOTE: if juce_add_binary_data produces ThereminData.h and the namespace ThereminData,
// use the exact headers generated. Check build/ThereminData_artefacts for the file names.
// If the header is actually "ThereminData.h", drop the <BinaryData.h> include.

#include <opencv2/imgproc.hpp>

namespace theremin {

namespace {
    // Retrieve bundled ONNX model data. The symbol names depend on how
    // juce_add_binary_data chose to identifier-ify "hand_landmark.onnx".
    // JUCE normalizes to "hand_landmark_onnx" → ThereminData::hand_landmark_onnx + ::hand_landmark_onnxSize.
    const char* getModelData() { return ThereminData::hand_landmark_onnx; }
    size_t getModelSize() { return ThereminData::hand_landmark_onnxSize; }
}

CameraWorker::CameraWorker() = default;

CameraWorker::~CameraWorker()
{
    stop();
}

bool CameraWorker::start(int deviceIndex)
{
    if (running.load()) return true;

    if (!tracker.loadModelFromMemory(getModelData(), getModelSize())) {
        lastError = "Failed to load hand-tracking model.";
        return false;
    }

    capture.open(deviceIndex);
    if (!capture.isOpened()) {
        lastError = "Cannot open camera device " + std::to_string(deviceIndex) + ".";
        return false;
    }

    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 30);

    running.store(true);
    worker = std::thread([this] { runLoop(); });
    return true;
}

void CameraWorker::stop()
{
    running.store(false);
    if (worker.joinable()) worker.join();
    if (capture.isOpened()) capture.release();
}

void CameraWorker::runLoop()
{
    cv::Mat frame;
    while (running.load()) {
        if (!capture.read(frame) || frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        auto lm = tracker.detect(frame);
        {
            std::lock_guard<std::mutex> lk(frameMutex);
            latestLandmarks = lm;
            latestImage = frame.clone();
        }
    }
}

LandmarkFrame CameraWorker::getLatestFrame() const
{
    std::lock_guard<std::mutex> lk(frameMutex);
    return latestLandmarks;
}

bool CameraWorker::copyLatestImage(cv::Mat& out) const
{
    std::lock_guard<std::mutex> lk(frameMutex);
    if (latestImage.empty()) return false;
    latestImage.copyTo(out);
    return true;
}

} // namespace theremin
```

> Note on audio-thread safety: a mutex lock is actually OK for this case because the audio thread only copies a trivial POD (`LandmarkFrame`) and the lock is held for a few dozen nanoseconds. If Mackie's profiling later shows audio dropouts from this, swap to `std::atomic<LandmarkFrame>` with relaxed memory ordering + seqlock.

- [ ] **Step 12.3: Handle binary-data header name carefully**

The include at the top of `CameraWorker.cpp` is fragile because JUCE's `juce_add_binary_data` generates a header whose path depends on the build. Verify the actual file location:

```bash
find build -name "ThereminData.h" | head -3
```

Expected path like `build/Theremin_artefacts/JuceLibraryCode/ThereminData.h`. Update the `#include` in `CameraWorker.cpp` to `#include "ThereminData.h"` without the `resources/` prefix — JUCE adds the generated dir to include paths automatically for the target. Also remove the `#include <BinaryData.h>` line; it was only a hedge.

After fix:

```cpp
#include "ThereminData.h"
```

- [ ] **Step 12.4: Update `CMakeLists.txt`**

```cmake
target_sources(Theremin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/mapping/Calibration.cpp
    src/mapping/Scales.cpp
    src/mapping/GestureMapper.cpp
    src/midi/MidiEmitter.cpp
    src/tracking/HandTracker.cpp
    src/camera/CameraWorker.cpp
)
```

- [ ] **Step 12.5: Build and link locally**

```bash
cmake --build build --config Debug --target Theremin_Standalone
```

Expected: clean build, no link errors.

- [ ] **Step 12.6: Commit**

```bash
git add src/camera/ CMakeLists.txt
git commit -m "feat: CameraWorker thread with OpenCV capture + ONNX inference"
git push
```

---

## Task 13: Wire CameraWorker into the plugin; replace debug sliders

**Goal:** The plugin starts a CameraWorker when the editor opens, pulls landmarks in `processBlock`, and drives MIDI from the real fingertip (landmark #8) position. Debug sliders go away.

**Files:**
- Modify: `src/PluginProcessor.h` — add `CameraWorker`, `Calibration`
- Modify: `src/PluginProcessor.cpp` — start/stop worker on editor open/close, use real landmarks in processBlock
- Modify: `src/PluginEditor.cpp` — remove debug sliders, add empty placeholder for camera preview

- [ ] **Step 13.1: Modify `PluginProcessor.h`**

```cpp
// Add includes:
#include "camera/CameraWorker.h"
#include "mapping/Calibration.h"

// Inside class, public:
    theremin::CameraWorker cameraWorker;
    theremin::Calibration calibration;

    void startCameraIfNeeded();
    void stopCamera();

// Remove the debug atomics (debugX, debugY, debugHandPresent)
```

- [ ] **Step 13.2: Rewrite `processBlock` to use real landmarks**

```cpp
void ThereminAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const bool enabled = apvts.getRawParameterValue(ThereminParams::enabled)->load() > 0.5f;
    int channel = static_cast<int>(apvts.getRawParameterValue(ThereminParams::channel)->load());

    if (!enabled) {
        midiEmitter.panic(midi, channel);
        return;
    }

    theremin::GestureParams p;
    p.octaves = static_cast<int>(apvts.getRawParameterValue(ThereminParams::octaves)->load());
    p.magnetism = apvts.getRawParameterValue(ThereminParams::magnetism)->load();
    p.scaleKind = static_cast<theremin::ScaleKind>(
        static_cast<int>(apvts.getRawParameterValue(ThereminParams::scale)->load()));
    p.rootKey = static_cast<int>(apvts.getRawParameterValue(ThereminParams::key)->load());

    auto lf = cameraWorker.getLatestFrame();

    theremin::MidiState state;
    if (lf.detected) {
        // Landmark 8 = index fingertip.
        theremin::Point2D raw{ lf.landmarks[8].x, lf.landmarks[8].y };
        theremin::Point2D normalized = calibration.map(raw);
        state = theremin::GestureMapper::map(normalized.x, normalized.y, p);
    } else {
        state = theremin::GestureMapper::inactive();
    }

    midiEmitter.apply(state, midi, channel);
}

void ThereminAudioProcessor::startCameraIfNeeded()
{
    if (!cameraWorker.isRunning()) {
        cameraWorker.start(0);
    }
}

void ThereminAudioProcessor::stopCamera()
{
    cameraWorker.stop();
    // Next processBlock will see no landmarks and emit note-off via the
    // normal transition path. No need for a panic call here — we don't
    // have a MidiBuffer on hand outside processBlock anyway.
}
```

- [ ] **Step 13.3: Call camera start from the editor**

Modify `PluginEditor.cpp` constructor:

```cpp
ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 400, 1600, 1200);
    processorRef.startCameraIfNeeded();
}
```

Remove the debug slider members from `PluginEditor.h` and the slider bodies from the `.cpp`. Simplify `paint()` + `resized()` back to just drawing the title for now — we'll build the real UI in Task 14.

- [ ] **Step 13.4: Also stop the camera when the whole plugin is destroyed**

In `PluginProcessor.cpp`, add a destructor body that cleans up (or rely on `~CameraWorker` since it's a member and will be destroyed when the processor is):

No explicit destructor needed; `cameraWorker.stop()` is called in its own destructor.

- [ ] **Step 13.5: Build and test locally (macOS standalone)**

```bash
cmake --build build --config Debug --target Theremin_Standalone
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```

Grant camera permission when macOS asks. Expected: app window shows "THEREMIN", camera LED activates. No audible output yet (standalone mode has no synth).

- [ ] **Step 13.6: Commit and verify CI**

```bash
git add src/
git commit -m "feat: wire CameraWorker to processBlock; replace debug sliders with camera input"
git push
```

Mackie test: install new `.vst3`, put on MIDI track → synth on next track, confirm waving a hand in front of the webcam produces MIDI notes.

---

## Task 14: Plugin UI — controls + camera preview + skeleton overlay

**Goal:** The final plugin UI from the spec: octave pills, scale + key dropdowns, magnetism slider, MIDI channel, enable toggle, calibrate button, camera preview with hand-skeleton overlay, note grid overlay, and live MIDI status strip.

**Files:**
- Create: `src/ui/CameraPreview.h`
- Create: `src/ui/CameraPreview.cpp`
- Create: `src/ui/ControlStrip.h`
- Create: `src/ui/ControlStrip.cpp`
- Modify: `src/PluginEditor.h` / `.cpp` — compose the two components

- [ ] **Step 14.1: Write `src/ui/CameraPreview.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../camera/CameraWorker.h"

namespace theremin {

class CameraPreview : public juce::Component, private juce::Timer {
public:
    explicit CameraPreview(CameraWorker& worker);
    ~CameraPreview() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setOctaves(int o) { octaves = o; }
    void setScaleKind(ScaleKind k) { scaleKind = k; }
    void setRootKey(int k) { rootKey = k; }

private:
    void timerCallback() override;

    CameraWorker& workerRef;
    juce::Image currentFrame;
    HandLandmarks landmarks{};
    bool handDetected = false;

    int octaves = 2;
    ScaleKind scaleKind = ScaleKind::Chromatic;
    int rootKey = 0;
};

} // namespace theremin
```

- [ ] **Step 14.2: Write `src/ui/CameraPreview.cpp`**

```cpp
#include "CameraPreview.h"
#include <opencv2/imgproc.hpp>

namespace theremin {

namespace {
    juce::Image matToImage(const cv::Mat& bgr) {
        juce::Image img(juce::Image::RGB, bgr.cols, bgr.rows, false);
        juce::Image::BitmapData bmp(img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < bgr.rows; ++y) {
            auto* dst = bmp.getLinePointer(y);
            const auto* src = bgr.ptr<uint8_t>(y);
            for (int x = 0; x < bgr.cols; ++x) {
                dst[x * 3 + 0] = src[x * 3 + 2]; // R
                dst[x * 3 + 1] = src[x * 3 + 1]; // G
                dst[x * 3 + 2] = src[x * 3 + 0]; // B
            }
        }
        return img;
    }

    static const std::array<std::pair<int,int>, 23> kConnections = {{
        // Thumb
        {0,1},{1,2},{2,3},{3,4},
        // Index
        {0,5},{5,6},{6,7},{7,8},
        // Middle
        {0,9},{9,10},{10,11},{11,12},
        // Ring
        {0,13},{13,14},{14,15},{15,16},
        // Pinky
        {0,17},{17,18},{18,19},{19,20},
        // Palm web
        {5,9},{9,13},{13,17}
    }};
}

CameraPreview::CameraPreview(CameraWorker& w) : workerRef(w)
{
    startTimerHz(30);
}

CameraPreview::~CameraPreview()
{
    stopTimer();
}

void CameraPreview::timerCallback()
{
    cv::Mat frame;
    if (workerRef.copyLatestImage(frame)) {
        currentFrame = matToImage(frame);
    }
    auto lf = workerRef.getLatestFrame();
    landmarks = lf.landmarks;
    handDetected = lf.detected;
    repaint();
}

void CameraPreview::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));

    if (currentFrame.isValid()) {
        g.drawImage(currentFrame, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::xMid,
                    false);
    }

    // Overlay: note grid lines (chromatic, so user sees where notes fall).
    const float totalSemis = static_cast<float>(octaves) * 12.0f;
    for (int i = 0; i <= (int)totalSemis; ++i) {
        float x = (static_cast<float>(i) / totalSemis) * getWidth();
        bool isAccidental = (i % 12 == 1 || i % 12 == 3 || i % 12 == 6 ||
                             i % 12 == 8 || i % 12 == 10);
        g.setColour(juce::Colour(0xff22d3ee).withAlpha(isAccidental ? 0.15f : 0.35f));
        g.drawLine(x, 0, x, (float)getHeight(), 1.0f);
    }

    if (!handDetected) {
        g.setColour(juce::Colour(0xff22d3ee).withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(18.0f)));
        g.drawFittedText("Show your hand to play",
                         getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    // Draw skeleton.
    g.setColour(juce::Colour(0xffec4899).withAlpha(0.6f));
    for (auto [a, b] : kConnections) {
        float ax = landmarks[a].x * getWidth();
        float ay = landmarks[a].y * getHeight();
        float bx = landmarks[b].x * getWidth();
        float by = landmarks[b].y * getHeight();
        g.drawLine(ax, ay, bx, by, 2.0f);
    }
    for (auto& lm : landmarks) {
        g.fillEllipse(lm.x * getWidth() - 3, lm.y * getHeight() - 3, 6, 6);
    }

    // Fingertip glow (landmark 8).
    float fx = landmarks[8].x * getWidth();
    float fy = landmarks[8].y * getHeight();
    juce::ColourGradient glow(juce::Colour(0xff22d3ee).withAlpha(0.9f), fx, fy,
                              juce::Colour(0xff22d3ee).withAlpha(0.0f), fx + 40, fy, true);
    g.setGradientFill(glow);
    g.fillEllipse(fx - 40, fy - 40, 80, 80);
    g.setColour(juce::Colour(0xff22d3ee));
    g.fillEllipse(fx - 6, fy - 6, 12, 12);
}

void CameraPreview::resized() {}

} // namespace theremin
```

- [ ] **Step 14.3: Write `src/ui/ControlStrip.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginState.h"

namespace theremin {

class ControlStrip : public juce::Component {
public:
    explicit ControlStrip(juce::AudioProcessorValueTreeState& apvts);

    void resized() override;
    void paint(juce::Graphics&) override;

    std::function<void()> onCalibrateClicked;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;

    juce::ComboBox octavesCombo;
    juce::ComboBox scaleCombo;
    juce::ComboBox keyCombo;
    juce::ComboBox channelCombo;
    juce::Slider magnetismSlider;
    juce::ToggleButton enabledToggle;
    juce::TextButton calibrateButton{ "Calibrate" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>  octavesAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>  scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>  keyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>  channelAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    magnetismAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>    enabledAtt;

    juce::Label octavesLabel{ {}, "Octaves" };
    juce::Label scaleLabel  { {}, "Scale" };
    juce::Label keyLabel    { {}, "Key" };
    juce::Label channelLabel{ {}, "MIDI Ch" };
    juce::Label magnetismLabel{ {}, "Magnetism" };
};

} // namespace theremin
```

- [ ] **Step 14.4: Write `src/ui/ControlStrip.cpp`**

```cpp
#include "ControlStrip.h"

namespace theremin {

ControlStrip::ControlStrip(juce::AudioProcessorValueTreeState& apvts) : apvtsRef(apvts)
{
    for (int i = 1; i <= 5; ++i) octavesCombo.addItem(juce::String(i), i);
    octavesCombo.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(octavesCombo);
    addAndMakeVisible(octavesLabel);
    octavesAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ThereminParams::octaves, octavesCombo);

    for (int i = 0; i < ThereminParams::scaleNames.size(); ++i)
        scaleCombo.addItem(ThereminParams::scaleNames[i], i + 1);
    addAndMakeVisible(scaleCombo);
    addAndMakeVisible(scaleLabel);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ThereminParams::scale, scaleCombo);

    for (int i = 0; i < ThereminParams::keyNames.size(); ++i)
        keyCombo.addItem(ThereminParams::keyNames[i], i + 1);
    addAndMakeVisible(keyCombo);
    addAndMakeVisible(keyLabel);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ThereminParams::key, keyCombo);

    for (int i = 1; i <= 16; ++i) channelCombo.addItem(juce::String(i), i);
    addAndMakeVisible(channelCombo);
    addAndMakeVisible(channelLabel);
    channelAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ThereminParams::channel, channelCombo);

    magnetismSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    magnetismSlider.setTextBoxStyle(juce::Slider::TextBoxRight, true, 60, 20);
    addAndMakeVisible(magnetismSlider);
    addAndMakeVisible(magnetismLabel);
    magnetismAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ThereminParams::magnetism, magnetismSlider);

    enabledToggle.setButtonText("Enabled");
    addAndMakeVisible(enabledToggle);
    enabledAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, ThereminParams::enabled, enabledToggle);

    addAndMakeVisible(calibrateButton);
    calibrateButton.onClick = [this] { if (onCalibrateClicked) onCalibrateClicked(); };

    auto labelStyle = [](juce::Label& l) {
        l.setColour(juce::Label::textColourId, juce::Colour(0xff22d3ee));
        l.setFont(juce::Font(juce::FontOptions(12.0f)));
        l.setJustificationType(juce::Justification::centredLeft);
    };
    labelStyle(octavesLabel);
    labelStyle(scaleLabel);
    labelStyle(keyLabel);
    labelStyle(channelLabel);
    labelStyle(magnetismLabel);
}

void ControlStrip::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111));
    g.setColour(juce::Colour(0xff22d3ee).withAlpha(0.1f));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
}

void ControlStrip::resized()
{
    auto b = getLocalBounds().reduced(10);
    auto row1 = b.removeFromTop(b.getHeight() / 2 - 4);
    auto row2 = b;

    auto column = [](juce::Rectangle<int>& r, int w, juce::Label& lbl, juce::Component& ctrl) {
        auto col = r.removeFromLeft(w);
        lbl.setBounds(col.removeFromTop(16));
        ctrl.setBounds(col);
        r.removeFromLeft(10);
    };

    column(row1, 100, octavesLabel,  octavesCombo);
    column(row1, 160, scaleLabel,    scaleCombo);
    column(row1,  80, keyLabel,      keyCombo);
    column(row1, 100, channelLabel,  channelCombo);
    enabledToggle.setBounds(row1.removeFromRight(100));

    column(row2, 300, magnetismLabel, magnetismSlider);
    calibrateButton.setBounds(row2.removeFromLeft(120));
}

} // namespace theremin
```

- [ ] **Step 14.5: Rewrite `PluginEditor.h` and `.cpp` to host both components**

`PluginEditor.h`:

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/CameraPreview.h"
#include "ui/ControlStrip.h"

class ThereminAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ThereminAudioProcessorEditor(ThereminAudioProcessor& p);
    ~ThereminAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ThereminAudioProcessor& processorRef;
    theremin::CameraPreview preview;
    theremin::ControlStrip  controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminAudioProcessorEditor)
};
```

`PluginEditor.cpp`:

```cpp
#include "PluginEditor.h"

ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      preview(p.cameraWorker),
      controls(p.apvts)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 450, 1600, 1200);

    addAndMakeVisible(preview);
    addAndMakeVisible(controls);

    processorRef.startCameraIfNeeded();

    // Update preview overlay params from APVTS every 100ms.
    startTimerHz(10);
}

ThereminAudioProcessorEditor::~ThereminAudioProcessorEditor()
{
    stopTimer();
}

void ThereminAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(24.0f)));
    g.drawFittedText("THEREMIN",
                     getLocalBounds().removeFromTop(48).reduced(20, 0),
                     juce::Justification::centredLeft, 1);
}

void ThereminAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop(48);  // title
    controls.setBounds(b.removeFromTop(100));
    preview.setBounds(b);
}

void ThereminAudioProcessorEditor::timerCallback()
{
    int oct = static_cast<int>(processorRef.apvts.getRawParameterValue(ThereminParams::octaves)->load());
    int scaleIdx = static_cast<int>(processorRef.apvts.getRawParameterValue(ThereminParams::scale)->load());
    int key = static_cast<int>(processorRef.apvts.getRawParameterValue(ThereminParams::key)->load());
    preview.setOctaves(oct);
    preview.setScaleKind(static_cast<theremin::ScaleKind>(scaleIdx));
    preview.setRootKey(key);
}
```

- [ ] **Step 14.6: Add UI sources to `CMakeLists.txt`**

```cmake
target_sources(Theremin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/mapping/Calibration.cpp
    src/mapping/Scales.cpp
    src/mapping/GestureMapper.cpp
    src/midi/MidiEmitter.cpp
    src/tracking/HandTracker.cpp
    src/camera/CameraWorker.cpp
    src/ui/CameraPreview.cpp
    src/ui/ControlStrip.cpp
)
```

- [ ] **Step 14.7: Build + visual smoke test**

```bash
cmake --build build --config Debug --target Theremin_Standalone
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```

Expected: title bar "THEREMIN", control strip with all 6 controls + calibrate button, camera preview with hand skeleton when hand is visible.

- [ ] **Step 14.8: Commit and push**

```bash
git add src/ui/ src/PluginEditor.* CMakeLists.txt
git commit -m "feat: final plugin UI with camera preview + control strip"
git push
```

---

## Task 15: Calibration flow — 4-corner guided capture

**Goal:** Clicking Calibrate starts a 4-step overlay that walks the user through pointing at TL, TR, BR, BL, holding each for 1.5 seconds. Stores the four corners into `Calibration` so subsequent hand positions are mapped through the trapezoid.

**Files:**
- Create: `src/ui/CalibrationOverlay.h`
- Create: `src/ui/CalibrationOverlay.cpp`
- Modify: `src/PluginEditor.h` / `.cpp` — host the overlay, wire the Calibrate button

- [ ] **Step 15.1: Write `src/ui/CalibrationOverlay.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../camera/CameraWorker.h"
#include "../mapping/Calibration.h"

namespace theremin {

class CalibrationOverlay : public juce::Component, private juce::Timer {
public:
    CalibrationOverlay(CameraWorker& worker, Calibration& cal);

    void begin();
    void cancel();

    std::function<void()> onFinished;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CameraWorker& workerRef;
    Calibration& calRef;

    enum class Stage { Idle, TL, TR, BR, BL, Done };
    Stage stage = Stage::Idle;
    double holdStartMs = 0.0;
    Point2D capturedTL{}, capturedTR{}, capturedBR{}, capturedBL{};

    Point2D currentTargetCorner() const;
    juce::String stagePrompt() const;
    void advance(Point2D captured);
};

} // namespace theremin
```

- [ ] **Step 15.2: Write `src/ui/CalibrationOverlay.cpp`**

```cpp
#include "CalibrationOverlay.h"

namespace theremin {

namespace {
    constexpr double kHoldDurationMs = 1500.0;
    constexpr float  kTargetRadius = 40.0f;
    constexpr float  kCaptureRadius = 0.1f; // in normalized image space
}

CalibrationOverlay::CalibrationOverlay(CameraWorker& w, Calibration& c)
    : workerRef(w), calRef(c) {}

void CalibrationOverlay::begin()
{
    stage = Stage::TL;
    holdStartMs = 0.0;
    setVisible(true);
    startTimerHz(30);
    repaint();
}

void CalibrationOverlay::cancel()
{
    stage = Stage::Idle;
    stopTimer();
    setVisible(false);
    if (onFinished) onFinished();
}

Point2D CalibrationOverlay::currentTargetCorner() const
{
    switch (stage) {
        case Stage::TL: return {0.1f, 0.1f};
        case Stage::TR: return {0.9f, 0.1f};
        case Stage::BR: return {0.9f, 0.9f};
        case Stage::BL: return {0.1f, 0.9f};
        default:        return {0.5f, 0.5f};
    }
}

juce::String CalibrationOverlay::stagePrompt() const
{
    switch (stage) {
        case Stage::TL: return "Hold fingertip at the TOP-LEFT corner";
        case Stage::TR: return "Hold fingertip at the TOP-RIGHT corner";
        case Stage::BR: return "Hold fingertip at the BOTTOM-RIGHT corner";
        case Stage::BL: return "Hold fingertip at the BOTTOM-LEFT corner";
        case Stage::Done: return "Calibrated!";
        default: return "";
    }
}

void CalibrationOverlay::advance(Point2D captured)
{
    switch (stage) {
        case Stage::TL: capturedTL = captured; stage = Stage::TR; break;
        case Stage::TR: capturedTR = captured; stage = Stage::BR; break;
        case Stage::BR: capturedBR = captured; stage = Stage::BL; break;
        case Stage::BL:
            capturedBL = captured;
            calRef.setCorners(capturedTL, capturedTR, capturedBR, capturedBL);
            stage = Stage::Done;
            break;
        default: break;
    }
    holdStartMs = 0.0;
}

void CalibrationOverlay::timerCallback()
{
    if (stage == Stage::Idle) return;

    if (stage == Stage::Done) {
        stopTimer();
        setVisible(false);
        if (onFinished) onFinished();
        return;
    }

    auto lf = workerRef.getLatestFrame();
    if (!lf.detected) {
        holdStartMs = 0.0;
        repaint();
        return;
    }

    Point2D fingertip{ lf.landmarks[8].x, lf.landmarks[8].y };
    Point2D target = currentTargetCorner();

    float dx = fingertip.x - target.x;
    float dy = fingertip.y - target.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < kCaptureRadius) {
        double now = juce::Time::getMillisecondCounterHiRes();
        if (holdStartMs == 0.0) holdStartMs = now;
        if (now - holdStartMs >= kHoldDurationMs) {
            advance(fingertip);
        }
    } else {
        holdStartMs = 0.0;
    }
    repaint();
}

void CalibrationOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0x99000000));
    g.setColour(juce::Colour(0xffffcc00));
    g.setFont(juce::Font(juce::FontOptions(20.0f)));
    g.drawFittedText(stagePrompt(),
                     getLocalBounds().removeFromTop(60).reduced(20, 0),
                     juce::Justification::centred, 1);

    if (stage == Stage::Idle || stage == Stage::Done) return;

    Point2D t = currentTargetCorner();
    float tx = t.x * getWidth();
    float ty = t.y * getHeight();

    // Target crosshair.
    g.setColour(juce::Colour(0xffffcc00));
    g.drawEllipse(tx - kTargetRadius, ty - kTargetRadius,
                  kTargetRadius * 2, kTargetRadius * 2, 2.0f);

    // Hold progress ring.
    if (holdStartMs > 0.0) {
        double now = juce::Time::getMillisecondCounterHiRes();
        float progress = (float)std::min(1.0, (now - holdStartMs) / kHoldDurationMs);
        juce::Path arc;
        arc.addCentredArc(tx, ty, kTargetRadius + 8, kTargetRadius + 8,
                          0.0f, 0.0f, juce::MathConstants<float>::twoPi * progress, true);
        g.setColour(juce::Colour(0xff22d3ee));
        g.strokePath(arc, juce::PathStrokeType(3.0f));
    }
}

void CalibrationOverlay::resized() {}

} // namespace theremin
```

- [ ] **Step 15.3: Host the overlay in `PluginEditor`**

In `PluginEditor.h` add include + member:

```cpp
#include "ui/CalibrationOverlay.h"

// private:
    theremin::CalibrationOverlay calibration;
```

In `PluginEditor.cpp` constructor:

```cpp
ThereminAudioProcessorEditor::ThereminAudioProcessorEditor(ThereminAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      preview(p.cameraWorker),
      controls(p.apvts),
      calibration(p.cameraWorker, p.calibration)
{
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(600, 450, 1600, 1200);

    addAndMakeVisible(preview);
    addAndMakeVisible(controls);
    addChildComponent(calibration);  // hidden by default

    controls.onCalibrateClicked = [this] { calibration.begin(); };

    processorRef.startCameraIfNeeded();
    startTimerHz(10);
}
```

And update `resized()` so `calibration` overlays the preview area:

```cpp
void ThereminAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop(48);
    controls.setBounds(b.removeFromTop(100));
    preview.setBounds(b);
    calibration.setBounds(b);
}
```

- [ ] **Step 15.4: Add to CMakeLists.txt**

```cmake
    src/ui/CalibrationOverlay.cpp
```

- [ ] **Step 15.5: Build, smoke-test calibration**

```bash
cmake --build build --config Debug --target Theremin_Standalone
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```

Click Calibrate. Expected: dark overlay, "Hold fingertip at the TOP-LEFT corner". Move index finger to that corner, hold ~1.5s, progress ring completes, prompt moves to TOP-RIGHT. Repeat for all four corners. Overlay closes. Normal play resumes, but now the hand position is mapped through the trapezoid.

- [ ] **Step 15.6: Commit**

```bash
git add src/ui/CalibrationOverlay.* src/PluginEditor.* CMakeLists.txt
git commit -m "feat: 4-corner guided calibration flow with 1.5s hold"
git push
```

---

## Task 16: Robustness — stuck-note watchdog, camera error UI, font load

**Goal:** Harden for realistic use: emit a watchdog all-notes-off if landmarks haven't arrived in 1 second (camera hang), surface a user-visible error if the camera fails to open, and actually load the bundled Major Mono Display font for the title.

**Files:**
- Modify: `src/PluginProcessor.cpp` — add landmark-freshness timestamp, watchdog in processBlock
- Modify: `src/camera/CameraWorker.h` / `.cpp` — track last-frame time
- Modify: `src/PluginEditor.cpp` — draw an error banner if `!isRunning && !lastError.empty()`
- Modify: `src/ui/CameraPreview.cpp` — load embedded font for "Show your hand" message

- [ ] **Step 16.1: Add a timestamp to `CameraWorker`**

In `CameraWorker.h` add:

```cpp
    // Milliseconds since the last successful frame. INT_MAX if none yet.
    int msSinceLastFrame() const;
```

In `CameraWorker.cpp`:

```cpp
// Add member in class:
std::atomic<double> lastFrameMs{ 0.0 };

// In runLoop, after publishing a frame:
lastFrameMs.store(juce::Time::getMillisecondCounterHiRes());

// Definition:
int CameraWorker::msSinceLastFrame() const {
    double last = lastFrameMs.load();
    if (last == 0.0) return INT_MAX;
    return (int)(juce::Time::getMillisecondCounterHiRes() - last);
}
```

- [ ] **Step 16.2: Watchdog in `processBlock`**

```cpp
// ...existing code...
auto lf = cameraWorker.getLatestFrame();
bool stale = cameraWorker.msSinceLastFrame() > 1000; // 1 second

theremin::MidiState state;
if (lf.detected && !stale) {
    theremin::Point2D raw{ lf.landmarks[8].x, lf.landmarks[8].y };
    theremin::Point2D normalized = calibration.map(raw);
    state = theremin::GestureMapper::map(normalized.x, normalized.y, p);
} else {
    state = theremin::GestureMapper::inactive();
}
// ...
```

- [ ] **Step 16.3: Error banner in editor**

Add a `juce::Label` or a custom paint block in `PluginEditor::paint`:

```cpp
void ThereminAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a15));
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(juce::FontOptions(24.0f)));
    g.drawFittedText("THEREMIN",
                     getLocalBounds().removeFromTop(48).reduced(20, 0),
                     juce::Justification::centredLeft, 1);

    if (!processorRef.cameraWorker.isRunning()
        && !processorRef.cameraWorker.getLastError().empty()) {
        auto errArea = getLocalBounds().reduced(40);
        g.setColour(juce::Colour(0xff7f1d1d));
        g.fillRoundedRectangle(errArea.toFloat(), 8.0f);
        g.setColour(juce::Colour(0xfffecaca));
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawFittedText(processorRef.cameraWorker.getLastError(),
                         errArea.reduced(20), juce::Justification::centred, 4);
    }
}
```

Trigger a repaint when state changes by adding a timer tick that calls `repaint()` if the error state changes.

- [ ] **Step 16.4: Load the bundled font for the title**

In `PluginEditor.cpp` constructor:

```cpp
#include "ThereminData.h"

// In ctor body:
auto typeface = juce::Typeface::createSystemTypefaceFor(
    ThereminData::MajorMonoDisplay_Regular_ttf,
    ThereminData::MajorMonoDisplay_Regular_ttfSize);
titleFont = juce::Font(typeface).withHeight(28.0f);
```

Make `titleFont` a member of the editor class. In `paint`:

```cpp
g.setFont(titleFont);
```

Note: JUCE mangles the file identifier. Run `grep -r "MajorMono" build/Theremin_artefacts/JuceLibraryCode/` after a build to confirm the exact symbol name, and fix the identifier if different.

- [ ] **Step 16.5: Build, test error path manually**

Temporarily change `cameraWorker.start(0)` to `start(99)` in the editor, build, run. Expected: red error banner with "Cannot open camera device 99."

Revert to `start(0)`.

- [ ] **Step 16.6: Commit**

```bash
git add src/
git commit -m "feat: stuck-note watchdog, camera error banner, Major Mono Display title"
git push
```

---

## Task 17: README with install instructions + v0.1.0 release

**Goal:** Ship an actual `v0.1.0` tagged release so Mackie has a stable download URL to share.

**Files:**
- Modify: `README.md` — full install + usage + troubleshooting
- Tag: `v0.1.0`

- [ ] **Step 17.1: Rewrite `README.md`**

```markdown
# theremin-vst

A Windows VST3 plugin that uses a webcam + hand tracking to emit MIDI. Wave your hand in front of your camera → drive any synth plugin in your DAW.

![status](https://github.com/brebuilds/theremin-vst/actions/workflows/build.yml/badge.svg)

## Install (Windows)

1. Download `Theremin-vst3-windows.zip` from [Releases](https://github.com/brebuilds/theremin-vst/releases/latest)
2. Unzip. Move `Theremin.vst3` (the whole folder) to:
   `C:\Program Files\Common Files\VST3\`
3. Open your DAW. Rescan VST3 plugins.
4. Add **Theremin** to a MIDI track. Route MIDI output to a synth plugin on the next track.
5. Grant camera access when prompted.
6. Wave your hand.

## How to use

- **X axis** (left ↔ right) = pitch
- **Y axis** (top ↔ bottom) = volume (CC7) + filter (CC74)
- **Hand present** = note on; hand leaves frame = note off
- **Magnetism slider** — 0 = smooth theremin glide via pitch bend. 1 = hard snap to semitones.
- **Scale + Key** — quantize to Chromatic / Major / Minor / Pentatonic / Blues in any key
- **Octaves** — 1 to 5, from A2 upward
- **Calibrate** — walks you through pointing at 4 corners; sets the playing area

## Troubleshooting

**Camera doesn't open:** another app (Zoom, OBS, Discord) has exclusive lock. Close it and reload the plugin.

**Notes stuck on:** toggle the **Enabled** switch off and back on.

**Need a different camera:** v0.1.0 uses device 0 (default). Camera selection in v0.2.0.

## Build from source

See [docs/DEV_SETUP.md](docs/DEV_SETUP.md).

## License

MIT
```

- [ ] **Step 17.2: Tag v0.1.0 and push**

```bash
git add README.md
git commit -m "docs: v0.1.0 readme with install + usage"
git push

git tag -a v0.1.0 -m "v0.1.0: first usable build — hand → MIDI, 4-corner calibration"
git push origin v0.1.0
```

- [ ] **Step 17.3: Verify GitHub Release is published**

Visit `https://github.com/brebuilds/theremin-vst/releases`. Expected: `v0.1.0` release with `Theremin-vst3-windows.zip` attached, auto-generated release notes from the commit history.

Send Mackie the link: `https://github.com/brebuilds/theremin-vst/releases/tag/v0.1.0`

---

## Deferred (v0.2.0+ and beyond)

- Camera device picker in the UI
- Two-hand control (left hand for expression CCs)
- Finger pinch → note trigger (instead of hand-presence)
- MPE (multi-dimensional polyphonic expression)
- macOS (AU + VST3) build
- Installer (`.msi`) packaging
- Preset bank
- OSC output in parallel with MIDI
- Low-end fallback to "lite" hand model when inference FPS drops

---

## Plan self-review notes

- **Spec coverage:** All of the spec's Goals are addressed: Task 1-2 ship a Windows VST, Task 4-7 replicate the web prototype's expressive mapping in pure-logic modules, Task 15 realizes the 4-corner calibration from Mackie's request.
- **Non-goals respected:** No audio synthesis, no MPE, no two-hand, no macOS build in v0.1.0.
- **Risks addressed:** Camera permission failures (Task 16 error UI), stuck notes (Task 7 diff logic + Task 16 watchdog), audio-thread safety (Task 12 locks are brief, documented upgrade path to seqlock).
- **Tech choice note:** Spec said "ONNX Runtime + MediaPipe HandLandmarker"; plan uses the same ORT 1.17.3 + a MediaPipe hand landmark ONNX export from PINTO_model_zoo. If that specific model URL goes stale, the implementer substitutes any equivalent 21-landmark hand ONNX model with matching input dimensions.
