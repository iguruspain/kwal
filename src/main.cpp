#include <QApplication>
#include <QMessageLogger>
#include "fdothumbnailprovider.h"
#include <QQmlApplicationEngine>
#include <cstdio>

// The system Qt build misroutes default logging; ensure QML warnings reach stderr.
static void messageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    fprintf(stderr, "%s\n", msg.toUtf8().constData());
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);
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
