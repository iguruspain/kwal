// Pure pixel maths for the overlay transitions (no Wayland here, so it is
// unit-testable).
//
// Model: the overlay holds the *outgoing* wallpaper (`orig`, opaque,
// premultiplied ARGB8888 as native `uint32_t`: `A<<24 | R<<16 | G<<8 | B`,
// the wl_shm ARGB8888 layout). Each effect makes a growing part of it
// transparent, revealing the new wallpaper that the compositor draws
// underneath. Every effect is **monotonic**: a pixel's transparency never
// decreases over time. That lets the shape-based effects (grow, outer, wipe,
// wave) rewrite only the pixels near the moving edge and `fill(0)` the
// interior, instead of touching the whole buffer.
//
// Drawing into a buffer therefore requires that it holds either the pristine
// image or a frame drawn with an earlier (less advanced) state.
//
// Two paint modes share the same shapes (Paint):
// * Reveal: `orig` is the *outgoing* wallpaper; the shape becomes
//   transparent, showing the new wallpaper underneath (buffer starts as
//   `orig`);
// * Cover: `orig` is the *incoming* wallpaper; the shape becomes `orig`,
//   covering the old wallpaper underneath (buffer starts fully transparent).
//
// Port of the awww transition suite (effects.rs of the Rust reference).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace transitions {

// Premultiplied ARGB8888 pixel (wl_shm ARGB8888 native-endian layout).
using Pixel = std::uint32_t;
using Buffer = std::vector<Pixel>;

// What the shape does to the overlay.
enum class Paint : int {
    // Shape = transparent hole in the opaque outgoing image.
    Reveal,
    // Shape = incoming image painted over a transparent overlay.
    Cover,
};

// CSS-style cubic bezier control points (x1, y1, x2, y2).
struct Bezier {
    double x1 = 0.54;
    double y1 = 0.0;
    double x2 = 0.34;
    double y2 = 0.99;

    bool operator==(const Bezier &o) const
    {
        return x1 == o.x1 && y1 == o.y1 && x2 == o.x2 && y2 == o.y2;
    }
};

// Evaluates the easing curve at time t in [0, 1].
double bezierEase(const Bezier &bezier, double t);

// Multiplies all four channels of a premultiplied pixel by a / 256
// (a in 0..=256; 256 is the identity, 0 gives fully transparent).
Pixel scalePx(Pixel p, std::uint32_t a);

// Whole-image fade: every pixel keeps `keep` / 256 of the original.
void drawFade(std::span<Pixel> buf, std::span<const Pixel> orig, std::uint32_t keep);

// Distance from (cx, cy) to the farthest corner of a w*h buffer.
double maxRadius(double cx, double cy, int w, int h);

// grow: the disc of radius `r` around (cx, cy) is transparent (antialiased
// 1px rim). `r` must not decrease between calls.
void drawGrow(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
              double cx, double cy, double r);

// outer: everything *outside* the circle of radius `r` is transparent.
// `r` must not increase between calls (the circle shrinks); `r <= -0.5`
// makes the whole buffer transparent.
void drawOuter(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
               double cx, double cy, double r);

// Projection range of the buffer's corners on `dir` (a unit vector):
// (min, max) of x*dirX + y*dirY.
std::pair<double, double> projRange(int w, int h, double dirX, double dirY);

// wipe / wave: the half-plane p*dir < front (plus a sine offset
// sin(perp / waveW) * waveH on the edge when waveH != 0) is transparent.
// `front` must not decrease between calls.
void drawWipe(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
              double dirX, double dirY, double front,
              double waveW, double waveH);

// Uniform random number in [0, 1) (splitmix64; no external dependency).
double randUnit();

} // namespace transitions
