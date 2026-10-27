// SPDX-FileCopyrightText: 2026 KDE
// SPDX-FileCopyrightText: 2023-2026 LGFae (awww, https://codeberg.org/LGFae/awww)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "path.h"

#include <QByteArray>


namespace transitions {

QString expandTilde(const QString &path)
{
    QString rest;
    bool hasRest = false;
    if (path == QLatin1String("~")) {
        hasRest = true;
    } else if (path.startsWith(QLatin1String("~/"))) {
        rest = path.mid(2);
        hasRest = true;
    }
    if (hasRest) {
        const QString home = qEnvironmentVariable("HOME");
        if (!home.isEmpty()) {
            return rest.isEmpty() ? home : home + QLatin1Char('/') + rest;
        }
    }
    return path;
}

QString percentDecode(const QString &s)
{
    const QByteArray b = s.toUtf8();
    auto hex = [](char c) -> int {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= '0' && uc <= '9') {
            return uc - '0';
        }
        if (uc >= 'a' && uc <= 'f') {
            return uc - 'a' + 10;
        }
        if (uc >= 'A' && uc <= 'F') {
            return uc - 'A' + 10;
        }
        return -1;
    };
    QByteArray out;
    out.reserve(b.size());
    for (size_t i = 0; i < b.size(); ++i) {
        if (b[i] == '%' && i + 2 < b.size()) {
            const int hi = hex(b[i + 1]);
            const int lo = hex(b[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.append(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.append(b[i]);
    }
    return QString::fromUtf8(out);
}

} // namespace transitions
