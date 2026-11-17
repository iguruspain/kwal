#include "tools/fileutil.h"

#include <QCryptographicHash>
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

    return renameAtomic(tmp.fileName(), path);
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
    QFile source(backup);
    if (!source.open(QIODevice::ReadOnly))
        return false;
    const QByteArray content = source.readAll();
    source.close();
    if (source.error() != QFileDevice::NoError)
        return false;

    // Restore through a temp file + atomic rename so a crash never leaves
    // the real file missing (a plain remove+copy would).
    const QFileInfo info(path);
    QTemporaryFile tmp;
    tmp.setFileTemplate(info.absoluteDir().filePath(
        QStringLiteral(".%1.restoring-XXXXXX").arg(info.fileName())));
    if (!tmp.open())
        return false;
    if (tmp.write(content) != content.size())
        return false;
    tmp.flush();
    if (tmp.error() != QFileDevice::NoError)
        return false;
    tmp.close();
    return renameAtomic(tmp.fileName(), path);
}

QString contentFingerprint(const QStringList &paths)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const QString &path : paths) {
        // The path is hashed too (with the file contents after it), so two
        // different file sets can never collide by shuffling bytes around.
        hash.addData(path.toUtf8());
        hash.addData(QByteArray(1, '\0'));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            hash.addData(QByteArrayLiteral("<absent>"));
            continue;
        }
        hash.addData(file.readAll());
    }
    return QString::fromLatin1(hash.result().toHex());
}

}
