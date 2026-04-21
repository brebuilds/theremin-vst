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

    MidiState off = GestureMapper::inactive();
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
