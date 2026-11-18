#pragma once

#include <QString>

// JSONC has no Qt parser: textual regex edits preserve comments and unrelated content.
namespace tools
{

// ~/.config/fastfetch/config.jsonc (XDG_CONFIG_HOME aware).
QString fastfetchConfigPath();

// First existing image among config_image/image/source/logo/icon, else chica-tinted.png, else empty.
QString detectConfigImage();

// Rewrites the `source` key (~/-relative when under home); false if config or source key missing.
bool setImageSource(const QString &newSourcePath);

// true when config.jsonc.bak exists.
bool hasConfigBackup();

bool restoreConfigBackup();

// Removes XDG_CACHE_HOME/fastfetch.
void clearCache();

}
