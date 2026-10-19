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
    Q_PROPERTY(QString selectedWallpaper READ selectedWallpaper NOTIFY selectedWallpaperChanged)
    Q_PROPERTY(QString selectedWallpaperSize READ selectedWallpaperSize NOTIFY selectedWallpaperChanged)
    Q_PROPERTY(bool enableTransparency READ enableTransparency WRITE setEnableTransparency NOTIFY enableTransparencyChanged)
    Q_PROPERTY(int defaultWindowWidth READ defaultWindowWidth WRITE setDefaultWindowWidth NOTIFY defaultWindowWidthChanged)
    Q_PROPERTY(int defaultWindowHeight READ defaultWindowHeight WRITE setDefaultWindowHeight NOTIFY defaultWindowHeightChanged)

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
    QString selectedWallpaper() const;
    QString selectedWallpaperSize() const;
    bool enableTransparency() const;
    void setEnableTransparency(bool enabled);
    int defaultWindowWidth() const;
    int defaultWindowHeight() const;
    void setDefaultWindowWidth(int width);
    void setDefaultWindowHeight(int height);

    Q_INVOKABLE void addFolder(const QString &path);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void setCurrentFolderIndex(int index);
    Q_INVOKABLE void selectWallpaper(const QString &filePath);
    Q_INVOKABLE void applyWallpaper();

signals:
    void foldersChanged();
    void currentFolderIndexChanged();
    void wallpaperBusyChanged();
    void wallpaperSetResult(bool success, const QString &message);
    void currentWallpaperChanged();
    void selectedWallpaperChanged();

    void enableTransparencyChanged();
    void defaultWindowWidthChanged();
    void defaultWindowHeightChanged();
private:
    void loadConfig();
    void saveConfig();
    void applyCurrentFolder();
    void setCurrentWallpaper(const QString &filePath);
    void setWallpaper(const QString &filePath);

    WallpaperModel *m_wallpaperModel = nullptr;
    std::unique_ptr<KConfig> m_config;
    QStringList m_folders;
    int m_currentFolderIndex = 0;
    bool m_wallpaperBusy = false;
    QString m_currentWallpaper;
    QString m_currentWallpaperSize;
    QString m_selectedWallpaper;
    QString m_selectedWallpaperSize;
    bool m_enableTransparency = true;
    int m_defaultWindowWidth = kDefaultWindowWidth;
    int m_defaultWindowHeight = kDefaultWindowHeight;

    // Default window geometry and the range the user may configure it to;
    // the SpinBox bounds and the Main window minimums mirror these values.
    static constexpr int kDefaultWindowWidth = 960;
    static constexpr int kDefaultWindowHeight = 650;
    static constexpr int kMinimumWindowWidth = 700;
    static constexpr int kMinimumWindowHeight = 400;
    static constexpr int kMaximumWindowWidth = 3840;
    static constexpr int kMaximumWindowHeight = 2160;
};
