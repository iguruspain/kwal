#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <vector>

namespace colorcategory {

// The 15 matugen color categories (12 chromatic + black/white/gray), in the
// order shown by the UI.
const QStringList &categoryList();

// Classify an opaque sRGB color (0xAARRGGBB) into one of the 15 categories.
// Uses HCT (OKLCH) as computed by material-color-utilities; see the
// boundary table in colorcategory.cpp.
QString colorCategory(quint32 argb);

// The top `desired` colors of the image (Celebi quantization + material
// ranking), most important first. Shared by the category pipeline (8) and
// the Material You palette (3).
std::vector<quint32> topImageColors(const QImage &image, std::size_t desired);

} // namespace colorcategory
