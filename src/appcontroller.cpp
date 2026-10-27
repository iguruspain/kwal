#include "appcontroller.h"
#include "templateinstaller.h"
#include "paletteextractor.h"
#include "transitions/transitionspec.h"
#include "wallpapermodel.h"
#include "wallpapersinfo.h"
#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QProcess>
#include <QRunnable>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>
#include <QDebug>
#include "transitions/perf.h"
#include <functional>

namespace {
// "WxH" string for a readable image, empty if it cannot be determined.
QString imageResolution(const QString &filePath)
{
    const QSize size = QImageReader(filePath).size();
    return (size.width() > 0 && size.height() > 0)
        ? QStringLiteral("%1×%2").arg(size.width()).arg(size.height())
        : QString();
}

// Whether a companion tool binary can be found on $PATH.
bool isToolInstalled(const QString &binary)
{
    return !QStandardPaths::findExecutable(binary).isEmpty();
}

// Runs the wallpaper switch off the main thread (D-Bus round trips +
// config edits), directly through PlasmaApply or, when a transition is
// enabled, through the standalone transition binary (the binary runs the
// overlay + switch + holds; the lockscreen mirror happens here).
class WallpaperApplyJob : public QRunnable
{
public:
    WallpaperApplyJob(PlasmaApply *apply, QString type, QString path, bool lockscreen,
                      const QString &binaryPath, QString framePath,
                      transitions::TransitionConfig cfg,
                      std::function<void(bool, const QString &)> finish)
        : m_apply(apply)
        , m_type(std::move(type))
        , m_path(std::move(path))
        , m_lockscreen(lockscreen)
        , m_binaryPath(binaryPath)
        , m_framePath(std::move(framePath))
        , m_cfg(std::move(cfg))
        , m_finish(std::move(finish))
    {
    }

    void run() override
    {
        if (!m_cfg.enabled || m_binaryPath.isEmpty()) {
            m_apply->applyWallpaper(m_type, m_path, m_lockscreen);
            return;
        }
        // Transition mode: run the standalone binary; its exit code is the
        // result.
        QStringList args;
        args << QStringLiteral("--type") << m_type
             << QStringLiteral("--file") << m_path;
        if (!m_framePath.isEmpty()) {
            args << QStringLiteral("--frame") << m_framePath;
        }
        args << QStringLiteral("--layer") << m_cfg.layer
             << QStringLiteral("--effect") << m_cfg.type
             << QStringLiteral("--duration") << QString::number(m_cfg.duration)
             << QStringLiteral("--fps") << QString::number(m_cfg.fps)
             << QStringLiteral("--step") << QString::number(m_cfg.step)
             << QStringLiteral("--angle") << QString::number(m_cfg.angle)
             << QStringLiteral("--pos") << m_cfg.pos
             << QStringLiteral("--bezier") << m_cfg.bezier
             << QStringLiteral("--wave-width") << QString::number(m_cfg.waveWidth)
             << QStringLiteral("--wave-height") << QString::number(m_cfg.waveHeight);
        if (m_cfg.advancedEnabled) {
            args << QStringLiteral("--advanced");
        }
        if (m_cfg.invertY) {
            args << QStringLiteral("--invert-y");
        }
        QProcess proc;
        proc.start(m_binaryPath, args);
        if (!proc.waitForStarted(5000)) {
            m_finish(false, QStringLiteral("Could not start the transition process."));
            return;
        }
        // The animation is a few seconds plus the holds; be generous.
        if (!proc.waitForFinished(120000)) {
            proc.kill();
            proc.waitForFinished(3000);
            m_finish(false, QStringLiteral("The transition timed out."));
            return;
        }
        const QByteArray err = proc.readAllStandardError();
        if (!err.isEmpty()) {
            qWarning().noquote() << err.constData();
        }
        const bool ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
        if (ok && m_lockscreen) {
            // The binary does not touch kscreenlockerrc; mirror it here
            // with the same plugin/URL resolution as the direct path.
            QString plugin = QString::fromLatin1(plasmaapply::kImagePluginId);
            QString imageUrl;
            QString videoEntryUrl;
            QString videoLastUrl;
            if (m_type == QLatin1String("video") && m_apply->rebornAvailable()) {
                plugin = QString::fromLatin1(plasmaapply::kRebornPluginId);
                videoEntryUrl = plasmaapply::fileUrl(m_path);
                videoLastUrl = plasmaapply::rawVideoUrl(m_path);
            } else {
                imageUrl = plasmaapply::fileUrl(m_framePath.isEmpty() ? m_path : m_framePath);
            }
            if (!m_apply->mirrorLockscreen(plugin, imageUrl, videoEntryUrl, videoLastUrl)) {
                m_finish(false, QStringLiteral("Could not write kscreenlockerrc."));
                return;
            }
        }
        QString message;
        if (!ok) {
            // The binary prints "kwal-transition: <reason>" to stderr on
            // failure; use the last such line.
            for (const QByteArray &line : err.split('\n')) {
                const QString lineStr = QString::fromUtf8(line);
                if (lineStr.startsWith(QStringLiteral("kwal-transition:"))) {
                    message = lineStr.mid(17);
                }
            }
            if (message.isEmpty()) {
                message = QStringLiteral("The wallpaper switch failed.");
            }
        }
        m_finish(ok, ok ? QStringLiteral("Wallpaper set.") : message);
    }

private:
    PlasmaApply *m_apply;
    QString m_type;
    QString m_path;
    bool m_lockscreen;
    QString m_binaryPath;
    QString m_framePath;
    transitions::TransitionConfig m_cfg;
    std::function<void(bool, const QString &)> m_finish;
};

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_wallpaperModel(new WallpaperModel(this))
    , m_plasmaApply(new PlasmaApply(this))
    , m_config(new KConfig(QStringLiteral("kwalrc"), KConfig::SimpleConfig))
    , m_fastfetchAvailable(isToolInstalled(QStringLiteral("fastfetch")))
    , m_starshipAvailable(isToolInstalled(QStringLiteral("starship")))
    , m_ulauncherAvailable(isToolInstalled(QStringLiteral("ulauncher")))
    , m_paletteExtractor(new PaletteExtractor(this))
{
    m_ulauncher = new UlauncherController(m_config->group(QStringLiteral("Ulauncher")), this);
    loadConfig();
    connect(m_plasmaApply, &PlasmaApply::result, this, [this](bool allOk, const QString &message) {
        wallpaperFinished(allOk, message);
    });
    // Kick the media info cache (first frames, ...) after every folder
    // re-scan. Must precede applyCurrentFolder(): the initial scan is
    // synchronous and its "scanned" signal would otherwise be missed.
    connect(m_wallpaperModel, &WallpaperModel::scanned, this, [this]() {
        WallpapersInfo *info = WallpapersInfo::instance();
        if (!info) {
            return;
        }
        const QStringList paths = m_wallpaperModel->filePaths();
        for (const QString &path : paths) {
            info->refreshFile(path, WallpaperModel::isVideo(path) ? QStringLiteral("video") : QStringLiteral("image"));
        }
    });
    // P7: purge orphaned entries and cache files at startup (the index was
    // already loaded in WallpapersInfo::initialize) and after every folder
    // scan; every purge prunes the dead playlist URLs from the desktop
    // appletsrc.
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        connect(info, &WallpapersInfo::scanFinished, info, &WallpapersInfo::sweep);
        connect(info, &WallpapersInfo::purged, m_plasmaApply, &PlasmaApply::pruneDeadVideoUrls);
        info->sweep();
    }
    applyCurrentFolder();
    tools::installTemplates();
}

WallpaperModel *AppController::wallpaperModel() const
{
    return m_wallpaperModel;
}

QStringList AppController::folders() const
{
    return m_folders;
}

int AppController::currentFolderIndex() const
{
    return m_currentFolderIndex;
}

QString AppController::currentFolderPath() const
{
    if (m_currentFolderIndex >= 0 && m_currentFolderIndex < m_folders.size()) {
        return m_folders.at(m_currentFolderIndex);
    }
    return {};
}

QString AppController::configFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kwalrc");
}

bool AppController::wallpaperBusy() const
{
    return m_wallpaperBusy;
}

QString AppController::currentWallpaper() const
{
    return m_currentWallpaper;
}

QString AppController::selectedWallpaper() const
{
    return m_selectedWallpaper;
}

QString AppController::selectedWallpaperSize() const
{
    return m_selectedWallpaperSize;
}

int AppController::selectedSourceColorIndex() const
{
    return m_selectedSourceColorIndex;
}

void AppController::setSelectedSourceColorIndex(int index)
{
    if (m_selectedSourceColorIndex == index) {
        return;
    }
    m_selectedSourceColorIndex = index;
    // The selection is per wallpaper, restored on the next startup and on
    // reselection; until selectWallpaper has run there is no wallpaper to
    // key it on.
    if (!m_selectedWallpaper.isEmpty()) {
        m_sourceColorSelections.insert(m_selectedWallpaper, index);
        saveConfig();
    }
    emit selectedSourceColorIndexChanged();
}

QString AppController::themeMode() const
{
    return m_themeMode;
}

void AppController::setThemeMode(const QString &mode)
{
    if (m_themeMode == mode) {
        return;
    }
    m_themeMode = mode;
    saveConfig();
    emit themeModeChanged();
}

QString AppController::customColor() const
{
    return m_customColor;
}

void AppController::setCustomColor(const QString &color)
{
    if (m_customColor == color) {
        return;
    }
    m_customColor = color;
    saveConfig();
    emit customColorChanged();
}

bool AppController::enableTransparency() const
{
    return m_enableTransparency;
}

void AppController::setEnableTransparency(bool enabled)
{
    if (m_enableTransparency == enabled) {
        return;
    }
    m_enableTransparency = enabled;
    saveConfig();
    emit enableTransparencyChanged();
}

int AppController::transparencyLevel() const
{
    return m_transparencyLevel;
}

void AppController::setTransparencyLevel(int level)
{
    const int clamped = qBound(kTransparencyLevelMin, level, kTransparencyLevelMax);
    if (m_transparencyLevel == clamped) {
        return;
    }
    m_transparencyLevel = clamped;
    saveConfig();
    emit transparencyLevelChanged();
}

int AppController::defaultWindowWidth() const
{
    return m_defaultWindowWidth;
}

int AppController::defaultWindowHeight() const
{
    return m_defaultWindowHeight;
}

bool AppController::fastfetchAvailable() const
{
    return m_fastfetchAvailable;
}

bool AppController::starshipAvailable() const
{
    return m_starshipAvailable;
}

bool AppController::ulauncherAvailable() const
{
    return m_ulauncherAvailable;
}

UlauncherController *AppController::ulauncher() const
{
    return m_ulauncher;
}

WallpapersInfo *AppController::wallpapersInfo() const
{
    return WallpapersInfo::instance();
}

PaletteExtractor *AppController::paletteExtractor() const
{
    return m_paletteExtractor;
}

bool AppController::matugenDotsEnabled() const
{
    return m_matugenDots;
}

void AppController::setMatugenDotsEnabled(bool enabled)
{
    if (m_matugenDots == enabled) {
        return;
    }
    m_matugenDots = enabled;
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        info->setMatugenEnabled(m_matugenDots);
    }
    saveConfig();
    emit matugenDotsEnabledChanged();
}

bool AppController::colorCategoriesEnabled() const
{
    return m_colorCategories;
}

void AppController::setColorCategoriesEnabled(bool enabled)
{
    if (m_colorCategories == enabled) {
        return;
    }
    m_colorCategories = enabled;
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        info->setMatugenEnabled(m_matugenDots);
    }
    saveConfig();
    emit colorCategoriesEnabledChanged();
}

bool AppController::matugenAvailable() const
{
    const WallpapersInfo *info = WallpapersInfo::instance();
    return info != nullptr && info->matugenAvailable();
}

void AppController::setDefaultWindowWidth(int width)
{
    const int clamped = qBound(kMinimumWindowWidth, width, kMaximumWindowWidth);
    if (m_defaultWindowWidth == clamped) {
        return;
    }
    m_defaultWindowWidth = clamped;
    saveConfig();
    emit defaultWindowWidthChanged();
}

void AppController::setDefaultWindowHeight(int height)
{
    const int clamped = qBound(kMinimumWindowHeight, height, kMaximumWindowHeight);
    if (m_defaultWindowHeight == clamped) {
        return;
    }
    m_defaultWindowHeight = clamped;
    saveConfig();
    emit defaultWindowHeightChanged();
}

void AppController::addFolder(const QString &path)
{
    QString trimmed = path.trimmed();
    if (trimmed.startsWith(QLatin1String("file://"))) {
        // FolderDialog hands us a file URL; the folder list stores plain paths.
        trimmed = QUrl(trimmed).toLocalFile();
    }
    if (trimmed.isEmpty() || m_folders.contains(trimmed)) {
        return;
    }
    m_folders.append(trimmed);
    saveConfig();
    emit foldersChanged();
    recomputeVideoFolders();
    setCurrentFolderIndex(m_folders.size() - 1);
}

void AppController::removeFolder(int index)
{
    if (index < 0 || index >= m_folders.size()) {
        return;
    }
    m_folders.removeAt(index);
    saveConfig();

    if (m_folders.isEmpty()) {
        m_currentFolderIndex = 0;
    } else if (index < m_currentFolderIndex) {
        m_currentFolderIndex = qMax(0, m_currentFolderIndex - 1);
    } else if (m_currentFolderIndex >= m_folders.size()) {
        m_currentFolderIndex = m_folders.size() - 1;
    }
    emit foldersChanged();
    recomputeVideoFolders();
    applyCurrentFolder();
    emit currentFolderIndexChanged();
}

void AppController::setCurrentFolderIndex(int index)
{
    if (index == m_currentFolderIndex || index < 0 || index >= m_folders.size()) {
        return;
    }
    m_currentFolderIndex = index;
    saveConfig();
    applyCurrentFolder();
    emit currentFolderIndexChanged();
}

void AppController::selectWallpaper(const QString &filePath)
{
    if (m_selectedWallpaper == filePath) {
        return;
    }
    m_selectedWallpaper = filePath;
    // Videos carry no dimensions QImageReader can read; the cached first
    // frame (extracted at the video's native resolution) does.
    QString source = filePath;
    if (WallpaperModel::isVideo(filePath)) {
        if (WallpapersInfo *info = wallpapersInfo()) {
            source = info->firstFrameFor(filePath);
        }
    }
    m_selectedWallpaperSize = source.isEmpty() ? QString() : imageResolution(source);
    // The dot selection is per wallpaper: restore this wallpaper's stored
    // index (the first dot when it has none).
    const int stored = m_sourceColorSelections.value(filePath, 0);
    if (m_selectedSourceColorIndex != stored) {
        m_selectedSourceColorIndex = stored;
        emit selectedSourceColorIndexChanged();
    }
    emit selectedWallpaperChanged();
}

QString AppController::fileUrl(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    return QUrl::fromLocalFile(path).toString();
}

void AppController::applyWallpaper()
{
    if (m_selectedWallpaper.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(m_selectedWallpaper)) {
        m_selectedWallpaper.clear();
        emit selectedWallpaperChanged();
        emit wallpaperSetResult(false, QStringLiteral("The selected wallpaper no longer exists."));
        return;
    }
    setWallpaper(m_selectedWallpaper);
}

void AppController::setWallpaper(const QString &filePath)
{
    if (m_wallpaperBusy) {
        return;
    }
    const bool video = WallpaperModel::isVideo(filePath);
    QString target = filePath;
    // P5: while the setting is on, prefer the resized cache copy.
    if (video && m_resizeEnabled) {
        if (WallpapersInfo *info = WallpapersInfo::instance()) {
            const WallpapersInfo::Entry entry = info->entryFor(filePath);
            if (!entry.resized.isEmpty() && QFileInfo::exists(entry.resized)) {
                target = entry.resized;
            }
        }
    }
    m_wallpaperBusy = true;
    emit wallpaperBusyChanged();
    qWarning("perf: %lld setWallpaper: job starting", perfMs());
    transitions::TransitionConfig cfg;
    cfg.enabled = m_transitionEnabled;
    cfg.layer = m_transitionLayer;
    cfg.type = m_transitionType;
    cfg.advancedEnabled = m_transitionAdvanced;
    cfg.duration = m_transitionDuration;
    cfg.fps = m_transitionFps;
    cfg.step = m_transitionStep;
    cfg.angle = m_transitionAngle;
    cfg.pos = m_transitionPos;
    cfg.invertY = m_transitionInvertY;
    cfg.bezier = m_transitionBezier;
    cfg.waveWidth = m_transitionWaveWidth;
    cfg.waveHeight = m_transitionWaveHeight;
    QString binaryPath;
    if (cfg.enabled) {
        // Standalone transition binary; without it (pre-install build)
        // fall back to the direct switch.
        binaryPath = QCoreApplication::applicationDirPath() + QStringLiteral("/kwal-transition");
        if (!QFileInfo::exists(binaryPath)) {
            binaryPath = QStandardPaths::findExecutable(QStringLiteral("kwal-transition"));
        }
        if (binaryPath.isEmpty()) {
            qWarning() << "kwal-transition binary not found; applying without transition";
        }
    }
    // First frame for a video: what the binary paints in Cover and what
    // Plasma shows as the video fallback (the entry is keyed by the
    // original path, not the resize target).
    QString framePath;
    if (video) {
        if (WallpapersInfo *info = WallpapersInfo::instance()) {
            framePath = info->entryFor(filePath).firstFrame;
        }
    }
    QThreadPool::globalInstance()->start(
        new WallpaperApplyJob(m_plasmaApply,
                              video ? QStringLiteral("video") : QStringLiteral("image"),
                              target,
                              m_lockscreen,
                              binaryPath,
                              framePath,
                              cfg,
                              [this](bool allOk, const QString &message) {
                                  wallpaperFinished(allOk, message);
                              }));
}

// Shared finish handler for both switch paths (direct PlasmaApply and the
// transition binary).
void AppController::wallpaperFinished(bool allOk, const QString &message)
{
    m_wallpaperBusy = false;
    emit wallpaperBusyChanged();
    if (allOk) {
        setCurrentWallpaper(m_selectedWallpaper);
    }
    emit wallpaperSetResult(allOk, message);
    // The custom commands are the last step of the apply, regardless of
    // the switch result (the old app ran them after the qdbus call too).
    runCustomCommands();
}

void AppController::setCurrentWallpaper(const QString &filePath)
{
    m_currentWallpaper = filePath;
    saveConfig();
    emit currentWallpaperChanged();
}

QVariantList AppController::customCommands() const
{
    QVariantList list;
    for (const CustomCommand &command : m_customCommands) {
        QVariantMap map;
        map[QStringLiteral("command")] = command.command;
        map[QStringLiteral("enabled")] = command.enabled;
        list.append(map);
    }
    return list;
}

void AppController::addCustomCommand()
{
    m_customCommands.append({QString(), true});
    saveConfig();
    emit customCommandsChanged();
}

void AppController::updateCustomCommand(int index, const QString &command)
{
    if (index < 0 || index >= m_customCommands.size() || m_customCommands[index].command == command) {
        return;
    }
    m_customCommands[index].command = command;
    saveConfig();
    emit customCommandsChanged();
}

void AppController::setCustomCommandEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_customCommands.size() || m_customCommands[index].enabled == enabled) {
        return;
    }
    m_customCommands[index].enabled = enabled;
    saveConfig();
    emit customCommandsChanged();
}

void AppController::removeCustomCommand(int index)
{
    if (index < 0 || index >= m_customCommands.size()) {
        return;
    }
    m_customCommands.removeAt(index);
    saveConfig();
    emit customCommandsChanged();
}

void AppController::moveCustomCommand(int from, int to)
{
    const int size = m_customCommands.size();
    if (from < 0 || from >= size || to < 0 || to >= size || from == to) {
        return;
    }
    const CustomCommand command = m_customCommands.takeAt(from);
    m_customCommands.insert(to, command);
    saveConfig();
    emit customCommandsChanged();
}

QString AppController::expandPlaceholders(const QString &command) const
{
    QString image = m_selectedWallpaper;
    QStringList colors;
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        // For videos the "image" is the cached first frame (what Plasma
        // shows as the fallback); the entry is keyed by the original path.
        if (WallpaperModel::isVideo(image)) {
            image = info->firstFrameFor(image);
        }
        colors = info->sourceColorsFor(m_selectedWallpaper);
    }
    if (image.isEmpty()) {
        image = m_selectedWallpaper;
    }
    const QString scolor = (m_selectedSourceColorIndex >= 0 && m_selectedSourceColorIndex < colors.size())
        ? colors.at(m_selectedSourceColorIndex)
        : QString();
    QString result = command;
    result.replace(QStringLiteral("%path%"), QStringLiteral("\"%1\"").arg(m_selectedWallpaper));
    result.replace(QStringLiteral("%image%"), QStringLiteral("\"%1\"").arg(image));
    result.replace(QStringLiteral("%ccolor%"), m_customColor);
    result.replace(QStringLiteral("%mode%"), m_themeMode);
    result.replace(QStringLiteral("%scolor%"), scolor);
    result.replace(QStringLiteral("%sc%"), QString::number(m_selectedSourceColorIndex));
    return result;
}

void AppController::runCustomCommands()
{
    QStringList parts;
    for (const CustomCommand &command : m_customCommands) {
        if (!command.enabled) {
            continue;
        }
        const QString expanded = expandPlaceholders(command.command).trimmed();
        if (!expanded.isEmpty()) {
            parts << expanded;
        }
    }
    if (parts.isEmpty()) {
        return;
    }
    // One shell chain, started detached (fire and forget).
    if (!QProcess::startDetached(QStringLiteral("sh"),
                                 {QStringLiteral("-c"), parts.join(QStringLiteral(" && "))})) {
        qWarning("Failed to launch the custom commands");
    }
}

void AppController::loadConfig()
{
    const KConfigGroup group = m_config->group(QStringLiteral("Wallpapers"));
    m_folders = group.readEntry("folders", QStringList());
    if (m_folders.isEmpty()) {
        // standardLocations() resolves the user's localized Pictures dir
        // ("Imágenes", "Pictures", ...) without creating it.
        const QStringList candidates = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        for (const QString &dir : candidates) {
            if (QDir(dir).exists()) {
                m_folders.append(dir);
                break;
            }
        }
    }
    m_currentFolderIndex = group.readEntry("currentFolderIndex", 0);
    if (m_currentFolderIndex >= m_folders.size()) {
        m_currentFolderIndex = 0;
    }
    m_currentWallpaper = group.readEntry("currentWallpaper", QString());
    m_lockscreen = group.readEntry("lockscreen", false);
    m_resizeEnabled = group.readEntry("resizeEnabled", false);
    m_resizeHeight = qBound(kResizeHeightMin,
                            group.readEntry("resizeHeight", kResizeHeightDefault),
                            kResizeHeightMax);
    m_resizeFps = qBound(0, group.readEntry("resizeFps", 0), kResizeFpsMax);
    const QByteArray sourceColorData = group.readEntry("sourceColorSelections", QByteArray());
    if (!sourceColorData.isEmpty()) {
        const QJsonDocument parsed = QJsonDocument::fromJson(sourceColorData);
        if (parsed.isObject()) {
            const QJsonObject obj = parsed.object();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                const int index = it.value().toInt(-1);
                if (index >= 0) {
                    m_sourceColorSelections.insert(it.key(), index);
                }
            }
        }
    }

    // Both color features are on by default when matugen is installed; the
    // keys are absent on first run. Extraction runs while either is on.
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        m_matugenDots = group.readEntry("matugenDots", info->matugenAvailable());
        m_colorCategories = group.readEntry("colorCategories", true);
        info->setMatugenEnabled(m_matugenDots);
    }

    const KConfigGroup commandsGroup = m_config->group(QStringLiteral("Commands"));
    m_themeMode = commandsGroup.readEntry("themeMode", QStringLiteral("dark"));
    m_customColor = commandsGroup.readEntry("customColor", QString());
    const QByteArray commandsData = commandsGroup.readEntry("commands", QByteArray());
    if (!commandsData.isEmpty()) {
        const QJsonDocument parsed = QJsonDocument::fromJson(commandsData);
        if (parsed.isArray()) {
            for (const QJsonValue &value : parsed.array()) {
                const QJsonObject obj = value.toObject();
                m_customCommands.append({obj.value("command").toString(),
                                         obj.value("enabled").toBool(true)});
            }
        }
    }

    const KConfigGroup transitionsGroup = m_config->group(QStringLiteral("Transitions"));
    m_transitionEnabled = transitionsGroup.readEntry("transitionEnabled", false);
    m_transitionLayer = transitionsGroup.readEntry("transitionLayer", QStringLiteral("bottom"));
    m_transitionType = transitionsGroup.readEntry("transitionType", QStringLiteral("grow"));
    m_transitionAdvanced = transitionsGroup.readEntry("transitionAdvanced", false);
    m_transitionDuration = transitionsGroup.readEntry("transitionDuration", 0.9);
    m_transitionFps = qBound(0, transitionsGroup.readEntry("transitionFps", 0), 240);
    m_transitionStep = qBound(1, transitionsGroup.readEntry("transitionStep", 2), 255);
    m_transitionAngle = transitionsGroup.readEntry("transitionAngle", 45.0);
    m_transitionPos = transitionsGroup.readEntry("transitionPos", QStringLiteral("center"));
    m_transitionInvertY = transitionsGroup.readEntry("transitionInvertY", false);
    m_transitionBezier = transitionsGroup.readEntry("transitionBezier", QStringLiteral(".54,0,.34,.99"));
    m_transitionWaveWidth = transitionsGroup.readEntry("transitionWaveWidth", 20.0);
    m_transitionWaveHeight = transitionsGroup.readEntry("transitionWaveHeight", 20.0);

    const KConfigGroup generalGroup = m_config->group(QStringLiteral("General"));
    m_defaultWindowWidth = qBound(kMinimumWindowWidth,
                                  generalGroup.readEntry("windowWidth", kDefaultWindowWidth),
                                  kMaximumWindowWidth);
    m_defaultWindowHeight = qBound(kMinimumWindowHeight,
                                   generalGroup.readEntry("windowHeight", kDefaultWindowHeight),
                                   kMaximumWindowHeight);
    m_enableTransparency = generalGroup.readEntry("transparency", true);
    m_transparencyLevel = qBound(kTransparencyLevelMin,
                                generalGroup.readEntry("transparencyLevel", kTransparencyLevelDefault),
                                kTransparencyLevelMax);

    // The resize settings no longer drive auto-generation; rebuild the
    // video-folder picker from the loaded folders.
    recomputeVideoFolders();
}

void AppController::saveConfig()
{
    KConfigGroup group = m_config->group(QStringLiteral("Wallpapers"));
    group.writeEntry("folders", m_folders);
    group.writeEntry("currentFolderIndex", m_currentFolderIndex);
    group.writeEntry("currentWallpaper", m_currentWallpaper);
    group.writeEntry("matugenDots", m_matugenDots);
    group.writeEntry("colorCategories", m_colorCategories);
    group.writeEntry("lockscreen", m_lockscreen);
    group.writeEntry("resizeEnabled", m_resizeEnabled);
    group.writeEntry("resizeHeight", m_resizeHeight);
    group.writeEntry("resizeFps", m_resizeFps);
    QJsonObject sourceColorObj;
    for (auto it = m_sourceColorSelections.constBegin(); it != m_sourceColorSelections.constEnd(); ++it) {
        sourceColorObj.insert(it.key(), it.value());
    }
    group.writeEntry("sourceColorSelections",
                     QString::fromUtf8(QJsonDocument(sourceColorObj).toJson(QJsonDocument::Compact)));
    KConfigGroup transitionsGroup = m_config->group(QStringLiteral("Transitions"));
    transitionsGroup.writeEntry("transitionEnabled", m_transitionEnabled);
    transitionsGroup.writeEntry("transitionLayer", m_transitionLayer);
    transitionsGroup.writeEntry("transitionType", m_transitionType);
    transitionsGroup.writeEntry("transitionAdvanced", m_transitionAdvanced);
    transitionsGroup.writeEntry("transitionDuration", m_transitionDuration);
    transitionsGroup.writeEntry("transitionFps", m_transitionFps);
    transitionsGroup.writeEntry("transitionStep", m_transitionStep);
    transitionsGroup.writeEntry("transitionAngle", m_transitionAngle);
    transitionsGroup.writeEntry("transitionPos", m_transitionPos);
    transitionsGroup.writeEntry("transitionInvertY", m_transitionInvertY);
    transitionsGroup.writeEntry("transitionBezier", m_transitionBezier);
    transitionsGroup.writeEntry("transitionWaveWidth", m_transitionWaveWidth);
    transitionsGroup.writeEntry("transitionWaveHeight", m_transitionWaveHeight);
    KConfigGroup generalGroup = m_config->group(QStringLiteral("General"));
    generalGroup.writeEntry("windowWidth", m_defaultWindowWidth);
    generalGroup.writeEntry("windowHeight", m_defaultWindowHeight);
    generalGroup.writeEntry("transparency", m_enableTransparency);
    generalGroup.writeEntry("transparencyLevel", m_transparencyLevel);
    KConfigGroup commandsGroup = m_config->group(QStringLiteral("Commands"));
    commandsGroup.writeEntry("themeMode", m_themeMode);
    commandsGroup.writeEntry("customColor", m_customColor);
    QJsonArray commandsArray;
    for (const CustomCommand &command : m_customCommands) {
        QJsonObject obj;
        obj[QStringLiteral("command")] = command.command;
        obj[QStringLiteral("enabled")] = command.enabled;
        commandsArray.append(obj);
    }
    commandsGroup.writeEntry("commands",
                             QString::fromUtf8(QJsonDocument(commandsArray).toJson(QJsonDocument::Compact)));
    m_config->sync();
}

void AppController::applyCurrentFolder()
{
    QString directory;
    if (m_currentFolderIndex >= 0 && m_currentFolderIndex < m_folders.size()) {
        directory = m_folders.at(m_currentFolderIndex);
    }
    m_wallpaperModel->setSourceDirectory(directory);
}

bool AppController::lockscreen() const
{
    return m_lockscreen;
}

void AppController::setLockscreen(bool enabled)
{
    if (m_lockscreen == enabled) {
        return;
    }
    m_lockscreen = enabled;
    saveConfig();
    emit lockscreenChanged();
}

bool AppController::resizeEnabled() const
{
    return m_resizeEnabled;
}

void AppController::setResizeEnabled(bool enabled)
{
    if (m_resizeEnabled == enabled) {
        return;
    }
    m_resizeEnabled = enabled;
    saveConfig();
    emit resizeEnabledChanged();
}

int AppController::resizeHeight() const
{
    return m_resizeHeight;
}

void AppController::setResizeHeight(int height)
{
    height = qBound(kResizeHeightMin, height, kResizeHeightMax);
    if (m_resizeHeight == height) {
        return;
    }
    m_resizeHeight = height;
    saveConfig();
    emit resizeHeightChanged();
}

int AppController::resizeFps() const
{
    return m_resizeFps;
}

void AppController::setResizeFps(int fps)
{
    fps = qBound(0, fps, kResizeFpsMax);
    if (m_resizeFps == fps) {
        return;
    }
    m_resizeFps = fps;
    saveConfig();
    emit resizeFpsChanged();
}

QStringList AppController::videoFolders() const
{
    return m_videoFolders;
}

void AppController::recomputeVideoFolders()
{
    QStringList out;
    for (const QString &folder : m_folders) {
        if (!videoFilesIn(folder).isEmpty()) {
            out.append(folder);
        }
    }
    if (m_videoFolders == out) {
        return;
    }
    m_videoFolders = std::move(out);
    emit videoFoldersChanged();
}

QStringList AppController::videoFilesIn(const QString &folder)
{
    QDir dir(folder);
    const QFileInfoList entries =
        dir.entryInfoList(WallpaperModel::videoNameFilters(), QDir::Files);
    QStringList paths;
    paths.reserve(entries.size());
    for (const QFileInfo &info : entries) {
        paths.append(info.absoluteFilePath());
    }
    return paths;
}

void AppController::generateVideoResizes(const QString &folder)
{
    WallpapersInfo *info = WallpapersInfo::instance();
    if (info == nullptr) {
        return;
    }
    const QStringList videos = videoFilesIn(folder);
    if (videos.isEmpty()) {
        return;
    }
    info->setResizeParameters(m_resizeHeight, m_resizeFps);
    info->beginResizeBatch(videos.size());
    for (const QString &path : videos) {
        info->planResize(path);
    }
}

bool AppController::transitionEnabled() const
{
    return m_transitionEnabled;
}

void AppController::setTransitionEnabled(bool enabled)
{
    if (m_transitionEnabled == enabled) {
        return;
    }
    m_transitionEnabled = enabled;
    saveConfig();
    emit transitionEnabledChanged();
}

QString AppController::transitionLayer() const
{
    return m_transitionLayer;
}

void AppController::setTransitionLayer(const QString &layer)
{
    if (m_transitionLayer == layer) {
        return;
    }
    m_transitionLayer = layer;
    saveConfig();
    emit transitionLayerChanged();
}

QStringList AppController::transitionLayers() const
{
    return {QStringLiteral("background"), QStringLiteral("bottom"), QStringLiteral("top"),
            QStringLiteral("overlay")};
}

QStringList AppController::transitionTypes() const
{
    return {QStringLiteral("grow"), QStringLiteral("fade"), QStringLiteral("wipe"),
            QStringLiteral("wave"), QStringLiteral("left"), QStringLiteral("right"),
            QStringLiteral("top"), QStringLiteral("bottom"), QStringLiteral("center"),
            QStringLiteral("outer"), QStringLiteral("simple"), QStringLiteral("any"),
            QStringLiteral("random")};
}

QString AppController::transitionType() const
{
    return m_transitionType;
}

void AppController::setTransitionType(const QString &type)
{
    if (m_transitionType == type) {
        return;
    }
    m_transitionType = type;
    saveConfig();
    emit transitionTypeChanged();
}

bool AppController::transitionAdvanced() const
{
    return m_transitionAdvanced;
}

void AppController::setTransitionAdvanced(bool enabled)
{
    if (m_transitionAdvanced == enabled) {
        return;
    }
    m_transitionAdvanced = enabled;
    saveConfig();
    emit transitionAdvancedChanged();
}

double AppController::transitionDuration() const
{
    return m_transitionDuration;
}

void AppController::setTransitionDuration(double duration)
{
    if (m_transitionDuration == duration) {
        return;
    }
    m_transitionDuration = duration;
    saveConfig();
    emit transitionDurationChanged();
}

int AppController::transitionFps() const
{
    return m_transitionFps;
}

void AppController::setTransitionFps(int fps)
{
    if (m_transitionFps == fps) {
        return;
    }
    m_transitionFps = fps;
    saveConfig();
    emit transitionFpsChanged();
}

int AppController::transitionStep() const
{
    return m_transitionStep;
}

void AppController::setTransitionStep(int step)
{
    if (m_transitionStep == step) {
        return;
    }
    m_transitionStep = step;
    saveConfig();
    emit transitionStepChanged();
}

double AppController::transitionAngle() const
{
    return m_transitionAngle;
}

void AppController::setTransitionAngle(double angle)
{
    if (m_transitionAngle == angle) {
        return;
    }
    m_transitionAngle = angle;
    emit transitionAngleChanged();
}

QString AppController::transitionPos() const
{
    return m_transitionPos;
}

void AppController::setTransitionPos(const QString &pos)
{
    if (m_transitionPos == pos) {
        return;
    }
    m_transitionPos = pos;
    saveConfig();
    emit transitionPosChanged();
}

bool AppController::transitionInvertY() const
{
    return m_transitionInvertY;
}

void AppController::setTransitionInvertY(bool invertY)
{
    if (m_transitionInvertY == invertY) {
        return;
    }
    m_transitionInvertY = invertY;
    saveConfig();
    emit transitionInvertYChanged();
}

QString AppController::transitionBezier() const
{
    return m_transitionBezier;
}

void AppController::setTransitionBezier(const QString &bezier)
{
    if (m_transitionBezier == bezier) {
        return;
    }
    m_transitionBezier = bezier;
    saveConfig();
    emit transitionBezierChanged();
}

double AppController::transitionWaveWidth() const
{
    return m_transitionWaveWidth;
}

void AppController::setTransitionWaveWidth(double width)
{
    if (m_transitionWaveWidth == width) {
        return;
    }
    m_transitionWaveWidth = width;
    saveConfig();
    emit transitionWaveWidthChanged();
}

double AppController::transitionWaveHeight() const
{
    return m_transitionWaveHeight;
}

void AppController::setTransitionWaveHeight(double height)
{
    if (m_transitionWaveHeight == height) {
        return;
    }
    m_transitionWaveHeight = height;
    saveConfig();
    emit transitionWaveHeightChanged();
}
