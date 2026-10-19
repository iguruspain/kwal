#pragma once

#include <QString>

// Textual JSONC handling of ~/.config/fastfetch/config.jsonc. JSONC has no
// first-party Qt parser and the edit is a single-key replacement, so the old
// app's regex approach is kept: it preserves comments and unrelated content.
namespace tools
{

// ~/.config/fastfetch/config.jsonc (XDG_CONFIG_HOME aware).
QString fastfetchConfigPath();

// Path of the image currently referenced by the config (first existing one
// among the config_image/image/source/logo/icon keys), the fallback
// chica-tinted.png when set, or an empty string.
QString detectConfigImage();

// Replaces the `source` key value with the given path (stored as ~/-relative
// when it lives under the home directory). Returns false when the config is
// missing or has no source key.
bool setImageSource(const QString &newSourcePath);

// Whether config.jsonc.bak exists.
bool hasConfigBackup();

// Restores config.jsonc from its .bak backup.
bool restoreConfigBackup();

// Removes the fastfetch cache directory (XDG_CACHE_HOME/fastfetch).
void clearCache();

}
