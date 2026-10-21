#pragma once

#include <QString>

// Plain file helpers shared by the tool integrations (fastfetch, starship,
// ulauncher): atomic writes and .bak backups next to the original file.
namespace tools
{

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
