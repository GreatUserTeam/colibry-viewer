#pragma once

#include "core/FileKind.h"

#include <QImage>
#include <QMainWindow>
#include <QStringList>

class QAction;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;

namespace colibry {
class SettingsStore;
}

namespace colibry::fv {

/// Viewer window: text, pictures and hex dumps, with previous / next file navigation.
/// Every user action is also a public method that shows no dialog, so it can be tested.
class ViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Page { Message, Text, Image, Hex };

    explicit ViewerWindow(SettingsStore &store, QWidget *parent = nullptr);

    /// Opens a file. Returns false (and shows the reason in the window) when it cannot be shown.
    bool openFile(const QString &path);

    QString currentPath() const { return m_path; }
    Kind kind() const { return m_kind; }
    Page page() const;
    QString statusText() const;
    QString messageText() const;

    // Navigation between files of the same folder
    bool hasNext() const;
    bool hasPrevious() const;
    bool next();
    bool previous();
    int fileCount() const { return static_cast<int>(m_siblings.size()); }
    int fileIndex() const { return m_index; }

    // Text and hex
    QString text() const;
    bool isWordWrap() const;
    void setWordWrap(bool on);
    bool isHexMode() const { return m_forceHex; }
    void setHexMode(bool on);
    bool findText(const QString &needle, bool backward = false);
    QString selectedText() const;
    void showFindBar();
    bool isFindBarVisible() const;
    void hideFindBar();

    // Images
    QSize imageSize() const; ///< size after rotation, 0x0 when no picture is shown
    QImage shownImage() const { return m_rotated; } ///< the picture as displayed (rotated, unscaled)
    double zoom() const { return m_zoom; }
    bool isFitMode() const { return m_fit; }
    int rotation() const { return m_rotation; } ///< 0..3 quarter turns clockwise
    void zoomIn();
    void zoomOut();
    void actualSize();
    void fitToWindow();
    void rotateRight();
    void rotateLeft();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static constexpr double kMinZoom = 0.05;
    static constexpr double kMaxZoom = 16.0;
    static constexpr qint64 kHexLimit = 1024 * 1024;

    void buildUi();
    void buildActions();
    void buildMenus();

    void resetState();
    void showMessage(const QString &text);
    bool showText(const QString &path);
    bool showImage(const QString &path);
    bool showHex(const QString &path);
    void finishOpen(const QString &path, const QString &info);

    void updateImage();
    void setRotation(int quarterTurns);
    void setZoomInternal(double zoom, bool fit);
    double maxZoomFor(const QSize &size) const;
    QPlainTextEdit *activeTextWidget() const;
    void updateActions();
    void updateTitle();
    void promptOpen();
    void copyImage();

    SettingsStore &m_store;

    QStackedWidget *m_stack = nullptr;
    QLabel *m_message = nullptr;
    QPlainTextEdit *m_text = nullptr;
    QPlainTextEdit *m_hex = nullptr;
    QScrollArea *m_imageScroll = nullptr;
    QLabel *m_imageLabel = nullptr;
    QLabel *m_status = nullptr;

    QWidget *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;
    QLabel *m_findResult = nullptr;

    QAction *m_open = nullptr;
    QAction *m_prev = nullptr;
    QAction *m_next = nullptr;
    QAction *m_zoomIn = nullptr;
    QAction *m_zoomOut = nullptr;
    QAction *m_actual = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_rotateRight = nullptr;
    QAction *m_rotateLeft = nullptr;
    QAction *m_copyImage = nullptr;
    QAction *m_wrap = nullptr;
    QAction *m_hexAction = nullptr;
    QAction *m_find = nullptr;
    QAction *m_findNext = nullptr;
    QAction *m_findPrev = nullptr;

    QString m_path;
    Kind m_kind = Kind::Missing;
    QStringList m_siblings;
    int m_index = -1;
    bool m_forceHex = false;
    QString m_info; ///< encoding / dimensions / "first 1 MiB shown" ...

    QImage m_image;   ///< as decoded (EXIF orientation applied)
    QImage m_rotated; ///< m_image turned by m_rotation
    int m_rotation = 0;
    double m_zoom = 1.0;
    bool m_fit = true;
};

} // namespace colibry::fv
