#pragma once

#include "aitagger.h"
#include "paletteextractor.h"
#include "plasmaapply.h"
#include "ulaunchercontroller.h"
#include "wallpapermodel.h"
#include "wallpapersinfo.h"

#include <QtQml/qqmlregistration.h>

#include <KConfig>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class KConfig;

struct CustomCommand {
    QString command;
    bool enabled = true;
};

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
    Q_PROPERTY(bool lockscreen READ lockscreen WRITE setLockscreen NOTIFY lockscreenChanged)
    Q_PROPERTY(bool resizeEnabled READ resizeEnabled WRITE setResizeEnabled NOTIFY resizeEnabledChanged)
    Q_PROPERTY(int resizeHeight READ resizeHeight WRITE setResizeHeight NOTIFY resizeHeightChanged)
    Q_PROPERTY(int resizeFps READ resizeFps WRITE setResizeFps NOTIFY resizeFpsChanged)
    Q_PROPERTY(QStringList videoFolders READ videoFolders NOTIFY videoFoldersChanged)
    Q_PROPERTY(QString selectedWallpaper READ selectedWallpaper NOTIFY selectedWallpaperChanged)
    Q_PROPERTY(QString selectedWallpaperSize READ selectedWallpaperSize NOTIFY selectedWallpaperChanged)
    Q_PROPERTY(int selectedSourceColorIndex READ selectedSourceColorIndex WRITE setSelectedSourceColorIndex NOTIFY selectedSourceColorIndexChanged)
    // list of {command, enabled} maps; persisted in [Commands] of kwalrc.
    Q_PROPERTY(QVariantList customCommands READ customCommands NOTIFY customCommandsChanged)
    // %mode% / %ccolor% placeholder state; persisted in [Commands].
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString customColor READ customColor WRITE setCustomColor NOTIFY customColorChanged)
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
    // OpenAI-compatible chat API; settings in [Wallpapers] of kwalrc.
    Q_PROPERTY(AiTagger *aiTagger READ aiTagger CONSTANT)
    Q_PROPERTY(bool transitionEnabled READ transitionEnabled WRITE setTransitionEnabled NOTIFY transitionEnabledChanged)
    Q_PROPERTY(QString transitionLayer READ transitionLayer WRITE setTransitionLayer NOTIFY transitionLayerChanged)
    Q_PROPERTY(QStringList transitionLayers READ transitionLayers CONSTANT)
    Q_PROPERTY(QStringList transitionTypes READ transitionTypes CONSTANT)
    Q_PROPERTY(QString transitionType READ transitionType WRITE setTransitionType NOTIFY transitionTypeChanged)
    Q_PROPERTY(bool transitionAdvanced READ transitionAdvanced WRITE setTransitionAdvanced NOTIFY transitionAdvancedChanged)
    Q_PROPERTY(double transitionDuration READ transitionDuration WRITE setTransitionDuration NOTIFY transitionDurationChanged)
    Q_PROPERTY(int transitionFps READ transitionFps WRITE setTransitionFps NOTIFY transitionFpsChanged)
    Q_PROPERTY(int transitionStep READ transitionStep WRITE setTransitionStep NOTIFY transitionStepChanged)
    Q_PROPERTY(double transitionAngle READ transitionAngle WRITE setTransitionAngle NOTIFY transitionAngleChanged)
    Q_PROPERTY(QString transitionPos READ transitionPos WRITE setTransitionPos NOTIFY transitionPosChanged)
    Q_PROPERTY(bool transitionInvertY READ transitionInvertY WRITE setTransitionInvertY NOTIFY transitionInvertYChanged)
    Q_PROPERTY(QString transitionBezier READ transitionBezier WRITE setTransitionBezier NOTIFY transitionBezierChanged)
    Q_PROPERTY(double transitionWaveWidth READ transitionWaveWidth WRITE setTransitionWaveWidth NOTIFY transitionWaveWidthChanged)
    Q_PROPERTY(double transitionWaveHeight READ transitionWaveHeight WRITE setTransitionWaveHeight NOTIFY transitionWaveHeightChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    WallpaperModel *wallpaperModel() const;
    QStringList folders() const;
    int currentFolderIndex() const;
    QString currentFolderPath() const;
    QString configFilePath() const;
    bool wallpaperBusy() const;
    QString currentWallpaper() const;
    bool lockscreen() const;
    void setLockscreen(bool enabled);
    bool resizeEnabled() const;
    void setResizeEnabled(bool enabled);
    int resizeHeight() const;
    void setResizeHeight(int height);
    int resizeFps() const;
    void setResizeFps(int fps);
    QStringList videoFolders() const;
    QString selectedWallpaper() const;
    QString selectedWallpaperSize() const;
    QVariantList customCommands() const;
    int selectedSourceColorIndex() const;
    void setSelectedSourceColorIndex(int index);
    QString themeMode() const;
    void setThemeMode(const QString &mode);
    QString customColor() const;
    void setCustomColor(const QString &color);
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
    AiTagger *aiTagger() const;
    bool matugenDotsEnabled() const;
    void setMatugenDotsEnabled(bool enabled);
    bool colorCategoriesEnabled() const;
    void setColorCategoriesEnabled(bool enabled);
    bool matugenAvailable() const;
    bool transitionEnabled() const;
    void setTransitionEnabled(bool enabled);
    QString transitionLayer() const;
    void setTransitionLayer(const QString &layer);
    QStringList transitionLayers() const;
    QStringList transitionTypes() const;
    QString transitionType() const;
    void setTransitionType(const QString &type);
    bool transitionAdvanced() const;
    void setTransitionAdvanced(bool enabled);
    double transitionDuration() const;
    void setTransitionDuration(double duration);
    int transitionFps() const;
    void setTransitionFps(int fps);
    int transitionStep() const;
    void setTransitionStep(int step);
    double transitionAngle() const;
    void setTransitionAngle(double angle);
    QString transitionPos() const;
    void setTransitionPos(const QString &pos);
    bool transitionInvertY() const;
    void setTransitionInvertY(bool invertY);
    QString transitionBezier() const;
    void setTransitionBezier(const QString &bezier);
    double transitionWaveWidth() const;
    void setTransitionWaveWidth(double width);
    double transitionWaveHeight() const;
    void setTransitionWaveHeight(double height);

    Q_INVOKABLE void addFolder(const QString &path);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void setCurrentFolderIndex(int index);
    Q_INVOKABLE void selectWallpaper(const QString &filePath);
    Q_INVOKABLE void applyWallpaper();
    Q_INVOKABLE void generateVideoResizes(const QString &folder);

    Q_INVOKABLE QString fileUrl(const QString &path) const;

    // wallpaper extensions this Qt build cannot decode (empty when all decode).
    Q_INVOKABLE QStringList unreadableWallpaperFormats() const;

    // Plasma Smart Video Wallpaper (reborn) plugin; without it videos apply as their first frame.
    Q_INVOKABLE bool videoWallpaperPluginInstalled() const;

    Q_INVOKABLE void addCustomCommand();
    Q_INVOKABLE void updateCustomCommand(int index, const QString &command);
    Q_INVOKABLE void setCustomCommandEnabled(int index, bool enabled);
    Q_INVOKABLE void removeCustomCommand(int index);
    // <to> uses new-list numbering, as Kirigami.ListItemDragHandle reports it.
    Q_INVOKABLE void moveCustomCommand(int from, int to);

signals:
    void foldersChanged();
    void currentFolderIndexChanged();
    void videoFoldersChanged();
    void wallpaperBusyChanged();
    void wallpaperSetResult(bool success, const QString &message);
    void currentWallpaperChanged();
    void selectedWallpaperChanged();
    void selectedSourceColorIndexChanged();
    void customCommandsChanged();
    void themeModeChanged();
    void customColorChanged();
    void lockscreenChanged();
    void resizeEnabledChanged();
    void resizeHeightChanged();
    void resizeFpsChanged();

    void enableTransparencyChanged();
    void transparencyLevelChanged();
    void defaultWindowWidthChanged();
    void defaultWindowHeightChanged();
    void matugenDotsEnabledChanged();
    void colorCategoriesEnabledChanged();
    void transitionEnabledChanged();
    void transitionLayerChanged();
    void transitionTypeChanged();
    void transitionAdvancedChanged();
    void transitionDurationChanged();
    void transitionFpsChanged();
    void transitionStepChanged();
    void transitionAngleChanged();
    void transitionPosChanged();
    void transitionInvertYChanged();
    void transitionBezierChanged();
    void transitionWaveWidthChanged();
    void transitionWaveHeightChanged();
private:
    // tests use the placeholder pipeline directly, not the D-Bus apply path.
    friend class tst_AppController;
    void loadConfig();
    void saveConfig();
    void applyCurrentFolder();
    void setCurrentWallpaper(const QString &filePath);
    void setWallpaper(const QString &filePath);
    void wallpaperFinished(bool allOk, const QString &message);
    // expands %path% %image% %scolor% %sc% %mode% %ccolor%.
    QString expandPlaceholders(const QString &command) const;
    // enabled commands joined into one detached shell chain; last apply step.
    void runCustomCommands();
    void recomputeVideoFolders();
    static QStringList videoFilesIn(const QString &folder);

    WallpaperModel *m_wallpaperModel = nullptr;
    // owned; D-Bus apply + config edits run on the thread pool.
    PlasmaApply *m_plasmaApply = nullptr;
    UlauncherController *m_ulauncher = nullptr;
    PaletteExtractor *m_paletteExtractor = nullptr;
    AiTagger *m_aitagger = nullptr;
    std::unique_ptr<KConfig> m_config;
    QStringList m_folders;
    QStringList m_videoFolders;
    int m_currentFolderIndex = 0;
    bool m_wallpaperBusy = false;
    QString m_currentWallpaper;
    bool m_lockscreen = false;
    // mirrored into WallpapersInfo on apply.
    bool m_resizeEnabled = false;
    int m_resizeHeight = kResizeHeightDefault;
    int m_resizeFps = 0;
    QString m_selectedWallpaper;
    QString m_selectedWallpaperSize;
    // restored from m_sourceColorSelections per wallpaper; 0 = first dot.
    int m_selectedSourceColorIndex = 0;
    // session cache path->dot index; only the applied wallpaper's entry persists ([Wallpapers] "sourceColorSelections").
    QMap<QString, int> m_sourceColorSelections;
    // execution order; compact JSON array in [Commands] "commands".
    QList<CustomCommand> m_customCommands;
    QString m_themeMode = QStringLiteral("dark");
    QString m_customColor;
    bool m_enableTransparency = true;
    int m_transparencyLevel = kTransparencyLevelDefault;
    bool m_matugenDots = false;
    bool m_colorCategories = false;
    // built into a TransitionConfig on each apply.
    bool m_transitionEnabled = false;
    QString m_transitionLayer = QStringLiteral("bottom");
    QString m_transitionType = QStringLiteral("grow");
    bool m_transitionAdvanced = false;
    double m_transitionDuration = 0.9;
    int m_transitionFps = 0;
    int m_transitionStep = 2;
    double m_transitionAngle = 45.0;
    QString m_transitionPos = QStringLiteral("center");
    bool m_transitionInvertY = false;
    QString m_transitionBezier = QStringLiteral(".54,0,.34,.99");
    double m_transitionWaveWidth = 20.0;
    double m_transitionWaveHeight = 20.0;
    bool m_fastfetchAvailable = false;
    bool m_starshipAvailable = false;
    bool m_ulauncherAvailable = false;
    int m_defaultWindowWidth = kDefaultWindowWidth;
    int m_defaultWindowHeight = kDefaultWindowHeight;
    static constexpr int kResizeHeightDefault = 1080;
    static constexpr int kResizeHeightMin = 480;
    static constexpr int kResizeHeightMax = 4320;
    static constexpr int kResizeFpsMax = 240;

    // window geometry defaults + configured range; the SpinBox bounds and Main minimums mirror these.
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
