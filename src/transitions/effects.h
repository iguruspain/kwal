// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Pure pixel maths for the overlay transitions (no Wayland here, so it is unit-testable).
// The overlay holds a premultiplied ARGB8888 wallpaper (`orig`); each effect grows a
// transparent area to reveal what the compositor draws under/over it. Effects are
// **monotonic** (a pixel's transparency never decreases): shape effects rewrite only
// the moving edge and `fill(0)` the interior, and a target buffer must hold either the
// pristine image or a frame from an earlier (less advanced) state.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace transitions {

// Premultiplied ARGB8888 pixel (wl_shm ARGB8888 native-endian layout).
using Pixel = std::uint32_t;
using Buffer = std::vector<Pixel>;

enum class Paint : int {
    // Shape = transparent hole in the opaque outgoing image.
    Reveal,
    // Shape = incoming image painted over a transparent overlay.
    Cover,
};

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

double bezierEase(const Bezier &bezier, double t);

// Scale all channels by a/256; a=256 is the identity (not 255), a=0 transparent.
Pixel scalePx(Pixel p, std::uint32_t a);

void drawFade(std::span<Pixel> buf, std::span<const Pixel> orig, std::uint32_t keep);

double maxRadius(double cx, double cy, int w, int h);

// The disc of radius `r` around (cx, cy) becomes transparent (antialiased 1px rim);
// `r` must not decrease between calls.
void drawGrow(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
              double cx, double cy, double r);

// Everything *outside* the circle of radius `r` is transparent; `r` must not
// increase between calls; `r <= -0.5` makes the whole buffer transparent.
void drawOuter(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
               double cx, double cy, double r);

// (min, max) of x*dirX + y*dirY over the buffer corners; dir must be a unit vector.
std::pair<double, double> projRange(int w, int h, double dirX, double dirY);

// The half-plane p*dir < front is transparent; `front` must not decrease between
// calls; with waveH != 0 the edge gets a sine offset sin(perp / waveW) * waveH.
void drawWipe(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
              double dirX, double dirY, double front,
              double waveW, double waveH);

// Uniform random in [0, 1) (splitmix64).
double randUnit();

} // namespace transitions
