// SPDX-FileCopyrightText: 2026 KDE
// SPDX-License-Identifier: MIT
#include "transitionspec.h"

#include <QList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace transitions
{
namespace
{

// One coordinate value (Rust Pos::from_str): `N%` is a fraction, a value
// with a dot is a fraction, an integer is logical pixels.
bool parseCoord(const QString &raw, Coord *kind, double *value, QString *error)
{
    const QString v = raw.trimmed();
    bool ok = false;
    if (v.endsWith(QLatin1Char('%'))) {
        const double n = v.chopped(1).trimmed().toDouble(&ok);
        if (!ok) {
            if (error)
                *error = QStringLiteral("invalid position value `%1`").arg(v);
            return false;
        }
        *kind = Coord::Frac;
        *value = n / 100.0;
        return true;
    }
    if (v.contains(QLatin1Char('.'))) {
        const double n = v.toDouble(&ok);
        if (!ok) {
            if (error)
                *error = QStringLiteral("invalid position value `%1`").arg(v);
            return false;
        }
        *kind = Coord::Frac;
        *value = n;
        return true;
    }
    const qlonglong n = v.toLongLong(&ok);
    if (!ok) {
        if (error)
            *error = QStringLiteral("invalid position value `%1`").arg(v);
        return false;
    }
    *kind = Coord::Px;
    *value = double(n);
    return true;
}

// `count` comma-separated doubles (Rust parse_floats).
bool parseNumberList(const QString &text, int count, const QString &what, double *out, QString *error)
{
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != count) {
        if (error)
            *error = QStringLiteral("expected %1 comma-separated numbers for %2").arg(count).arg(what);
        return false;
    }
    for (int i = 0; i < count; ++i) {
        bool ok = false;
        out[i] = parts.at(i).trimmed().toDouble(&ok);
        if (!ok) {
            if (error)
                *error = QStringLiteral("invalid number `%1` in %2").arg(parts.at(i).trimmed()).arg(what);
            return false;
        }
    }
    return true;
}

} // namespace

bool Pos::parse(const QString &text, Pos *out, QString *error)
{
    struct Alias {
        const char *name;
        double x;
        double y;
    };
    static const Alias aliases[] = {
        {"center", 0.5, 0.5},
        {"centre", 0.5, 0.5},
        {"top", 0.5, 1.0},
        {"bottom", 0.5, 0.0},
        {"left", 0.0, 0.5},
        {"right", 1.0, 0.5},
        {"top-left", 0.0, 1.0},
        {"top-right", 1.0, 1.0},
        {"bottom-left", 0.0, 0.0},
        {"bottom-right", 1.0, 0.0},
    };
    const QString s = text.trimmed().toLower();
    for (const auto &a : aliases) {
        if (s == QLatin1String(a.name)) {
            *out = Pos{Coord::Frac, a.x, Coord::Frac, a.y};
            return true;
        }
    }
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() != 2) {
        if (error)
            *error = QStringLiteral("expected an alias (center, top-left, ...) or `X,Y`");
        return false;
    }
    Pos p;
    if (!parseCoord(parts.at(0), &p.xKind, &p.x, error)
        || !parseCoord(parts.at(1), &p.yKind, &p.y, error))
        return false;
    *out = p;
    return true;
}

bool Bezier::parse(const QString &text, Bezier *out, QString *error)
{
    double v[4];
    if (!parseNumberList(text, 4, QStringLiteral("bezier"), v, error))
        return false;
    *out = Bezier{v[0], v[1], v[2], v[3]};
    return true;
}

bool Wave::parse(const QString &text, Wave *out, QString *error)
{
    double v[2];
    if (!parseNumberList(text, 2, QStringLiteral("wave"), v, error))
        return false;
    *out = Wave{v[0], v[1]};
    return true;
}

bool parseTransitionType(const QString &text, TransitionType *out, QString *error)
{
    static const std::pair<const char *, TransitionType> names[] = {
        {"none", TransitionType::None},
        {"simple", TransitionType::Simple},
        {"fade", TransitionType::Fade},
        {"left", TransitionType::Left},
        {"right", TransitionType::Right},
        {"top", TransitionType::Top},
        {"bottom", TransitionType::Bottom},
        {"wipe", TransitionType::Wipe},
        {"wave", TransitionType::Wave},
        {"grow", TransitionType::Grow},
        {"center", TransitionType::Center},
        {"any", TransitionType::Any},
        {"outer", TransitionType::Outer},
        {"random", TransitionType::Random},
    };
    const QString s = text.trimmed().toLower();
    for (const auto &n : names) {
        if (s == QLatin1String(n.first)) {
            *out = n.second;
            return true;
        }
    }
    if (error)
        *error = QStringLiteral("unknown transition type `%1`").arg(text.trimmed());
    return false;
}

TransitionSpec::Resolved TransitionSpec::resolve(const std::function<double()> &rand) const
{
    Resolved r;
    r.duration = std::max(0.01, duration);
    r.fps = fps > 0 ? fps : 0;
    r.step = std::clamp(step, 1, 255);
    r.angle = angle;
    r.wave = wave;
    r.pos = pos;
    r.invertY = invertY;
    r.bezier = bezier;

    TransitionType t = type;
    if (t == TransitionType::Random) {
        static const TransitionType pool[] = {
            TransitionType::Fade,
            TransitionType::Left,
            TransitionType::Right,
            TransitionType::Top,
            TransitionType::Bottom,
            TransitionType::Wipe,
            TransitionType::Wave,
            TransitionType::Grow,
            TransitionType::Outer,
            TransitionType::Any,
        };
        static constexpr std::size_t poolCount = sizeof(pool) / sizeof(pool[0]);
        t = pool[static_cast<std::size_t>(rand() * poolCount) % poolCount];
        r.angle = rand() * 360.0;
    }
    switch (t) {
    case TransitionType::None:
        r.effect = Resolved::Effect::None;
        break;
    case TransitionType::Simple:
        r.effect = Resolved::Effect::Simple;
        break;
    case TransitionType::Fade:
        r.effect = Resolved::Effect::Fade;
        break;
    case TransitionType::Left:
        r.effect = Resolved::Effect::Wipe;
        r.angle = 180.0;
        break;
    case TransitionType::Right:
        r.effect = Resolved::Effect::Wipe;
        r.angle = 0.0;
        break;
    case TransitionType::Top:
        r.effect = Resolved::Effect::Wipe;
        r.angle = 90.0;
        break;
    case TransitionType::Bottom:
        r.effect = Resolved::Effect::Wipe;
        r.angle = 270.0;
        break;
    case TransitionType::Wipe:
        r.effect = Resolved::Effect::Wipe;
        r.wave = Wave{20.0, 0.0};
        break;
    case TransitionType::Wave:
        r.effect = Resolved::Effect::Wave;
        r.wave = Wave{wave.width, std::abs(wave.height)};
        break;
    case TransitionType::Grow:
        r.effect = Resolved::Effect::Grow;
        break;
    case TransitionType::Center:
        r.effect = Resolved::Effect::Grow;
        r.pos = Pos{};
        r.invertY = false;
        break;
    case TransitionType::Any:
        r.effect = Resolved::Effect::Grow;
        r.pos = Pos{Coord::Frac, rand(), Coord::Frac, rand()};
        r.invertY = false;
        break;
    case TransitionType::Outer:
        r.effect = Resolved::Effect::Outer;
        break;
    case TransitionType::Random:
        break; // resolved above
    }
    return r;
}

}
