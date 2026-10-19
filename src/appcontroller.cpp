#include "appcontroller.h"
#include "wallpapermodel.h"
#include <KConfig>
#include <KConfigGroup>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QUrl>

namespace {
// Plasma shell script that applies an image wallpaper to all current desktops.
QString wallpaperScript(const QString &filePath)
{
    QString escaped = filePath;
    escaped.replace(QStringLiteral("'"), QStringLiteral("\\'"));
    return QStringLiteral("var allDesktops = desktops();"
                          "for (var i = 0; i < allDesktops.length; i++) {"
                          "  var d = allDesktops[i];"
                          "  d.wallpaperPlugin = 'org.kde.image';"
                          "  d.currentConfigGroup = Array('Wallpaper', 'org.kde.image', 'General');"
                          "  d.writeConfig('Image', 'file://%1');"
                          "}")
        .arg(escaped);
}

// "WxH" string for a readable image, empty if it cannot be determined.
QString imageResolution(const QString &filePath)
{
    const QSize size = QImageReader(filePath).size();
    return (size.width() > 0 && size.height() > 0)
        ? QStringLiteral("%1×%2").arg(size.width()).arg(size.height())
        : QString();
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_wallpaperModel(new WallpaperModel(this))
    , m_config(new KConfig(QStringLiteral("kwalrc"), KConfig::SimpleConfig))
{
    loadConfig();
    applyCurrentFolder();
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

QString AppController::currentWallpaperSize() const
{
    return m_currentWallpaperSize;
}

QString AppController::selectedWallpaper() const
{
    return m_selectedWallpaper;
}

QString AppController::selectedWallpaperSize() const
{
    return m_selectedWallpaperSize;
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
    emit enableTransparencyChanged();
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
    m_selectedWallpaperSize = filePath.isEmpty() ? QString() : imageResolution(filePath);
    emit selectedWallpaperChanged();
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
    QDBusInterface iface(QStringLiteral("org.kde.plasmashell"),
                               QStringLiteral("/PlasmaShell"),
                               QStringLiteral("org.kde.PlasmaShell"),
                               QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        emit wallpaperSetResult(false, QStringLiteral("Plasma Shell is not running."));
        return;
    }

    m_wallpaperBusy = true;
    emit wallpaperBusyChanged();

    QDBusPendingReply<> reply = iface.asyncCall(QStringLiteral("evaluateScript"), wallpaperScript(filePath));
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, filePath](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        m_wallpaperBusy = false;
        emit wallpaperBusyChanged();
        const QDBusPendingReply<> r = *w;
        if (r.isError()) {
            emit wallpaperSetResult(false, r.error().message());
        } else {
            setCurrentWallpaper(filePath);
            emit wallpaperSetResult(true, QStringLiteral("Wallpaper set."));
        }
    });
}

void AppController::setCurrentWallpaper(const QString &filePath)
{
    m_currentWallpaper = filePath;
    m_currentWallpaperSize = imageResolution(filePath);
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
    m_currentWallpaperSize = group.readEntry("currentWallpaperSize", QString());
}

void AppController::saveConfig()
{
    KConfigGroup group = m_config->group(QStringLiteral("Wallpapers"));
    group.writeEntry("folders", m_folders);
    group.writeEntry("currentFolderIndex", m_currentFolderIndex);
    group.writeEntry("currentWallpaper", m_currentWallpaper);
    group.writeEntry("currentWallpaperSize", m_currentWallpaperSize);
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
