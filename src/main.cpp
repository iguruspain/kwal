#include <QApplication>
#include <QProcessEnvironment>
#include <QMessageLogger>
extern "C" {
#include <libavutil/avutil.h>
} // extern "C": ffmpeg headers lack C++ guards
#include <QtWebEngineQuick>
#include "aboutinfo.h"
#include "fdothumbnailprovider.h"
#include "wallpapersinfo.h"
#include <QQmlApplicationEngine>
#include <cstdio>

// The system Qt build misroutes default logging; ensure QML warnings reach stderr.
static void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    fprintf(stderr, "%s\n", msg.toUtf8().constData());
}

// KDE only; never overrides explicit user settings.
static void applyDesktopIntegration()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.value(QStringLiteral("XDG_CURRENT_DESKTOP")).startsWith(QStringLiteral("KDE")))
        return;
    if (env.value(QStringLiteral("QT_QPA_PLATFORMTHEME")).isEmpty())
        qputenv("QT_QPA_PLATFORMTHEME", "kde");
    if (env.value(QStringLiteral("QT_QUICK_CONTROLS_STYLE")).isEmpty())
        qputenv("QT_QUICK_CONTROLS_STYLE", "org.kde.desktop");
}

int main(int argc, char *argv[])
{
    applyDesktopIntegration();

    // libavutil's default callback prints MediaPlayer probe dumps to stderr.
    av_log_set_level(AV_LOG_ERROR);

    // Chromium software path: avoids "GBM not supported" freezes; never overrides an explicit setting.
    const QProcessEnvironment webEnv = QProcessEnvironment::systemEnvironment();
    if (webEnv.value(QStringLiteral("QTWEBENGINE_CHROMIUM_FLAGS")).isEmpty())
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");

    // Must run before the application object is created.
    QtWebEngineQuick::initialize();
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KDE"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setApplicationName(QStringLiteral("kwal"));
    app.setApplicationVersion(QStringLiteral(KWAL_VERSION));
    // must be the installed org.kde.kwal.desktop base name (icon/name, StartupNotify).
    app.setDesktopFileName(QStringLiteral("org.kde.kwal"));
    // WebEngine's initialize() installs its own handler that swallows QML
    // warnings; re-install ours after the application object exists.
    qInstallMessageHandler(messageHandler);
    WallpapersInfo::initialize(&app);
    AboutInfo::initialize(&app);
    QQmlApplicationEngine engine;
    engine.setExternalSingletonInstance(QStringLiteral("org.kde.kwal"),
                                        QStringLiteral("WallpapersInfo"),
                                        WallpapersInfo::instance());
    engine.setExternalSingletonInstance(QStringLiteral("org.kde.kwal"),
                                        QStringLiteral("AboutInfo"),
                                        AboutInfo::instance());
    engine.addImageProvider(QStringLiteral("fdo_thumbnail"), new FdoThumbnailProvider());
    engine.loadFromModule(QStringLiteral("org.kde.kwal"), QStringLiteral("Main"));

    return app.exec();
}
