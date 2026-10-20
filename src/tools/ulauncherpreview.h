#pragma once

#include <QList>
#include <QString>

#include <map>

namespace tools
{

// One row of the launcher mockup: the icon, the item name, the shortcut
// label, and whether the row is the selected one. The typed query (shared by
// all rows, like Ulauncher's single input) is passed separately to the
// renderer.
struct UlauncherRow
{
    UlauncherRow() = default;
    UlauncherRow(const QString &icon_, const QString &name_, const QString &shortcut_,
                 bool selected_ = false)
        : icon(icon_), name(name_), shortcut(shortcut_), selected(selected_)
    {
    }

    QString icon; // icon name or path; unknown -> placeholder icon
    QString name;
    QString shortcut;
    bool selected = false;
};

inline bool operator==(const UlauncherRow &a, const UlauncherRow &b)
{
    return a.icon == b.icon && a.name == b.name
        && a.shortcut == b.shortcut && a.selected == b.selected;
}

// The sample rows and typed query used when no custom list is provided.
QList<UlauncherRow> sampleRows();
QString sampleQuery();

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
// `rows` and `query` define the items to show and the text typed in the
// input (its characters are the ones highlighted inside each name); when
// either is empty the built-in sample list / sample query is used.
QString ulauncherPreviewHtml(const QString &themePath,
                             const std::map<QString, QString> &liveColors = {},
                             double scale = 0.75, int windowWidth = 0,
                             const QString &manifestJson = QString(),
                             const QString &cssContent = QString(),
                             const QList<UlauncherRow> &rows = {},
                             const QString &query = QString());

}
