// SPDX-FileCopyrightText: 2026 KDE
// SPDX-License-Identifier: MIT
#include "effects.h"

#include <array>
#include <cmath>

#include <algorithm>

namespace transitions
{

px scalePx(px p, std::uint32_t a)
{
    const px rb = (((p & 0x00FF00FFu) * a) >> 8) & 0x00FF00FFu;
    const px ag = (((p >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u;
    return rb | ag;
}

std::uint32_t keep256(double cover)
{
    return static_cast<std::uint32_t>((1.0 - std::clamp(cover, 0.0, 1.0)) * 256.0 + 0.5);
}

void put(Paint m, px *buf, const px *orig, std::size_t i, double cover)
{
    const std::uint32_t k = (m == Paint::Reveal) ? keep256(cover) : 256 - keep256(cover);
    buf[i] = scalePx(orig[i], k);
}

void fillFull(Paint m, px *buf, const px *orig, std::size_t a, std::size_t b)
{
    if (b <= a)
        return;
    if (m == Paint::Reveal)
        std::fill(buf + a, buf + b, 0u);
    else
        std::copy(orig + a, orig + b, buf + a);
}

void drawFade(px *buf, const px *orig, std::size_t n, std::uint32_t keep)
{
    keep = std::min<std::uint32_t>(keep, 256);
    if (keep == 0) {
        std::fill(buf, buf + n, 0u);
        return;
    }
    for (std::size_t i = 0; i < n; ++i)
        buf[i] = scalePx(orig[i], keep);
}

QPointF resolveCenter(const Pos &pos, bool invertY, int logicalW, int logicalH, int bufW, int bufH)
{
    const double lw = std::max(1.0, static_cast<double>(logicalW));
    const double lh = std::max(1.0, static_cast<double>(logicalH));
    const double bw = static_cast<double>(bufW);
    const double bh = static_cast<double>(bufH);
    const double x = pos.xKind == Coord::Frac ? pos.x * bw : pos.x * bw / lw;
    const double y = pos.yKind == Coord::Frac ? (invertY ? pos.y : 1.0 - pos.y) * bh
                                              : (invertY ? pos.y : lh - pos.y) * bh / lh;
    return {x, y};
}

double maxRadius(double cx, double cy, int w, int h)
{
    return std::hypot(std::max(cx, static_cast<double>(w) - cx),
                      std::max(cy, static_cast<double>(h) - cy));
}

void drawGrow(Paint m, px *buf, const px *orig, int w, int h, double cx, double cy, double r)
{
    if (r <= 0)
        return;
    const double rIn = r - 0.5, rOut = r + 0.5;
    const int y0 = static_cast<int>(std::max(0.0, std::floor(cy - rOut)));
    const int y1 = static_cast<int>(std::clamp(std::ceil(cy + rOut), 0.0, static_cast<double>(h)));
    for (int y = y0; y < y1; ++y) {
        const double dy = y + 0.5 - cy;
        const double ho2 = rOut * rOut - dy * dy;
        if (ho2 <= 0)
            continue; // the disc does not touch this row
        const double ho = std::sqrt(ho2);
        const std::size_t row = static_cast<std::size_t>(y) * w;
        const int xo0 = static_cast<int>(std::max(0.0, std::floor(cx - ho - 0.5)));
        const int xo1 = static_cast<int>(std::max(0.0, std::min(std::ceil(cx + ho - 0.5),
                                                                static_cast<double>(w) - 1.0)));
        int xi0 = 0, xi1 = 0; // fully covered stretch [xi0, xi1)
        const double ri2 = rIn * rIn - dy * dy;
        if (rIn > 0 && ri2 > 0) {
            const double hi = std::sqrt(ri2);
            const double a = std::max(0.0, std::ceil(cx - hi - 0.5));
            const double b = std::min(std::floor(cx + hi - 0.5), static_cast<double>(w) - 1.0);
            if (a <= b) {
                xi0 = static_cast<int>(a);
                xi1 = static_cast<int>(b) + 1;
            }
        }
        auto rim = [&](int x) {
            const double dx = x + 0.5 - cx;
            put(m, buf, orig, row + x, rOut - std::sqrt(dx * dx + dy * dy));
        };
        if (xi1 > xi0) {
            fillFull(m, buf, orig, row + xi0, row + xi1);
            for (int x = xo0; x < std::min(xi0, xo1 + 1); ++x)
                rim(x);
            for (int x = xi1; x <= xo1; ++x)
                rim(x);
        } else {
            for (int x = xo0; x <= xo1; ++x)
                rim(x); // small radius: only the rim
        }
    }
}

void drawOuter(Paint m, px *buf, const px *orig, int w, int h, double cx, double cy, double r)
{
    const double rIn = r - 0.5, rOut = r + 0.5;
    for (int y = 0; y < h; ++y) {
        const double dy = y + 0.5 - cy;
        const std::size_t row = static_cast<std::size_t>(y) * w;
        const double ho2 = rOut * rOut - dy * dy;
        if (rOut <= 0 || ho2 <= 0) {
            fillFull(m, buf, orig, row, row + w);
            continue; // the whole row is outside
        }
        const double ho = std::sqrt(ho2);
        const int lEnd = static_cast<int>(std::clamp(std::floor(cx - ho - 0.5) + 1.0, 0.0,
                                                     static_cast<double>(w)));
        const int rStart = std::max(static_cast<int>(std::clamp(std::ceil(cx + ho - 0.5), 0.0,
                                                                static_cast<double>(w))),
                                    lEnd);
        fillFull(m, buf, orig, row, row + lEnd);          // outside on the left
        fillFull(m, buf, orig, row + rStart, row + w);    // outside on the right
        int i0 = rStart, i1 = rStart; // untouched interior [i0, i1)
        const double ri2 = rIn * rIn - dy * dy;
        if (rIn > 0 && ri2 > 0) {
            const double hi = std::sqrt(ri2);
            i0 = std::clamp(static_cast<int>(std::max(0.0, std::ceil(cx - hi - 0.5))), lEnd, rStart);
            i1 = std::clamp(static_cast<int>(std::clamp(std::floor(cx + hi - 0.5) + 1.0, 0.0,
                                                        static_cast<double>(w))),
                            i0, rStart);
        }
        auto rim = [&](int x) {
            const double dx = x + 0.5 - cx;
            put(m, buf, orig, row + x, std::sqrt(dx * dx + dy * dy) - rIn); // cover = dist - (r - 0.5)
        };
        for (int x = lEnd; x < i0; ++x)
            rim(x);
        for (int x = i1; x < rStart; ++x)
            rim(x);
    }
}

double wipeCover(int x, int y, double dx, double dy, double front, const Wave &wv)
{
    const double px = x + 0.5, py = y + 0.5;
    const double s = px * dx + py * dy;
    double off = 0;
    if (wv.height != 0) {
        const double perp = -px * dy + py * dx;
        off = std::sin(perp / std::max(std::abs(wv.width), 1e-3)) * wv.height;
    }
    return front + off - s + 0.5; // >= 1 fully covered, <= 0 untouched
}

Span rowRanges(std::size_t w, double a, double dx, double m)
{
    auto cl = [&](double v) {
        return static_cast<std::size_t>(std::clamp(v, 0.0, static_cast<double>(w)));
    };
    if (std::abs(dx) < 1e-9) // horizontal front: the whole row is equal
        return a >= m ? Span{0, 0, w, w} : a <= -m ? Span{0, 0, 0, 0} : Span{0, 0, 0, w};
    if (dx > 0) { // d decreases with x
        const std::size_t f1 = cl(std::floor((a - m) / dx) + 1.0);
        const std::size_t b1 = std::max(cl(std::ceil((a + m) / dx)), f1);
        return {0, 0, f1, b1};
    }
    const double k = -dx; // d increases with x
    const std::size_t f0 = cl(std::ceil((m - a) / k));
    const std::size_t b0 = std::min(cl(std::floor((-m - a) / k)), f0);
    return {b0, f0, w, w};
}

void drawWipe(Paint m, px *buf, const px *orig, int w, int h, double dx, double dy, double front,
              const Wave &wv)
{
    const double margin = std::abs(wv.height) + 0.5;
    for (int y = 0; y < h; ++y) {
        const double a = front - 0.5 * dx - (y + 0.5) * dy;
        const Span sp = rowRanges(static_cast<std::size_t>(w), a, dx, margin);
        const std::size_t row = static_cast<std::size_t>(y) * w;
        fillFull(m, buf, orig, row + sp.f0, row + sp.f1);
        for (std::size_t x = sp.b0; x < sp.f0; ++x)
            put(m, buf, orig, row + x, wipeCover(static_cast<int>(x), y, dx, dy, front, wv));
        for (std::size_t x = sp.f1; x < sp.b1; ++x)
            put(m, buf, orig, row + x, wipeCover(static_cast<int>(x), y, dx, dy, front, wv));
    }
}

std::pair<double, double> projRange(int w, int h, double dx, double dy)
{
    const double c[4] = {0.0,
                         static_cast<double>(w) * dx,
                         static_cast<double>(h) * dy,
                         static_cast<double>(w) * dx + static_cast<double>(h) * dy};
    return {*std::min_element(c, c + 4), *std::max_element(c, c + 4)};
}

double bezierEase(const Bezier &b, double t)
{
    if (t <= 0)
        return 0;
    if (t >= 1)
        return 1;
    auto coef = [](double a, double c2) {
        const double c = 3 * a;
        const double bb = 3 * (c2 - a) - c;
        return std::array<double, 3>{1 - c - bb, bb, c};
    };
    auto ev = [](const std::array<double, 3> &k, double s) {
        return ((k[0] * s + k[1]) * s + k[2]) * s;
    };
    const auto kx = coef(b.x1, b.x2);
    const auto ky = coef(b.y1, b.y2);
    double lo = 0, hi = 1;
    for (int i = 0; i < 32; ++i) {
        const double mid = (lo + hi) / 2;
        if (ev(kx, mid) < t)
            lo = mid;
        else
            hi = mid;
    }
    return ev(ky, (lo + hi) / 2);
}

Frame stepFrame(const TransitionSpec::Resolved &spec, double t, int frameN)
{
    t = std::clamp(t, 0.0, 1.0);
    Frame f;
    switch (spec.effect) {
    case TransitionSpec::Resolved::Effect::None:
        f.p = 1.0;
        f.done = true;
        break;
    case TransitionSpec::Resolved::Effect::Simple: {
        const int covered = std::min(frameN * spec.step, 255);
        f.p = covered / 255.0;
        f.done = covered >= 255;
        break;
    }
    default:
        f.p = bezierEase(spec.bezier, t);
        f.done = t >= 1.0;
        break;
    }
    return f;
}

void drawFrame(const TransitionSpec::Resolved &spec, Paint m, px *buf, const px *orig, int bufW,
               int bufH, int logicalW, int logicalH, double p, bool done)
{
    switch (spec.effect) {
    case TransitionSpec::Resolved::Effect::None:
    case TransitionSpec::Resolved::Effect::Simple:
    case TransitionSpec::Resolved::Effect::Fade: {
        std::uint32_t keep;
        if (done)
            keep = (m == Paint::Reveal) ? 0 : 256;
        else
            keep = (m == Paint::Reveal) ? static_cast<std::uint32_t>(std::lround((1.0 - p) * 256.0))
                                        : static_cast<std::uint32_t>(std::lround(p * 256.0));
        drawFade(buf, orig, static_cast<std::size_t>(bufW) * bufH, keep);
        break;
    }
    case TransitionSpec::Resolved::Effect::Wipe:
    case TransitionSpec::Resolved::Effect::Wave: {
        const Wave wv = spec.wave; // Wipe: {20, 0}; Wave: {width, |height|} (see resolve())
        const double a = spec.angle * M_PI / 180.0;
        const double dx = -std::cos(a), dy = std::sin(a);
        const auto [lo, hi] = projRange(bufW, bufH, dx, dy);
        const double mg = std::abs(wv.height) + 1.0;
        const double front = done ? hi + mg : (lo - mg) + p * ((hi + mg) - (lo - mg));
        drawWipe(m, buf, orig, bufW, bufH, dx, dy, front, wv);
        break;
    }
    case TransitionSpec::Resolved::Effect::Grow: {
        const QPointF c = resolveCenter(spec.pos, spec.invertY, logicalW, logicalH, bufW, bufH);
        const double rMax = maxRadius(c.x(), c.y(), bufW, bufH);
        const double r = done ? rMax + 2.0 : p * rMax;
        drawGrow(m, buf, orig, bufW, bufH, c.x(), c.y(), r);
        break;
    }
    case TransitionSpec::Resolved::Effect::Outer: {
        const QPointF c = resolveCenter(spec.pos, spec.invertY, logicalW, logicalH, bufW, bufH);
        const double rMax = maxRadius(c.x(), c.y(), bufW, bufH) + 1.0;
        const double r = done ? -1.0 : (1.0 - p) * rMax - p;
        drawOuter(m, buf, orig, bufW, bufH, c.x(), c.y(), r);
        break;
    }
    }
}

}
