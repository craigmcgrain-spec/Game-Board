#include "mainwindow.h"
#include "platformmessagefilter.h"

#include <QAction>
#include <QCoreApplication>
#include <QDockWidget>
#include <QSettings>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QtTest>

class MainWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void waylandMenuGrabWarningFilterIsExact()
    {
        const QString warning =
            QStringLiteral("This plugin supports grabbing the mouse only for popup windows");
        QVERIFY(PlatformMessageFilter::shouldSuppress(QStringLiteral("wayland"), warning));
        QVERIFY(PlatformMessageFilter::shouldSuppress(QStringLiteral("wayland-egl"), warning));
        QVERIFY(!PlatformMessageFilter::shouldSuppress(QStringLiteral("xcb"), warning));
        QVERIFY(!PlatformMessageFilter::shouldSuppress(
            QStringLiteral("wayland"),
            QStringLiteral("A different Qt warning")));
    }

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
        auto *toolsPanel = window.findChild<QWidget *>(QStringLiteral("toolsPanel"));
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("toolTabs"));
        auto *launcher =
            window.findChild<QToolBar *>(QStringLiteral("toolPanelLauncher"));
        auto *openDice = window.findChild<QAction *>(QStringLiteral("openDiceRollerAction"));
        auto *openWheel = window.findChild<QAction *>(QStringLiteral("openChanceWheelAction"));
        auto *openGear = window.findChild<QAction *>(QStringLiteral("openGearGeneratorAction"));
        auto *openName = window.findChild<QAction *>(QStringLiteral("openNameGeneratorAction"));
        auto *compactTools =
            window.findChild<QAction *>(QStringLiteral("toggleCompactToolsAction"));
        auto *toggleTools =
            window.findChild<QAction *>(QStringLiteral("toggleToolsPanelAction"));
        QVERIFY(toolsDock);
        QVERIFY(toolsPanel);
        QVERIFY(tabs);
        QVERIFY(launcher);
        QVERIFY(openDice);
        QVERIFY(openWheel);
        QVERIFY(openGear);
        QVERIFY(openName);
        QVERIFY(compactTools);
        QVERIFY(!openDice->icon().isNull());
        QVERIFY(!openWheel->icon().isNull());
        QVERIFY(!openGear->icon().isNull());
        QVERIFY(!openName->icon().isNull());
        QVERIFY(toggleTools);
        QVERIFY(toolsDock->features().testFlag(QDockWidget::DockWidgetClosable));
        QVERIFY(toolsDock->autoFillBackground());
        QVERIFY(toolsPanel->autoFillBackground());
        QVERIFY(tabs->autoFillBackground());
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(toolsDock->isVisible(), 500);
        QCOMPARE(window.dockWidgetArea(toolsDock), Qt::LeftDockWidgetArea);
        QCOMPARE(tabs->count(), 0);
        QVERIFY(window.centralWidget());
        QCOMPARE(window.centralWidget()->metaObject()->className(), "BoardWidget");

        toolsDock->setFloating(true);
        QTRY_VERIFY_WITH_TIMEOUT(toolsDock->isFloating(), 500);
        QVERIFY(toolsDock->titleBarWidget());
        QCOMPARE(
            toolsDock->titleBarWidget()->objectName(),
            QStringLiteral("toolsFloatingTitleBar"));
        QVERIFY(toolsDock->titleBarWidget()->autoFillBackground());
        auto *dockButton = toolsDock->findChild<QToolButton *>(
            QStringLiteral("dockToolsPanelButton"));
        QVERIFY(dockButton);
        dockButton->click();
        QTRY_VERIFY_WITH_TIMEOUT(!toolsDock->isFloating(), 500);
        QVERIFY(!toolsDock->titleBarWidget());

        compactTools->trigger();
        QVERIFY(compactTools->isChecked());
        QVERIFY(tabs->isHidden());
        QCOMPARE(launcher->orientation(), Qt::Vertical);
        QCOMPARE(launcher->toolButtonStyle(), Qt::ToolButtonIconOnly);
        QVERIFY(toolsDock->maximumWidth() <= 64);
        QTRY_VERIFY_WITH_TIMEOUT(toolsDock->width() <= 80, 500);

        openDice->trigger();
        QVERIFY(!compactTools->isChecked());
        QVERIFY(!tabs->isHidden());
        QCOMPARE(launcher->orientation(), Qt::Horizontal);
        QCOMPARE(launcher->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
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

        toggleTools->trigger();
        QVERIFY(toolsDock->isHidden());
        toggleTools->trigger();
        QVERIFY(!toolsDock->isHidden());
        toggleTools->trigger();
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
