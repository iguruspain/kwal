#include "wallpapermodel.h"

#include <QDir>
#include <QFileInfo>

namespace {
const QStringList &imageExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("webp"),
        QStringLiteral("bmp"),
        QStringLiteral("gif"),
        QStringLiteral("svg"),
    };
    return extensions;
}
} // namespace

WallpaperModel::WallpaperModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QHash<int, QByteArray> WallpaperModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {ThumbnailRole, "thumbnail"},
        {ExtensionRole, "extension"},
    };
}

int WallpaperModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant WallpaperModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return {};
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case PathRole:
        return entry.path;
    case NameRole:
        return entry.name;
    case ThumbnailRole:
        return entry.thumbnail;
    case ExtensionRole:
        return entry.extension;
    default:
        return {};
    }
}

QString WallpaperModel::pathAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return {};
    }
    return m_entries.at(row).path;
}

void WallpaperModel::setSourceDirectory(const QString &directory)
{
    if (m_directory == directory) {
        return;
    }
    m_directory = directory;
    refresh();
}

QString WallpaperModel::filterText() const
{
    return m_filterText;
}

void WallpaperModel::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    m_filterText = text;
    beginResetModel();
    applyFilter();
    endResetModel();
    emit filterTextChanged();
    emit countChanged();
}

void WallpaperModel::refresh()
{
    m_all.clear();

    const QDir dir(m_directory);
    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &fileName : files) {
        if (!imageExtensions().contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive)) {
            continue;
        }
        const QString filePath = dir.filePath(fileName);
        m_all.append({filePath, QFileInfo(filePath).fileName(), thumbnailFor(filePath), QFileInfo(filePath).suffix()});
    }

    beginResetModel();
    applyFilter();
    endResetModel();
    emit countChanged();
}

void WallpaperModel::applyFilter()
{
    m_entries.clear();
    if (m_filterText.isEmpty()) {
        m_entries = m_all;
        return;
    }
    for (const Entry &entry : m_all) {
        if (entry.name.contains(m_filterText, Qt::CaseInsensitive)) {
            m_entries.append(entry);
        }
    }
}

QUrl WallpaperModel::thumbnailFor(const QString &filePath) const
{
    // The "fdo_thumbnail" provider (FdoThumbnailProvider) resolves the FreeDesktop
    // thumbnail cache and generates a 256 px thumbnail on a miss.
    return QUrl(QStringLiteral("image://fdo_thumbnail/") + filePath);
}
