#include "colorcategory.h"

#include <QColor>

#include <array>

#include "cpp/cam/hct.h"
#include "cpp/quantize/celebi.h"
#include "cpp/score/score.h"
#include "cpp/utils/utils.h"

namespace colorcategory {
namespace {

using material_color_utilities::Argb;
using material_color_utilities::Hct;

// Chromatic categories walking the OKLCH hue wheel clockwise, starting at
// red. kBoundaries[i] is where category i begins on the hue wheel (rose
// wraps past 0).
//
// 1:1 port of the original app's table (color_extractor.py
// get_color_category): midpoints between the pure-color hue anchors on the
// OKLCH hue wheel.
constexpr std::array<const char *, 12> kChromatic = {{
    "red", "orange", "yellow", "yellow-green", "green", "cyan-green",
    "cyan", "blue-cyan", "blue", "violet", "magenta", "rose",
}};
constexpr std::array<double, 12> kBoundaries = {{
    15.0,    // rose | red
    40.0,    // red | orange
    81.8,    // orange | yellow
    123.6,   // yellow | yellow-green
    139.0,   // yellow-green | green
    147.4,   // green | cyan-green
    174.6,   // cyan-green | cyan
    230.2,   // cyan | blue-cyan
    273.4,   // blue-cyan | blue
    293.7,   // blue | violet
    319.6,   // violet | magenta
    348.5,   // magenta | rose
}};

} // namespace

const QStringList &categoryList()
{
    static const QStringList list = {
        QStringLiteral("red"), QStringLiteral("orange"), QStringLiteral("yellow"),
        QStringLiteral("yellow-green"), QStringLiteral("green"),
        QStringLiteral("cyan-green"), QStringLiteral("cyan"),
        QStringLiteral("blue-cyan"), QStringLiteral("blue"), QStringLiteral("violet"),
        QStringLiteral("magenta"), QStringLiteral("rose"), QStringLiteral("black"),
        QStringLiteral("gray"), QStringLiteral("white"),
    };
    return list;
}

QString colorCategory(quint32 argb)
{
    const Hct hct(argb);
    const double tone = hct.get_tone();
    const double chroma = hct.get_chroma();
    // Achromatic colors first (HCT space, the original app's thresholds).
    if (tone < 20.0) {
        return QStringLiteral("black");
    }
    if (tone > 90.0 && chroma < 10.0) {
        return QStringLiteral("white");
    }
    if (chroma < 10.0) {
        return QStringLiteral("gray");
    }
    const double hue = hct.get_hue();
    for (std::size_t i = 0; i < kChromatic.size(); ++i) {
        const double lo = kBoundaries[i];
        const double hi = kBoundaries[(i + 1) % kBoundaries.size()];
        const bool in = (i == kChromatic.size() - 1)
                ? (hue >= lo || hue < hi) // rose wraps past 0
                : (hue >= lo && hue < hi);
        if (in) {
            return QString::fromLatin1(kChromatic[i]);
        }
    }
    return QStringLiteral("red"); // unreachable: the sectors cover the wheel
}

std::vector<quint32> topImageColors(const QImage &image, std::size_t desired)
{
    // Downscale to 128x128 before quantizing (the original app's size).
    const QImage scaled = image.scaled(128, 128, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
    std::vector<Argb> pixels;
    pixels.reserve(static_cast<std::size_t>(scaled.width()) * scaled.height());
    for (int y = 0; y < scaled.height(); ++y) {
        for (int x = 0; x < scaled.width(); ++x) {
            const QRgb pixel = scaled.pixel(x, y);
            if (qAlpha(pixel) > 128) {
                pixels.push_back(material_color_utilities::ArgbFromRgb(
                        qRed(pixel), qGreen(pixel), qBlue(pixel)));
            }
        }
    }
    if (pixels.empty()) {
        return {};
    }
    const auto quantized = material_color_utilities::QuantizeCelebi(pixels, 128);
    material_color_utilities::ScoreOptions options;
    options.desired = desired;
    const auto ranked =
            material_color_utilities::RankedSuggestions(quantized.color_to_count, options);
    std::vector<quint32> result;
    result.reserve(ranked.size());
    for (const Argb argb : ranked) {
        result.push_back(argb);
    }
    return result;
}

} // namespace colorcategory
