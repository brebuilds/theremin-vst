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
    // Clamp to +/- 2 semitones (our pitch-bend range).
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
