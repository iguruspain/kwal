#pragma once

#include <QString>

#include <filesystem>
#include <system_error>

// Plain file helpers shared by the tool integrations (fastfetch, starship,
// ulauncher): atomic writes and .bak backups next to the original file.
namespace tools
{

// Replaces target with source atomically via POSIX rename(2): on the same
// filesystem the target is swapped in a single step, so a crash between a
// remove and the rename can never leave the destination missing (unlike
// QFile::rename, which refuses to replace an existing target). source and
// target must live on the same filesystem. Returns false on failure.
inline bool renameAtomic(const QString &source, const QString &target)
{
    std::error_code ec;
    std::filesystem::rename(source.toStdString(), target.toStdString(), ec);
    return !ec;
}

// Writes content to path atomically (temp file in the same directory, then
// rename). Returns false on failure.
bool writeFileAtomic(const QString &path, const QString &content);

// Creates path.bak as a copy of the current path, refreshing it on every
// call so the backup always holds the state right before the next
// modification. Returns the backup path on success, or an empty string
// when path does not exist or the copy failed.
QString backupFile(const QString &path);

// Whether a .bak backup for path exists.
bool hasBackup(const QString &path);

// Copies path.bak back over path. Returns false when there is no backup.
bool restoreBackup(const QString &path);

}
