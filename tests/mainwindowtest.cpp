#include "mainwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QSettings>
#include <QTabWidget>
#include <QtTest>

class MainWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("HexboardTests"));
        QCoreApplication::setApplicationName(QStringLiteral("MainWindowTests"));
        QSettings().clear();
    }

    void toolsAreLazyUniqueAndRecreated()
    {
        MainWindow window;
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("mainTabs"));
        auto *openDice = window.findChild<QAction *>(QStringLiteral("openDiceRollerAction"));
        QVERIFY(tabs);
        QVERIFY(openDice);
        QCOMPARE(tabs->count(), 1);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Board"));

        openDice->trigger();
        QCOMPARE(tabs->count(), 2);
        QWidget *firstDice = tabs->currentWidget();
        QCOMPARE(firstDice->objectName(), QStringLiteral("diceRollerWidget"));

        openDice->trigger();
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->currentWidget(), firstDice);

        tabs->tabCloseRequested(1);
        QCOMPARE(tabs->count(), 1);
        openDice->trigger();
        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->currentWidget() != firstDice);

        tabs->tabCloseRequested(0);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Board"));
    }

    void cleanupTestCase()
    {
        QSettings().clear();
    }
};

QTEST_MAIN(MainWindowTest)
#include "mainwindowtest.moc"
