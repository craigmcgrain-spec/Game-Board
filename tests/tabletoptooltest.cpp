#include "chancewheelwidget.h"
#include "dicerollerwidget.h"
#include "geargeneratorwidget.h"
#include "namegeneratorwidget.h"

#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>
#include <QtTest>

#include <limits>

class TabletopToolTest final : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("HexboardTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TabletopToolTests"));
        QSettings().clear();
    }

    void diceRollsDeterministicallyAndResetsCount()
    {
        DiceRollerWidget widget;
        widget.setRollAnimationDurationForTesting(0);
        widget.setRandomIndexGenerator([](int upperBound) {
            return upperBound - 1;
        });
        auto *d6 = widget.findChild<QPushButton *>(QStringLiteral("diceTypeD6Button"));
        auto *animatedFace = widget.findChild<QWidget *>(QStringLiteral("diceAnimatedFace"));
        auto *count = widget.findChild<QSpinBox *>(QStringLiteral("diceCountSelector"));
        auto *total = widget.findChild<QLabel *>(QStringLiteral("diceTotalLabel"));
        auto *notation = widget.findChild<QLabel *>(QStringLiteral("diceNotationLabel"));
        auto *feedback = widget.findChild<QLabel *>(QStringLiteral("diceCriticalFeedbackLabel"));
        auto *history = widget.findChild<QListWidget *>(QStringLiteral("diceHistoryList"));
        QVERIFY(d6);
        QVERIFY(animatedFace);
        QVERIFY(count);
        QVERIFY(total);
        QVERIFY(notation);
        QVERIFY(feedback);
        QVERIFY(history);

        widget.roll();
        QCOMPARE(total->text(), QStringLiteral("Total: 20"));
        QCOMPARE(notation->text(), QStringLiteral("1d20"));
        QCOMPARE(feedback->text(), QStringLiteral("Natural 20!"));
        QCOMPARE(history->count(), 1);

        count->setValue(4);
        for (const int sides : {4, 6, 8, 10, 12, 20, 100}) {
            auto *button = widget.findChild<QPushButton *>(
                QStringLiteral("diceTypeD%1Button").arg(sides));
            QVERIFY(button);
            button->click();
            QCOMPARE(count->value(), 1);
            QCOMPARE(animatedFace->property("dieSides").toInt(), sides);
        }
        d6->click();

        for (int index = 0; index < 25; ++index) {
            widget.roll();
        }
        QCOMPARE(history->count(), 20);
        QVERIFY(history->item(0)->text().startsWith(QStringLiteral("1d6: 6")));
        for (int index = 0; index < history->count(); ++index) {
            QVERIFY(!history->item(index)->text().startsWith(QStringLiteral("1d20:")));
        }
    }

    void diceShowsAndLocksDuringRollAnimation()
    {
        DiceRollerWidget widget;
        int nextValue = 0;
        widget.setRandomIndexGenerator([&nextValue](int upperBound) {
            return nextValue++ % upperBound;
        });
        widget.setRollAnimationDurationForTesting(120);
        auto *die = widget.findChild<QPushButton *>(QStringLiteral("diceTypeD20Button"));
        auto *count = widget.findChild<QSpinBox *>(QStringLiteral("diceCountSelector"));
        auto *roll = widget.findChild<QPushButton *>(QStringLiteral("diceRollButton"));
        auto *history = widget.findChild<QListWidget *>(QStringLiteral("diceHistoryList"));
        QVERIFY(die);
        QVERIFY(count);
        QVERIFY(roll);
        QVERIFY(history);

        widget.roll();
        QVERIFY(widget.isRolling());
        QVERIFY(!die->isEnabled());
        QVERIFY(!count->isEnabled());
        QVERIFY(!roll->isEnabled());
        QCOMPARE(roll->text(), QStringLiteral("Rolling..."));
        QCOMPARE(history->count(), 0);

        QTRY_VERIFY_WITH_TIMEOUT(!widget.isRolling(), 500);
        QVERIFY(die->isEnabled());
        QVERIFY(count->isEnabled());
        QVERIFY(roll->isEnabled());
        QCOMPARE(roll->text(), QStringLiteral("Roll Dice"));
        QCOMPARE(history->count(), 1);
    }

    void wheelGeometryAndValidation()
    {
        const QVector<WheelSegment> segments{
            {QStringLiteral("One"), 1.0, Qt::red},
            {QStringLiteral("Two"), 3.0, Qt::blue}
        };
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(segments, 0.0), 0);
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(segments, 89.999), 0);
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(segments, 90.0), 1);
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(segments, 359.999), 1);
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(segments, 360.0), 0);

        QString error;
        QVERIFY(ChanceWheelWidget::validateSegments({}, &error));
        QVERIFY(!ChanceWheelWidget::validateSegments(
            {{QString(), 1.0, Qt::red}},
            &error));
        QVERIFY(!ChanceWheelWidget::validateSegments(
            {{QStringLiteral("Bad"), std::numeric_limits<double>::infinity(), Qt::red}},
            &error));
        QVERIFY(!ChanceWheelWidget::validateSegments(
            {{QStringLiteral("Bad"), 0.0, Qt::red}},
            &error));

        const QVector<WheelSegment> hugeWeights{
            {QStringLiteral("Huge One"), 1.0e308, Qt::red},
            {QStringLiteral("Huge Two"), 1.0e308, Qt::blue}
        };
        QVERIFY(ChanceWheelWidget::validateSegments(hugeWeights, &error));
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(hugeWeights, 179.0), 0);
        QCOMPARE(ChanceWheelWidget::segmentIndexAtAngle(hugeWeights, 180.0), 1);
    }

    void wheelLocksDuringSpinAndUsesPointerResult()
    {
        ChanceWheelWidget widget;
        widget.setSegmentsForTesting({
            {QStringLiteral("First"), 1.0, Qt::red},
            {QStringLiteral("Second"), 1.0, Qt::blue}
        });
        widget.setRandomAngleGenerator([] {
            return 90.0;
        });
        widget.setSpinDurationForTesting(30);
        auto *spin = widget.findChild<QPushButton *>(QStringLiteral("chanceWheelSpinButton"));
        auto *status = widget.findChild<QLabel *>(QStringLiteral("chanceWheelStatusLabel"));
        auto *edit = widget.findChild<QToolButton *>(QStringLiteral("chanceWheelEditToggle"));
        QVERIFY(spin);
        QVERIFY(status);
        QVERIFY(edit);

        widget.spin();
        QVERIFY(widget.isSpinning());
        QVERIFY(!spin->isEnabled());
        QVERIFY(!edit->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!widget.isSpinning(), 500);
        QCOMPARE(status->text(), QStringLiteral("Selected: Second"));
        QVERIFY(spin->isEnabled());
        QVERIFY(edit->isEnabled());
    }

    void gearKeepsOneCategoryAndDoesNotRegenerateOnToggle()
    {
        GearGeneratorWidget widget;
        auto *name = widget.findChild<QLabel *>(QStringLiteral("gearNameLabel"));
        QVERIFY(name);
        QVERIFY(!name->text().isEmpty());
        const QString originalName = name->text();

        const auto checks = widget.findChildren<QCheckBox *>();
        QCOMPARE(checks.size(), 5);
        for (int index = 1; index < checks.size(); ++index) {
            checks.at(index)->setChecked(false);
        }
        checks.first()->setChecked(false);
        QCOMPARE(widget.enabledCategories().size(), 1);
        QCOMPARE(name->text(), originalName);
    }

    void customNamesPoolAndFallback()
    {
        const NameParts custom = NameGeneratorWidget::customParts(
            QStringLiteral("Oak, RUNE! x 42"));
        QCOMPARE(custom.first.size(), 6);
        QCOMPARE(custom.lastStart.size(), 6);
        QCOMPARE(custom.lastEnd.size(), 6);
        QVERIFY(custom.first.contains(QStringLiteral("Oak")));
        QVERIFY(custom.first.contains(QStringLiteral("Rune")));

        const NameParts friendly = NameGeneratorWidget::builtInParts(QStringLiteral("Friendly"));
        QCOMPARE(friendly.first.size(), friendly.lastStart.size());
        QCOMPARE(friendly.first.size(), friendly.lastEnd.size());

        const NameParts fallback = NameGeneratorWidget::pooledParts(
            {QStringLiteral("Custom")},
            QStringLiteral("x"));
        QCOMPARE(fallback.first, friendly.first);

        const NameParts mixed = NameGeneratorWidget::pooledParts(
            {QStringLiteral("Friendly"), QStringLiteral("Boss")},
            {});
        QCOMPARE(mixed.first.size(), friendly.first.size() * 2);
    }

    void nameGeneratorKeepsOneStyle()
    {
        NameGeneratorWidget widget;
        auto *friendly = widget.findChild<QCheckBox *>(QStringLiteral("nameStyleFriendly"));
        auto *result = widget.findChild<QLabel *>(QStringLiteral("generatedNameLabel"));
        QVERIFY(friendly);
        QVERIFY(result);
        QVERIFY(!result->text().isEmpty());
        friendly->setChecked(false);
        QCOMPARE(widget.selectedStyles(), QStringList{QStringLiteral("Friendly")});
    }

    void cleanup()
    {
        QSettings().clear();
    }
};

QTEST_MAIN(TabletopToolTest)
#include "tabletoptooltest.moc"
