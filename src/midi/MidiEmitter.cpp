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
