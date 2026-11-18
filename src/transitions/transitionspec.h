// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <cstdint>
#include <optional>

#include "effects.h"

namespace transitions {

// Transition names accepted as the `type` setting (same set as awww).
// Left/Right/Top/Bottom: wipe from that edge; Wipe: at `angle`; Wave: wavy edge;
// Grow/Center/Any: growing disc from `pos`/screen centre/random point; Outer:
// shrinking disc, content appears outside it; Simple: stepped fade (`step`,
// duration ignored); Fade: bezier fade; None: instant, internal only (not in UI);
// Random: random non-None type.
enum class TransitionType : int {
    None,
    Simple,
    Fade,
    Left,
    Right,
    Top,
    Bottom,
    Wipe,
    Wave,
    Grow,
    Center,
    Any,
    Outer,
    Random,
};

std::optional<TransitionType> parseTransitionType(const QString &name);

// One position axis, awww-style: fractions are of the screen, pixels screen pixels.
struct Coord {
    bool isFraction = false;
    double value = 0.0;

    static Coord frac(double f);
    static Coord px(double p);
    bool isValid() const;
    bool operator==(const Coord &) const = default;
};

// Centre of grow / outer; like awww, **y is measured from the bottom** unless invertY.
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

struct TransitionConfig {
    bool enabled = false;
    // Layer-shell layer: background, bottom, top or overlay.
    QString layer = QStringLiteral("bottom");
    // A TransitionType name (lowercase), without `none`.
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
    QString pos = QStringLiteral("center");
    // Count the y of `pos` from the top instead of the bottom.
    bool invertY = false;
    QString bezier = QStringLiteral(".54,0,.34,.99");
    double waveWidth = 20.0;
    double waveHeight = 20.0;
};

// Concrete effect families (center/any/random/left... already mapped).
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
    int step = 2;
    double angle = 45.0;
    double waveWidth = 20.0;
    double waveHeight = 20.0;
    Pos pos{};
    bool invertY = false;

    bool operator==(const Effect &) const = default;
};

struct Spec {
    Effect effect{};
    double durationSec = 0.9;
    // Frame-rate cap; 0 = uncapped (simple is always capped at 30).
    std::uint32_t fps = 0;
    Bezier bezier{};
};

// Maps the config to a concrete Spec (rolls the dice for `any`/`random`).
// advancedEnabled false → per-type presets; invalid values → defaults.
Spec resolve(const TransitionConfig &cfg);

} // namespace transitions
