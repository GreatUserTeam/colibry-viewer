#include "ViewerWindow.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("colibry-viewer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Viewer"));
    QApplication::setApplicationVersion(QStringLiteral(Colibry_APPS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setDesktopFileName(QStringLiteral("colibry-viewer"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("File viewer for the Great User System"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Files to open, one window each."));
    parser.process(app);

    colibry::SettingsStore store;
    colibry::ThemeSync themeSync(app, store);

    const QStringList files = parser.positionalArguments();
    if (files.isEmpty()) {
        auto *window = new colibry::fv::ViewerWindow(store);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    } else {
        for (const QString &file : files) {
            auto *window = new colibry::fv::ViewerWindow(store);
            window->setAttribute(Qt::WA_DeleteOnClose);
            window->openFile(file);
            window->show();
        }
    }
    return QApplication::exec();
}
