// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Wayland output enumeration and Plasma screen numbering
// (port of monitors.rs of the Rust reference).
//
// `Output::id` starts as the `wl_output` registry order, which is only a
// guess of Plasma's screen numbering (the `lastScreen` setting and the
// `screen` argument of D-Bus `setWallpaper`). Plasma assigns its own ids, so
// callers should remap with assignPlasmaIds() using the geometry that
// plasmashell reports for each of its screens.
#pragma once

#include <QString>

#include <cstdint>
#include <vector>

struct wl_display;

namespace transitions {

// One output in the compositor's global order.
struct Output {
    // Plasma's screen number after assignPlasmaIds; until then, the position
    // in the compositor's global order.
    quint32 id = 0;
    // Connector name (e.g. `DP-1`).
    QString name;
    // Make + model.
    QString description;
    // Current mode in pixels.
    int width = 0;
    int height = 0;
    // Logical position (xdg-output when offered, else `wl_output.geometry`,
    // which is not reliable for that).
    int x = 0;
    int y = 0;
    int scale = 1;
};

// Geometry of one Plasma screen as reported by plasmashell's
// `screenGeometry(id)` (logical coordinates).
struct ScreenGeom {
    quint32 id = 0;
    int x = 0;
    int y = 0;
};

// Parses `id:x,y,w,h;id:x,y,w,h;…` (the output of the query script).
std::vector<ScreenGeom> parseScreenGeoms(const QString &s);

// Re-labels `outputs` with Plasma's screen ids by matching each output's
// logical position with the geometry plasmashell reports, and sorts them by
// id. Returns false and leaves `outputs` untouched if any output cannot be
// matched unambiguously (e.g. mirrored outputs sharing a position).
bool assignPlasmaIds(std::vector<Output> &outputs, const std::vector<ScreenGeom> &geoms);

// Lists the connection's outputs in registry order (the order the
// compositor announces them). Blocks on the display until every output has
// completed its initial events.
std::vector<Output> listOutputs(struct wl_display *display);

} // namespace transitions
