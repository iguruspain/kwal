#include "templateinstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace tools
{

QString templateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwal/templates");
}

namespace {

void copyDir(const QString &src, const QString &dst)
{
    QDir dir(src);
    QDir().mkpath(dst);
    for (const QFileInfo &fi :
             dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (fi.isDir())
            copyDir(fi.filePath(), dst + QLatin1Char('/') + fi.fileName());
        else if (!QFileInfo::exists(dst + QLatin1Char('/') + fi.fileName())) {
            QFile src(fi.filePath());
            if (src.open(QIODevice::ReadOnly)) {
                QFile out(dst + QLatin1Char('/') + fi.fileName());
                if (out.open(QIODevice::WriteOnly))
                    out.write(src.readAll());
            }
        }
    }
}

}

void installTemplates()
{
    copyDir(QStringLiteral(":/templates"), templateDir());
}

}
