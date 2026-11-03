#pragma once

#include <QByteArray>
#include <QString>

namespace colibry::fv {

enum class Kind {
    Text,      ///< shown as text
    Image,     ///< a picture Qt can decode
    Binary,    ///< anything else: shown as a hex dump
    Directory, ///< a folder: cannot be viewed
    Special,   ///< device, pipe or socket: reading could block, so it is refused
    Missing,   ///< does not exist
};

class FileKind
{
public:
    static Kind detect(const QString &path);

    /// Decides from the first bytes of a file whether it is text.
    static bool looksLikeText(const QByteArray &sample);

    static constexpr int kSampleSize = 8192;
};

} // namespace colibry::fv
