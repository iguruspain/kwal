// SPDX-FileCopyrightText: 2026 KDE
// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace transitions
{

// `~` and `~/...` at the start of the path expand to $HOME (the rest is
// untouched); anything else and a missing/empty HOME pass through as-is.
QString expandTilde(const QString &path);

// `%XX` becomes the byte 0xXX; invalid sequences are kept verbatim
// (percentDecode is Rust percent-decoding).
QString percentDecode(const QString &s);

}
