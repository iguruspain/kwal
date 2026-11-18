// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// Wallpaper planes read from the appletsrc snapshot and their image compositor.
#pragma once

#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <vector>

#include "effects.h"

namespace plasmaapply {
class Ini;
}

namespace transitions {

// How the outgoing image was fitted; the `FillMode` value of the
// `org.kde.image` config (Qt Image.fillMode numbers): 0 stretch, 1 fit (bars use
// the background colour), 2 crop (Plasma default; also absent/invalid values and
// the tiled modes 3-5), 6 unscaled centred.
enum class Fill : int {
    Stretch,
    Fit,
    Crop,
    Pad,
};

Fill fillFromConfig(const QString &value);

struct Plane {
    // Plasma screen number (`lastScreen`).
    quint32 screen = 0;
    QString path;
    Fill fill = Fill::Crop;
    // Background colour for letterboxing (config key `Color`, default black).
    std::array<std::uint8_t, 3> color{0, 0, 0};
};

// Parses `r,g,b` or `#rrggbb`; absent or invalid values are black.
std::array<std::uint8_t, 3> parseColor(const QString &value);

// The desktop's outgoing plugin id; "(none)" when no `wallpaperplugin` key.
QString outgoingPlugin(const plasmaapply::Ini &ini, const QString &desktopId);

// Target desktops with an image wallpaper, deduplicated by (screen, path).
std::vector<Plane> outgoingPlanes(const plasmaapply::Ini &ini, const QStringList &desktopIds);

// Cover direction: `imagePath` on every target screen, fitted like the target
// pluginId will fit it (existing `FillMode`/`Color`, else crop); dedup by screen.
std::vector<Plane> incomingPlanes(const plasmaapply::Ini &ini,
                                  const QStringList &desktopIds,
                                  const QString &imagePath,
                                  const QString &pluginId);

// Decodes the plane's image and renders it to exactly w*h (opaque premultiplied
// ARGB8888) with the plugin's fill mode; empty if undecodable. Qt::SmoothTransformation
// not Lanczos3 (absent in Qt 6.12; indistinguishable for full-screen fills).
Buffer composeImage(const Plane &plane, int w, int h);

} // namespace transitions
