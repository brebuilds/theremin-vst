#include "Calibration.h"
#include <algorithm>
#include <cmath>

namespace theremin {

namespace {
    float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

    // Solve for (u, v) such that:
    //   P = (1-u)(1-v)*tl + u(1-v)*tr + u*v*br + (1-u)*v*bl
    // via iterative Newton-Raphson (converges in ~5 iters for reasonable quads).
    Point2D inverseBilerp(Point2D p, Point2D tl, Point2D tr, Point2D br, Point2D bl)
    {
        float u = 0.5f, v = 0.5f;
        for (int i = 0; i < 12; ++i) {
            float one_minus_u = 1.0f - u;
            float one_minus_v = 1.0f - v;

            float Px = one_minus_u * one_minus_v * tl.x
                     + u * one_minus_v * tr.x
                     + u * v * br.x
                     + one_minus_u * v * bl.x;
            float Py = one_minus_u * one_minus_v * tl.y
                     + u * one_minus_v * tr.y
                     + u * v * br.y
                     + one_minus_u * v * bl.y;

            float dPx_du = -one_minus_v * tl.x + one_minus_v * tr.x + v * br.x - v * bl.x;
            float dPx_dv = -one_minus_u * tl.x - u * tr.x + u * br.x + one_minus_u * bl.x;
            float dPy_du = -one_minus_v * tl.y + one_minus_v * tr.y + v * br.y - v * bl.y;
            float dPy_dv = -one_minus_u * tl.y - u * tr.y + u * br.y + one_minus_u * bl.y;

            float det = dPx_du * dPy_dv - dPx_dv * dPy_du;
            if (std::abs(det) < 1e-8f) break;

            float ex = Px - p.x;
            float ey = Py - p.y;

            float du = (dPy_dv * ex - dPx_dv * ey) / det;
            float dv = (-dPy_du * ex + dPx_du * ey) / det;

            u -= du;
            v -= dv;

            if (std::abs(du) < 1e-5f && std::abs(dv) < 1e-5f) break;
        }
        return { clamp01(u), clamp01(v) };
    }
}

void Calibration::setCorners(Point2D tl_, Point2D tr_, Point2D br_, Point2D bl_)
{
    tl = tl_;
    tr = tr_;
    br = br_;
    bl = bl_;
    calibrated = true;
}

Point2D Calibration::map(Point2D input) const
{
    if (!calibrated) {
        return { clamp01(input.x), clamp01(input.y) };
    }
    return inverseBilerp(input, tl, tr, br, bl);
}

} // namespace theremin
