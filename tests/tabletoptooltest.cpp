#include "chancewheelwidget.h"
#include "dicerollerwidget.h"
#include "geargeneratorwidget.h"
#include "namegeneratorwidget.h"

#include <QCheckBox>
#include <QComboBox>
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
        widget.setRandomIndexGenerator([](int upperBound) {
            return upperBound - 1;
        });
        auto *die = widget.findChild<QComboBox *>(QStringLiteral("diceTypeSelector"));
        auto *count = widget.findChild<QSpinBox *>(QStringLiteral("diceCountSelector"));
        auto *total = widget.findChild<QLabel *>(QStringLiteral("diceTotalLabel"));
        auto *notation = widget.findChild<QLabel *>(QStringLiteral("diceNotationLabel"));
        auto *feedback = widget.findChild<QLabel *>(QStringLiteral("diceCriticalFeedbackLabel"));
        auto *history = widget.findChild<QListWidget *>(QStringLiteral("diceHistoryList"));
        QVERIFY(die);
        QVERIFY(count);
        QVERIFY(total);
        QVERIFY(notation);
        QVERIFY(feedback);
        QVERIFY(history);

        widget.roll();
        QCOMPARE(total->text(), QStringLiteral("20"));
        QCOMPARE(notation->text(), QStringLiteral("1d20"));
        QCOMPARE(feedback->text(), QStringLiteral("Natural 20!"));
        QCOMPARE(history->count(), 1);

        count->setValue(4);
        die->setCurrentIndex(die->findData(6));
        QCOMPARE(count->value(), 1);

        for (int index = 0; index < 25; ++index) {
            widget.roll();
        }
        QCOMPARE(history->count(), 20);
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
