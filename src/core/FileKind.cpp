#include "FileKind.h"

#include "TextLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>

namespace colibry::fv {

bool FileKind::looksLikeText(const QByteArray &sample)
{
    if (sample.isEmpty())
        return true;

    // Byte-order marks are text even though UTF-16 contains NUL bytes.
    if (sample.startsWith("\xEF\xBB\xBF") || sample.startsWith("\xFF\xFE") || sample.startsWith("\xFE\xFF"))
        return true;
    if (sample.contains('\0'))
        return false;

    // Many control bytes mean binary data, even when the bytes happen to be valid UTF-8.
    int control = 0;
    for (const char c : sample) {
        const auto b = static_cast<unsigned char>(c);
        if ((b < 0x20 && b != '\t' && b != '\n' && b != '\r' && b != '\f' && b != 0x1B) || b == 0x7F)
            ++control;
    }
    if (control * 10 > sample.size())
        return false;

    // Valid UTF-8 is text. Otherwise it may be an old 8-bit encoding, but only when it is
    // mostly plain ASCII: a sample full of high bytes is compressed or encrypted data.
    const bool cut = sample.size() >= kSampleSize;
    if (TextLoader::isValidUtf8(cut ? TextLoader::trimIncompleteUtf8(sample) : sample))
        return true;
    int high = 0;
    for (const char c : sample) {
        if (static_cast<unsigned char>(c) >= 0x80)
            ++high;
    }
    return high * 3 <= sample.size();
}

Kind FileKind::detect(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return Kind::Missing;
    if (info.isDir())
        return Kind::Directory;
    if (!info.isFile())
        return Kind::Special; // fifo, device, socket: opening some of these blocks forever

    if (!QImageReader::imageFormat(path).isEmpty())
        return Kind::Image;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Kind::Binary;
    return looksLikeText(file.read(kSampleSize)) ? Kind::Text : Kind::Binary;
}

} // namespace colibry::fv
