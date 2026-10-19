#pragma once

#include <QString>

#include <map>

namespace tools
{

// Renders an Ulauncher launcher mockup as a self-contained HTML document for
// the WebEngine preview (port of the original app's ulauncher_preview.py).
// `themePath` points at a theme directory (manifest.json + theme.css);
// `liveColors` override theme.css values for the colors currently being
// edited (variable name -> CSS color); `scale` and `windowWidth` control the
// layout. When `manifestJson` / `cssContent` are not empty they are used
// instead of reading those files from disk.
QString ulauncherPreviewHtml(const QString &themePath,
                             const std::map<QString, QString> &liveColors = {},
                             double scale = 0.75, int windowWidth = 0,
                             const QString &manifestJson = QString(),
                             const QString &cssContent = QString());

}
