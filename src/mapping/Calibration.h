#pragma once

namespace theremin {

struct Point2D {
    float x;
    float y;
};

class Calibration {
public:
    Calibration() = default;

    // Set the four corners in normalized image coords (0..1).
    // Order: top-left, top-right, bottom-right, bottom-left.
    void setCorners(Point2D tl, Point2D tr, Point2D br, Point2D bl);

    // Map a hand-position point (normalized image coords) into the calibrated
    // unit square. Output is clamped to [0,1] x [0,1].
    // If setCorners has not been called, returns input unchanged (clamped).
    Point2D map(Point2D input) const;

    bool isCalibrated() const { return calibrated; }

private:
    Point2D tl{0.0f, 0.0f};
    Point2D tr{1.0f, 0.0f};
    Point2D br{1.0f, 1.0f};
    Point2D bl{0.0f, 1.0f};
    bool calibrated = false;
};

} // namespace theremin
