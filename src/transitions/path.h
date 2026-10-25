// Path helpers shared by the transition plane readers (port of the lib.rs
// helpers of the Rust reference).
#pragma once

#include <QString>

namespace transitions {

// Expands a leading `~` / `~/` to `$HOME`. Needed because a shell does not
// expand `~` inside quotes (`"~/Videos/a b.mp4"`).
QString expandTilde(const QString &path);

// `%XX` -> byte (invalid sequences are kept verbatim).
QString percentDecode(const QString &s);

} // namespace transitions
