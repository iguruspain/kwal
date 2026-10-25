#include "transitionspec.h"

#include <QList>
#include <QStringList>

#include <array>
#include <cmath>
#include "effects.h"

namespace transitions {

Coord Coord::frac(double f)
{
    return Coord{true, f};
}

Coord Coord::px(double p)
{
    return Coord{false, p};
}

bool Coord::isValid() const
{
    return std::isfinite(value);
}

bool Pos::isValid() const
{
    return x.isValid() && y.isValid();
}

std::pair<double, double> Pos::toPx(bool invertY, int logicalW, int logicalH,
                                    int bufferW, int bufferH) const
{
    const double lw = std::max(logicalW, 1);
    const double lh = std::max(logicalH, 1);
    const double bw = static_cast<double>(bufferW);
    const double bh = static_cast<double>(bufferH);
    double xv = x.isFraction ? x.value * bw : x.value * bw / lw;
    double yv;
    if (y.isFraction) {
        yv = (invertY ? y.value : 1.0 - y.value) * bh;
    } else {
        yv = (invertY ? y.value : lh - y.value) * bh / lh;
    }
    return {xv, yv};
}

std::optional<TransitionType> parseTransitionType(const QString &name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("none")) {
        return TransitionType::None;
    }
    if (n == QLatin1String("simple")) {
        return TransitionType::Simple;
    }
    if (n == QLatin1String("fade")) {
        return TransitionType::Fade;
    }
    if (n == QLatin1String("left")) {
        return TransitionType::Left;
    }
    if (n == QLatin1String("right")) {
        return TransitionType::Right;
    }
    if (n == QLatin1String("top")) {
        return TransitionType::Top;
    }
    if (n == QLatin1String("bottom")) {
        return TransitionType::Bottom;
    }
    if (n == QLatin1String("wipe")) {
        return TransitionType::Wipe;
    }
    if (n == QLatin1String("wave")) {
        return TransitionType::Wave;
    }
    if (n == QLatin1String("grow")) {
        return TransitionType::Grow;
    }
    if (n == QLatin1String("center") || n == QLatin1String("centre")) {
        return TransitionType::Center;
    }
    if (n == QLatin1String("any")) {
        return TransitionType::Any;
    }
    if (n == QLatin1String("outer")) {
        return TransitionType::Outer;
    }
    if (n == QLatin1String("random")) {
        return TransitionType::Random;
    }
    return std::nullopt;
}

std::optional<Pos> parsePos(const QString &s, QString *error)
{
    const auto setErr = [&](const QString &m) {
        if (error) {
            *error = m;
        }
    };
    const auto f = [](double x, double y) {
        Pos p;
        p.x = Coord::frac(x);
        p.y = Coord::frac(y);
        return p;
    };
    const QString lower = s.trimmed().toLower();
    if (lower == QLatin1String("center") || lower == QLatin1String("centre")) {
        return f(0.5, 0.5);
    }
    if (lower == QLatin1String("top")) {
        return f(0.5, 1.0);
    }
    if (lower == QLatin1String("bottom")) {
        return f(0.5, 0.0);
    }
    if (lower == QLatin1String("left")) {
        return f(0.0, 0.5);
    }
    if (lower == QLatin1String("right")) {
        return f(1.0, 0.5);
    }
    if (lower == QLatin1String("top-left")) {
        return f(0.0, 1.0);
    }
    if (lower == QLatin1String("top-right")) {
        return f(1.0, 1.0);
    }
    if (lower == QLatin1String("bottom-left")) {
        return f(0.0, 0.0);
    }
    if (lower == QLatin1String("bottom-right")) {
        return f(1.0, 0.0);
    }

    const auto toNum = [](const QString &v) -> std::optional<double> {
        if (v.isEmpty()) {
            return std::nullopt;
        }
        bool ok = false;
        const double n = v.toDouble(&ok);
        return ok ? std::make_optional(n) : std::nullopt;
    };
    const auto coord = [&](const QString &raw) -> std::optional<Coord> {
        const QString v = raw.trimmed();
        if (v.endsWith(QLatin1Char('%'))) {
            auto n = toNum(v.left(v.size() - 1).trimmed());
            return n ? std::make_optional(Coord::frac(*n / 100.0)) : std::nullopt;
        }
        if (v.contains(QLatin1Char('.'))) {
            auto n = toNum(v);
            return n ? std::make_optional(Coord::frac(*n)) : std::nullopt;
        }
        auto n = toNum(v);
        return n ? std::make_optional(Coord::px(*n)) : std::nullopt;
    };
    const int comma = s.indexOf(QLatin1Char(','));
    if (comma < 0) {
        setErr(QStringLiteral("expected an alias (center, top-left, ...) or `X,Y`"));
        return std::nullopt;
    }
    auto x = coord(s.left(comma));
    if (!x) {
        setErr(QStringLiteral("invalid position value `%1`").arg(s.left(comma).trimmed()));
        return std::nullopt;
    }
    auto y = coord(s.mid(comma + 1));
    if (!y) {
        setErr(QStringLiteral("invalid position value `%1`").arg(s.mid(comma + 1).trimmed()));
        return std::nullopt;
    }
    Pos p;
    p.x = *x;
    p.y = *y;
    return p;
}

namespace {

template <std::size_t N>
std::optional<std::array<double, N>> parseFloats(const QString &s, const QString &what,
                                                 QString *error)
{
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() != static_cast<int>(N)) {
        if (error) {
            *error = QStringLiteral("expected %1 comma-separated numbers for %2")
                         .arg(N)
                         .arg(what);
        }
        return std::nullopt;
    }
    std::array<double, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        const QString v = parts.at(i).trimmed();
        bool ok = false;
        const double n = v.toDouble(&ok);
        if (v.isEmpty() || !ok) {
            if (error) {
                *error = QStringLiteral("invalid number `%1` in %2").arg(v, what);
            }
            return std::nullopt;
        }
        out[i] = n;
    }
    return out;
}

} // namespace

std::optional<Bezier> parseBezier(const QString &s, QString *error)
{
    auto v = parseFloats<4>(s, QStringLiteral("bezier"), error);
    if (!v) {
        return std::nullopt;
    }
    return Bezier{(*v)[0], (*v)[1], (*v)[2], (*v)[3]};
}

std::optional<std::pair<double, double>> parseWave(const QString &s, QString *error)
{
    auto v = parseFloats<2>(s, QStringLiteral("wave"), error);
    if (!v) {
        return std::nullopt;
    }
    return std::make_pair((*v)[0], (*v)[1]);
}

Spec resolve(const TransitionConfig &cfg)
{
    TransitionConfig c = cfg;
    if (!c.advancedEnabled) {
        // The per-type defaults are the preset: the stored advanced values
        // only apply once the switch is on.
        c.duration = 0.9;
        c.fps = 0;
        c.step = 2;
        c.angle = 45.0;
        c.pos = QStringLiteral("center");
        c.invertY = false;
        c.bezier = QStringLiteral(".54,0,.34,.99");
        c.waveWidth = 20.0;
        c.waveHeight = 20.0;
    }

    TransitionType t = parseTransitionType(c.type).value_or(TransitionType::Grow);
    if (t == TransitionType::Random) {
        static constexpr TransitionType pool[] = {
            TransitionType::Fade, TransitionType::Left, TransitionType::Right,
            TransitionType::Top, TransitionType::Bottom, TransitionType::Wipe,
            TransitionType::Wave, TransitionType::Grow, TransitionType::Outer,
            TransitionType::Any,
        };
        t = pool[static_cast<std::size_t>(randUnit() * std::size(pool)) % std::size(pool)];
        c.angle = randUnit() * 360.0;
    }

    const Pos pos = parsePos(c.pos).value_or(Pos{});
    const Bezier bezier = parseBezier(c.bezier).value_or(Bezier{});

    Effect effect;
    switch (t) {
    case TransitionType::None:
        break;
    case TransitionType::Simple:
        effect.type = EffectType::Simple;
        effect.step = std::max(1, c.step);
        break;
    case TransitionType::Fade:
        effect.type = EffectType::Fade;
        break;
    case TransitionType::Left:
        effect.type = EffectType::Wipe;
        effect.angle = 180.0;
        break;
    case TransitionType::Right:
        effect.type = EffectType::Wipe;
        effect.angle = 0.0;
        break;
    case TransitionType::Top:
        effect.type = EffectType::Wipe;
        effect.angle = 90.0;
        break;
    case TransitionType::Bottom:
        effect.type = EffectType::Wipe;
        effect.angle = 270.0;
        break;
    case TransitionType::Wipe:
        effect.type = EffectType::Wipe;
        effect.angle = c.angle;
        break;
    case TransitionType::Wave:
        effect.type = EffectType::Wave;
        effect.angle = c.angle;
        effect.waveWidth = c.waveWidth;
        effect.waveHeight = c.waveHeight;
        break;
    case TransitionType::Grow:
        effect.type = EffectType::Grow;
        effect.pos = pos;
        effect.invertY = c.invertY;
        break;
    case TransitionType::Center:
        effect.type = EffectType::Grow;
        effect.pos = Pos{};
        effect.invertY = false;
        break;
    case TransitionType::Any:
        effect.type = EffectType::Grow;
        {
            Pos p;
            p.x = Coord::frac(randUnit());
            p.y = Coord::frac(randUnit());
            effect.pos = p;
        }
        effect.invertY = false;
        break;
    case TransitionType::Outer:
        effect.type = EffectType::Outer;
        effect.pos = pos;
        effect.invertY = c.invertY;
        break;
    }

    return Spec{
        effect,
        std::max(0.01, c.duration),
        c.fps > 0 ? static_cast<std::uint32_t>(c.fps) : 0u,
        bezier,
    };
}

} // namespace transitions
