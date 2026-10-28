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

// One custom command: the shell line and whether it runs on apply.
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
    // Configured folders that contain at least one video; drives the
    // "Generate resizes" folder picker.
    Q_PROPERTY(QStringList videoFolders READ videoFolders NOTIFY videoFoldersChanged)
    Q_PROPERTY(QString selectedWallpaper READ selectedWallpaper NOTIFY selectedWallpaperChanged)
    Q_PROPERTY(QString selectedWallpaperSize READ selectedWallpaperSize NOTIFY selectedWallpaperChanged)
    Q_PROPERTY(int selectedSourceColorIndex READ selectedSourceColorIndex WRITE setSelectedSourceColorIndex NOTIFY selectedSourceColorIndexChanged)
    // Custom commands, the last step of a wallpaper apply; the drawer
    // edits and drag-reorders them, persisted in [Commands] of kwalrc.
    Q_PROPERTY(QVariantList customCommands READ customCommands NOTIFY customCommandsChanged)
    // Custom command placeholder state, persisted in [Commands]: the
    // mode (dark/light) for %mode% and the custom color hex for %ccolor%.
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
    // AI tag generation against an OpenAI-compatible chat API; settings
    // persist in [Wallpapers] of kwalrc.
    Q_PROPERTY(AiTagger *aiTagger READ aiTagger CONSTANT)
    // [Transitions]: layer-shell overlay animation applied on wallpaper
    // switches.
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
    // Runs the P5 video-resize generation for every video in <folder>.
    Q_INVOKABLE void generateVideoResizes(const QString &folder);

    // file:// URL (percent-encoded) for a local path; for QML media sources.
    Q_INVOKABLE QString fileUrl(const QString &path) const;

    // Custom commands, in execution order; the drawer's list mirrors these.
    Q_INVOKABLE void addCustomCommand();
    Q_INVOKABLE void updateCustomCommand(int index, const QString &command);
    Q_INVOKABLE void setCustomCommandEnabled(int index, bool enabled);
    Q_INVOKABLE void removeCustomCommand(int index);
    // Reorders the command at <from> to the position <to> (new-list
    // numbering, as Kirigami.ListItemDragHandle reports it).
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
    // Test access to the custom-command pipeline (placeholder expansion
    // and execution); the tests must not go through the D-Bus apply path.
    friend class tst_AppController;
    void loadConfig();
    void saveConfig();
    void applyCurrentFolder();
    void setCurrentWallpaper(const QString &filePath);
    void setWallpaper(const QString &filePath);
    // Shared finish handler for the wallpaper switch (direct path and
    // transition binary).
    void wallpaperFinished(bool allOk, const QString &message);
    // Expands the placeholders of one custom command for the current
    // selection: %path%, %image%, %scolor%, %sc%, %mode% and %ccolor%.
    QString expandPlaceholders(const QString &command) const;
    // Joins the enabled commands into one shell chain and starts it
    // detached; the last step of a wallpaper apply.
    void runCustomCommands();
    void recomputeVideoFolders();
    static QStringList videoFilesIn(const QString &folder);

    WallpaperModel *m_wallpaperModel = nullptr;
    // Owned by this object; the D-Bus apply + linear config edits run from
    // the thread pool so the UI stays responsive during a slow shell.
    PlasmaApply *m_plasmaApply = nullptr;
    // Owned via the QObject child relationship; created with this app's own
    // KConfig group so the preview settings persist in [Ulauncher] of
    // kwalrc.
    UlauncherController *m_ulauncher = nullptr;
    // Owned by this object; runs palette extraction on a thread pool.
    PaletteExtractor *m_paletteExtractor = nullptr;
    // Owned by this object; AI tag generation, settings in [Wallpapers].
    AiTagger *m_aitagger = nullptr;
    std::unique_ptr<KConfig> m_config;
    QStringList m_folders;
    QStringList m_videoFolders;
    int m_currentFolderIndex = 0;
    bool m_wallpaperBusy = false;
    QString m_currentWallpaper;
    // [Wallpapers] "lockscreen": mirror the applied wallpaper to the
    // KScreenLock config as well.
    bool m_lockscreen = false;
    // [Wallpapers] "resizeEnabled"/"resizeHeight"/"resizeFps": the P5
    // optional video resize; the settings are mirrored into WallpapersInfo.
    bool m_resizeEnabled = false;
    int m_resizeHeight = kResizeHeightDefault;
    int m_resizeFps = 0;
    QString m_selectedWallpaper;
    QString m_selectedWallpaperSize;
    // Index of the selected matugen source color dot of the current
    // wallpaper; the first dot is selected by default. Per-wallpaper:
    // restored from m_sourceColorSelections on every wallpaper change.
    int m_selectedSourceColorIndex = 0;
    // Wallpaper path -> selected matugen source color dot index, so the
    // dot selection survives a restart; persisted as a compact JSON
    // object in [Wallpapers] "sourceColorSelections" of kwalrc.
    QMap<QString, int> m_sourceColorSelections;
    // Custom commands, in execution order; persisted as a compact JSON
    // array in [Commands] "commands" of kwalrc.
    QList<CustomCommand> m_customCommands;
    // [Commands] "themeMode"/"customColor": the %mode% and %ccolor%
    // placeholder state of the custom commands.
    QString m_themeMode = QStringLiteral("dark");
    QString m_customColor;
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
    // [Transitions] "transitionEnabled"/"transitionLayer"/…; the overlay
    // animation settings, built into a TransitionConfig on each apply.
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
