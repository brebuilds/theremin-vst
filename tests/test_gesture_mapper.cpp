#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "mapping/GestureMapper.h"

using namespace theremin;
using Catch::Approx;

TEST_CASE("GestureMapper x=0 yields lowest note A2 (MIDI 45)", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.0f, 0.5f, p);
    REQUIRE(s.active);
    REQUIRE(s.note == 45);
}

TEST_CASE("GestureMapper x=1 at 2 octaves yields A4 (MIDI 69)", "[mapper]") {
    GestureParams p{2, 1.0f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(1.0f, 0.5f, p);
    REQUIRE(s.note == 69); // 45 + 24
}

TEST_CASE("GestureMapper x=1 at 5 octaves yields A7 (MIDI 105)", "[mapper]") {
    GestureParams p{5, 1.0f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(1.0f, 0.5f, p);
    REQUIRE(s.note == 105); // 45 + 60
}

TEST_CASE("GestureMapper y=0 (top) yields max volume CC7=127", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f, 0.0f, p);
    REQUIRE(s.cc7Volume == 127);
}

TEST_CASE("GestureMapper y=1 (bottom) yields near-zero volume", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f, 1.0f, p);
    REQUIRE(s.cc7Volume <= 5);
}

TEST_CASE("GestureMapper magnetism=1 produces no pitch bend", "[mapper]") {
    GestureParams p{2, 1.0f, ScaleKind::Chromatic, 0};
    // Half-semitone between A2 and A#2 → would normally be a pitch bend,
    // but with magnetism=1 it snaps hard.
    auto s = GestureMapper::map(0.5f / 24.0f, 0.5f, p);
    REQUIRE(s.pitchBend == 8192);
}

TEST_CASE("GestureMapper magnetism=0 produces pitch bend between semitones", "[mapper]") {
    GestureParams p{2, 0.0f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f / 24.0f, 0.5f, p);
    REQUIRE(s.pitchBend != 8192);
}

TEST_CASE("GestureMapper scale lock prevents illegal notes", "[mapper]") {
    GestureParams p{2, 1.0f, ScaleKind::Major, 0}; // C major
    // Position 1/24 = ~A#2 which is not in C major. Should snap to A2 or B2.
    auto s = GestureMapper::map(1.0f / 24.0f, 0.5f, p);
    Scale scale(ScaleKind::Major, 0);
    REQUIRE(scale.isLegalNote(s.note));
}

TEST_CASE("GestureMapper inactive returns active=false", "[mapper]") {
    auto s = GestureMapper::inactive();
    REQUIRE_FALSE(s.active);
}

TEST_CASE("GestureMapper velocity stays within 40..127 floor for audible notes", "[mapper]") {
    GestureParams p{2, 0.8f, ScaleKind::Chromatic, 0};
    auto s = GestureMapper::map(0.5f, 1.0f, p); // bottom of frame
    REQUIRE(s.velocity >= 40);
}
