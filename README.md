# theremin-vst

A Windows VST3 plugin that uses a webcam + hand tracking to emit MIDI. Wave your hand in front of your camera → pitch + volume + filter control → drive any synth plugin in your DAW.

**Status:** 🚧 In design. See [`docs/superpowers/specs/2026-04-21-theremin-vst-design.md`](docs/superpowers/specs/2026-04-21-theremin-vst-design.md) for the full spec.

## How it works

1. Plugin loads in your DAW on a MIDI track.
2. Your webcam tracks your index fingertip using an on-device ML model (no cloud).
3. X position → MIDI note + pitch bend. Y position → volume + filter cutoff.
4. Route the MIDI output to any synth plugin on the next track. That synth makes the sound.

## Install (when v1 ships)

1. Download `Theremin.vst3` from [Releases](https://github.com/brebuilds/theremin-vst/releases/latest)
2. Drop into `C:\Program Files\Common Files\VST3\`
3. Rescan plugins in your DAW

## Built with

- [JUCE 8](https://juce.com) — C++ audio plugin framework
- [ONNX Runtime](https://onnxruntime.ai) — hand-tracking inference
- [OpenCV](https://opencv.org) — webcam capture
- [MediaPipe HandLandmarker](https://ai.google.dev/edge/mediapipe/solutions/vision/hand_landmarker) — hand-tracking model

## License

MIT
