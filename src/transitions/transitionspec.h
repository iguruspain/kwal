// SPDX-FileCopyrightText: 2026 KDE
// SPDX-License-Identifier: MIT
#pragma once

// awww-style transition options (plan-implementacion-transiciones-plasma.md
// §4.1): the same option set, names and semantics as the Rust reference
// (kdewall-plugin-switcher src/transitions.rs).

#include <QString>

#include <functional>
#include <random>

namespace transitions
{

// Transition names, the same set as awww.
enum class TransitionType
{
    None,    // complete instantly
    Simple,  // fade in small steps (step); ignores the duration
    Fade,    // fade along the bezier curve
    Left,    // wipe from the left edge
    Right,   // wipe from the right edge
    Top,     // wipe from the top edge
    Bottom,  // wipe from the bottom edge
    Wipe,    // wipe at angle
    Wave,    // wipe with a wavy edge
    Grow,    // growing circle from pos
    Center,  // growing circle from the screen centre
    Any,     // growing circle from a random point
    Outer,   // shrinking circle: the new wallpaper appears outside it
    Random,  // a random one of the above
};

// One axis of the grow/outer centre: a fraction of the screen or logical pixels.
enum class Coord
{
    Frac,
    Px,
};

// Centre of the grow/outer circle. Like awww, y is measured from the bottom
// of the screen unless invertY is set.
struct Pos
{
    Coord xKind = Coord::Frac;
    double x = 0.5;
    Coord yKind = Coord::Frac;
    double y = 0.5;

    // Alias (center, top, left, right, bottom, top-left, top-right,
    // bottom-left, bottom-right) or X,Y: values with a dot or a % are
    // fractions, integers are logical pixels.
    static bool parse(const QString &text, Pos *out, QString *error);
};

// Easing curve as a CSS cubic-bezier(x1, y1, x2, y2).
struct Bezier
{
    double x1 = 0.54;
    double y1 = 0.0;
    double x2 = 0.34;
    double y2 = 0.99;

    static bool parse(const QString &text, Bezier *out, QString *error);
};

// Width/height of the waves of `wave`; height 0 is a plain wipe.
struct Wave
{
    double width = 20.0;
    double height = 20.0;

    static bool parse(const QString &text, Wave *out, QString *error);
};

// Uniform [0, 1) random numbers for resolve(); overridable per call.
inline std::function<double()> defaultRand()
{
    return []() {
        static std::mt19937_64 gen{std::random_device{}()};
        return double(gen() >> 11) * (1.0 / 9007199254740992.0);
    };
}

// Transition options; the defaults follow awww (§4.1).
struct TransitionSpec
{
    TransitionType type = TransitionType::Grow;
    double duration = 0.9; // seconds (ignored by simple); clamped to >= 0.01 on resolve
    int fps = 0;           // frame-rate cap; 0 = the display's refresh (simple: 30)
    int step = 2;          // simple only, 1..255
    double angle = 45.0;   // wipe/wave: 0 = right to left, 90 = top to bottom, 270 = bottom to top
    Pos pos;               // grow/outer centre
    bool invertY = false;  // count the y of pos from the top instead of the bottom
    Bezier bezier;
    Wave wave;

    // A fully resolved effect: center/any/random/left... already mapped to a
    // concrete shape. resolve() rolls the dice for any/random with values
    // from `rand` ([0, 1)), injectable so tests stay deterministic.
    struct Resolved
    {
        enum class Effect { None, Simple, Fade, Wipe, Wave, Grow, Outer };
        Effect effect = Effect::Grow;
        int step = 2;
        double angle = 45.0;
        Wave wave = {20.0, 0.0};
        Pos pos;
        bool invertY = false;
        double duration = 0.9;
        int fps = 0;
        Bezier bezier;
    };

    Resolved resolve(const std::function<double()> &rand = defaultRand()) const;
};

// The awww type names, lower case.
bool parseTransitionType(const QString &text, TransitionType *out, QString *error);

}
