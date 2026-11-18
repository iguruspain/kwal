#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QSet>
#include <QtQml/qqmlregistration.h>
class QFileSystemWatcher;
class QTimer;


class WallpaperModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QStringList categoryFilter READ categoryFilter NOTIFY categoryFilterChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        ThumbnailRole,
        ExtensionRole,
        IsVideoRole,
        SourceColorsRole,
        IsAnimatedRole,
    };

    explicit WallpaperModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE QString pathAt(int row) const;
    // Row within the filtered entries, -1 if filtered out. Keep the cursor as a
    // path, never a row number: a filter change renumbers rows.
    Q_INVOKABLE int rowForPath(const QString &path) const;
    Q_INVOKABLE void setSourceDirectory(const QString &directory);
    Q_INVOKABLE QStringList filePaths() const;
    // Category AND-filter; empty = no restriction.
    QStringList categoryFilter() const;
    Q_INVOKABLE void setCategoryFilter(const QStringList &categories);

    static bool isVideo(const QString &filePath);
    // Instance form: QML cannot call static invokables.
    Q_INVOKABLE bool isVideoFile(const QString &filePath) const;
    static bool isImage(const QString &filePath);
    static const QStringList &imageExtensions();
    // Glob patterns (e.g. "*.mp4") for the supported video extensions.
    static QStringList videoNameFilters();
    QString filterText() const;
    void setFilterText(const QString &text);

signals:
    void countChanged();
    void filterTextChanged();
    void categoryFilterChanged();
    void scanned();

private:
    void refresh();
    void applyFilter();
    void onEntryChanged(const QString &path);
    QUrl thumbnailFor(const QString &filePath) const;
    QUrl videoThumbnailFor(const QString &filePath) const;

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;

    // Batches file-system event bursts into one re-scan.
    static constexpr int kRefreshDebounceMs = 200;
    QString m_directory;
    struct Entry {
        QString path;
        QString name;
        QUrl thumbnail;
        QString extension;
    };
    QVector<Entry> m_all;
    QVector<Entry> m_entries;
    QString m_filterText;
    QSet<QString> m_categoryFilter;
};

namespace wallpapermodel {
// Lowercased, deduped `accepted` entries no format in `readable`
// (QImageReader::supportedImageFormats()) can decode; matches any alias
// (jpg/jpeg/jfif…).
QStringList unreadableFormats(const QList<QByteArray> &readable, const QStringList &accepted);
} // namespace wallpapermodel
