// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

// The outgoing/incoming wallpaper planes read from the appletsrc snapshot,
// and the plane compositor (port of the plane half of the Rust reference).
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

// How the outgoing image was fitted to the screen (`FillMode` in the
// `org.kde.image` config; Qt `Image.fillMode` values).
enum class Fill : int {
    // 0: scaled, ignoring the aspect ratio.
    Stretch,
    // 1: scaled to fit, keeping proportions (bars use the background colour).
    Fit,
    // 2 (Plasma default): scaled to cover, centre-cropped.
    Crop,
    // 6: unscaled, centred.
    Pad,
};

// Parses the `FillMode` value; 2 and absent values are crop, the tiled
// modes (3-5) are approximated with crop.
Fill fillFromConfig(const QString &value);

// One wallpaper plane to show on one screen.
struct Plane {
    // Plasma screen number (`lastScreen`).
    quint32 screen = 0;
    QString path;
    Fill fill = Fill::Crop;
    // Background colour used for letterboxing (`Color`, default black).
    std::array<std::uint8_t, 3> color{0, 0, 0};
};

// Parses `r,g,b` or `#rrggbb`; absent or invalid values are black.
std::array<std::uint8_t, 3> parseColor(const QString &value);

// The plugin id of the desktop's outgoing wallpaper ("(none)" when the
// section has no `wallpaperplugin` key), for the caller's log messages.
QString outgoingPlugin(const plasmaapply::Ini &ini, const QString &desktopId);

// Target desktops whose outgoing plugin is an image wallpaper, deduplicated
// by (screen, path).
std::vector<Plane> outgoingPlanes(const plasmaapply::Ini &ini, const QStringList &desktopIds);

// Planes for the `Cover` direction: `imagePath` (the new image, or the
// first frame of the new video) on every target screen, fitted like the
// target `pluginId` will fit it (its existing `FillMode` / `Color` if the
// desktop has a section for it, else Plasma's default crop). Deduplicated
// by screen.
std::vector<Plane> incomingPlanes(const plasmaapply::Ini &ini,
                                  const QStringList &desktopIds,
                                  const QString &imagePath,
                                  const QString &pluginId);

// Decodes the plane's image and renders it to exactly w*h (opaque
// premultiplied ARGB8888) using the same fill mode as the wallpaper plugin.
// Empty when the image cannot be decoded.
//
// Documented deviation from the Rust reference: the Qt 6.12 resize filter
// is `Qt::SmoothTransformation` (the installed Qt has no Lanczos3
// `Qt::TransformationMode`; the two are indistinguishable for full-screen
// wallpaper fills).
Buffer composeImage(const Plane &plane, int w, int h);

} // namespace transitions
