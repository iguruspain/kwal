// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "effects.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <unistd.h>

namespace transitions {

namespace {

// Transparency `cover` (0 = opaque, 1 = fully transparent) -> pixel scale.
constexpr std::uint32_t keep256(double cover)
{
    return static_cast<std::uint32_t>((1.0 - std::clamp(cover, 0.0, 1.0)) * 256.0 + 0.5);
}

// Pixels [a, b) are entirely inside the shape.
void fillFull(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, size_t a, size_t b)
{
    if (b <= a) {
        return;
    }
    if (paint == Paint::Reveal) {
        std::fill(buf.begin() + static_cast<long>(a), buf.begin() + static_cast<long>(b), Pixel(0));
    } else {
        std::copy(orig.begin() + static_cast<long>(a), orig.begin() + static_cast<long>(b),
                  buf.begin() + static_cast<long>(a));
    }
}

// Pixel `i` is `cover` (0..1) inside the shape.
void putPixel(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, size_t i, double cover)
{
    const std::uint32_t keep =
        paint == Paint::Reveal ? keep256(cover) : 256 - keep256(cover);
    buf[i] = scalePx(orig[i], keep);
}

// Per-row split of a half-plane edge. d(x) = a - dxX*x is the signed
// distance (px, positive = revealed) at pixel index x ignoring any wave, m
// the margin within which the exact cover must be evaluated. Returns
// (b0, f0, f1, b1): pixels [f0, f1) are fully transparent, [b0, f0) and
// [f1, b1) need per-pixel evaluation, the rest is untouched.
std::array<size_t, 4> rowRanges(size_t w, double a, double dx, double m)
{
    const double wf = static_cast<double>(w);
    const auto cl = [&w](double v) {
        return static_cast<size_t>(std::clamp(v, 0.0, static_cast<double>(w)));
    };
    if (std::abs(dx) < 1e-9) {
        if (a >= m) {
            return {0, 0, w, w};
        }
        if (a <= -m) {
            return {0, 0, 0, 0};
        }
        return {0, 0, 0, w};
    }
    if (dx > 0.0) {
        // d decreases with x: full for x <= (a-m)/dx, untouched past (a+m)/dx.
        const size_t f1 = cl(std::floor((a - m) / dx) + 1.0);
        const size_t b1 = std::max(cl(std::ceil((a + m) / dx)), f1);
        return {0, 0, f1, b1};
    }
    // d increases with x.
    const double k = -dx;
    const size_t f0 = cl(std::ceil((m - a) / k));
    const size_t b0 = std::min(cl(std::floor((-m - a) / k)), f0);
    return {b0, f0, w, w};
}

// Exact cover of pixel (x, y) for a wipe whose front is at `front` along
// dir, with an optional sine wave (width, height) on the edge.
double wipeCover(size_t x, size_t y, double dirX, double dirY, double front,
                 double waveW, double waveH)
{
    const double px = static_cast<double>(x) + 0.5;
    const double py = static_cast<double>(y) + 0.5;
    const double s = px * dirX + py * dirY;
    double off = 0.0;
    if (waveH != 0.0) {
        const double perp = -px * dirY + py * dirX;
        off = std::sin(perp / std::max(std::abs(waveW), 1e-3)) * waveH;
    }
    return front + off - s + 0.5;
}

} // namespace

double bezierEase(const Bezier &b, double t)
{
    if (t <= 0.0) {
        return 0.0;
    }
    if (t >= 1.0) {
        return 1.0;
    }
    const auto coef = [](double a, double bb) {
        const double c = 3.0 * a;
        const double d = 3.0 * (bb - a) - c;
        return std::array<double, 3>{1.0 - c - d, d, c};
    };
    const auto eval = [](const std::array<double, 3> &c, double s) {
        return (c[0] * s + c[1]) * s * s + c[2] * s;
    };
    const auto cx = coef(b.x1, b.x2);
    const auto cy = coef(b.y1, b.y2);
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < 32; ++i) {
        const double mid = (lo + hi) / 2.0;
        if (eval(cx, mid) < t) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return eval(cy, (lo + hi) / 2.0);
}

Pixel scalePx(Pixel p, std::uint32_t a)
{
    const std::uint32_t rb = (((p & 0x00FF00FF) * a) >> 8) & 0x00FF00FF;
    const std::uint32_t ag = (((p >> 8) & 0x00FF00FF) * a) & 0xFF00FF00;
    return rb | ag;
}

void drawFade(std::span<Pixel> buf, std::span<const Pixel> orig, std::uint32_t keep)
{
    keep = std::min(keep, std::uint32_t(256));
    if (keep == 0) {
        std::fill(buf.begin(), buf.end(), Pixel(0));
        return;
    }
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = scalePx(orig[i], keep);
    }
}

double maxRadius(double cx, double cy, int w, int h)
{
    const double dx = std::max(cx, static_cast<double>(w) - cx);
    const double dy = std::max(cy, static_cast<double>(h) - cy);
    return std::hypot(dx, dy);
}

void drawGrow(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
              double cx, double cy, double r)
{
    if (r <= 0.0) {
        return;
    }
    const int wu = w;
    const int hu = h;
    const double rIn = r - 0.5;
    const double rOut = r + 0.5;
    const size_t y0 = static_cast<size_t>(std::max(std::floor(cy - rOut), 0.0));
    const size_t y1 = static_cast<size_t>(std::max(std::min(std::ceil(cy + rOut), static_cast<double>(hu)), 0.0));
    for (size_t y = y0; y < y1; ++y) {
        const double dy = static_cast<double>(y) + 0.5 - cy;
        const double ho2 = rOut * rOut - dy * dy;
        if (ho2 <= 0.0) {
            continue;
        }
        const double ho = std::sqrt(ho2);
        const size_t row = y * static_cast<size_t>(wu);
        const size_t xo0 = static_cast<size_t>(std::max(std::floor(cx - ho - 0.5), 0.0));
        const size_t xo1 = static_cast<size_t>(std::max(std::min(std::ceil(cx + ho - 0.5), static_cast<double>(wu) - 1.0), 0.0));

        // Fully transparent interior: pixel centres within r - 0.5.
        const double ri2 = rIn * rIn - dy * dy;
        size_t xi0 = 0;
        size_t xi1 = 0;
        if (rIn > 0.0 && ri2 > 0.0) {
            const double hiR = std::sqrt(ri2);
            const double a = std::max(std::ceil(cx - hiR - 0.5), 0.0);
            const double b = std::min(std::floor(cx + hiR - 0.5), static_cast<double>(wu) - 1.0);
            if (a <= b) {
                xi0 = static_cast<size_t>(a);
                xi1 = static_cast<size_t>(b) + 1;
            }
        }
        const auto rim = [&](size_t x) {
            const double dx = static_cast<double>(x) + 0.5 - cx;
            const double d = std::sqrt(dx * dx + dy * dy);
            putPixel(paint, buf, orig, row + x, rOut - d);
        };
        if (xi1 > xi0) {
            fillFull(paint, buf, orig, row + xi0, row + xi1);
            for (size_t x = xo0; x < std::min(xi0, xo1 + 1); ++x) {
                rim(x);
            }
            for (size_t x = xi1; x <= xo1; ++x) {
                rim(x);
            }
        } else {
            for (size_t x = xo0; x <= xo1; ++x) {
                rim(x);
            }
        }
    }
}

void drawOuter(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
               double cx, double cy, double r)
{
    const int wu = w;
    const int hu = h;
    const double rIn = r - 0.5;
    const double rOut = r + 0.5;
    for (size_t y = 0; y < static_cast<size_t>(hu); ++y) {
        const double dy = static_cast<double>(y) + 0.5 - cy;
        const size_t row = y * static_cast<size_t>(wu);
        const double ho2 = rOut * rOut - dy * dy;
        if (rOut <= 0.0 || ho2 <= 0.0) {
            fillFull(paint, buf, orig, row, row + static_cast<size_t>(wu));
            continue;
        }
        const double ho = std::sqrt(ho2);
        // Fully transparent: centre distance >= r + 0.5.
        const size_t lEnd = static_cast<size_t>(std::clamp(std::floor(cx - ho - 0.5) + 1.0, 0.0, static_cast<double>(wu)));
        size_t rStart = static_cast<size_t>(std::clamp(std::ceil(cx + ho - 0.5), 0.0, static_cast<double>(wu)));
        rStart = std::max(rStart, lEnd);
        fillFull(paint, buf, orig, row, row + lEnd);
        fillFull(paint, buf, orig, row + rStart, row + static_cast<size_t>(wu));

        // Untouched interior: centre distance <= r - 0.5.
        const double ri2 = rIn * rIn - dy * dy;
        size_t i0 = rStart;
        size_t i1 = rStart;
        if (rIn > 0.0 && ri2 > 0.0) {
            const double hiR = std::sqrt(ri2);
            i0 = std::clamp(static_cast<size_t>(std::max(std::ceil(cx - hiR - 0.5), 0.0)), lEnd, rStart);
            i1 = std::clamp(static_cast<size_t>(std::clamp(std::floor(cx + hiR - 0.5) + 1.0, 0.0, static_cast<double>(wu))), i0, rStart);
        }
        const auto rim = [&](size_t x) {
            const double dx = static_cast<double>(x) + 0.5 - cx;
            const double d = std::sqrt(dx * dx + dy * dy);
            putPixel(paint, buf, orig, row + x, d - rIn);
        };
        for (size_t x = lEnd; x < i0; ++x) {
            rim(x);
        }
        for (size_t x = i1; x < rStart; ++x) {
            rim(x);
        }
    }
}

std::pair<double, double> projRange(int w, int h, double dirX, double dirY)
{
    const double c[4] = {
        0.0,
        static_cast<double>(w) * dirX,
        static_cast<double>(h) * dirY,
        static_cast<double>(w) * dirX + static_cast<double>(h) * dirY,
    };
    double lo = c[0];
    double hi = c[0];
    for (double v : c) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    return {lo, hi};
}

void drawWipe(Paint paint, std::span<Pixel> buf, std::span<const Pixel> orig, int w, int h,
              double dirX, double dirY, double front,
              double waveW, double waveH)
{
    const int wu = w;
    const int hu = h;
    const double m = std::abs(waveH) + 0.5;
    for (size_t y = 0; y < static_cast<size_t>(hu); ++y) {
        const double a = front - 0.5 * dirX - (static_cast<double>(y) + 0.5) * dirY;
        const auto [b0, f0, f1, b1] = rowRanges(static_cast<size_t>(wu), a, dirX, m);
        const size_t row = y * static_cast<size_t>(wu);
        fillFull(paint, buf, orig, row + f0, row + f1);
        for (size_t x = b0; x < f0; ++x) {
            putPixel(paint, buf, orig, row + x, wipeCover(x, y, dirX, dirY, front, waveW, waveH));
        }
        for (size_t x = f1; x < b1; ++x) {
            putPixel(paint, buf, orig, row + x, wipeCover(x, y, dirX, dirY, front, waveW, waveH));
        }
    }
}

double randUnit()
{
    static std::atomic<std::uint64_t> state(0);
    std::uint64_t x = state.load(std::memory_order_relaxed);
    if (x == 0) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const std::uint64_t t = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        x = t ^ (static_cast<std::uint64_t>(::getpid()) << 32) | 1u;
    }
    x += 0x9E3779B97F4A7C15u;
    state.store(x, std::memory_order_relaxed);
    std::uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) / std::ldexp(1.0, 53);
}

} // namespace transitions
