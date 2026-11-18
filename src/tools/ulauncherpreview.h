#pragma once

#include <QList>
#include <QString>

#include <map>

namespace tools
{

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

QList<UlauncherRow> sampleRows();
QString sampleQuery();

// Injects the theme's theme.css after resolving GTK @define-color/alpha()/darker(); liveColors override by variable name.
// Non-empty manifestJson/cssContent replace disk reads; empty rows/query use the built-in samples.
QString ulauncherPreviewHtml(const QString &themePath,
                             const std::map<QString, QString> &liveColors = {},
                             double scale = 0.75, int windowWidth = 0,
                             const QString &manifestJson = QString(),
                             const QString &cssContent = QString(),
                             const QList<UlauncherRow> &rows = {},
                             const QString &query = QString());

}
