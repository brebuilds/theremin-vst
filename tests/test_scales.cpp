#include <catch2/catch_test_macros.hpp>
#include "mapping/Scales.h"

using namespace theremin;

TEST_CASE("Chromatic scale accepts all notes", "[scales]") {
    Scale s(ScaleKind::Chromatic, 0); // C chromatic
    for (int n = 0; n < 128; ++n) {
        REQUIRE(s.isLegalNote(n));
        REQUIRE(s.snapToLegal(n) == n);
    }
}

TEST_CASE("C Major accepts only white-key notes", "[scales]") {
    Scale s(ScaleKind::Major, 0); // C major
    // C D E F G A B in the 4th octave (MIDI 60..71)
    REQUIRE(s.isLegalNote(60)); // C
    REQUIRE_FALSE(s.isLegalNote(61)); // C#
    REQUIRE(s.isLegalNote(62)); // D
    REQUIRE_FALSE(s.isLegalNote(63)); // D#
    REQUIRE(s.isLegalNote(64)); // E
    REQUIRE(s.isLegalNote(65)); // F
    REQUIRE_FALSE(s.isLegalNote(66)); // F#
    REQUIRE(s.isLegalNote(67)); // G
    REQUIRE_FALSE(s.isLegalNote(68)); // G#
    REQUIRE(s.isLegalNote(69)); // A
    REQUIRE_FALSE(s.isLegalNote(70)); // A#
    REQUIRE(s.isLegalNote(71)); // B
}

TEST_CASE("C Major snaps C# down to C", "[scales]") {
    Scale s(ScaleKind::Major, 0);
    REQUIRE(s.snapToLegal(61) == 60);
}

TEST_CASE("A Minor accepts A B C D E F G", "[scales]") {
    Scale s(ScaleKind::Minor, 9); // A = key 9
    REQUIRE(s.isLegalNote(69)); // A
    REQUIRE(s.isLegalNote(71)); // B
    REQUIRE(s.isLegalNote(72)); // C
    REQUIRE(s.isLegalNote(74)); // D
    REQUIRE(s.isLegalNote(76)); // E
    REQUIRE(s.isLegalNote(77)); // F
    REQUIRE(s.isLegalNote(79)); // G
    REQUIRE_FALSE(s.isLegalNote(70)); // A#
    REQUIRE_FALSE(s.isLegalNote(73)); // C#
}

TEST_CASE("C Pentatonic Major has 5 notes per octave", "[scales]") {
    Scale s(ScaleKind::PentatonicMajor, 0);
    int count = 0;
    for (int n = 60; n < 72; ++n) if (s.isLegalNote(n)) ++count;
    REQUIRE(count == 5);
}

TEST_CASE("Blues scale at C includes the b5 blue note", "[scales]") {
    Scale s(ScaleKind::Blues, 0);
    REQUIRE(s.isLegalNote(66)); // F# = b5 above C
}

TEST_CASE("snapToLegal returns closest legal note when input is between two", "[scales]") {
    Scale s(ScaleKind::Major, 0); // C major; D=62 and E=64 are legal, D#=63 is not
    // D# snaps to D (distance 1) not E (distance 1) — ties broken toward lower
    REQUIRE(s.snapToLegal(63) == 62);
}
