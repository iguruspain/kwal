#pragma once

#include "wallpapermodel.h"

#include <QtQml/qqmlregistration.h>

#include <KConfig>
#include <QObject>
#include <QString>
#include <QStringList>

class KConfig;

// Application state and services; registered for QML and owned by the Main window.
class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(WallpaperModel *wallpaperModel READ wallpaperModel CONSTANT)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(int currentFolderIndex READ currentFolderIndex NOTIFY currentFolderIndexChanged)
    Q_PROPERTY(QString currentFolderPath READ currentFolderPath NOTIFY currentFolderIndexChanged)
    Q_PROPERTY(QString configFilePath READ configFilePath CONSTANT)
    Q_PROPERTY(bool wallpaperBusy READ wallpaperBusy NOTIFY wallpaperBusyChanged)
    Q_PROPERTY(QString currentWallpaper READ currentWallpaper NOTIFY currentWallpaperChanged)
    Q_PROPERTY(QString currentWallpaperSize READ currentWallpaperSize NOTIFY currentWallpaperChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    WallpaperModel *wallpaperModel() const;
    QStringList folders() const;
    int currentFolderIndex() const;
    QString currentFolderPath() const;
    QString configFilePath() const;
    bool wallpaperBusy() const;
    QString currentWallpaper() const;
    QString currentWallpaperSize() const;

    Q_INVOKABLE void addFolder(const QString &path);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void setCurrentFolderIndex(int index);
    Q_INVOKABLE void setWallpaper(const QString &filePath);

signals:
    void foldersChanged();
    void currentFolderIndexChanged();
    void wallpaperBusyChanged();
    void wallpaperSetResult(bool success, const QString &message);
    void currentWallpaperChanged();

private:
    void loadConfig();
    void saveConfig();
    void applyCurrentFolder();
    void setCurrentWallpaper(const QString &filePath);

    WallpaperModel *m_wallpaperModel = nullptr;
    std::unique_ptr<KConfig> m_config;
    QStringList m_folders;
    int m_currentFolderIndex = 0;
    bool m_wallpaperBusy = false;
    QString m_currentWallpaper;
    QString m_currentWallpaperSize;
};
