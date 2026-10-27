// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Transition option types, parsers and resolution (the awww option set).
// Port of the config half of the Rust reference (transitions.rs).
#pragma once

#include <QString>

#include <cstdint>
#include <optional>

#include "effects.h"

namespace transitions {

// Transition names accepted as the `type` setting (same set as awww).
// `None` is internal only and is not offered in the UI.
enum class TransitionType : int {
    // Complete instantly.
    None,
    // Fade in small steps (`step`; ignores the duration).
    Simple,
    // Fade along the bezier curve.
    Fade,
    // Wipe from the left edge.
    Left,
    // Wipe from the right edge.
    Right,
    // Wipe from the top edge.
    Top,
    // Wipe from the bottom edge.
    Bottom,
    // Wipe at `angle`.
    Wipe,
    // Wipe with a wavy edge (`waveWidth`, `waveHeight`).
    Wave,
    // Growing circle from `pos`.
    Grow,
    // Growing circle from the screen centre.
    Center,
    // Growing circle from a random point.
    Any,
    // Shrinking circle: the new wallpaper appears outside it.
    Outer,
    // A random one of the above.
    Random,
};

std::optional<TransitionType> parseTransitionType(const QString &name);

// One axis of a position, awww-style: fractions are of the screen, pixels
// are screen pixels.
struct Coord {
    bool isFraction = false;
    double value = 0.0;

    static Coord frac(double f);
    static Coord px(double p);
    bool isValid() const;
    bool operator==(const Coord &) const = default;
};

// Centre of the grow / outer circle. Like awww, **y is measured from the
// bottom** of the screen unless `invertY` is given.
struct Pos {
    Coord x{};
    Coord y{};

    Pos()
        : x(Coord::frac(0.5))
        , y(Coord::frac(0.5))
    {
    }

    bool isValid() const;
    bool operator==(const Pos &) const = default;

    // Position in **buffer pixels**, y from the top.
    std::pair<double, double> toPx(bool invertY, int logicalW, int logicalH,
                                   int bufferW, int bufferH) const;
};

// `center | top | left | right | bottom | top-left | top-right |
// bottom-left | bottom-right`, or `X,Y` (`0.5,0.5` = fractions,
// `200,400` = pixels; `50%` also accepted).
std::optional<Pos> parsePos(const QString &s, QString *error = nullptr);

// Parses `.54,0,.34,.99` (four comma-separated numbers).
std::optional<Bezier> parseBezier(const QString &s, QString *error = nullptr);

// Parses `20,20` (width, height).
std::optional<std::pair<double, double>> parseWave(const QString &s, QString *error = nullptr);

// Transition options; the names and defaults follow `awww img`. The
// `advancedEnabled` flag selects between the per-type defaults (the
// complete preset for each type) and the user's advanced values.
struct TransitionConfig {
    bool enabled = false;
    // Layer-shell layer: background, bottom, top or overlay.
    QString layer = QStringLiteral("bottom");
    // One of the TransitionType names (lowercase), without `none`.
    QString type = QStringLiteral("grow");
    bool advancedEnabled = false;
    // Duration in seconds (ignored by `simple`); minimum 0.01 on resolve.
    double duration = 0.9;
    // Frame-rate cap; 0 = the display's refresh rate (simple: 30).
    int fps = 0;
    // How fast `simple` approaches the new image, per frame (1-255).
    int step = 2;
    // Angle of `wipe` and `wave` in degrees: 0 = right to left,
    // 90 = top to bottom, 270 = bottom to top.
    double angle = 45.0;
    // Centre of `grow` / `outer`.
    QString pos = QStringLiteral("center");
    // Count the y of `pos` from the top instead of the bottom.
    bool invertY = false;
    // Easing curve as a cubic bezier.
    QString bezier = QStringLiteral(".54,0,.34,.99");
    // Width, height of the waves of `wave`.
    double waveWidth = 20.0;
    double waveHeight = 20.0;
};

// One of the concrete effect families (center/any/random/left... already
// mapped).
enum class EffectType : int {
    None,
    Simple,
    Fade,
    Wipe,
    Wave,
    Grow,
    Outer,
};

struct Effect {
    EffectType type = EffectType::None;
    // Simple: fade steps per frame.
    int step = 2;
    // Wipe / Wave: travel direction in degrees.
    double angle = 45.0;
    // Wave: edge wave width and height.
    double waveWidth = 20.0;
    double waveHeight = 20.0;
    // Grow / Outer: centre.
    Pos pos{};
    bool invertY = false;

    bool operator==(const Effect &) const = default;
};

// Everything the overlay animation needs.
struct Spec {
    Effect effect{};
    double durationSec = 0.9;
    // Frame-rate cap; 0 = uncapped (simple is always capped at 30).
    std::uint32_t fps = 0;
    Bezier bezier{};
};

// Maps the config to a concrete Spec (rolls the dice for `any` / `random`).
// With `advancedEnabled` false the per-type defaults are used, so picking a
// type without enabling advanced is the same as not configuring it. Invalid
// values (hand-edited config) fall back to the defaults.
Spec resolve(const TransitionConfig &cfg);

} // namespace transitions
