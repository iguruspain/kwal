#include "tools/fileutil.h"

#include <QDir>
#include <QFile>
#include <QTemporaryFile>

namespace tools
{

bool writeFileAtomic(const QString &path, const QString &content)
{
    const QFileInfo info(path);
    const QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;

    const QByteArray utf8 = content.toUtf8();
    QTemporaryFile tmp;
    tmp.setFileTemplate(dir.absolutePath() + QStringLiteral("/.") + info.fileName() + QStringLiteral(".XXXXXX"));
    if (!tmp.open())
        return false;
    if (tmp.write(utf8) != utf8.size())
        return false;
    tmp.flush();
    if (tmp.error() != QFileDevice::NoError)
        return false;
    tmp.close();

    QFile::remove(path);
    return QFile::rename(tmp.fileName(), path);
}

QString backupFile(const QString &path)
{
    const QString backup = path + QStringLiteral(".bak");
    if (!QFileInfo::exists(path))
        return QString();

    QFile::remove(backup);
    return QFile::copy(path, backup) ? backup : QString();
}

bool hasBackup(const QString &path)
{
    return QFileInfo::exists(path + QStringLiteral(".bak"));
}

bool restoreBackup(const QString &path)
{
    const QString backup = path + QStringLiteral(".bak");
    if (!QFileInfo::exists(backup))
        return false;

    QFile::remove(path);
    return QFile::copy(backup, path);
}

}
