#include "appcontroller.h"
#include "templateinstaller.h"
#include "paletteextractor.h"
#include "wallpapermodel.h"
#include "wallpapersinfo.h"
#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QRunnable>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

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

// Runs PlasmaApply off the main thread (D-Bus round trips + config edits).
class WallpaperApplyJob : public QRunnable
{
public:
    WallpaperApplyJob(PlasmaApply *apply, QString type, QString path, bool lockscreen)
        : m_apply(apply)
        , m_type(std::move(type))
        , m_path(std::move(path))
        , m_lockscreen(lockscreen)
    {
    }

    void run() override
    {
        m_apply->applyWallpaper(m_type, m_path, m_lockscreen);
    }

private:
    PlasmaApply *m_apply;
    QString m_type;
    QString m_path;
    bool m_lockscreen;
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
        m_wallpaperBusy = false;
        emit wallpaperBusyChanged();
        if (allOk) {
            setCurrentWallpaper(m_selectedWallpaper);
        }
        emit wallpaperSetResult(allOk, message);
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
    emit selectedSourceColorIndexChanged();
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
    if (m_selectedSourceColorIndex != -1) {
        m_selectedSourceColorIndex = -1;
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
    QThreadPool::globalInstance()->start(new WallpaperApplyJob(m_plasmaApply,
                                                               video ? QStringLiteral("video") : QStringLiteral("image"),
                                                               target,
                                                               m_lockscreen));
}

void AppController::setCurrentWallpaper(const QString &filePath)
{
    m_currentWallpaper = filePath;
    saveConfig();
    emit currentWallpaperChanged();
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

    // Both color features are on by default when matugen is installed; the
    // keys are absent on first run. Extraction runs while either is on.
    if (WallpapersInfo *info = WallpapersInfo::instance()) {
        m_matugenDots = group.readEntry("matugenDots", info->matugenAvailable());
        m_colorCategories = group.readEntry("colorCategories", true);
        info->setMatugenEnabled(m_matugenDots);
    }

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
    KConfigGroup generalGroup = m_config->group(QStringLiteral("General"));
    generalGroup.writeEntry("windowWidth", m_defaultWindowWidth);
    generalGroup.writeEntry("windowHeight", m_defaultWindowHeight);
    generalGroup.writeEntry("transparency", m_enableTransparency);
    generalGroup.writeEntry("transparencyLevel", m_transparencyLevel);
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
