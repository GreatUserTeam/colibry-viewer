#include "TextLoader.h"

#include <QFile>
#include <QStringDecoder>

namespace colibry::fv {

namespace {
// A "stateful" decoder (the default) waits for more input after a half-finished sequence and
// reports no error, which would silently drop the last byte. Files are decoded in one piece.
constexpr QStringConverter::Flags kWholeBuffer = QStringConverter::Flag::Stateless;
} // namespace

bool TextLoader::isValidUtf8(const QByteArray &bytes)
{
    QStringDecoder decoder(QStringConverter::Utf8, kWholeBuffer);
    const QString ignored = decoder(bytes);
    Q_UNUSED(ignored)
    return !decoder.hasError();
}

QByteArray TextLoader::trimIncompleteUtf8(const QByteArray &bytes)
{
    // Look at the last up-to-3 bytes for a lead byte whose sequence is not complete.
    const int size = static_cast<int>(bytes.size());
    for (int back = 1; back <= 3 && back <= size; ++back) {
        const auto b = static_cast<unsigned char>(bytes.at(size - back));
        if ((b & 0xC0) == 0x80)
            continue; // continuation byte, keep looking for its lead byte
        int needed = 1;
        if ((b & 0xE0) == 0xC0)
            needed = 2;
        else if ((b & 0xF0) == 0xE0)
            needed = 3;
        else if ((b & 0xF8) == 0xF0)
            needed = 4;
        return needed > back ? bytes.left(size - back) : bytes;
    }
    return bytes;
}

LoadedText TextLoader::decode(const QByteArray &input, bool truncated)
{
    LoadedText result;
    result.ok = true;
    result.truncated = truncated;

    QByteArray data = input;
    auto finish = [&result](QString text, const QString &encoding) {
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        result.text = text;
        result.encoding = encoding;
        return result;
    };

    if (data.startsWith("\xEF\xBB\xBF")) {
        data.remove(0, 3);
        if (truncated)
            data = trimIncompleteUtf8(data);
        QStringDecoder decoder(QStringConverter::Utf8, kWholeBuffer);
        return finish(decoder(data), QStringLiteral("UTF-8"));
    }
    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        const bool little = data.startsWith("\xFF\xFE");
        data.remove(0, 2);
        if (data.size() % 2 != 0)
            data.chop(1);
        QStringDecoder decoder(little ? QStringConverter::Utf16LE : QStringConverter::Utf16BE, kWholeBuffer);
        return finish(decoder(data), little ? QStringLiteral("UTF-16LE") : QStringLiteral("UTF-16BE"));
    }

    if (truncated)
        data = trimIncompleteUtf8(data);
    if (isValidUtf8(data)) {
        QStringDecoder decoder(QStringConverter::Utf8, kWholeBuffer);
        return finish(decoder(data), QStringLiteral("UTF-8"));
    }
    return finish(QString::fromLatin1(data), QStringLiteral("ISO-8859-1"));
}

LoadedText TextLoader::load(const QString &path, qint64 maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LoadedText failed;
        failed.error = file.errorString();
        return failed;
    }

    const qint64 size = file.size();
    const QByteArray bytes = file.read(maxBytes);
    if (bytes.isEmpty() && size > 0) {
        LoadedText failed;
        failed.error = file.errorString();
        return failed;
    }

    LoadedText result = decode(bytes, size > maxBytes);
    result.fileSize = size;
    return result;
}

} // namespace colibry::fv
