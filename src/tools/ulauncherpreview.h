#pragma once

#include <QList>
#include <QString>

#include <map>

namespace tools
{

// One row of the launcher mockup: the icon, the item name, the text whose
// matched characters get highlighted inside the name (like the fuzzy match of
// the typed query), and the shortcut label.
struct UlauncherRow
{
    UlauncherRow() = default;
    UlauncherRow(const QString &icon_, const QString &name_, const QString &highlight_,
                 const QString &shortcut_, bool selected_ = false)
        : icon(icon_), name(name_), highlight(highlight_), shortcut(shortcut_),
          selected(selected_)
    {
    }

    QString icon; // icon name or path; unknown -> placeholder icon
    QString name;
    QString highlight;
    QString shortcut;
    bool selected = false;
};

// Renders an Ulauncher launcher mockup as a self-contained HTML document for
// the WebEngine preview. The theme's own theme.css is transformed (GTK
// @define-color variables, alpha() and darker() calls resolved) and injected,
// so the mockup reproduces the real theme: translucent window background,
// borders, selected-row gradients, shortcut text-shadows, and the window
// shadow over a transparent page background.
// `themePath` points at a theme directory (manifest.json + theme.css);
// `liveColors` override theme values for the colors currently being edited
// (variable name -> CSS color); `scale` and `windowWidth` control the layout.
// When `manifestJson` / `cssContent` are not empty they are used instead of
// reading those files from disk.
// `rows` defines the items to show (icon, name, text to highlight, shortcut,
// selected); when empty a small sample list is used.
QString ulauncherPreviewHtml(const QString &themePath,
                             const std::map<QString, QString> &liveColors = {},
                             double scale = 0.75, int windowWidth = 0,
                             const QString &manifestJson = QString(),
                             const QString &cssContent = QString(),
                             const QList<UlauncherRow> &rows = {});

}
