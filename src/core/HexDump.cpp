#include "HexDump.h"

namespace colibry::fv {

QString HexDump::format(const QByteArray &data, qint64 baseOffset)
{
    QString out;
    out.reserve(static_cast<qsizetype>(data.size() / 16 + 1) * 80);

    for (qsizetype line = 0; line < data.size(); line += 16) {
        const qsizetype count = qMin<qsizetype>(16, data.size() - line);

        QString hex;
        QString ascii;
        for (int i = 0; i < 16; ++i) {
            if (i < count) {
                const auto byte = static_cast<unsigned char>(data.at(line + i));
                hex += QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0'));
                ascii += (byte >= 0x20 && byte < 0x7F) ? QLatin1Char(static_cast<char>(byte)) : QLatin1Char('.');
            } else {
                hex += QStringLiteral("   ");
            }
            if (i == 7)
                hex += QLatin1Char(' ');
        }

        // Concatenate instead of chaining arg(): file bytes may contain "%1" and the like.
        out += QStringLiteral("%1").arg(static_cast<qulonglong>(baseOffset + line), 8, 16, QLatin1Char('0'));
        out += QStringLiteral("  ");
        out += hex;
        out += QStringLiteral(" |");
        out += ascii;
        out += QStringLiteral("|\n");
    }
    return out;
}

} // namespace colibry::fv
