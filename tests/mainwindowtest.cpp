#include "mainwindow.h"

#include <QAction>
#include <QCoreApplication>
#include <QDockWidget>
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

    void toolsUseLeftPanelAndRemainLazyUniqueAndRecreated()
    {
        MainWindow window;
        auto *toolsDock = window.findChild<QDockWidget *>(QStringLiteral("toolsDock"));
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("toolTabs"));
        auto *openDice = window.findChild<QAction *>(QStringLiteral("openDiceRollerAction"));
        QVERIFY(toolsDock);
        QVERIFY(tabs);
        QVERIFY(openDice);
        QCOMPARE(window.dockWidgetArea(toolsDock), Qt::LeftDockWidgetArea);
        QCOMPARE(tabs->count(), 0);
        QVERIFY(window.centralWidget());
        QCOMPARE(window.centralWidget()->metaObject()->className(), "BoardWidget");

        openDice->trigger();
        QCOMPARE(tabs->count(), 1);
        QWidget *firstDice = tabs->currentWidget();
        QCOMPARE(firstDice->objectName(), QStringLiteral("diceRollerWidget"));

        openDice->trigger();
        QCOMPARE(tabs->count(), 1);
        QCOMPARE(tabs->currentWidget(), firstDice);

        tabs->tabCloseRequested(0);
        QCOMPARE(tabs->count(), 0);
        openDice->trigger();
        QCOMPARE(tabs->count(), 1);
        QVERIFY(tabs->currentWidget() != firstDice);

        toolsDock->hide();
        QVERIFY(toolsDock->isHidden());
        openDice->trigger();
        QVERIFY(!toolsDock->isHidden());
        QCOMPARE(tabs->count(), 1);
    }

    void cleanupTestCase()
    {
        QSettings().clear();
    }
};

QTEST_MAIN(MainWindowTest)
#include "mainwindowtest.moc"
