#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Hexboard"));
    QApplication::setApplicationDisplayName(QStringLiteral("Hexboard"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("craigmcgrain"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.craigmcgrain.Hexboard"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.craigmcgrain.Hexboard")));

    MainWindow window;
    window.show();
    return app.exec();
}
