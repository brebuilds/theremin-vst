# Theremin VST — Design Spec

**Date:** 2026-04-21
**Status:** Draft — awaiting review
**Author:** Bre (project lead), Mackie (primary user/tester), Claude (technical design)

---

## Summary

A Windows VST3 plugin that uses a webcam and hand-tracking to emit MIDI. The user points their index finger in front of the camera; the plugin converts fingertip position into note-on/off + pitch bend + CC messages, which the DAW routes to any synth plugin the user chooses.

This is a rebuild-from-scratch of a Vite/React web prototype (`/Users/bre/Thermostatwebapp`) as a native C++ plugin. The core idea, axis mappings, and "magnetism" snap-to-note behavior carry over; the web app's built-in synth (waveforms, filter, reverb) does not — the VST is a MIDI controller, not a sound generator. Sound comes from whatever synth plugin the user places on the downstream MIDI track.

---

## Goals

1. Ship a working Windows VST3 that Mackie can install and use with any DAW (Ableton, FL Studio, Reaper, Cubase, Studio One).
2. Keep the source repository owned by Bre (Mac user) with zero need to run a Windows build locally. Windows builds produced via CI.
3. Preserve the expressive feel of the web prototype (smooth glide, note magnetism, fingertip-driven pitch + volume) while adapting it to MIDI-output semantics.
4. Ship fast (v1 in days/weeks, not months). Defer polish features (two-hand, MPE, standalone mode) to v2.

## Non-goals (v1)

- Built-in synthesis. The plugin emits MIDI only; the downstream synth provides the sound.
- macOS (AU) build. Easy to add later; not needed for Mackie's Windows workflow.
- Two-hand control, finger-pose recognition, gesture recording, preset system.
- Installer packaging (`.msi`). Raw `.vst3` drop-in is the v1 install path.

---

## Architecture

```
┌─────────────────── PLUGIN PROCESS ───────────────────┐
│                                                      │
│  ┌──────────────┐   ┌──────────────┐                 │
│  │  Camera      │──▶│  Hand        │                 │
│  │  (OpenCV)    │   │  Tracker     │                 │
│  │  worker      │   │  (ONNX Run-  │                 │
│  │  thread      │   │  time model) │                 │
│  └──────────────┘   └──────┬───────┘                 │
│                            │ landmarks               │
│                            ▼                         │
│  ┌─────────────────┐  ┌─────────────────┐            │
│  │  Plugin UI      │◀─│  Gesture →      │            │
│  │  (JUCE)         │  │  MIDI Mapper    │            │
│  │  - camera       │  │  (lock-free     │            │
│  │    preview      │  │   queue)        │            │
│  │  - controls     │  └────────┬────────┘            │
│  │  - calibrate    │           │                     │
│  └─────────────────┘           ▼                     │
│                        ┌──────────────┐              │
│                        │  Audio thread│              │
│                        │  emits MIDI  │─────▶ DAW    │
│                        │  events      │       MIDI   │
│                        └──────────────┘       output │
└──────────────────────────────────────────────────────┘
```

### Framework choices

| Concern | Choice | Why |
|---|---|---|
| Plugin framework | JUCE 8 (C++20) | Industry standard, builds VST3/AU/standalone from one codebase, mature MIDI/graphics APIs |
| Webcam capture | OpenCV 4.x `VideoCapture` | Cross-platform, simple, works on Windows + macOS. Not used for image processing — just frame grab. |
| Hand tracking | ONNX Runtime + MediaPipe HandLandmarker (pre-converted `.onnx` model) | Portable, no Bazel build headache, CPU-only ~30fps |
| Build system | CMake (via JUCE's `juce_add_plugin` macro) | Standard C++ tooling, plays well with GitHub Actions |
| CI | GitHub Actions, `windows-latest` runner | Windows builds happen in cloud, Bre never touches a Windows machine |

### Threading model

- **UI thread** — JUCE message thread. Draws plugin UI, handles user input (sliders, buttons, calibration clicks).
- **Worker thread** — owns OpenCV camera, runs ONNX inference per frame, writes latest hand landmarks to a lock-free `std::atomic<LandmarkFrame*>` slot (double-buffer).
- **Audio thread** — called by host. Reads the latest landmarks snapshot, computes MIDI events, writes to the plugin's MIDI output buffer. **Never allocates, never locks.**

Dropping a landmark frame (worker running ahead of audio thread) is acceptable — audio just sees the most recent one.

---

## Components

### 1. `CameraWorker`
- Owns the OpenCV `VideoCapture` and ONNX session.
- Loop: grab frame → run hand landmark inference → publish landmarks to shared state.
- Starts/stops with plugin lifecycle.
- Camera device selectable in UI (default: device 0).

### 2. `HandTracker`
- Wraps ONNX Runtime session.
- Input: RGB frame (cv::Mat).
- Output: 21 landmarks (x, y, z normalized to [0, 1]) + confidence.
- Model file bundled as binary resource in the plugin — no external install.

### 3. `GestureMapper`
- Pure logic (no threading, no I/O). Testable in isolation.
- Input: landmark frame + plugin parameters (octaves, magnetism, scale, key).
- Output: desired MIDI state (note, pitch-bend amount, CC values).
- Handles calibration mapping (trapezoidal warp from the 4 captured corners into a unit square).
- Handles scale quantization (chromatic, major, minor, pentatonic, blues).

### 4. `MidiEmitter`
- Runs on audio thread inside `processBlock`.
- Reads latest `GestureMapper` output, diffs against last sent state, emits MIDI events for changes.
- Manages note-on/note-off bookkeeping (never leaves a stuck note).

### 5. `PluginProcessor` (JUCE `AudioProcessor`)
- Wires everything together.
- Manages plugin parameters (automatable by DAW): octaves, magnetism, scale, key, MIDI channel, bend range, enable/disable.

### 6. `PluginEditor` (JUCE `AudioProcessorEditor`)
- The UI. Draws camera preview, controls, calibration flow.
- Reads landmarks from shared state to render preview at ~30fps.

---

## MIDI output contract

All outputs on user-selectable MIDI channel (default 1).

| Hand signal | MIDI output | Detail |
|---|---|---|
| X position (left → right) | Note-on + pitch bend | Bottom note = A2 (MIDI 45). Top note = A2 + `octaves × 12`. |
| Y position (top → bottom) | CC7 (Channel volume) | Top = 127, bottom = 0. Exponential curve for feel. |
| Y position (derived) | CC74 (Filter cutoff) | Mirrors web version behavior: `filterFreq = pitch × 3 + (1 - y) × 3000`, scaled into CC range. |
| Hand enters frame | Note-on | Velocity mapped from Y position, clamped to range 40–127 so hand presence always plays audibly. |
| Hand leaves frame | Note-off, CC7 → 0 | All-notes-off safety on longer absences. |
| Magnetism slider (param) | Shapes note+bend blend | 0 → continuous pitch bend only (target note = lowest, bend covers full range). 1 → hard snap, no bend. Between: quantize + residual bend. |
| Scale lock | Filters which notes are legal | Chromatic / Major / Minor / Pentatonic major / Pentatonic minor / Blues. Root key selectable. |

Pitch bend range: fixed at ±2 semitones in v1 (matches most synth defaults). User-configurable range deferred to v2.

---

## Plugin UI

JUCE-native components. ~800×600 default, resizable.

```
┌──────────────────────────────────────────────────┐
│ THEREMIN                        [○●○] MIDI ✓     │
│ ──────────────────────────────────────────────── │
│ Octaves: [1][2][3][4][5]   Key: [C ▼] [Major ▼]  │
│ Magnetism: [────●─────]      MIDI Ch: [1 ▼]      │
│ [🎯 Calibrate]                [👁 Hide Camera]   │
│ ──────────────────────────────────────────────── │
│ ┌──────────────────────────────────────────────┐ │
│ │                                              │ │
│ │       [camera feed with skeleton overlay]    │ │
│ │       note grid lines overlaid               │ │
│ │       glowing fingertip dot                  │ │
│ │                                              │ │
│ └──────────────────────────────────────────────┘ │
│ Note: C4 | Vel: 94 | Pitch: +23 | Filter: 6.2k   │
└──────────────────────────────────────────────────┘
```

- Title font: **Major Mono Display** (embedded TTF, free Google Font)
- Color palette: dark theme, cyan accent — matches the web prototype so Mackie's muscle memory transfers.
- Octave selector: pill-button group, 1–5
- Scale + key: dropdowns
- Magnetism: horizontal slider 0–1
- MIDI channel: dropdown 1–16
- Calibrate button: launches the 4-corner flow
- Hide Camera button: collapses preview, UI shrinks to the control strip (camera still runs in background)

### Calibration flow (per Mackie's request)

1. User clicks **Calibrate**. Camera view dims. Play disabled.
2. Crosshair target appears at **top-left**. Overlay: *"Hold index finger at the top-left corner of your playing area"*
3. User holds fingertip inside the target zone.
4. Progress ring around the crosshair fills over **1.5 seconds**.
5. Success chime + ✓ checkmark. Corner recorded.
6. Crosshair moves to **top-right**. Repeat steps 3–5.
7. Then bottom-right, then bottom-left.
8. Four corner points stored as a trapezoidal mapping. Any hand position inside that quad maps to a unit square (x, y ∈ [0, 1]).
9. "Calibrated" toast. Normal play resumes.

Re-calibration at any time via the button. Bounds persisted per plugin instance (saved in DAW session state).

---

## Repository & build

### Repo
- **Name:** `theremin-vst`
- **Owner:** `brebuilds`
- **License:** MIT
- **Visibility:** Public (invites future contributors)

### Layout

```
theremin-vst/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── .github/
│   └── workflows/
│       └── build.yml
├── src/
│   ├── PluginProcessor.cpp / .h
│   ├── PluginEditor.cpp / .h
│   ├── CameraWorker.cpp / .h
│   ├── HandTracker.cpp / .h
│   ├── GestureMapper.cpp / .h
│   └── MidiEmitter.cpp / .h
├── resources/
│   ├── hand_landmarker.onnx      # bundled model
│   └── MajorMonoDisplay.ttf      # bundled font
├── vendor/                        # git submodules
│   ├── JUCE/
│   ├── onnxruntime/               # prebuilt binaries per platform
│   └── opencv/                    # prebuilt binaries per platform
├── tests/
│   └── GestureMapperTests.cpp     # pure-logic unit tests
└── docs/
    └── superpowers/
        └── specs/
            └── 2026-04-21-theremin-vst-design.md
```

### GitHub Actions (`.github/workflows/build.yml`)

- Triggers: push to `main`, push of `v*` tag, pull requests.
- Jobs:
  - `build-windows`: `windows-latest` runner, installs CMake + Visual Studio tooling, fetches JUCE/OpenCV/ONNX runtime, builds `Theremin.vst3`.
  - `build-windows` uploads `.vst3` as a workflow artifact.
  - On `v*` tag push: additional job creates a GitHub Release and attaches the `.vst3`.
- Future (deferred): `build-macos` job for AU + VST3.

### Install flow (Mackie)

1. Visit `https://github.com/brebuilds/theremin-vst/releases`
2. Download latest `Theremin.vst3` (or zip containing it)
3. Drop into `C:\Program Files\Common Files\VST3\`
4. Rescan plugins in DAW
5. Insert on a MIDI track, route output to synth plugin on next track

---

## Testing strategy

- **`GestureMapper` unit tests** (pure logic): calibration warp math, scale quantization, magnetism blend, note-change detection. Runs in CI.
- **Manual integration testing**: Bre sanity-checks locally if she can get a macOS build working (v2); Mackie runs authoritative tests on Windows rig in his DAW.
- **Smoke test checklist** (documented in README):
  - Plugin loads in Reaper / Ableton / FL Studio without crashing
  - Camera feed appears in plugin UI
  - Hand waves → MIDI note-on/off visible in DAW MIDI monitor
  - Routing to downstream synth produces sound
  - Magnetism slider changes quantization behavior audibly
  - All 5 octave settings produce correct note ranges
  - Calibration flow completes and improves playing-area tracking
  - Scale dropdown filters notes correctly

---

## Risks & unknowns

1. **Camera permissions inside the DAW process.** On Windows this is usually fine, but some DAWs (especially ones with sandboxing, like Ableton's newer versions) may block camera access. Mitigation: worst case, we ship a standalone mode as well and the VST connects to it via loopback (fallback to the web app's Option A pattern).
2. **ONNX Runtime + OpenCV binary size.** Plugin `.vst3` may be 20–50 MB instead of the usual 2–5 MB. Acceptable for a gesture plugin; users accept larger downloads for this class of tool.
3. **Performance on low-end hardware.** ONNX hand model runs ~30fps on modern CPUs; older machines may struggle. We can fall back to MediaPipe's "lite" model variant if needed.
4. **MIDI note-stuck bugs.** A lost hand while a note is ringing = classic stuck-note risk. Mitigation: aggressive all-notes-off on hand-lost + frame-timeout watchdog in MidiEmitter.
5. **Camera device conflicts.** If Mackie has OBS or Zoom open, the plugin can't grab the camera. Needs a clear error message.

---

## Deferred features (v2+)

- Two-hand control (left hand = expression/mod, right hand = pitch — "real theremin" split)
- Finger-pinch to trigger note-on (rather than hand-presence)
- MPE (polyphonic per-finger control)
- Gesture-triggered preset changes
- macOS (AU + VST3) build
- Standalone app mode
- Installer packaging
- Preset bank / scene memory
- OSC output in parallel with MIDI
