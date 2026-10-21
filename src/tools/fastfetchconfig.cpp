#include "tools/fastfetchconfig.h"

#include "tools/fileutil.h"

#include <QDir>
#include <QFile>
#include <QString>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace tools
{

QString fastfetchConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/fastfetch/config.jsonc");
}

namespace {

QString readStringKey(const QString &path, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    const QString raw = QString::fromUtf8(file.readAll());

    // Quoted key, then unquoted JSONC key.
    const QRegularExpression quoted(QStringLiteral("\\\"%1\\\"\\s*:\\s*\"([^\"]*)\"").arg(QRegularExpression::escape(key)));
    const QRegularExpression unquoted(QStringLiteral("^[ \\t]*%1[ \\t]*:[ \\t]*\"([^\"]*)\"").arg(QRegularExpression::escape(key)));
    auto match = quoted.match(raw);
    if (match.hasMatch())
        return match.captured(1);
    match = unquoted.match(raw);
    if (match.hasMatch())
        return match.captured(1);
    return QString();
}

}

QString detectConfigImage()
{
    const QString cfg = fastfetchConfigPath();

    for (const QString &key : {QStringLiteral("config_image"), QStringLiteral("image"),
             QStringLiteral("source"), QStringLiteral("logo"), QStringLiteral("icon")}) {
        QString value = readStringKey(cfg, key);
        if (value.isEmpty())
            continue;
        value = QDir::fromNativeSeparators(value);
        if (value.startsWith(QLatin1Char('~')))
            value = QDir::homePath() + value.mid(1);
        if (QFileInfo::exists(value))
            return QFileInfo(value).absoluteFilePath();
    }

    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/fastfetch/chica-tinted.png");
    if (QFileInfo::exists(fallback))
        return fallback;
    return QString();
}

bool setImageSource(const QString &newSourcePath)
{
    const QString cfg = fastfetchConfigPath();
    QFile file(cfg);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString raw = QString::fromUtf8(file.readAll());

    const QFileInfo srcInfo(newSourcePath);
    QString value;
    if (srcInfo.isAbsolute() && srcInfo.filePath().startsWith(QDir::homePath() + QLatin1Char('/')))
        value = QStringLiteral("~/") + srcInfo.filePath().mid(QDir::homePath().size() + 1);
    else
        value = srcInfo.absoluteFilePath();
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));

    const QRegularExpression quoted(QStringLiteral("\"source\"\\s*:\\s*(\"[^\"]*\"|'[^']*')"));
    auto match = quoted.match(raw);
    if (match.hasMatch()) {
        raw.replace(match.capturedStart(), match.capturedLength(), QStringLiteral("\"source\": \"%1\"").arg(value));
        return writeFileAtomic(cfg, raw);
    }

    // Unquoted JSONC key at line start.
    const QRegularExpression unquoted(QStringLiteral("^[ \\t]*source[ \\t]*:[ \\t]*(\"[^\"]*\"|'[^']*'|[^,\\n]*),?[ \\t]*$"),
        QRegularExpression::MultilineOption);
    match = unquoted.match(raw);
    if (match.hasMatch()) {
        const bool hadComma = match.captured(0).contains(QLatin1Char(','));
        raw.replace(match.capturedStart(), match.capturedLength(),
            QStringLiteral("source: \"%1\"%2").arg(value, hadComma ? QStringLiteral(",") : QString()));
        return writeFileAtomic(cfg, raw);
    }

    return false;
}

bool hasConfigBackup()
{
    return hasBackup(fastfetchConfigPath());
}

bool restoreConfigBackup()
{
    return restoreBackup(fastfetchConfigPath());
}

void clearCache()
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/fastfetch");
    QDir(cacheDir).removeRecursively();
}

}
