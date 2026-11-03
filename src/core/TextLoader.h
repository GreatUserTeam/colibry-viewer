#pragma once

#include <QByteArray>
#include <QString>

namespace colibry::fv {

struct LoadedText
{
    bool ok = false;
    QString text;
    QString encoding;       ///< "UTF-8", "UTF-16LE", "UTF-16BE" or "ISO-8859-1"
    bool truncated = false; ///< only the first part of a large file was read
    qint64 fileSize = 0;
    QString error;
};

/// Reads a text file and works out its encoding. Never throws away data silently:
/// bytes that are not valid UTF-8 fall back to ISO-8859-1, and a file larger than
/// the limit is flagged as truncated.
class TextLoader
{
public:
    static constexpr qint64 kDefaultLimit = 20 * 1024 * 1024;

    static LoadedText load(const QString &path, qint64 maxBytes = kDefaultLimit);

    /// Decodes bytes that were already read. `truncated` tells that more data followed,
    /// so an incomplete multi-byte sequence at the end is not treated as an error.
    static LoadedText decode(const QByteArray &bytes, bool truncated);

    /// Drops a UTF-8 sequence that was cut in half at the end of the buffer.
    static QByteArray trimIncompleteUtf8(const QByteArray &bytes);

    static bool isValidUtf8(const QByteArray &bytes);
};

} // namespace colibry::fv
