#include "planes.h"

#include "path.h"
#include "../plasmaapply.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>

#include <algorithm>
#include <cmath>
#include <set>

namespace transitions {

namespace {

constexpr char kImagePluginId[] = "org.kde.image";

} // namespace

Fill fillFromConfig(const QString &value)
{
    const QString t = value.trimmed();
    bool ok = false;
    const int n = t.toInt(&ok);
    if (ok) {
        switch (n) {
        case 0:
            return Fill::Stretch;
        case 1:
            return Fill::Fit;
        case 6:
            return Fill::Pad;
        default:
            break; // 2 = crop; tiled modes (3-5) are approximated with crop.
        }
    }
    return Fill::Crop;
}

std::array<std::uint8_t, 3> parseColor(const QString &value)
{
    const QString t = value.trimmed();
    if (t.isEmpty()) {
        return {0, 0, 0};
    }
    if (t.startsWith(QLatin1Char('#'))) {
        const QString hex = t.mid(1);
        if (hex.size() >= 6) {
            bool ok = false;
            const std::uint32_t n = hex.right(6).toUInt(&ok, 16);
            if (ok) {
                return {static_cast<std::uint8_t>((n >> 16) & 0xFF),
                        static_cast<std::uint8_t>((n >> 8) & 0xFF),
                        static_cast<std::uint8_t>(n & 0xFF)};
            }
        }
        return {0, 0, 0};
    }
    auto num = [](const QString &s) -> std::uint8_t {
        bool ok = false;
        const unsigned int v = s.trimmed().toUInt(&ok);
        return ok && v <= 0xFF ? static_cast<std::uint8_t>(v) : 0;
    };
    const QStringList parts = t.split(QLatin1Char(','));
    auto at = [&](int i) -> std::uint8_t {
        return i < parts.size() ? num(parts.at(i)) : 0;
    };
    return {at(0), at(1), at(2)};
}

QString outgoingPlugin(const plasmaapply::Ini &ini, const QString &desktopId)
{
    const QString plugin =
        ini.get({QStringLiteral("Containments"), desktopId}, QStringLiteral("wallpaperplugin"));
    return plugin.isEmpty() ? QStringLiteral("(none)") : plugin;
}

std::vector<Plane> outgoingPlanes(const plasmaapply::Ini &ini, const QStringList &desktopIds)
{
    std::set<std::pair<quint32, QString>> seen;
    std::vector<Plane> out;
    for (const QString &id : desktopIds) {
        const QStringList base = {QStringLiteral("Containments"), id};
        const QString plugin = ini.get(base, QStringLiteral("wallpaperplugin"));
        if (plugin != QLatin1String(kImagePluginId)) {
            continue;
        }
        const QStringList sec = {QStringLiteral("Containments"), id,
                                 QStringLiteral("Wallpaper"), plugin,
                                 QStringLiteral("General")};
        QString raw = ini.get(sec, QStringLiteral("Image"));
        if (raw.isEmpty()) {
            raw = ini.get(sec, QStringLiteral("WallpaperImage"));
        }
        if (raw.isEmpty()) {
            continue;
        }
        QString path = percentDecode(raw.startsWith(QLatin1String("file://"))
                                         ? raw.mid(QStringLiteral("file://").size())
                                         : raw);
        if (!QFileInfo(path).isFile()) {
            continue;
        }
        const int screen = ini.get(base, QStringLiteral("lastScreen")).toInt();
        if (seen.insert({static_cast<quint32>(screen), path}).second) {
            Plane plane;
            plane.screen = static_cast<quint32>(screen);
            plane.path = std::move(path);
            plane.fill = fillFromConfig(ini.get(sec, QStringLiteral("FillMode")));
            plane.color = parseColor(ini.get(sec, QStringLiteral("Color")));
            out.push_back(std::move(plane));
        }
    }
    return out;
}

std::vector<Plane> incomingPlanes(const plasmaapply::Ini &ini,
                                  const QStringList &desktopIds,
                                  const QString &imagePath,
                                  const QString &pluginId)
{
    std::set<quint32> seen;
    std::vector<Plane> out;
    for (const QString &id : desktopIds) {
        const QStringList base = {QStringLiteral("Containments"), id};
        const int screen = ini.get(base, QStringLiteral("lastScreen")).toInt();
        if (!seen.insert(static_cast<quint32>(screen)).second) {
            continue;
        }
        const QStringList sec = {QStringLiteral("Containments"), id,
                                 QStringLiteral("Wallpaper"), pluginId,
                                 QStringLiteral("General")};
        Plane plane;
        plane.screen = static_cast<quint32>(screen);
        plane.path = imagePath;
        plane.fill = fillFromConfig(ini.get(sec, QStringLiteral("FillMode")));
        plane.color = parseColor(ini.get(sec, QStringLiteral("Color")));
        out.push_back(std::move(plane));
    }
    return out;
}

Buffer composeImage(const Plane &plane, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return {};
    }
    QImageReader reader(plane.path);
    QImage img = reader.read();
    if (img.isNull()) {
        return {};
    }
    if (img.format() != QImage::Format_RGB32) {
        img = img.convertToFormat(QImage::Format_RGB32);
    }
    const int iw = img.width();
    const int ih = img.height();

    // Size of the image once fitted.
    int nw = iw;
    int nh = ih;
    switch (plane.fill) {
    case Fill::Stretch:
        nw = w;
        nh = h;
        break;
    case Fill::Crop: {
        const double s = std::max(double(w) / iw, double(h) / ih);
        nw = static_cast<int>(std::ceil(iw * s));
        nh = static_cast<int>(std::ceil(ih * s));
        break;
    }
    case Fill::Fit: {
        const double s = std::min(double(w) / iw, double(h) / ih);
        nw = static_cast<int>(std::round(iw * s));
        nh = static_cast<int>(std::round(ih * s));
        break;
    }
    case Fill::Pad:
        break;
    }
    nw = std::max(nw, 1);
    nh = std::max(nh, 1);

    QImage scaled;
    const QImage *src = &img;
    if (nw != iw || nh != ih) {
        scaled = img.scaled(nw, nh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        src = &scaled;
    }

    const Pixel bg = Pixel(0xFF000000u)
        | (static_cast<std::uint32_t>(plane.color[0]) << 16)
        | (static_cast<std::uint32_t>(plane.color[1]) << 8)
        | static_cast<std::uint32_t>(plane.color[2]);
    Buffer out(static_cast<size_t>(w) * h, bg);
    const long long ox = (w - nw) / 2;
    const long long oy = (h - nh) / 2;
    // Copy whole scanlines (QImage::pixel() per pixel is far slower).
    const long long x0 = std::max<long long>(0, ox);
    const long long x1 = std::min<long long>(w, ox + nw);
    for (long long y = 0; y < h && x1 > x0; ++y) {
        const long long sy = y - oy;
        if (sy < 0 || sy >= nh) {
            continue;
        }
        const auto *line = reinterpret_cast<const QRgb *>(src->constScanLine(static_cast<int>(sy)));
        Pixel *dst = out.data() + static_cast<size_t>(y) * w;
        for (long long x = x0; x < x1; ++x) {
            dst[x] = Pixel(0xFF000000u) | (line[x - ox] & 0x00FFFFFFu);
        }
    }
    return out;
}

} // namespace transitions
