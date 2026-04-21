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
    // Map a calibrated (x, y) in [0,1]^2 to MIDI state.
    // x = 0 -> lowest note (A2, MIDI 45). x = 1 -> highest note (A2 + octaves*12).
    // y = 0 -> top of frame -> loud. y = 1 -> bottom -> silent.
    static MidiState map(float x, float y, GestureParams params);

    // Called when the hand leaves the frame -- emit an "inactive" state.
    static MidiState inactive();
};

} // namespace theremin
