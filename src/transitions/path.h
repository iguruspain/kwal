// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace transitions {

// Expands a leading `~`/`~/` to $HOME (a shell would not expand `~` inside quotes).
QString expandTilde(const QString &path);

// `%XX` -> byte; invalid sequences are kept verbatim.
QString percentDecode(const QString &s);

} // namespace transitions
