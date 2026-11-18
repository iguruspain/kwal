// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Wayland output enumeration. `id` starts as the `wl_output` registry order, only a
// guess at Plasma's screen numbering — remap with assignPlasmaIds().
#pragma once

#include <QString>

#include <cstdint>
#include <vector>

struct wl_display;

namespace transitions {

struct Output {
    // Plasma screen number after assignPlasmaIds; registry position until then.
    quint32 id = 0;
    QString name;
    // Make + model.
    QString description;
    // Current mode in pixels.
    int width = 0;
    int height = 0;
    // Logical position (xdg-output; `wl_output.geometry` is unreliable for that).
    int x = 0;
    int y = 0;
    int scale = 1;
};

// Plasma screen geometry from plasmashell's `screenGeometry(id)` (logical coords).
struct ScreenGeom {
    quint32 id = 0;
    int x = 0;
    int y = 0;
};

// Parses `id:x,y,w,h;id:x,y,w,h;…` (the query script's output).
std::vector<ScreenGeom> parseScreenGeoms(const QString &s);

// Relabels `outputs` with Plasma ids by matching logical positions, sorted by id.
// Returns false, leaving `outputs` untouched, if any output cannot be matched
// unambiguously (e.g. mirrored outputs sharing a position).
bool assignPlasmaIds(std::vector<Output> &outputs, const std::vector<ScreenGeom> &geoms);

// Outputs in registry order; blocks until every wl_output sent its initial events.
std::vector<Output> listOutputs(struct wl_display *display);

} // namespace transitions
