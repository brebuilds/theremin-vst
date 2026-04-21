#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "mapping/Calibration.h"

using Catch::Approx;

TEST_CASE("Calibration::map returns (0,0) at top-left corner", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners(
        {0.1f, 0.1f},  // top-left
        {0.9f, 0.1f},  // top-right
        {0.9f, 0.9f},  // bottom-right
        {0.1f, 0.9f}); // bottom-left

    auto result = cal.map({0.1f, 0.1f});
    REQUIRE(result.x == Approx(0.0f).margin(0.001f));
    REQUIRE(result.y == Approx(0.0f).margin(0.001f));
}

TEST_CASE("Calibration::map returns (1,1) at bottom-right corner", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners({0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f});

    auto result = cal.map({0.9f, 0.9f});
    REQUIRE(result.x == Approx(1.0f).margin(0.001f));
    REQUIRE(result.y == Approx(1.0f).margin(0.001f));
}

TEST_CASE("Calibration::map returns (0.5, 0.5) at center of rectangular bounds", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners({0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f});

    auto result = cal.map({0.5f, 0.5f});
    REQUIRE(result.x == Approx(0.5f).margin(0.001f));
    REQUIRE(result.y == Approx(0.5f).margin(0.001f));
}

TEST_CASE("Calibration::map clamps values outside the quad to [0,1]", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners({0.1f, 0.1f}, {0.9f, 0.1f}, {0.9f, 0.9f}, {0.1f, 0.9f});

    auto below = cal.map({0.0f, 0.0f});
    REQUIRE(below.x >= 0.0f);
    REQUIRE(below.y >= 0.0f);

    auto above = cal.map({1.0f, 1.0f});
    REQUIRE(above.x <= 1.0f);
    REQUIRE(above.y <= 1.0f);
}

TEST_CASE("Calibration::map handles a trapezoidal quad (top narrower than bottom)", "[calibration]") {
    theremin::Calibration cal;
    cal.setCorners(
        {0.3f, 0.1f},  // top-left (narrower top)
        {0.7f, 0.1f},  // top-right
        {0.9f, 0.9f},  // bottom-right (wider bottom)
        {0.1f, 0.9f}); // bottom-left

    // Point inside the quad should map inside [0,1]x[0,1]
    auto result = cal.map({0.5f, 0.5f});
    REQUIRE(result.x >= 0.0f);
    REQUIRE(result.x <= 1.0f);
    REQUIRE(result.y >= 0.0f);
    REQUIRE(result.y <= 1.0f);
}

TEST_CASE("Calibration default (no corners set) maps input identically", "[calibration]") {
    theremin::Calibration cal;
    auto result = cal.map({0.42f, 0.73f});
    REQUIRE(result.x == Approx(0.42f));
    REQUIRE(result.y == Approx(0.73f));
}
