#include "wallpapermodel.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>

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

void WallpaperModel::refresh()
{
    beginResetModel();
    m_entries.clear();

    const QDir dir(m_directory);
    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &fileName : files) {
        if (!imageExtensions().contains(QFileInfo(fileName).suffix(), Qt::CaseInsensitive)) {
            continue;
        }
        const QString filePath = dir.filePath(fileName);
        m_entries.append({filePath, QFileInfo(filePath).fileName(), thumbnailFor(filePath), QFileInfo(filePath).suffix()});
    }
    endResetModel();
    emit countChanged();
}

QUrl WallpaperModel::thumbnailFor(const QString &filePath) const
{
    const QFileInfo info(filePath);
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir + QStringLiteral("/thumbnails"));

    // Key on path + mtime so a replaced image gets a fresh thumbnail.
    const QString key = filePath + QStringLiteral(":") + QString::number(info.lastModified().toSecsSinceEpoch());
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex()).left(16);
    const QString thumbnailPath = cacheDir + QStringLiteral("/thumbnails/") + hash + QStringLiteral(".jpg");
    if (QFile::exists(thumbnailPath)) {
        return QUrl::fromLocalFile(thumbnailPath);
    }

    QImageReader reader(filePath);
    reader.setScaledSize(QSize(256, 256));
    const QImage image = reader.read();
    if (image.isNull()) {
        return QUrl();
    }
    image.save(thumbnailPath, "JPG", 85);
    return QUrl::fromLocalFile(thumbnailPath);
}
