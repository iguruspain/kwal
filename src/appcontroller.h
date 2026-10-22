#pragma once

#include "paletteextractor.h"
#include "ulaunchercontroller.h"
#include "wallpapermodel.h"
#include "wallpapersinfo.h"

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
    Q_PROPERTY(int selectedSourceColorIndex READ selectedSourceColorIndex WRITE setSelectedSourceColorIndex NOTIFY selectedSourceColorIndexChanged)
    Q_PROPERTY(bool enableTransparency READ enableTransparency WRITE setEnableTransparency NOTIFY enableTransparencyChanged)
    Q_PROPERTY(int transparencyLevel READ transparencyLevel WRITE setTransparencyLevel NOTIFY transparencyLevelChanged)
    Q_PROPERTY(int defaultWindowWidth READ defaultWindowWidth WRITE setDefaultWindowWidth NOTIFY defaultWindowWidthChanged)
    Q_PROPERTY(int defaultWindowHeight READ defaultWindowHeight WRITE setDefaultWindowHeight NOTIFY defaultWindowHeightChanged)
    Q_PROPERTY(bool fastfetchAvailable READ fastfetchAvailable CONSTANT)
    Q_PROPERTY(bool starshipAvailable READ starshipAvailable CONSTANT)
    Q_PROPERTY(bool ulauncherAvailable READ ulauncherAvailable CONSTANT)
    Q_PROPERTY(bool matugenDotsEnabled READ matugenDotsEnabled WRITE setMatugenDotsEnabled NOTIFY matugenDotsEnabledChanged)
    Q_PROPERTY(bool colorCategoriesEnabled READ colorCategoriesEnabled WRITE setColorCategoriesEnabled NOTIFY colorCategoriesEnabledChanged)
    Q_PROPERTY(bool matugenAvailable READ matugenAvailable CONSTANT)
    Q_PROPERTY(UlauncherController *ulauncher READ ulauncher CONSTANT)
    Q_PROPERTY(WallpapersInfo *wallpapersInfo READ wallpapersInfo CONSTANT)
    Q_PROPERTY(PaletteExtractor *paletteExtractor READ paletteExtractor CONSTANT)

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
    int selectedSourceColorIndex() const;
    void setSelectedSourceColorIndex(int index);
    bool enableTransparency() const;
    void setEnableTransparency(bool enabled);
    int transparencyLevel() const;
    void setTransparencyLevel(int level);
    int defaultWindowWidth() const;
    int defaultWindowHeight() const;
    void setDefaultWindowWidth(int width);
    void setDefaultWindowHeight(int height);
    bool fastfetchAvailable() const;
    bool starshipAvailable() const;
    bool ulauncherAvailable() const;
    UlauncherController *ulauncher() const;
    WallpapersInfo *wallpapersInfo() const;
    PaletteExtractor *paletteExtractor() const;
    bool matugenDotsEnabled() const;
    void setMatugenDotsEnabled(bool enabled);
    bool colorCategoriesEnabled() const;
    void setColorCategoriesEnabled(bool enabled);
    bool matugenAvailable() const;

    Q_INVOKABLE void addFolder(const QString &path);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void setCurrentFolderIndex(int index);
    Q_INVOKABLE void selectWallpaper(const QString &filePath);
    Q_INVOKABLE void applyWallpaper();

    // file:// URL (percent-encoded) for a local path; for QML media sources.
    Q_INVOKABLE QString fileUrl(const QString &path) const;

signals:
    void foldersChanged();
    void currentFolderIndexChanged();
    void wallpaperBusyChanged();
    void wallpaperSetResult(bool success, const QString &message);
    void currentWallpaperChanged();
    void selectedWallpaperChanged();
    void selectedSourceColorIndexChanged();

    void enableTransparencyChanged();
    void transparencyLevelChanged();
    void defaultWindowWidthChanged();
    void defaultWindowHeightChanged();
    void matugenDotsEnabledChanged();
    void colorCategoriesEnabledChanged();
private:
    void loadConfig();
    void saveConfig();
    void applyCurrentFolder();
    void setCurrentWallpaper(const QString &filePath);
    void setWallpaper(const QString &filePath);

    WallpaperModel *m_wallpaperModel = nullptr;
    // Owned via the QObject child relationship; created with this app's own
    // KConfig group so the preview settings persist in [Ulauncher] of
    // kwalrc.
    UlauncherController *m_ulauncher = nullptr;
    // Owned by this object; runs palette extraction on a thread pool.
    PaletteExtractor *m_paletteExtractor = nullptr;
    std::unique_ptr<KConfig> m_config;
    QStringList m_folders;
    int m_currentFolderIndex = 0;
    bool m_wallpaperBusy = false;
    QString m_currentWallpaper;
    QString m_currentWallpaperSize;
    QString m_selectedWallpaper;
    QString m_selectedWallpaperSize;
    // Index of the selected matugen source color dot of the current wallpaper;
    // -1 = none. Reset on every wallpaper change.
    int m_selectedSourceColorIndex = -1;
    bool m_enableTransparency = true;
    // Window background opacity in percent (0-100) used when transparency
    // is enabled; steps of 5 in the settings SpinBox.
    int m_transparencyLevel = kTransparencyLevelDefault;
    // Show matugen color dots on the selected wallpaper; persisted in
    // [Wallpapers] "matugenDots".
    bool m_matugenDots = false;
    // Show the color-category chips in the wallpapers search; persisted in
    // [Wallpapers] "colorCategories".
    bool m_colorCategories = false;
    bool m_fastfetchAvailable = false;
    bool m_starshipAvailable = false;
    bool m_ulauncherAvailable = false;
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
    static constexpr int kTransparencyLevelDefault = 80;
    static constexpr int kTransparencyLevelMin = 0;
    static constexpr int kTransparencyLevelMax = 100;
};
