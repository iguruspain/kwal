#pragma once

#include <QString>

#include <map>
#include <toml++/toml.hpp>

namespace tools
{

// Renders the starship prompt as self-contained HTML for the WebEngine preview.
// paletteIndex -1 keeps the file's active palette; overrides replace runtime context values ($username, $directory).
QString starshipPreviewHtml(const toml::table &doc, int paletteIndex,
                            double scale = 1.0, int width = 800,
                            const std::map<QString, QString> &overrides = {});

}
