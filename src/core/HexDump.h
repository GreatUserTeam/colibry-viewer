#pragma once

#include <QByteArray>
#include <QString>

namespace colibry::fv {

/// Classic 16-bytes-per-line hex dump, identical to `hexdump -C`:
///   00000000  48 65 6c 6c 6f 2c 20 77  6f 72 6c 64 21 0a        |Hello, world!.|
class HexDump
{
public:
    static QString format(const QByteArray &data, qint64 baseOffset = 0);
};

} // namespace colibry::fv
