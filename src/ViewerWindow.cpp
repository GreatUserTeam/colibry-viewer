#include "ViewerWindow.h"

#include "core/FolderFiles.h"
#include "core/HexDump.h"
#include "core/TextLoader.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextCursor>
#include <QTransform>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace colibry::fv {

namespace {

constexpr int kPageMessage = 0;
constexpr int kPageText = 1;
constexpr int kPageImage = 2;
constexpr int kPageHex = 3;

QString humanSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace

ViewerWindow::ViewerWindow(SettingsStore &store, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
{
    setWindowIcon(QIcon::fromTheme(QStringLiteral("document-viewer")));
    resize(900, 640);
    setMinimumSize(420, 300);
    setAcceptDrops(true);

    buildUi();
    buildActions();
    buildMenus();

    const QByteArray geometry = m_store.value(Keys::ViewerGeometry).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    resetState();
    showMessage(tr("Open a file with Ctrl+O, or drop it here."));
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void ViewerWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget(central);

    m_message = new QLabel(m_stack);
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_message->setMargin(24);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    m_text = new QPlainTextEdit(m_stack);
    m_text->setReadOnly(true);
    m_text->setFont(mono);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')) * 4);

    m_imageLabel = new QLabel;
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageScroll = new QScrollArea(m_stack);
    m_imageScroll->setWidget(m_imageLabel);
    m_imageScroll->setWidgetResizable(false);
    m_imageScroll->setAlignment(Qt::AlignCenter);
    m_imageScroll->setFrameShape(QFrame::NoFrame);
    m_imageScroll->setBackgroundRole(QPalette::Dark);
    m_imageScroll->viewport()->installEventFilter(this);

    m_hex = new QPlainTextEdit(m_stack);
    m_hex->setReadOnly(true);
    m_hex->setFont(mono);
    m_hex->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_hex->setFrameShape(QFrame::NoFrame);

    m_stack->addWidget(m_message);     // kPageMessage
    m_stack->addWidget(m_text);        // kPageText
    m_stack->addWidget(m_imageScroll); // kPageImage
    m_stack->addWidget(m_hex);         // kPageHex
    layout->addWidget(m_stack, 1);

    // Find bar
    m_findBar = new QWidget(central);
    auto *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(8, 4, 8, 4);
    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText(tr("Find"));
    m_findEdit->setClearButtonEnabled(true);
    auto *prev = new QPushButton(tr("Previous"), m_findBar);
    auto *next = new QPushButton(tr("Next"), m_findBar);
    auto *close = new QPushButton(tr("Close"), m_findBar);
    m_findResult = new QLabel(m_findBar);
    findLayout->addWidget(m_findEdit, 1);
    findLayout->addWidget(prev);
    findLayout->addWidget(next);
    findLayout->addWidget(m_findResult);
    findLayout->addWidget(close);
    layout->addWidget(m_findBar);
    m_findBar->hide();

    connect(m_findEdit, &QLineEdit::returnPressed, this, [this] {
        m_findResult->setText(findText(m_findEdit->text(), false) ? QString() : tr("Not found"));
    });
    connect(m_findEdit, &QLineEdit::textChanged, this, [this] { m_findResult->clear(); });
    connect(next, &QPushButton::clicked, this, [this] {
        m_findResult->setText(findText(m_findEdit->text(), false) ? QString() : tr("Not found"));
    });
    connect(prev, &QPushButton::clicked, this, [this] {
        m_findResult->setText(findText(m_findEdit->text(), true) ? QString() : tr("Not found"));
    });
    connect(close, &QPushButton::clicked, this, [this] { hideFindBar(); });

    auto *escape = new QAction(this);
    escape->setShortcut(QKeySequence(Qt::Key_Escape));
    escape->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QAction::triggered, this, [this] { hideFindBar(); });
    m_findBar->addAction(escape);

    setCentralWidget(central);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);
}

void ViewerWindow::buildActions()
{
    auto make = [this](const QString &text, QKeySequence shortcut = {}) {
        auto *action = new QAction(text, this);
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        return action;
    };

    m_open = make(tr("Open…"), QKeySequence::Open);
    m_prev = make(tr("Previous File"), QKeySequence(Qt::ALT | Qt::Key_Left));
    m_next = make(tr("Next File"), QKeySequence(Qt::ALT | Qt::Key_Right));

    m_zoomIn = make(tr("Zoom In"), QKeySequence(Qt::CTRL | Qt::Key_Plus));
    m_zoomOut = make(tr("Zoom Out"), QKeySequence(Qt::CTRL | Qt::Key_Minus));
    m_actual = make(tr("Actual Size"), QKeySequence(Qt::CTRL | Qt::Key_1));
    m_fitAction = make(tr("Fit to Window"), QKeySequence(Qt::CTRL | Qt::Key_0));
    m_rotateRight = make(tr("Rotate Right"), QKeySequence(Qt::CTRL | Qt::Key_R));
    m_rotateLeft = make(tr("Rotate Left"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    m_copyImage = make(tr("Copy Image"));
    m_copyImage->setShortcut(QKeySequence::Copy);
    m_copyImage->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_imageScroll->addAction(m_copyImage);

    m_wrap = make(tr("Word Wrap"));
    m_wrap->setCheckable(true);
    m_hexAction = make(tr("Show as Hex Dump"));
    m_hexAction->setCheckable(true);

    m_find = make(tr("Find…"), QKeySequence::Find);
    m_findNext = make(tr("Find Next"), QKeySequence::FindNext);
    m_findPrev = make(tr("Find Previous"), QKeySequence::FindPrevious);

    // Left / Right switch pictures, but only while a picture has the focus.
    auto *left = new QAction(this);
    left->setShortcut(QKeySequence(Qt::Key_Left));
    left->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(left, &QAction::triggered, this, [this] { previous(); });
    m_imageScroll->addAction(left);
    auto *right = new QAction(this);
    right->setShortcut(QKeySequence(Qt::Key_Right));
    right->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(right, &QAction::triggered, this, [this] { next(); });
    m_imageScroll->addAction(right);

    connect(m_open, &QAction::triggered, this, [this] { promptOpen(); });
    connect(m_prev, &QAction::triggered, this, [this] { previous(); });
    connect(m_next, &QAction::triggered, this, [this] { next(); });
    connect(m_zoomIn, &QAction::triggered, this, [this] { zoomIn(); });
    connect(m_zoomOut, &QAction::triggered, this, [this] { zoomOut(); });
    connect(m_actual, &QAction::triggered, this, [this] { actualSize(); });
    connect(m_fitAction, &QAction::triggered, this, [this] { fitToWindow(); });
    connect(m_rotateRight, &QAction::triggered, this, [this] { rotateRight(); });
    connect(m_rotateLeft, &QAction::triggered, this, [this] { rotateLeft(); });
    connect(m_copyImage, &QAction::triggered, this, [this] { copyImage(); });
    connect(m_wrap, &QAction::toggled, this, [this](bool on) { setWordWrap(on); });
    connect(m_hexAction, &QAction::toggled, this, [this](bool on) { setHexMode(on); });
    connect(m_find, &QAction::triggered, this, [this] { showFindBar(); });
    connect(m_findNext, &QAction::triggered, this, [this] {
        m_findResult->setText(findText(m_findEdit->text(), false) ? QString() : tr("Not found"));
    });
    connect(m_findPrev, &QAction::triggered, this, [this] {
        m_findResult->setText(findText(m_findEdit->text(), true) ? QString() : tr("Not found"));
    });

    for (QAction *action : {m_prev, m_next, m_zoomIn, m_zoomOut, m_actual, m_fitAction, m_rotateRight,
                            m_rotateLeft, m_find, m_findNext, m_findPrev})
        addAction(action);
}

void ViewerWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_open);
    file->addSeparator();
    file->addAction(m_prev);
    file->addAction(m_next);
    file->addSeparator();
    QAction *quit = file->addAction(tr("Close"));
    quit->setShortcut(QKeySequence::Close);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_copyImage);
    edit->addAction(m_find);
    edit->addAction(m_findNext);
    edit->addAction(m_findPrev);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_zoomIn);
    view->addAction(m_zoomOut);
    view->addAction(m_actual);
    view->addAction(m_fitAction);
    view->addSeparator();
    view->addAction(m_rotateRight);
    view->addAction(m_rotateLeft);
    view->addSeparator();
    view->addAction(m_wrap);
    view->addAction(m_hexAction);
}

// ---------------------------------------------------------------------------
// Opening files
// ---------------------------------------------------------------------------

void ViewerWindow::resetState()
{
    m_path.clear();
    m_kind = Kind::Missing;
    m_siblings.clear();
    m_index = -1;
    m_info.clear();
    m_image = QImage();
    m_rotated = QImage();
    m_rotation = 0;
    m_zoom = 1.0;
    m_fit = true;
    m_imageLabel->clear();
    m_text->clear();
    m_hex->clear();
    hideFindBar();
}

void ViewerWindow::showMessage(const QString &text)
{
    m_message->setText(text);
    m_stack->setCurrentIndex(kPageMessage);
    updateActions();
    updateTitle();
}

bool ViewerWindow::openFile(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    const QString name = QFileInfo(path).fileName();
    const Kind kind = FileKind::detect(absolute);

    resetState();
    switch (kind) {
    case Kind::Missing:
        showMessage(tr("The file \"%1\" does not exist.").arg(name));
        return false;
    case Kind::Directory:
        showMessage(tr("\"%1\" is a folder. Choose a file to view.").arg(name));
        return false;
    case Kind::Special:
        showMessage(tr("\"%1\" is not a regular file and cannot be shown.").arg(name));
        return false;
    default:
        break;
    }

    m_kind = kind;
    bool ok = false;
    if (m_forceHex && kind != Kind::Image)
        ok = showHex(absolute);
    else if (kind == Kind::Image)
        ok = showImage(absolute);
    else if (kind == Kind::Text)
        ok = showText(absolute);
    else
        ok = showHex(absolute);

    if (!ok) {
        m_kind = Kind::Missing;
        return false;
    }
    finishOpen(absolute, m_info);
    return true;
}

void ViewerWindow::finishOpen(const QString &path, const QString &info)
{
    m_path = path;
    m_info = info;
    m_siblings = FolderFiles::siblings(path);
    m_index = static_cast<int>(m_siblings.indexOf(path));
    updateActions();
    updateTitle();
}

bool ViewerWindow::showText(const QString &path)
{
    const LoadedText loaded = TextLoader::load(path);
    if (!loaded.ok) {
        showMessage(tr("Cannot read \"%1\": %2").arg(QFileInfo(path).fileName(), loaded.error));
        return false;
    }

    m_text->setPlainText(loaded.text);
    m_text->moveCursor(QTextCursor::Start);
    m_stack->setCurrentIndex(kPageText);

    QString info = loaded.encoding;
    info += tr(" · %1 lines").arg(QLocale().toString(m_text->document()->blockCount()));
    if (loaded.truncated)
        info += tr(" · only the first %1 are shown").arg(humanSize(TextLoader::kDefaultLimit));
    m_info = info;
    return true;
}

bool ViewerWindow::showHex(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showMessage(tr("Cannot read \"%1\": %2").arg(QFileInfo(path).fileName(), file.errorString()));
        return false;
    }
    const qint64 size = file.size();
    const QByteArray bytes = file.read(kHexLimit);

    m_hex->setPlainText(HexDump::format(bytes));
    m_hex->moveCursor(QTextCursor::Start);
    m_stack->setCurrentIndex(kPageHex);

    m_info = tr("hex dump");
    if (size > kHexLimit)
        m_info += tr(" · only the first %1 are shown").arg(humanSize(kHexLimit));
    return true;
}

bool ViewerWindow::showImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true); // honour the EXIF orientation of photos
    const QImage image = reader.read();
    if (image.isNull()) {
        showMessage(tr("Cannot read the picture \"%1\": %2").arg(QFileInfo(path).fileName(), reader.errorString()));
        return false;
    }

    m_image = image;
    m_rotation = 0;
    m_rotated = m_image;
    m_fit = true;
    m_stack->setCurrentIndex(kPageImage);
    updateImage();
    m_info.clear();
    return true;
}

void ViewerWindow::promptOpen()
{
    const QString start = m_path.isEmpty() ? QString() : QFileInfo(m_path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), start);
    if (!path.isEmpty())
        openFile(path);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

bool ViewerWindow::hasNext() const
{
    return m_index >= 0 && m_index < m_siblings.size() - 1;
}

bool ViewerWindow::hasPrevious() const
{
    return m_index > 0;
}

bool ViewerWindow::next()
{
    if (!hasNext())
        return false;
    // Skip files that vanished since the list was made.
    for (int i = m_index + 1; i < m_siblings.size(); ++i) {
        if (QFileInfo::exists(m_siblings.at(i)))
            return openFile(m_siblings.at(i));
    }
    return false;
}

bool ViewerWindow::previous()
{
    if (!hasPrevious())
        return false;
    for (int i = m_index - 1; i >= 0; --i) {
        if (QFileInfo::exists(m_siblings.at(i)))
            return openFile(m_siblings.at(i));
    }
    return false;
}

// ---------------------------------------------------------------------------
// Text and hex
// ---------------------------------------------------------------------------

ViewerWindow::Page ViewerWindow::page() const
{
    switch (m_stack->currentIndex()) {
    case kPageText:
        return Page::Text;
    case kPageImage:
        return Page::Image;
    case kPageHex:
        return Page::Hex;
    default:
        return Page::Message;
    }
}

QPlainTextEdit *ViewerWindow::activeTextWidget() const
{
    switch (m_stack->currentIndex()) {
    case kPageText:
        return m_text;
    case kPageHex:
        return m_hex;
    default:
        return nullptr;
    }
}

QString ViewerWindow::text() const
{
    const QPlainTextEdit *edit = activeTextWidget();
    return edit ? edit->toPlainText() : QString();
}

QString ViewerWindow::messageText() const
{
    return m_stack->currentIndex() == kPageMessage ? m_message->text() : QString();
}

QString ViewerWindow::selectedText() const
{
    const QPlainTextEdit *edit = activeTextWidget();
    return edit ? edit->textCursor().selectedText() : QString();
}

bool ViewerWindow::isWordWrap() const
{
    return m_text->lineWrapMode() == QPlainTextEdit::WidgetWidth;
}

void ViewerWindow::setWordWrap(bool on)
{
    m_text->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    const QSignalBlocker blocker(m_wrap);
    m_wrap->setChecked(on);
}

void ViewerWindow::setHexMode(bool on)
{
    if (m_forceHex == on)
        return;
    m_forceHex = on;
    {
        const QSignalBlocker blocker(m_hexAction);
        m_hexAction->setChecked(on);
    }
    // Show the current file again in the new mode.
    if (!m_path.isEmpty() && m_kind != Kind::Image) {
        const QString path = m_path;
        openFile(path);
    }
}

bool ViewerWindow::findText(const QString &needle, bool backward)
{
    QPlainTextEdit *edit = activeTextWidget();
    if (!edit || needle.isEmpty())
        return false;

    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;

    const QTextCursor original = edit->textCursor();
    if (edit->find(needle, flags))
        return true;

    // Nothing after (or before) the cursor: wrap around once.
    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
    edit->setTextCursor(cursor);
    if (edit->find(needle, flags))
        return true;

    edit->setTextCursor(original);
    return false;
}

void ViewerWindow::showFindBar()
{
    if (!activeTextWidget())
        return;
    const QString selected = selectedText();
    if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator))
        m_findEdit->setText(selected);
    m_findBar->show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

bool ViewerWindow::isFindBarVisible() const
{
    return !m_findBar->isHidden();
}

void ViewerWindow::hideFindBar()
{
    m_findBar->hide();
    m_findResult->clear();
    if (QPlainTextEdit *edit = activeTextWidget())
        edit->setFocus();
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

QSize ViewerWindow::imageSize() const
{
    return m_rotated.isNull() ? QSize() : m_rotated.size();
}

double ViewerWindow::maxZoomFor(const QSize &size) const
{
    const int longest = std::max(size.width(), size.height());
    if (longest <= 0)
        return kMaxZoom;
    // Never build a pixmap with a side above 16384 px.
    return std::clamp(16384.0 / longest, kMinZoom, kMaxZoom);
}

void ViewerWindow::setZoomInternal(double zoom, bool fit)
{
    m_fit = fit;
    m_zoom = zoom;
    updateImage();
}

void ViewerWindow::zoomIn()
{
    if (m_rotated.isNull())
        return;
    setZoomInternal(m_zoom * 1.25, false);
}

void ViewerWindow::zoomOut()
{
    if (m_rotated.isNull())
        return;
    setZoomInternal(m_zoom / 1.25, false);
}

void ViewerWindow::actualSize()
{
    if (m_rotated.isNull())
        return;
    setZoomInternal(1.0, false);
}

void ViewerWindow::fitToWindow()
{
    if (m_rotated.isNull())
        return;
    setZoomInternal(m_zoom, true);
}

void ViewerWindow::rotateRight()
{
    setRotation(m_rotation + 1);
}

void ViewerWindow::rotateLeft()
{
    setRotation(m_rotation + 3);
}

void ViewerWindow::setRotation(int quarterTurns)
{
    if (m_image.isNull())
        return;
    m_rotation = ((quarterTurns % 4) + 4) % 4;
    m_rotated = m_rotation == 0 ? m_image : m_image.transformed(QTransform().rotate(90.0 * m_rotation));
    updateImage();
}

void ViewerWindow::updateImage()
{
    if (m_rotated.isNull())
        return;

    const QSize original = m_rotated.size();
    if (m_fit) {
        const QSize area = m_imageScroll->viewport()->size();
        if (area.width() > 0 && area.height() > 0) {
            const double fit = std::min(double(area.width()) / original.width(),
                                        double(area.height()) / original.height());
            m_zoom = std::min(1.0, fit); // fitting shrinks big pictures but never enlarges small ones
        } else {
            m_zoom = 1.0;
        }
    }
    m_zoom = std::clamp(m_zoom, kMinZoom, maxZoomFor(original));

    const QSize target(std::max(1, int(std::lround(original.width() * m_zoom))),
                       std::max(1, int(std::lround(original.height() * m_zoom))));
    const QImage shown = target == original ? m_rotated
                                            : m_rotated.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(QPixmap::fromImage(shown));
    m_imageLabel->resize(shown.size());
    updateTitle();
}

void ViewerWindow::copyImage()
{
    if (!m_rotated.isNull())
        QGuiApplication::clipboard()->setImage(m_rotated);
}

// ---------------------------------------------------------------------------
// State shown to the user
// ---------------------------------------------------------------------------

QString ViewerWindow::statusText() const
{
    if (m_path.isEmpty())
        return QString();

    QStringList parts;
    parts << QFileInfo(m_path).fileName();
    parts << humanSize(QFileInfo(m_path).size());
    if (m_kind == Kind::Image && !m_rotated.isNull()) {
        parts << QStringLiteral("%1 × %2").arg(m_rotated.width()).arg(m_rotated.height());
        parts << tr("%1%").arg(int(std::lround(m_zoom * 100)));
    } else if (!m_info.isEmpty()) {
        parts << m_info;
    }
    if (m_siblings.size() > 1 && m_index >= 0)
        parts << tr("%1 of %2").arg(m_index + 1).arg(m_siblings.size());
    return parts.join(QStringLiteral("  ·  "));
}

void ViewerWindow::updateTitle()
{
    const QString name = QFileInfo(m_path).fileName();
    setWindowTitle(name.isEmpty() ? tr("Viewer") : tr("%1 — Viewer").arg(name));
    m_status->setText(statusText());
}

void ViewerWindow::updateActions()
{
    const bool image = m_stack->currentIndex() == kPageImage;
    const bool textual = activeTextWidget() != nullptr;
    const bool hasFile = !m_path.isEmpty();

    m_prev->setEnabled(hasPrevious());
    m_next->setEnabled(hasNext());

    for (QAction *action : {m_zoomIn, m_zoomOut, m_actual, m_fitAction, m_rotateRight, m_rotateLeft, m_copyImage})
        action->setEnabled(image);

    m_wrap->setEnabled(m_stack->currentIndex() == kPageText);
    m_hexAction->setEnabled(hasFile && !image);
    m_find->setEnabled(textual);
    m_findNext->setEnabled(textual);
    m_findPrev->setEnabled(textual);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

bool ViewerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_imageScroll->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            if (wheel->angleDelta().y() > 0)
                zoomIn();
            else if (wheel->angleDelta().y() < 0)
                zoomOut();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void ViewerWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_fit && !m_rotated.isNull())
        updateImage();
}

void ViewerWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ViewerWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            openFile(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }
}

void ViewerWindow::closeEvent(QCloseEvent *event)
{
    m_store.setValue(Keys::ViewerGeometry, saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace colibry::fv
