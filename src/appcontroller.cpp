#include "appcontroller.h"
#include "wallpapermodel.h"
#include <KConfig>
#include <KConfigGroup>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
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
    QImageReader reader(filePath);
    const QSize size = reader.size();
    m_currentWallpaperSize =
        (size.width() > 0 && size.height() > 0)
        ? QStringLiteral("%1×%2").arg(size.width()).arg(size.height())
        : QString();
    saveConfig();
    emit currentWallpaperChanged();
}

void AppController::loadConfig()
{
    const KConfigGroup group = m_config->group(QStringLiteral("Wallpapers"));
    m_folders = group.readEntry("folders", QStringList());
    qWarning("kwal: loaded %d folders: %s", m_folders.size(), qPrintable(m_folders.join(QStringLiteral(","))));
    if (m_folders.isEmpty()) {
        // Sensible first run: show the pictures folder if it exists.
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (!pictures.isEmpty() && QDir(pictures).exists()) {
            m_folders.append(pictures);
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
