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
