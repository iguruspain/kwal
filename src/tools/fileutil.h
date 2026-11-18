#pragma once

#include <QString>
#include <QStringList>

#include <filesystem>
#include <system_error>

// Atomic writes and .bak backups next to the original file.
namespace tools
{

// rename(2) is a single-step swap but needs source and target on the same
// filesystem; QFile::rename refuses to replace an existing target.
inline bool renameAtomic(const QString &source, const QString &target)
{
    std::error_code ec;
    std::filesystem::rename(source.toStdString(), target.toStdString(), ec);
    return !ec;
}

// Writes via temp file in the same directory + rename (rename needs a same-fs target).
bool writeFileAtomic(const QString &path, const QString &content);

// Refreshes path.bak so it always holds the state right before the modification;
// empty string when path is missing or the copy failed.
QString backupFile(const QString &path);

bool hasBackup(const QString &path);

bool restoreBackup(const QString &path);

// SHA-1 over the given files' contents; missing files hash a marker so
// creation/deletion counts too. External rewrites change the value; the
// caller re-arms after its own writes.
QString contentFingerprint(const QStringList &paths);

}
