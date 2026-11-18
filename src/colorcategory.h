#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <vector>

namespace colorcategory {

// 12 chromatic + black/white/gray, in UI order.
const QStringList &categoryList();

// classifies 0xAARRGGBB via HCT (OKLCH); boundary table in colorcategory.cpp.
QString colorCategory(quint32 argb);

// Celebi quantization + material ranking, most important first.
std::vector<quint32> topImageColors(const QImage &image, std::size_t desired);

} // namespace colorcategory
