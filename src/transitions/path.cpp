// SPDX-FileCopyrightText: 2026 KDE
// SPDX-License-Identifier: MIT
#include "path.h"

#include <QByteArray>

#include <cstddef>

namespace transitions
{

QString expandTilde(const QString &path)
{
    QString rest;
    bool tilde = false;
    if (path == QLatin1String("~")) {
        tilde = true;
    } else if (path.startsWith(QLatin1String("~/"))) {
        rest = path.mid(2);
        tilde = true;
    }
    if (!tilde)
        return path;
    const QByteArray home = qgetenv("HOME");
    if (home.isEmpty())
        return path;
    if (rest.isEmpty())
        return QString::fromUtf8(home);
    QString h = QString::fromUtf8(home);
    while (h.endsWith(QLatin1Char('/')))
        h.chop(1);
    return h + QLatin1Char('/') + rest;
}

QString percentDecode(const QString &s)
{
    const QByteArray in = s.toUtf8();
    QByteArray out;
    out.reserve(in.size());
    auto hex = [](char c) -> int {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= '0' && u <= '9')
            return u - '0';
        if (u >= 'a' && u <= 'f')
            return u - 'a' + 10;
        if (u >= 'A' && u <= 'F')
            return u - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            const int h = hex(in[i + 1]);
            const int l = hex(in[i + 2]);
            if (h >= 0 && l >= 0) {
                out.append(static_cast<char>((h << 4) | l));
                i += 2;
                continue;
            }
        }
        out.append(in[i]);
    }
    return QString::fromUtf8(out);
}

}
