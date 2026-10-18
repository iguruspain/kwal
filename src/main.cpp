#include <QApplication>
#include <QProcessEnvironment>
#include <QMessageLogger>
#include "fdothumbnailprovider.h"
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
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KDE"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setApplicationName(QStringLiteral("kwal"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("fdo_thumbnail"), new FdoThumbnailProvider());
    engine.loadFromModule(QStringLiteral("org.kde.kwal"), QStringLiteral("Main"));

    return app.exec();
}
