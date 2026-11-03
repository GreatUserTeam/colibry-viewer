#include "ViewerWindow.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace colibry;
using namespace colibry::fv;

namespace {

QString writeText(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("cannot write test file");
    f.write(data);
    return path;
}

QString writeImage(const QString &path, int w, int h)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QPainter p(&img);
    p.fillRect(0, 0, w / 2, h / 2, Qt::red); // asymmetric, so rotation is visible
    p.end();
    if (!img.save(path))
        qFatal("cannot write test image");
    return path;
}

} // namespace

class ViewerUiTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        ++m_counter;
        // Every test gets its own empty folder: previous/next navigation lists the whole folder.
        m_root = m_dir.filePath(QStringLiteral("case-%1").arg(m_counter));
        QVERIFY(QDir().mkpath(m_root));
        m_store = std::make_unique<SettingsStore>(m_dir.filePath(QStringLiteral("viewer-%1.ini").arg(m_counter)));
    }

    QString p(const char *name) const { return m_root + QLatin1Char('/') + QString::fromLatin1(name); }

    std::unique_ptr<ViewerWindow> shown()
    {
        auto w = std::make_unique<ViewerWindow>(*m_store);
        w->resize(800, 600);
        w->show();
        if (!QTest::qWaitForWindowExposed(w.get()))
            qFatal("window was not exposed");
        return w;
    }

    // ---- empty and failing states -------------------------------------
    void startsWithHelpfulMessage()
    {
        auto w = shown();
        QCOMPARE(w->page(), ViewerWindow::Page::Message);
        QVERIFY(w->messageText().contains(QStringLiteral("Ctrl+O")));
        QVERIFY(w->currentPath().isEmpty());
        QVERIFY(!w->hasNext());
        QVERIFY(!w->hasPrevious());
    }

    void refusesMissingFolderAndSpecialFiles()
    {
        auto w = shown();
        QVERIFY(!w->openFile(p("nothing.txt")));
        QVERIFY(w->messageText().contains(QStringLiteral("does not exist")));

        QDir(m_root).mkdir(QStringLiteral("dir"));
        QVERIFY(!w->openFile(p("dir")));
        QVERIFY(w->messageText().contains(QStringLiteral("folder")));

        QVERIFY(!w->openFile(QStringLiteral("/dev/zero")));
        QCOMPARE(w->page(), ViewerWindow::Page::Message);
        QVERIFY(w->currentPath().isEmpty());
    }

    // ---- text ---------------------------------------------------------
    void showsTextFile()
    {
        writeText(p("a.txt"), QStringLiteral("Привіт\nsecond line\n").toUtf8());
        auto w = shown();
        QVERIFY(w->openFile(p("a.txt")));
        QCOMPARE(w->page(), ViewerWindow::Page::Text);
        QCOMPARE(w->kind(), Kind::Text);
        QVERIFY(w->text().startsWith(QStringLiteral("Привіт")));
        QVERIFY(w->statusText().contains(QStringLiteral("UTF-8")));
        QVERIFY(w->windowTitle().contains(QStringLiteral("a.txt")));
    }

    void emptyFileIsShownAsEmptyText()
    {
        writeText(p("empty.txt"), QByteArray());
        auto w = shown();
        QVERIFY(w->openFile(p("empty.txt")));
        QCOMPARE(w->page(), ViewerWindow::Page::Text);
        QVERIFY(w->text().isEmpty());
    }

    void wordWrapToggles()
    {
        writeText(p("w.txt"), QByteArray(500, 'x'));
        auto w = shown();
        QVERIFY(w->openFile(p("w.txt")));
        QVERIFY(!w->isWordWrap());
        w->setWordWrap(true);
        QVERIFY(w->isWordWrap());
        w->setWordWrap(false);
        QVERIFY(!w->isWordWrap());
    }

    void findWrapsAroundAndReportsMisses()
    {
        writeText(p("f.txt"), "alpha\nbeta\nGamma\nbeta again\n");
        auto w = shown();
        QVERIFY(w->openFile(p("f.txt")));

        QVERIFY(w->findText(QStringLiteral("beta")));
        QCOMPARE(w->selectedText(), QStringLiteral("beta"));
        QVERIFY(w->findText(QStringLiteral("beta")));      // second occurrence
        QVERIFY(w->findText(QStringLiteral("beta")));      // wraps to the first
        QVERIFY(w->findText(QStringLiteral("GAMMA")));     // case-insensitive
        QVERIFY(w->findText(QStringLiteral("alpha"), true)); // backwards wraps too
        QVERIFY(!w->findText(QStringLiteral("zzz")));
        QVERIFY(!w->findText(QString()));
    }

    void failedFindKeepsSelection()
    {
        writeText(p("k.txt"), "one two three");
        auto w = shown();
        QVERIFY(w->openFile(p("k.txt")));
        QVERIFY(w->findText(QStringLiteral("two")));
        QVERIFY(!w->findText(QStringLiteral("nope")));
        QCOMPARE(w->selectedText(), QStringLiteral("two"));
    }

    void findBarOpensOnlyForTextAndCloses()
    {
        auto w = shown();
        w->showFindBar();
        QVERIFY(!w->isFindBarVisible()); // nothing to search yet

        writeText(p("s.txt"), "x");
        QVERIFY(w->openFile(p("s.txt")));
        w->showFindBar();
        QVERIFY(w->isFindBarVisible());
        w->hideFindBar();
        QVERIFY(!w->isFindBarVisible());
    }

    void latin1FileIsDecodedNotMangled()
    {
        writeText(p("l.txt"), QByteArray("caf\xE9"));
        auto w = shown();
        QVERIFY(w->openFile(p("l.txt")));
        QCOMPARE(w->text(), QStringLiteral("café"));
        QVERIFY(w->statusText().contains(QStringLiteral("ISO-8859-1")));
    }

    // ---- binary and hex -----------------------------------------------
    void binaryFileShowsHexDump()
    {
        QByteArray bin;
        for (int i = 0; i < 256; ++i)
            bin.append(static_cast<char>(i));
        writeText(p("b.bin"), bin);
        auto w = shown();
        QVERIFY(w->openFile(p("b.bin")));
        QCOMPARE(w->page(), ViewerWindow::Page::Hex);
        QCOMPARE(w->kind(), Kind::Binary);
        QVERIFY(w->text().startsWith(QStringLiteral("00000000  00 01 02 03")));
        QVERIFY(w->text().contains(QStringLiteral("000000f0")));
    }

    void hugeBinaryIsCutAndSaysSo()
    {
        QByteArray big(1024 * 1024 + 4096, '\0');
        big[0] = 1;
        writeText(p("big.bin"), big);
        auto w = shown();
        QVERIFY(w->openFile(p("big.bin")));
        QCOMPARE(w->page(), ViewerWindow::Page::Hex);
        QVERIFY(w->statusText().contains(QStringLiteral("only the first")));
        QVERIFY(!w->text().contains(QStringLiteral("00100000"))); // offset 1 MiB is not shown
    }

    void textCanBeShownAsHexAndBack()
    {
        writeText(p("h.txt"), "Hello");
        auto w = shown();
        QVERIFY(w->openFile(p("h.txt")));
        QCOMPARE(w->page(), ViewerWindow::Page::Text);

        w->setHexMode(true);
        QCOMPARE(w->page(), ViewerWindow::Page::Hex);
        QVERIFY(w->text().contains(QStringLiteral("48 65 6c 6c 6f")));

        w->setHexMode(false);
        QCOMPARE(w->page(), ViewerWindow::Page::Text);
        QCOMPARE(w->text(), QStringLiteral("Hello"));
    }

    // ---- images -------------------------------------------------------
    void showsImageWithSizeInStatus()
    {
        writeImage(p("small.png"), 120, 60);
        auto w = shown();
        QVERIFY(w->openFile(p("small.png")));
        QCOMPARE(w->page(), ViewerWindow::Page::Image);
        QCOMPARE(w->kind(), Kind::Image);
        QCOMPARE(w->imageSize(), QSize(120, 60));
        QVERIFY(w->statusText().contains(QStringLiteral("120 × 60")));
    }

    void fitShrinksBigPicturesButNeverEnlargesSmallOnes()
    {
        writeImage(p("huge.png"), 4000, 3000);
        writeImage(p("tiny.png"), 40, 30);
        auto w = shown();

        QVERIFY(w->openFile(p("huge.png")));
        QVERIFY(w->isFitMode());
        QVERIFY(w->zoom() < 0.5);
        QVERIFY(w->zoom() >= 0.05);

        QVERIFY(w->openFile(p("tiny.png")));
        QVERIFY(w->isFitMode());
        QCOMPARE(w->zoom(), 1.0);
    }

    void zoomStepsAndLimits()
    {
        writeImage(p("z.png"), 100, 100);
        auto w = shown();
        QVERIFY(w->openFile(p("z.png")));

        w->actualSize();
        QCOMPARE(w->zoom(), 1.0);
        QVERIFY(!w->isFitMode());

        w->zoomIn();
        QVERIFY(qFuzzyCompare(w->zoom(), 1.25));
        w->zoomOut();
        w->zoomOut();
        QVERIFY(qFuzzyCompare(w->zoom(), 0.8));

        for (int i = 0; i < 100; ++i)
            w->zoomIn();
        QVERIFY(w->zoom() <= 16.0);
        for (int i = 0; i < 200; ++i)
            w->zoomOut();
        QVERIFY(w->zoom() >= 0.05);

        w->fitToWindow();
        QVERIFY(w->isFitMode());
    }

    void largeImageCannotBlowUpMemoryWhenZoomedIn()
    {
        writeImage(p("wide.png"), 3000, 10);
        auto w = shown();
        QVERIFY(w->openFile(p("wide.png")));
        for (int i = 0; i < 50; ++i)
            w->zoomIn();
        QVERIFY(w->zoom() * 3000 <= 16384.5); // pixmap side stays under 16384
    }

    void rotationSwapsSizeAndWrapsAround()
    {
        writeImage(p("r.png"), 100, 40);
        auto w = shown();
        QVERIFY(w->openFile(p("r.png")));

        w->rotateRight();
        QCOMPARE(w->rotation(), 1);
        QCOMPARE(w->imageSize(), QSize(40, 100));
        w->rotateRight();
        QCOMPARE(w->imageSize(), QSize(100, 40));
        w->rotateLeft();
        w->rotateLeft();
        w->rotateLeft();
        QCOMPARE(w->rotation(), 3);
        w->rotateRight();
        QCOMPARE(w->rotation(), 0);
        QCOMPARE(w->imageSize(), QSize(100, 40));
    }

    void rotationResetsWhenAnotherPictureOpens()
    {
        writeImage(p("1.png"), 100, 40);
        writeImage(p("2.png"), 100, 40);
        auto w = shown();
        QVERIFY(w->openFile(p("1.png")));
        w->rotateRight();
        QVERIFY(w->next());
        QCOMPARE(w->rotation(), 0);
        QCOMPARE(w->imageSize(), QSize(100, 40));
    }

    void rotationActuallyMovesPixels()
    {
        // writeImage() puts a red block in the top-left corner of a blue picture.
        writeImage(p("px.png"), 100, 40);
        auto w = shown();
        QVERIFY(w->openFile(p("px.png")));
        QCOMPARE(QColor(w->shownImage().pixel(2, 2)), QColor(Qt::red)); // top-left

        w->rotateRight(); // clockwise: top-left -> top-right
        QCOMPARE(w->imageSize(), QSize(40, 100));
        QCOMPARE(QColor(w->shownImage().pixel(37, 2)), QColor(Qt::red));
        QCOMPARE(QColor(w->shownImage().pixel(2, 2)), QColor(Qt::blue));

        w->rotateRight(); // 180: top-left -> bottom-right
        QCOMPARE(QColor(w->shownImage().pixel(97, 37)), QColor(Qt::red));

        w->rotateRight(); // 270: top-left -> bottom-left
        QCOMPARE(QColor(w->shownImage().pixel(2, 97)), QColor(Qt::red));

        w->rotateRight(); // back to the start
        QCOMPARE(QColor(w->shownImage().pixel(2, 2)), QColor(Qt::red));
    }

    void truncatedImageShowsAMessageAndKeepsTheWindowUsable()
    {
        writeImage(p("broken.png"), 200, 200);
        QFile f(p("broken.png"));
        QVERIFY(f.open(QIODevice::ReadWrite));
        const QByteArray head = f.read(60); // PNG signature and header only, no pixel data
        f.resize(0);
        f.write(head);
        f.close();

        auto w = shown();
        QVERIFY(!w->openFile(p("broken.png")));
        QCOMPARE(w->page(), ViewerWindow::Page::Message);
        QVERIFY(w->messageText().contains(QStringLiteral("Cannot read the picture")));
        QVERIFY(w->imageSize().isEmpty());

        // The window still works afterwards.
        writeText(p("ok.txt"), "fine");
        QVERIFY(w->openFile(p("ok.txt")));
        QCOMPARE(w->text(), QStringLiteral("fine"));
    }

    // ---- navigation ---------------------------------------------------
    void nextAndPreviousWalkTheFolderInNaturalOrder()
    {
        writeText(p("note2.txt"), "two");
        writeText(p("note10.txt"), "ten");
        writeText(p("note1.txt"), "one");
        auto w = shown();
        QVERIFY(w->openFile(p("note1.txt")));
        QCOMPARE(w->fileCount(), 3);
        QCOMPARE(w->fileIndex(), 0);
        QVERIFY(!w->hasPrevious());
        QVERIFY(w->hasNext());

        QVERIFY(w->next());
        QCOMPARE(w->text(), QStringLiteral("two"));
        QVERIFY(w->next());
        QCOMPARE(w->text(), QStringLiteral("ten"));
        QVERIFY(!w->hasNext());
        QVERIFY(!w->next());
        QVERIFY(w->previous());
        QCOMPARE(w->text(), QStringLiteral("two"));
        QVERIFY(w->statusText().contains(QStringLiteral("2 of 3")));
    }

    void navigationMovesBetweenDifferentKinds()
    {
        writeText(p("a.txt"), "text");
        writeImage(p("b.png"), 20, 20);
        writeText(p("c.bin"), QByteArray("\0\1\2\3", 4));
        auto w = shown();
        QVERIFY(w->openFile(p("a.txt")));
        QCOMPARE(w->page(), ViewerWindow::Page::Text);
        QVERIFY(w->next());
        QCOMPARE(w->page(), ViewerWindow::Page::Image);
        QVERIFY(w->next());
        QCOMPARE(w->page(), ViewerWindow::Page::Hex);
    }

    void nextSkipsFilesThatDisappeared()
    {
        writeText(p("1.txt"), "1");
        writeText(p("2.txt"), "2");
        writeText(p("3.txt"), "3");
        auto w = shown();
        QVERIFY(w->openFile(p("1.txt")));
        QVERIFY(QFile::remove(p("2.txt")));
        QVERIFY(w->next());
        QCOMPARE(w->text(), QStringLiteral("3"));
    }

    void singleFileHasNoNavigation()
    {
        writeText(p("only.txt"), "x");
        auto w = shown();
        QVERIFY(w->openFile(p("only.txt")));
        QVERIFY(!w->hasNext());
        QVERIFY(!w->hasPrevious());
        QVERIFY(!w->next());
        QVERIFY(!w->previous());
    }

    void failedOpenClearsNavigation()
    {
        writeText(p("a.txt"), "a");
        writeText(p("b.txt"), "b");
        auto w = shown();
        QVERIFY(w->openFile(p("a.txt")));
        QVERIFY(!w->openFile(p("gone.txt")));
        QVERIFY(!w->hasNext());
        QVERIFY(!w->hasPrevious());
        QVERIFY(w->currentPath().isEmpty());
    }

    void geometryIsRemembered()
    {
        {
            auto w = shown();
            w->resize(640, 480);
            w->close();
        }
        QVERIFY(!m_store->value(Keys::ViewerGeometry).toByteArray().isEmpty());
    }

private:
    QTemporaryDir m_dir;
    QString m_root;
    std::unique_ptr<SettingsStore> m_store;
    int m_counter = 0;
};

QTEST_MAIN(ViewerUiTest)
#include "test_ui.moc"
