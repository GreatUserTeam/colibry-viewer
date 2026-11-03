#include "core/FileKind.h"
#include "core/FolderFiles.h"
#include "core/HexDump.h"
#include "core/TextLoader.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry::fv;

namespace {
QString write(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("cannot write test file");
    f.write(data);
    return path;
}
} // namespace

class ViewerCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); }
    QString p(const char *name) const { return m_dir.filePath(QString::fromLatin1(name)); }

    // ---- hex dump -----------------------------------------------------
    void hexDumpMatchesHexdumpC()
    {
        const QString out = HexDump::format("Hello, world!\n");
        QCOMPARE(out, QStringLiteral("00000000  48 65 6c 6c 6f 2c 20 77  6f 72 6c 64 21 0a        |Hello, world!.|\n"));
    }

    void hexDumpFullLinesAndOffsets()
    {
        QByteArray data;
        for (int i = 0; i < 20; ++i)
            data.append(static_cast<char>(i));
        const QStringList lines = HexDump::format(data).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines.at(0), QStringLiteral("00000000  00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f  |................|"));
        QVERIFY(lines.at(1).startsWith(QStringLiteral("00000010  10 11 12 13")));
        QVERIFY(HexDump::format(data, 0x1000).startsWith(QStringLiteral("00001000")));
        QVERIFY(HexDump::format(QByteArray()).isEmpty());
    }

    void hexDumpKeepsPercentSignsFromData()
    {
        QVERIFY(HexDump::format("%1%2%3").contains(QStringLiteral("|%1%2%3|")));
    }

    // ---- text loader --------------------------------------------------
    void decodesUtf8AndKeepsCyrillic()
    {
        const auto r = TextLoader::decode(QStringLiteral("Привіт, світе! 日本").toUtf8(), false);
        QVERIFY(r.ok);
        QCOMPARE(r.encoding, QStringLiteral("UTF-8"));
        QCOMPARE(r.text, QStringLiteral("Привіт, світе! 日本"));
    }

    void stripsUtf8Bom()
    {
        const auto r = TextLoader::decode(QByteArray("\xEF\xBB\xBF") + "abc", false);
        QCOMPARE(r.text, QStringLiteral("abc"));
        QCOMPARE(r.encoding, QStringLiteral("UTF-8"));
    }

    void decodesUtf16WithBom()
    {
        QByteArray le("\xFF\xFE");
        le += QByteArray("h\0i\0", 4);
        QCOMPARE(TextLoader::decode(le, false).text, QStringLiteral("hi"));
        QCOMPARE(TextLoader::decode(le, false).encoding, QStringLiteral("UTF-16LE"));

        QByteArray be("\xFE\xFF");
        be += QByteArray("\0h\0i", 4);
        QCOMPARE(TextLoader::decode(be, false).text, QStringLiteral("hi"));
        QCOMPARE(TextLoader::decode(be, false).encoding, QStringLiteral("UTF-16BE"));
    }

    void fallsBackToLatin1ForInvalidUtf8()
    {
        const auto r = TextLoader::decode(QByteArray("caf\xE9"), false);
        QCOMPARE(r.encoding, QStringLiteral("ISO-8859-1"));
        QCOMPARE(r.text, QStringLiteral("café"));
    }

    void normalizesLineEndings()
    {
        QCOMPARE(TextLoader::decode("a\r\nb\rc\nd", false).text, QStringLiteral("a\nb\nc\nd"));
    }

    void truncationDoesNotBreakMultiByteCharacters()
    {
        // "é" is C3 A9; cut after C3 as a truncated read would.
        const QByteArray cut("ab\xC3");
        QCOMPARE(TextLoader::trimIncompleteUtf8(cut), QByteArray("ab"));
        QCOMPARE(TextLoader::trimIncompleteUtf8(QByteArray("ab\xE2\x82")), QByteArray("ab"));
        QCOMPARE(TextLoader::trimIncompleteUtf8(QByteArray("ab\xF0\x9F\x98")), QByteArray("ab"));
        QCOMPARE(TextLoader::trimIncompleteUtf8(QByteArray("ab\xC3\xA9")), QByteArray("ab\xC3\xA9"));
        QCOMPARE(TextLoader::trimIncompleteUtf8(QByteArray("abc")), QByteArray("abc"));

        const auto r = TextLoader::decode(cut, true);
        QCOMPARE(r.encoding, QStringLiteral("UTF-8"));
        QCOMPARE(r.text, QStringLiteral("ab"));
        QVERIFY(r.truncated);
    }

    void loadsFileAndReportsTruncation()
    {
        write(p("big.txt"), QByteArray(1000, 'x'));
        const auto whole = TextLoader::load(p("big.txt"));
        QVERIFY(whole.ok);
        QVERIFY(!whole.truncated);
        QCOMPARE(whole.fileSize, 1000);

        const auto part = TextLoader::load(p("big.txt"), 100);
        QVERIFY(part.ok);
        QVERIFY(part.truncated);
        QCOMPARE(part.text.size(), 100);
        QCOMPARE(part.fileSize, 1000);
    }

    void loadReportsMissingFile()
    {
        const auto r = TextLoader::load(p("missing.txt"));
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
    }

    void emptyFileLoadsAsEmptyText()
    {
        write(p("empty.txt"), QByteArray());
        const auto r = TextLoader::load(p("empty.txt"));
        QVERIFY(r.ok);
        QVERIFY(r.text.isEmpty());
    }

    // ---- file kind ----------------------------------------------------
    void detectsText()
    {
        write(p("a.txt"), "hello\nworld\n");
        write(p("ru.txt"), QStringLiteral("Привіт").toUtf8());
        write(p("empty"), QByteArray());
        write(p("latin"), QByteArray("caf\xE9 au lait"));
        write(p("noext"), "#!/bin/sh\necho hi\n");
        QCOMPARE(FileKind::detect(p("a.txt")), Kind::Text);
        QCOMPARE(FileKind::detect(p("ru.txt")), Kind::Text);
        QCOMPARE(FileKind::detect(p("empty")), Kind::Text);
        QCOMPARE(FileKind::detect(p("latin")), Kind::Text);
        QCOMPARE(FileKind::detect(p("noext")), Kind::Text);
    }

    void detectsUtf16AsText()
    {
        QByteArray le("\xFF\xFE");
        le += QByteArray("h\0i\0", 4);
        write(p("u16.txt"), le);
        QCOMPARE(FileKind::detect(p("u16.txt")), Kind::Text);
    }

    void detectsBinary()
    {
        QByteArray bin;
        for (int i = 0; i < 256; ++i)
            bin.append(static_cast<char>(i));
        write(p("data.bin"), bin);
        QCOMPARE(FileKind::detect(p("data.bin")), Kind::Binary);
        write(p("nul"), QByteArray("abc\0def", 7));
        QCOMPARE(FileKind::detect(p("nul")), Kind::Binary);
        write(p("controls"), QByteArray(500, '\x01'));
        QCOMPARE(FileKind::detect(p("controls")), Kind::Binary);

        QByteArray highBytes;
        for (int i = 0; i < 300; ++i)
            highBytes.append(static_cast<char>(0x80 + (i % 128)));
        write(p("high"), highBytes);
        QCOMPARE(FileKind::detect(p("high")), Kind::Binary);
    }

    void detectsImagesByContentNotExtension()
    {
        QImage img(8, 8, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(p("pic.png")));
        QVERIFY(QFile::copy(p("pic.png"), p("disguised.txt")));
        QCOMPARE(FileKind::detect(p("pic.png")), Kind::Image);
        QCOMPARE(FileKind::detect(p("disguised.txt")), Kind::Image);
    }

    void detectsFoldersAndMissing()
    {
        QDir(m_dir.path()).mkdir(QStringLiteral("folder"));
        QCOMPARE(FileKind::detect(p("folder")), Kind::Directory);
        QCOMPARE(FileKind::detect(p("nothing-here")), Kind::Missing);
    }

    void refusesSpecialFiles()
    {
        QCOMPARE(FileKind::detect(QStringLiteral("/dev/null")), Kind::Special);
        QCOMPARE(FileKind::detect(QStringLiteral("/dev/zero")), Kind::Special);
    }

    void bigTextSampleCutInsideCharacterStillText()
    {
        // 8192-byte sample that ends in the middle of a 2-byte character.
        QByteArray data(FileKind::kSampleSize - 1, 'a');
        data += QStringLiteral("é").toUtf8();
        write(p("edge.txt"), data);
        QCOMPARE(FileKind::detect(p("edge.txt")), Kind::Text);
    }

    // ---- siblings -----------------------------------------------------
    void siblingsAreSortedNaturallyAndSkipFoldersAndHidden()
    {
        QDir d(m_dir.path());
        d.mkdir(QStringLiteral("sub"));
        for (const char *name : {"img10.png", "img2.png", "Img1.png", ".hidden"})
            write(p(name), "x");
        write(p("folder-file"), "x");

        const QStringList list = FolderFiles::siblings(p("img2.png"));
        QStringList names;
        for (const QString &path : list)
            names << QFileInfo(path).fileName();

        QVERIFY(!names.contains(QStringLiteral("sub")));
        QVERIFY(!names.contains(QStringLiteral(".hidden")));
        const int i1 = static_cast<int>(names.indexOf(QStringLiteral("Img1.png")));
        const int i2 = static_cast<int>(names.indexOf(QStringLiteral("img2.png")));
        const int i10 = static_cast<int>(names.indexOf(QStringLiteral("img10.png")));
        QVERIFY(i1 >= 0 && i1 < i2 && i2 < i10);
    }

    void hiddenFileSeesOtherHiddenFiles()
    {
        write(p(".a"), "x");
        write(p(".b"), "x");
        write(p("visible"), "x");
        QVERIFY(FolderFiles::siblings(p(".a")).contains(p(".b")));
        QVERIFY(!FolderFiles::siblings(p("visible")).contains(p(".b")));
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(ViewerCoreTest)
#include "test_core.moc"
