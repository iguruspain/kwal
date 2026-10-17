#include <QApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KDE"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setApplicationName(QStringLiteral("kwal"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("org.kde.kwal"), QStringLiteral("Main"));

    return app.exec();
}
