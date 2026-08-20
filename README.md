# theremin-vst

A VST3 plugin that turns hand position into MIDI. Move your hand through the air,
the plugin emits notes, pitch bend, volume and filter CC — routed to any synth on
the next track in your DAW. It makes no sound of its own.

**Status:** the instrument engine is complete and tested — calibration, gesture
mapping, scale quantization and MIDI emission, covered by **28 Catch2 unit tests**
with a green Windows CI build. The **vision layer is not wired yet**: nothing
currently feeds hand coordinates in from a webcam. See [Where it stands](#where-it-stands).

## How it's built

Everything downstream of the camera consumes a normalized `(x, y)` in `[0,1]²`
and is pure and deterministic. That's the whole design decision, and it's why the
musical behavior is unit-testable without a camera, an ML runtime, or a DAW:

```
webcam ──▶ hand landmark ──▶ Calibration ──▶ GestureMapper ──▶ MidiEmitter ──▶ DAW
           (not wired)        quad → unit     x → note         state → MIDI
                              square          y → volume       note on/off
```

| Stage | What it does | Tested |
| --- | --- | --- |
| `Calibration` | Inverse-maps an arbitrary four-corner quad — your actual reachable play area, which is a trapezoid, not a rectangle — into the unit square, clamped. | 6 cases |
| `GestureMapper` | X to MIDI note across 1–5 octaves from A2; Y to CC7 volume and CC74 filter; `magnetism` blends between continuous pitch bend and hard scale snapping. | 10 cases |
| `Scale` | Chromatic, major, minor, both pentatonics, blues. `snapToLegal` finds the nearest in-scale note, ties breaking downward. | 7 cases |
| `MidiEmitter` | Turns a stream of gesture states into correct note-on/note-off pairs — only emitting when the note actually changes — plus an all-notes-off panic. | 5 cases |

The interesting part is `magnetism`. At `1.0` you get no pitch bend and every note
snaps to the scale, so you cannot play a wrong note. At `0.0` you get continuous
pitch between semitones, which is what a real theremin does and what makes it hard
to play. The parameter is the dial between "instrument anyone can play" and
"instrument that takes practice."

## Where it stands

Done and tested: the four stages above, plus JUCE plugin scaffolding
(`PluginProcessor`, `PluginEditor`, `PluginState`) and a Windows CI build.

Not done: webcam capture and hand-landmark inference. The design targets OpenCV
for capture and MediaPipe HandLandmarker via ONNX Runtime, and `docs/` carries the
full spec and implementation plan — but no ONNX or OpenCV code exists in `src/`
yet. There are no releases, and there is no downloadable plugin.

If you want to see the part that works, read `src/mapping/` and `tests/`.

## Build

Requires CMake and a C++20 compiler; JUCE 8 comes in as a submodule.
`docs/DEV_SETUP.md` covers dependency setup on macOS.

```bash
git clone --recursive https://github.com/brebuilds/theremin-vst
cd theremin-vst
cmake -B build
cmake --build build
```

Tests run standalone — no DAW, no camera:

```bash
cmake --build build --target theremin_tests
./build/tests/theremin_tests
```

## Built with

[JUCE 8](https://juce.com) · [Catch2](https://github.com/catchorg/Catch2) ·
planned: [ONNX Runtime](https://onnxruntime.ai), [OpenCV](https://opencv.org),
[MediaPipe HandLandmarker](https://ai.google.dev/edge/mediapipe/solutions/vision/hand_landmarker)

## License

MIT — see [LICENSE](LICENSE).
