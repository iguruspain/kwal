// SPDX-FileCopyrightText: 2026 KDE
// SPDX-License-Identifier: MIT
#pragma once

// Pure pixel maths for the wallpaper-transition overlay.
// plan-implementacion-transiciones-plasma.md §4.2 is the normative
// specification; this is a line-by-line port of it (and of the Rust
// reference, kdewall-plugin-switcher src/effects.rs) so the results match
// the Appendix C test vectors bit for bit. No Qt Gui and no per-frame
// allocation: the buffer is modified in place and each pixel is written
// only while the shape passes over it (monotonic shapes, §4.2.11).

#include "transitionspec.h"

#include <QPointF>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace transitions
{

// Premultiplied ARGB32 pixel: A<<24 | R<<16 | G<<8 | B, the native format of
// QImage::Format_ARGB32_Premultiplied on little endian.
using px = std::uint32_t;

// What a shape does to the overlay buffer.
enum class Paint
{
    Reveal, // orig = the outgoing wallpaper; the buffer starts as orig; the shape becomes transparent
    Cover,  // orig = the incoming wallpaper; the buffer starts at zeros; the shape becomes orig
};

// Multiplies the four channels of a premultiplied pixel by a/256
// (a in 0..256; 256 is the identity, 0 is fully transparent). Truncates.
px scalePx(px p, std::uint32_t a);

// Cover (0..1, clamped) → how many of the 256 levels of the original stay.
std::uint32_t keep256(double cover);

// Pixel i is `cover` inside the shape.
void put(Paint m, px *buf, const px *orig, std::size_t i, double cover);

// Pixels [a, b) are entirely inside the shape.
void fillFull(Paint m, px *buf, const px *orig, std::size_t a, std::size_t b);

// Whole-image fade: every pixel keeps keep/256 of the original (keep 0..256).
void drawFade(px *buf, const px *orig, std::size_t n, std::uint32_t keep);

// The centre of a Pos in buffer pixels, y from the top (§4.2.9).
QPointF resolveCenter(const Pos &pos, bool invertY, int logicalW, int logicalH, int bufW, int bufH);

// Distance from (cx, cy) to the farthest corner of a w x h buffer.
double maxRadius(double cx, double cy, int w, int h);

// grow: the disc of radius r around (cx, cy), 1 px antialiased rim. r must
// not decrease between calls.
void drawGrow(Paint m, px *buf, const px *orig, int w, int h, double cx, double cy, double r);

// outer: everything outside the circle of radius r; r must not increase
// between calls; r <= -0.5 fills the whole buffer.
void drawOuter(Paint m, px *buf, const px *orig, int w, int h, double cx, double cy, double r);

// Exact cover of pixel (x, y) for a wipe whose front is at `front` along
// dir, with an optional sine wave (wv.height != 0).
double wipeCover(int x, int y, double dx, double dy, double front, const Wave &wv);

// Per-row split of a half-plane edge: d(x) = a - dx*x is the signed distance
// at pixel x ignoring the wave, m the margin within which the exact cover
// must be evaluated. Pixels [f0, f1) are full, [b0, f0) and [f1, b1) need
// per-pixel evaluation, the rest stays untouched.
struct Span
{
    std::size_t b0, f0, f1, b1;
};
Span rowRanges(std::size_t w, double a, double dx, double m);

// wipe / wave: the half-plane p·dir < front (plus the sine offset on the
// edge when wv.height != 0) is painted. front must not decrease between
// calls.
void drawWipe(Paint m, px *buf, const px *orig, int w, int h, double dx, double dy, double front,
              const Wave &wv);

// Min and max of the buffer's corners projected on dir (a unit vector).
std::pair<double, double> projRange(int w, int h, double dx, double dy);

// CSS cubic-bezier easing (x1, y1, x2, y2), x(s)=t solved by bisection.
double bezierEase(const Bezier &b, double t);

// Conductor (§4.2.8): progress p and done flag for one frame.
// t = min(elapsed / duration, 1); frameN = frame number from 1 (simple only).
struct Frame
{
    double p = 0.0;
    bool done = false;
};
Frame stepFrame(const TransitionSpec::Resolved &spec, double t, int frameN);

// Draws one frame of the resolved effect at progress p into buf, which must
// hold orig (Reveal) or zeros (Cover) or an earlier frame of the same
// animation. (bufW, bufH) is the buffer size, (logicalW, logicalH) the
// logical surface size (for pixel positions). done forces the exact end
// state.
void drawFrame(const TransitionSpec::Resolved &spec, Paint m, px *buf, const px *orig, int bufW,
               int bufH, int logicalW, int logicalH, double p, bool done);

}
