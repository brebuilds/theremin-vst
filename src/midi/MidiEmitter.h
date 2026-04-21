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
