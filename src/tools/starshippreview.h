#pragma once

#include <QString>

#include <map>
#include <toml++/toml.hpp>

namespace tools
{

// Renders the starship prompt as a self-contained HTML document for the
// WebEngine preview (port of the original app's starship_preview.py).
// `doc` is the parsed starship.toml; `paletteIndex` selects the palette to
// render (-1 keeps the file's active palette); `scale` and `width` control
// the preview layout; `overrides` replace runtime context values such as
// $username or $directory.
QString starshipPreviewHtml(const toml::table &doc, int paletteIndex,
                            double scale = 1.0, int width = 800,
                            const std::map<QString, QString> &overrides = {});

}
