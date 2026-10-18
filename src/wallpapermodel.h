#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QtQml/qqmlregistration.h>


// Provides the wallpaper images of one folder: path, display name and a cached thumbnail.
class WallpaperModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        ThumbnailRole,
        ExtensionRole,
    };

    explicit WallpaperModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE void setSourceDirectory(const QString &directory);

signals:
    // Emitted when the number of entries changes (via model reset).
    void countChanged();

private:
    void refresh();
    QUrl thumbnailFor(const QString &filePath) const;

    QString m_directory;
    struct Entry {
        QString path;
        QString name;
        QUrl thumbnail;
        QString extension;
    };
    QVector<Entry> m_entries;
};
