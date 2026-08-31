#include "mainwindow.h"
#include "platformmessagefilter.h"

#include <QAction>
#include <QCoreApplication>
#include <QDockWidget>
#include <QSettings>
#include <QStackedWidget>
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

    void toolIconsToggleSingleCleanToolWithoutTabs()
    {
        MainWindow window;
        auto *toolsDock = window.findChild<QDockWidget *>(QStringLiteral("toolsDock"));
        auto *toolsPanel = window.findChild<QWidget *>(QStringLiteral("toolsPanel"));
        auto *stack =
            window.findChild<QStackedWidget *>(QStringLiteral("toolContentStack"));
        auto *launcher =
            window.findChild<QToolBar *>(QStringLiteral("toolPanelLauncher"));
        auto *openDice = window.findChild<QAction *>(QStringLiteral("openDiceRollerAction"));
        auto *openWheel = window.findChild<QAction *>(QStringLiteral("openChanceWheelAction"));
        auto *openGear = window.findChild<QAction *>(QStringLiteral("openGearGeneratorAction"));
        auto *openName = window.findChild<QAction *>(QStringLiteral("openNameGeneratorAction"));
        auto *toggleTools =
            window.findChild<QAction *>(QStringLiteral("toggleToolsPanelAction"));
        QVERIFY(toolsDock);
        QVERIFY(toolsPanel);
        QVERIFY(stack);
        QVERIFY(launcher);
        QVERIFY(openDice);
        QVERIFY(openWheel);
        QVERIFY(openGear);
        QVERIFY(openName);
        QVERIFY(!openDice->icon().isNull());
        QVERIFY(!openWheel->icon().isNull());
        QVERIFY(!openGear->icon().isNull());
        QVERIFY(!openName->icon().isNull());
        QVERIFY(toggleTools);
        QVERIFY(toolsDock->features().testFlag(QDockWidget::DockWidgetClosable));
        QVERIFY(toolsDock->autoFillBackground());
        QVERIFY(toolsPanel->autoFillBackground());
        QVERIFY(stack->autoFillBackground());
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(toolsDock->isVisible(), 500);
        QCOMPARE(window.dockWidgetArea(toolsDock), Qt::LeftDockWidgetArea);
        QCOMPARE(stack->count(), 0);
        QVERIFY(stack->isHidden());
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

        openDice->trigger();
        QVERIFY(openDice->isChecked());
        QVERIFY(!stack->isHidden());
        QCOMPARE(launcher->orientation(), Qt::Vertical);
        QCOMPARE(launcher->toolButtonStyle(), Qt::ToolButtonIconOnly);
        QCOMPARE(stack->count(), 1);
        QWidget *firstDice = stack->currentWidget();
        QCOMPARE(firstDice->objectName(), QStringLiteral("diceRollerWidget"));
        window.resizeDocks({toolsDock}, {430}, Qt::Horizontal);
        QTRY_VERIFY_WITH_TIMEOUT(toolsDock->width() >= 400, 500);
        const int expandedWidth = toolsDock->width();

        openDice->trigger();
        QVERIFY(!openDice->isChecked());
        QVERIFY(stack->isHidden());
        QCOMPARE(stack->count(), 0);
        QVERIFY(toolsDock->maximumWidth() <= 64);
        QTRY_VERIFY_WITH_TIMEOUT(toolsDock->width() <= 80, 500);

        openDice->trigger();
        QCOMPARE(stack->count(), 1);
        QVERIFY(stack->currentWidget() != firstDice);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(toolsDock->width() - expandedWidth) <= 2, 500);
        openWheel->trigger();
        QVERIFY(!openDice->isChecked());
        QVERIFY(openWheel->isChecked());
        QCOMPARE(stack->count(), 1);
        QCOMPARE(
            stack->currentWidget()->objectName(),
            QStringLiteral("chanceWheelWidget"));

        toggleTools->trigger();
        QVERIFY(toolsDock->isHidden());
        toggleTools->trigger();
        QVERIFY(!toolsDock->isHidden());
        toggleTools->trigger();
        QVERIFY(toolsDock->isHidden());
        openWheel->trigger();
        QVERIFY(!toolsDock->isHidden());
        QVERIFY(!stack->isHidden());
        QCOMPARE(stack->count(), 1);
    }

    void cleanupTestCase()
    {
        QSettings().clear();
    }
};

QTEST_MAIN(MainWindowTest)
#include "mainwindowtest.moc"
