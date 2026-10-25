// Unit tests for the pure transition maths and parsers (no Wayland). The
// vectors are ported from the test modules of the Rust reference
// (effects.rs, transitions.rs, monitors.rs, lib.rs).
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "transitions/effects.h"
#include "transitions/outputs.h"
#include "transitions/path.h"
#include "transitions/planes.h"
#include "transitions/transitionspec.h"

using namespace transitions;

namespace {

constexpr Pixel OPAQUE = 0xFF4080C0;

Buffer solid(int w, int h)
{
    return Buffer(static_cast<size_t>(w) * h, OPAQUE);
}

std::uint32_t alpha(const Buffer &buf, int w, int x, int y)
{
    return buf[static_cast<size_t>(y) * w + x] >> 24;
}

std::pair<double, double> dirOf(double angleDeg)
{
    const double a = angleDeg * M_PI / 180.0;
    return {-std::cos(a), std::sin(a)};
}

// Reference implementation of the per-pixel wipe cover (the formula
// documented in effects.h), used to check the incremental fast path.
std::uint32_t keep256(double cover)
{
    return static_cast<std::uint32_t>((1.0 - std::clamp(cover, 0.0, 1.0)) * 256.0 + 0.5);
}

double bruteCover(size_t x, size_t y, double dirX, double dirY, double front,
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

Buffer bruteWipe(Paint paint, const Buffer &orig, int w, int h, double dirX, double dirY,
                 double front, double waveW, double waveH)
{
    Buffer out = paint == Paint::Cover ? Buffer(orig.size(), Pixel(0)) : orig;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t keep =
                paint == Paint::Reveal ? keep256(bruteCover(x, y, dirX, dirY, front, waveW,
                                                            waveH))
                                       : 256 - keep256(bruteCover(x, y, dirX, dirY, front,
                                                                  waveW, waveH));
            out[static_cast<size_t>(y) * w + x] = scalePx(orig[static_cast<size_t>(y) * w + x],
                                                          keep);
        }
    }
    return out;
}

} // namespace

class TestTransitions : public QObject
{
    Q_OBJECT
private slots:
    void easingIsMonotonicAndBounded()
    {
        const Bezier b{0.54, 0.0, 0.34, 0.99};
        double prev = 0.0;
        for (int i = 0; i <= 100; ++i) {
            const double v = bezierEase(b, i / 100.0);
            QVERIFY(v >= prev - 1e-9);
            QVERIFY(v >= 0.0 && v <= 1.0);
            prev = v;
        }
        // Linear curve is the identity.
        const Bezier linear{0.0, 0.0, 1.0, 1.0};
        QVERIFY(std::abs(bezierEase(linear, 0.37) - 0.37) < 1e-6);
    }

    void scalePxIdentityZeroAndHalf()
    {
        QCOMPARE(scalePx(OPAQUE, 256), OPAQUE);
        QCOMPARE(scalePx(OPAQUE, 0), Pixel(0));
        QCOMPARE(scalePx(0xFF8080FE, 128), Pixel(0x7F40407F));
    }

    void fadeScalesEverything()
    {
        const Buffer orig = solid(8, 8);
        Buffer buf = orig;
        drawFade(buf, orig, 128);
        for (Pixel p : buf) {
            QCOMPARE(p >> 24, std::uint32_t(0x7F));
        }
        drawFade(buf, orig, 0);
        for (Pixel p : buf) {
            QCOMPARE(p, Pixel(0));
        }
    }

    void growRevealsEverythingAtMaxRadius()
    {
        const int w = 97, h = 61;
        const Buffer orig = solid(w, h);
        Buffer buf = orig;
        const double cx = w / 2.0, cy = h / 2.0;
        drawGrow(Paint::Reveal, buf, orig, w, h, cx, cy, maxRadius(cx, cy, w, h) + 2.0);
        for (Pixel p : buf) {
            QCOMPARE(p, Pixel(0));
        }
    }

    void growIsIncrementalAndMatchesFromScratch()
    {
        const int w = 120, h = 80;
        const Buffer orig = solid(w, h);
        const double cx = 30.0, cy = 25.0;
        const double radii[] = {3.0, 3.4, 10.0, 22.5, 41.0};
        Buffer inc = orig;
        for (double r : radii) {
            drawGrow(Paint::Reveal, inc, orig, w, h, cx, cy, r);
        }
        Buffer direct = orig;
        drawGrow(Paint::Reveal, direct, orig, w, h, cx, cy, 41.0);
        QCOMPARE(inc, direct);
        QCOMPARE(alpha(direct, w, 30, 25), std::uint32_t(0));
        QCOMPARE(alpha(direct, w, w - 1, h - 1), std::uint32_t(255));
    }

    void outerShrinksToNothingAndIsIncremental()
    {
        const int w = 110, h = 70;
        const Buffer orig = solid(w, h);
        const double cx = 40.0, cy = 30.0;
        const double rmax = maxRadius(cx, cy, w, h) + 1.0;
        // Start: nothing revealed.
        Buffer start = orig;
        drawOuter(Paint::Reveal, start, orig, w, h, cx, cy, rmax);
        QCOMPARE(start, orig);
        // Shrink step by step == drawing straight to the final radius.
        const double radii[] = {rmax, 60.0, 33.3, 12.0, 5.5};
        Buffer inc = orig;
        for (double r : radii) {
            drawOuter(Paint::Reveal, inc, orig, w, h, cx, cy, r);
        }
        Buffer direct = orig;
        drawOuter(Paint::Reveal, direct, orig, w, h, cx, cy, 5.5);
        QCOMPARE(inc, direct);
        QCOMPARE(alpha(direct, w, 40, 30), std::uint32_t(255)); // still inside
        QCOMPARE(alpha(direct, w, 100, 60), std::uint32_t(0)); // outside
        // End: everything revealed.
        drawOuter(Paint::Reveal, inc, orig, w, h, cx, cy, -1.0);
        for (Pixel p : inc) {
            QCOMPARE(p, Pixel(0));
        }
    }

    void wipeMatchesBruteForceForManyAngles()
    {
        const int w = 83, h = 47;
        const Buffer orig = solid(w, h);
        const double angles[] = {0.0, 17.0, 45.0, 90.0, 133.0, 180.0, 225.0, 270.0, 359.0};
        const double waves[][2] = {{20.0, 0.0}, {20.0, 12.0}, {7.0, 5.0}};
        for (double angle : angles) {
            const auto [dirX, dirY] = dirOf(angle);
            const auto [lo, hi] = projRange(w, h, dirX, dirY);
            for (int k = 0; k <= 10; ++k) {
                const double front = lo - 1.0 + (hi - lo + 2.0) * k / 10.0;
                for (const auto &wave : waves) {
                    for (Paint paint : {Paint::Reveal, Paint::Cover}) {
                        Buffer got = paint == Paint::Cover ? Buffer(orig.size(), Pixel(0))
                                                           : orig;
                        drawWipe(paint, got, orig, w, h, dirX, dirY, front, wave[0], wave[1]);
                        QCOMPARE(got, bruteWipe(paint, orig, w, h, dirX, dirY, front, wave[0],
                                                 wave[1]));
                    }
                }
            }
        }
    }

    void wipeIsIncrementalAndComplete()
    {
        const int w = 90, h = 60;
        const Buffer orig = solid(w, h);
        const auto [dirX, dirY] = dirOf(45.0);
        const double waveW = 20.0, waveH = 10.0;
        const auto [lo0, hi0] = projRange(w, h, dirX, dirY);
        const double lo = lo0 - 11.0, hi = hi0 + 11.0;
        Buffer inc = orig;
        drawWipe(Paint::Reveal, inc, orig, w, h, dirX, dirY, lo, waveW, waveH);
        QCOMPARE(inc, orig); // nothing revealed at the start
        for (int k = 1; k <= 20; ++k) {
            drawWipe(Paint::Reveal, inc, orig, w, h, dirX, dirY, lo + (hi - lo) * k / 20.0,
                     waveW, waveH);
        }
        for (Pixel p : inc) {
            QCOMPARE(p, Pixel(0)); // everything revealed at the end
        }
    }

    void leftWipeGoesLeftToRight()
    {
        // awww: `left` = angle 180 = from the left edge to the right edge.
        const int w = 100, h = 40;
        const Buffer orig = solid(w, h);
        auto [dirX, dirY] = dirOf(180.0);
        auto [lo, hi] = projRange(w, h, dirX, dirY);
        Buffer buf = orig;
        drawWipe(Paint::Reveal, buf, orig, w, h, dirX, dirY, lo + (hi - lo) * 0.5, 20.0, 0.0);
        QCOMPARE(alpha(buf, w, 5, 20), std::uint32_t(0));
        QCOMPARE(alpha(buf, w, 95, 20), std::uint32_t(255));
        // `top` = angle 90 = top edge to bottom edge.
        std::tie(dirX, dirY) = dirOf(90.0);
        std::tie(lo, hi) = projRange(w, h, dirX, dirY);
        buf = orig;
        drawWipe(Paint::Reveal, buf, orig, w, h, dirX, dirY, lo + (hi - lo) * 0.5, 20.0, 0.0);
        QCOMPARE(alpha(buf, w, 50, 2), std::uint32_t(0));
        QCOMPARE(alpha(buf, w, 50, 38), std::uint32_t(255));
    }

    void premultipliedRimsNeverExceedAlpha()
    {
        const int w = 64, h = 64;
        const Buffer orig(static_cast<size_t>(w) * h, Pixel(0xFFFFFFFF));
        Buffer a = orig;
        drawGrow(Paint::Reveal, a, orig, w, h, 32.0, 32.0, 20.3);
        Buffer b = orig;
        drawOuter(Paint::Reveal, b, orig, w, h, 32.0, 32.0, 20.3);
        const auto [dirX, dirY] = dirOf(30.0);
        Buffer c = orig;
        drawWipe(Paint::Reveal, c, orig, w, h, dirX, dirY, 40.0, 20.0, 8.0);
        for (const auto &buf : {a, b, c}) {
            for (Pixel p : buf) {
                const auto al = p >> 24;
                QVERIFY(((p >> 16) & 255) <= al);
                QVERIFY(((p >> 8) & 255) <= al);
                QVERIFY((p & 255) <= al);
            }
        }
    }

    void coverStartsTransparentEndsAsIncoming()
    {
        const int w = 101, h = 67;
        Buffer img(static_cast<size_t>(w) * h);
        for (size_t i = 0; i < img.size(); ++i) {
            img[i] = 0xFF000000 | (static_cast<Pixel>(i) * 2654435761u) & 0x00FFFFFF;
        }
        const double cx = 50.0, cy = 20.0;
        const double rmax = maxRadius(cx, cy, w, h) + 2.0;
        // grow: transparent at the start, exact image at the end.
        Buffer buf(img.size(), Pixel(0));
        drawGrow(Paint::Cover, buf, img, w, h, cx, cy, 0.0);
        for (Pixel p : buf) {
            QCOMPARE(p, Pixel(0));
        }
        const double radii[] = {4.0, 9.5, 31.0, 70.0, rmax};
        for (double r : radii) {
            drawGrow(Paint::Cover, buf, img, w, h, cx, cy, r);
        }
        QCOMPARE(buf, img);
        // incremental == direct for grow
        Buffer inc(img.size(), Pixel(0));
        const double steps[] = {5.0, 6.0, 20.0, 33.3};
        for (double r : steps) {
            drawGrow(Paint::Cover, inc, img, w, h, cx, cy, r);
        }
        Buffer direct(img.size(), Pixel(0));
        drawGrow(Paint::Cover, direct, img, w, h, cx, cy, 33.3);
        QCOMPARE(inc, direct);
        // outer: starts transparent, ends as the image
        Buffer o(img.size(), Pixel(0));
        const double start = maxRadius(cx, cy, w, h) + 1.0;
        drawOuter(Paint::Cover, o, img, w, h, cx, cy, start);
        for (Pixel p : o) {
            QCOMPARE(p, Pixel(0));
        }
        const double shrinks[] = {60.0, 25.0, 8.0, 2.0, -1.0};
        for (double r : shrinks) {
            drawOuter(Paint::Cover, o, img, w, h, cx, cy, r);
        }
        QCOMPARE(o, img);
        const auto [dirX, dirY] = dirOf(30.0);
        const auto [lo, hi] = projRange(w, h, dirX, dirY);
        Buffer wp(img.size(), Pixel(0));
        for (int k = 0; k <= 12; ++k) {
            drawWipe(Paint::Cover, wp, img, w, h, dirX, dirY, (lo - 1.0) + (hi - lo + 2.0) * k / 12.0,
                     20.0, 0.0);
        }
        QCOMPARE(wp, img);
    }

    void revealAndCoverAreComplementary()
    {
        const int w = 80, h = 50;
        const Buffer orig = solid(w, h);
        Buffer rev = orig;
        Buffer cov(orig.size(), Pixel(0));
        drawGrow(Paint::Reveal, rev, orig, w, h, 30.0, 20.0, 17.3);
        drawGrow(Paint::Cover, cov, orig, w, h, 30.0, 20.0, 17.3);
        for (size_t i = 0; i < rev.size(); ++i) {
            const auto sum = (rev[i] >> 24) + (cov[i] >> 24);
            QVERIFY(sum >= 254 && sum <= 256);
        }
    }

    void randIsInRangeAndVaries()
    {
        std::vector<double> v;
        bool varied = false;
        for (int i = 0; i < 50; ++i) {
            const double x = randUnit();
            QVERIFY(x >= 0.0 && x < 1.0);
            if (i > 0 && v.back() != x) {
                varied = true;
            }
            v.push_back(x);
        }
        QVERIFY(varied);
    }

    void posParsingFollowsAwww()
    {
        QCOMPARE(parsePos(QStringLiteral("center")).value(), Pos());
        Pos expected{};
        expected.x = Coord::frac(0.25);
        expected.y = Coord::frac(0.75);
        QCOMPARE(parsePos(QStringLiteral("0.25,0.75")).value(), expected);
        expected.x = Coord::px(200.0);
        expected.y = Coord::px(400.0);
        QCOMPARE(parsePos(QStringLiteral("200,400")).value(), expected);
        expected.x = Coord::frac(0.5);
        expected.y = Coord::frac(0.1);
        QCOMPARE(parsePos(QStringLiteral("50%,10%")).value(), expected);
        QVERIFY(!parsePos(QStringLiteral("nope")).has_value());
        QVERIFY(!parsePos(QStringLiteral("1,x")).has_value());
    }

    void posYCountsFromBottomUnlessInverted()
    {
        // (1000, 500) logical, (2000, 1000) buffer: 2x.
        const Pos top = parsePos(QStringLiteral("top")).value();
        QCOMPARE(top.toPx(false, 1000, 500, 2000, 1000), std::pair(1000.0, 0.0));
        QCOMPARE(top.toPx(true, 1000, 500, 2000, 1000), std::pair(1000.0, 1000.0));
        // 200px from the left, 100px from the bottom -> buffer px, y from top.
        const Pos p = parsePos(QStringLiteral("200,100")).value();
        QCOMPARE(p.toPx(false, 1000, 500, 2000, 1000), std::pair(400.0, 800.0));
        QCOMPARE(p.toPx(true, 1000, 500, 2000, 1000), std::pair(400.0, 200.0));
    }

    void typeResolution()
    {
        auto r = [](const char *t) {
            TransitionConfig cfg;
            cfg.type = QString::fromLatin1(t);
            return resolve(cfg).effect;
        };
        QCOMPARE(r("grow").type, EffectType::Grow);
        QCOMPARE(r("left"), (Effect{EffectType::Wipe, 2, 180.0, 20.0, 20.0}));
        QCOMPARE(r("right"), (Effect{EffectType::Wipe, 2, 0.0, 20.0, 20.0}));
        QCOMPARE(r("top"), (Effect{EffectType::Wipe, 2, 90.0, 20.0, 20.0}));
        QCOMPARE(r("bottom"), (Effect{EffectType::Wipe, 2, 270.0, 20.0, 20.0}));
        QCOMPARE(r("simple"), (Effect{EffectType::Simple, 2, 45.0, 20.0, 20.0}));
        QCOMPARE(r("none").type, EffectType::None);
        for (int i = 0; i < 30; ++i) {
            // `random` and `any` always resolve to something drawable.
            QVERIFY(r("random").type != EffectType::None);
            QCOMPARE(r("any").type, EffectType::Grow);
        }
    }

    void optionParsers()
    {
        QCOMPARE(parseBezier(QStringLiteral(".54,0,.34,.99")).value(),
                 (Bezier{0.54, 0.0, 0.34, 0.99}));
        QVERIFY(!parseBezier(QStringLiteral("1,2,3")).has_value());
        QCOMPARE(parseWave(QStringLiteral("20,10")).value(), std::pair(20.0, 10.0));
        QCOMPARE(parseColor(QStringLiteral("10, 20,30")), ((std::array<uint8_t, 3>){10, 20, 30}));
        QCOMPARE(parseColor(QStringLiteral("#ff8000")), ((std::array<uint8_t, 3>){255, 128, 0}));
        QCOMPARE(parseColor(QStringLiteral("nope")), ((std::array<uint8_t, 3>){0, 0, 0}));
    }

    void plasmaIdsCanDifferFromRegistryOrder()
    {
        // Registry order: DP-1 (x=0), DP-2 (x=1920). Plasma numbers them
        // the other way round (primary DP-2 is screen 0).
        Output m0{};
        m0.id = 0;
        m0.name = QStringLiteral("DP-1");
        m0.width = 1920;
        m0.height = 1080;
        m0.x = 0;
        Output m1{};
        m1.id = 1;
        m1.name = QStringLiteral("DP-2");
        m1.width = 1920;
        m1.height = 1080;
        m1.x = 1920;
        std::vector<Output> mons{m0, m1};
        const auto geoms = parseScreenGeoms(QStringLiteral("0:1920,0,1920,1080;1:0,0,1920,1080"));
        QVERIFY(assignPlasmaIds(mons, geoms));
        QCOMPARE(mons[0].id, std::uint32_t(0));
        QCOMPARE(mons[0].name, QStringLiteral("DP-2"));
        QCOMPARE(mons[1].id, std::uint32_t(1));
        QCOMPARE(mons[1].name, QStringLiteral("DP-1"));
    }

    void unmatchedOrMirroredOutputsLeaveMonitorsUntouched()
    {
        auto make = [](std::uint32_t id, const char *n, int x) {
            Output m{};
            m.id = id;
            m.name = QString::fromLatin1(n);
            m.width = 1920;
            m.height = 1080;
            m.x = x;
            return m;
        };
        std::vector<Output> mons{make(0, "A", 0), make(1, "B", 500)};
        QVERIFY(!assignPlasmaIds(mons,
                                 parseScreenGeoms(QStringLiteral("0:0,0,1,1;1:9,9,1,1"))));
        QCOMPARE(mons[0].id, std::uint32_t(0));
        QCOMPARE(mons[1].id, std::uint32_t(1));
        std::vector<Output> mirrored{make(0, "A", 0), make(1, "B", 0)};
        QVERIFY(!assignPlasmaIds(mirrored, parseScreenGeoms(QStringLiteral("0:0,0,1,1"))));
    }

    void geometryParsingIsLenient()
    {
        QVERIFY(parseScreenGeoms(QString()).empty());
        const auto geoms = parseScreenGeoms(QStringLiteral("junk;1:2.0,3.4,5,6"));
        QCOMPARE(geoms.size(), 1);
        QCOMPARE(geoms[0].id, std::uint32_t(1));
        QCOMPARE(geoms[0].x, 2);
        QCOMPARE(geoms[0].y, 3);
    }

    void percentDecoding()
    {
        QCOMPARE(percentDecode(QStringLiteral("/home/u/Im%C3%A1genes/my%20pic%20%231.jpg")),
                 QString::fromUtf8("/home/u/Im\u00e1genes/my pic #1.jpg"));
        QCOMPARE(percentDecode(QStringLiteral("100%")), QStringLiteral("100%"));
        QCOMPARE(percentDecode(QStringLiteral("a%2")), QStringLiteral("a%2"));
        QCOMPARE(percentDecode(QStringLiteral("a%zzb")), QStringLiteral("a%zzb"));
    }

    void tildeExpansion()
    {
        const QString oldHome = qEnvironmentVariable("HOME");
        qputenv("HOME", "/home/test");
        QCOMPARE(expandTilde(QStringLiteral("~/a b/c.mp4")),
                 QStringLiteral("/home/test/a b/c.mp4"));
        QCOMPARE(expandTilde(QStringLiteral("~")), QStringLiteral("/home/test"));
        QCOMPARE(expandTilde(QStringLiteral("/abs/~x")), QStringLiteral("/abs/~x"));
        QCOMPARE(expandTilde(QStringLiteral("~other/x")), QStringLiteral("~other/x"));
        if (oldHome.isEmpty()) {
            qunsetenv("HOME");
        } else {
            qputenv("HOME", oldHome.toUtf8());
        }
    }
};

QTEST_MAIN(TestTransitions)

#include "tst_transitions.moc"
