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
