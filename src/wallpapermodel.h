#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QSet>
#include <QtQml/qqmlregistration.h>
class QFileSystemWatcher;
class QTimer;


// Provides the wallpaper media (images and videos) of one folder: path,
// display name and a cached thumbnail.
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
    };

    explicit WallpaperModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE void setSourceDirectory(const QString &directory);
    Q_INVOKABLE QStringList filePaths() const;
    // Selected color categories; the visible entries must contain all of
    // them (AND). Empty means "no category restriction".
    QStringList categoryFilter() const;
    Q_INVOKABLE void setCategoryFilter(const QStringList &categories);

    // True if the file has one of the supported video extensions.
    static bool isVideo(const QString &filePath);
    // Instance form exposed to QML (QML cannot call static invokables);
    // used by the detail panel's isVideo binding.
    Q_INVOKABLE bool isVideoFile(const QString &filePath) const;
    // True if the file has one of the supported image extensions.
    static bool isImage(const QString &filePath);
    // Name filters (e.g. "*.mp4") for the supported video extensions; the
    // single source for enumerating video files in a folder.
    static QStringList videoNameFilters();
    QString filterText() const;
    void setFilterText(const QString &text);

signals:
    // Emitted when the number of entries changes (via model reset).
    void countChanged();
    void filterTextChanged();
    void categoryFilterChanged();
    // Emitted after a full re-scan of the source directory.
    void scanned();

private:
    void refresh();
    void applyFilter();
    void onEntryChanged(const QString &path);
    QUrl thumbnailFor(const QString &filePath) const;
    QUrl videoThumbnailFor(const QString &filePath) const;

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;

    // Batches a burst of file-system events into a single re-scan.
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
