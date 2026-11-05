#include <QApplication>
#include <QProcessEnvironment>
#include <QMessageLogger>
extern "C" {
#include <libavutil/avutil.h>
} // extern "C": the ffmpeg headers ship without C++ guards
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

// Let Qt integrate with the running KDE desktop instead of falling back to
// built-in defaults: query the desktop for the user's widget style and color
// scheme, and render QML controls as native Plasma widgets. Never overrides
// explicit user settings; inactive on non-KDE desktops.
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

    // Qt Multimedia's FFmpeg backend leaves libavutil's default log callback
    // in place, so every MediaPlayer prints its raw "Input #0, ..." probe
    // dump straight to stderr. Silence everything below real errors.
    av_log_set_level(AV_LOG_ERROR);

    // WebEngine preview pages: keep Chromium on the software path to avoid
    // "GBM not supported" warnings and freezes on systems without working
    // GPU acceleration (same behavior as the original app). Never overrides
    // an explicit user setting.
    const QProcessEnvironment webEnv = QProcessEnvironment::systemEnvironment();
    if (webEnv.value(QStringLiteral("QTWEBENGINE_CHROMIUM_FLAGS")).isEmpty())
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");

    // Must run before the application object is created.
    QtWebEngineQuick::initialize();
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KDE"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setApplicationName(QStringLiteral("kwal"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    // Match the running window to the installed desktop entry (app id
    // org.kde.kwal) so Plasma shows the right icon/name and StartupNotify
    // works. Must be the installed org.kde.kwal.desktop base name.
    app.setDesktopFileName(QStringLiteral("org.kde.kwal"));
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
