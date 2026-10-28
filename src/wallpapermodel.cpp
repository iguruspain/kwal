#include "wallpapermodel.h"
#include "wallpapersinfo.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

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

const QStringList &videoExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("mp4"),
        QStringLiteral("webm"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("m4v"),
        QStringLiteral("avi"),
    };
    return extensions;
}
} // namespace

WallpaperModel::WallpaperModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kRefreshDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &WallpaperModel::refresh);
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        connect(info, &WallpapersInfo::entryChanged, this, &WallpaperModel::onEntryChanged);
    }
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        // Re-arm in case the directory was deleted and recreated meanwhile.
        if (!m_directory.isEmpty() && !m_watcher->directories().contains(m_directory)) {
            m_watcher->addPath(m_directory);
        }
        m_debounce->start();
    });
}

QHash<int, QByteArray> WallpaperModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {ThumbnailRole, "thumbnail"},
        {ExtensionRole, "extension"},
        {IsVideoRole, "isVideo"},
        {SourceColorsRole, "sourceColors"},
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
    case IsVideoRole:
        return isVideo(entry.path);
    case SourceColorsRole: {
        const WallpapersInfo *info = WallpapersInfo::instance();
        return info ? info->sourceColorsFor(entry.path) : QStringList();
    }
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

QStringList WallpaperModel::filePaths() const
{
    QStringList paths;
    paths.reserve(m_entries.size());
    for (const Entry &entry : m_entries) {
        paths.append(entry.path);
    }
    return paths;
}

bool WallpaperModel::isVideo(const QString &filePath)
{
    return videoExtensions().contains(QFileInfo(filePath).suffix(), Qt::CaseInsensitive);
}

bool WallpaperModel::isImage(const QString &filePath)
{
    return imageExtensions().contains(QFileInfo(filePath).suffix(), Qt::CaseInsensitive);
}

QStringList WallpaperModel::videoNameFilters()
{
    static const QStringList filters = [] {
        QStringList out;
        for (const QString &ext : videoExtensions()) {
            out.append(QStringLiteral("*.") + ext);
        }
        return out;
    }();
    return filters;
}

void WallpaperModel::setSourceDirectory(const QString &directory)
{
    if (m_directory == directory) {
        return;
    }
    const QString previous = m_directory;
    m_directory = directory;
    m_debounce->stop();
    if (!previous.isEmpty()) {
        m_watcher->removePath(previous);
    }
    if (!directory.isEmpty() && QDir(directory).exists()) {
        m_watcher->addPath(directory);
    }
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

QStringList WallpaperModel::categoryFilter() const
{
    return QStringList(m_categoryFilter.begin(), m_categoryFilter.end());
}

void WallpaperModel::setCategoryFilter(const QStringList &categories)
{
    const QSet<QString> next(categories.begin(), categories.end());
    if (next == m_categoryFilter) {
        return;
    }
    m_categoryFilter = next;
    beginResetModel();
    applyFilter();
    endResetModel();
    emit countChanged();
    emit categoryFilterChanged();
}

void WallpaperModel::refresh()
{
    m_all.clear();

    const QDir dir(m_directory);
    const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &fileName : files) {
        const QString suffix = QFileInfo(fileName).suffix();
        if (!imageExtensions().contains(suffix, Qt::CaseInsensitive)
            && !videoExtensions().contains(suffix, Qt::CaseInsensitive)) {
            continue;
        }
        const QString filePath = dir.filePath(fileName);
        const QFileInfo info(filePath);
        const Entry entry{filePath,
                          info.fileName(),
                          isVideo(filePath) ? videoThumbnailFor(filePath) : thumbnailFor(filePath),
                          suffix};
        m_all.append(entry);
    }

    beginResetModel();
    applyFilter();
    endResetModel();
    emit countChanged();
    // Every folder scan also drops cache entries whose file was deleted
    // (images, videos and their first-frame artifacts) from any folder.
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        info->sweep();
    }
    emit scanned();
}

void WallpaperModel::applyFilter()
{
    m_entries.clear();
    const WallpapersInfo *info = WallpapersInfo::instance();
    for (const Entry &entry : m_all) {
        if (!m_filterText.isEmpty()) {
            bool match = entry.name.contains(m_filterText, Qt::CaseInsensitive);
            if (!match && info) {
                for (const QString &tag : info->tagsFor(entry.path)) {
                    if (tag.contains(m_filterText, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            if (!match) {
                continue;
            }
        }
        if (!m_categoryFilter.isEmpty()) {
            const QStringList categories = info ? info->colorCategoriesFor(entry.path)
                                                : QStringList();
            bool match = true;
            for (const QString &category : m_categoryFilter) {
                if (!categories.contains(category)) {
                    match = false;
                    break;
                }
            }
            if (!match) {
                continue;
            }
        }
        m_entries.append(entry);
    }
}

void WallpaperModel::onEntryChanged(const QString &path)
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (!info) {
        return;
    }
    const WallpapersInfo::Entry entry = info->entryFor(path);
    const QUrl frameThumbnail = entry.type == QLatin1String("video") && !entry.firstFrame.isEmpty()
        ? QUrl(QStringLiteral("image://fdo_thumbnail/") + entry.firstFrame)
        : QUrl();
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).path != path) {
            continue;
        }
        QVector<int> roles{SourceColorsRole};
        if (!frameThumbnail.isEmpty()) {
            m_entries[row].thumbnail = frameThumbnail;
            roles.prepend(ThumbnailRole);
        }
        emit dataChanged(index(row), index(row), roles);
        break;
    }
    // Not visible in the current filter; the next re-scan picks it up.
    if (!frameThumbnail.isEmpty()) {
        for (Entry &full : m_all) {
            if (full.path == path) {
                full.thumbnail = frameThumbnail;
                break;
            }
        }
    }
    // New metadata (colors, tags) may change which entries match the active
    // category filter.
    if (!m_categoryFilter.isEmpty()) {
        beginResetModel();
        applyFilter();
        endResetModel();
        emit countChanged();
    }
}

QUrl WallpaperModel::thumbnailFor(const QString &filePath) const
{
    // The "fdo_thumbnail" provider (FdoThumbnailProvider) resolves the FreeDesktop
    // thumbnail cache and generates a 256 px thumbnail on a miss.
    return QUrl(QStringLiteral("image://fdo_thumbnail/") + filePath);
}

QUrl WallpaperModel::videoThumbnailFor(const QString &filePath) const
{
    // Video thumbnail: the cached first frame (wallpapers_info.json), served
    // through the same "fdo_thumbnail" provider; empty while it is missing.
    const WallpapersInfo *info = WallpapersInfo::instance();
    if (!info) {
        return {};
    }
    const QString frame = info->firstFrameFor(filePath);
    if (frame.isEmpty()) {
        return {};
    }
    return QUrl(QStringLiteral("image://fdo_thumbnail/") + frame);
}
