#include "dicerollerwidget.h"

#include <QButtonGroup>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

class DiceFaceWidget final : public QWidget
{
public:
    explicit DiceFaceWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("diceAnimatedFace"));
        setFixedSize(92, 92);
    }

    void setRoll(int sides, int value, double phase, bool rolling)
    {
        m_sides = sides;
        m_value = value;
        m_phase = phase;
        m_rolling = rolling;
        setProperty("dieSides", sides);
        setAccessibleName(tr("Animated d%1 die").arg(sides));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(rect().center());
        if (m_rolling) {
            painter.rotate(m_phase);
            const double pulse = 0.9 + 0.1 * std::sin(qDegreesToRadians(m_phase * 2.0));
            painter.scale(pulse, pulse);
        }

        QColor faceColor = palette().color(QPalette::Highlight);
        if (!isEnabled()) {
            faceColor = palette().color(QPalette::Mid);
        }
        const QPainterPath diePath = outlinePath();
        painter.setPen(QPen(faceColor.darker(135), 3.0));
        painter.setBrush(faceColor);
        painter.drawPath(diePath);
        drawFacets(&painter, diePath, faceColor);

        painter.setPen(palette().color(QPalette::HighlightedText));
        QFont valueFont = painter.font();
        valueFont.setBold(true);
        valueFont.setPointSize(valueFont.pointSize() + (m_value >= 100 ? 6 : 10));
        painter.setFont(valueFont);
        const QRectF valueRect(-34.0, -28.0, 68.0, 49.0);
        painter.drawText(valueRect, Qt::AlignCenter, QString::number(m_value));

        QFont typeFont = painter.font();
        typeFont.setPointSize(std::max(7, typeFont.pointSize() - 10));
        painter.setFont(typeFont);
        painter.drawText(
            QRectF(-32.0, 18.0, 64.0, 20.0),
            Qt::AlignHCenter,
            QStringLiteral("d%1").arg(m_sides));
    }

private:
    static QPolygonF regularPolygon(int vertices, double radius, double startDegrees = -90.0)
    {
        QPolygonF polygon;
        polygon.reserve(vertices);
        for (int index = 0; index < vertices; ++index) {
            const double angle =
                qDegreesToRadians(startDegrees + index * 360.0 / vertices);
            polygon.append(QPointF(std::cos(angle) * radius, std::sin(angle) * radius));
        }
        return polygon;
    }

    QPainterPath outlinePath() const
    {
        QPainterPath path;
        if (m_sides == 100) {
            path.addEllipse(QRectF(-39.0, -39.0, 78.0, 78.0));
            return path;
        }

        QPolygonF polygon;
        switch (m_sides) {
        case 4:
            polygon = {{0.0, -41.0}, {38.0, 31.0}, {-38.0, 31.0}};
            break;
        case 6:
            polygon = {{-33.0, -33.0}, {27.0, -38.0}, {39.0, -23.0},
                       {34.0, 29.0}, {7.0, 39.0}, {-37.0, 24.0}};
            break;
        case 8:
            polygon = {{0.0, -42.0}, {37.0, 0.0}, {0.0, 42.0}, {-37.0, 0.0}};
            break;
        case 10:
            polygon = {{0.0, -42.0}, {31.0, -18.0}, {38.0, 12.0},
                       {0.0, 40.0}, {-38.0, 12.0}, {-31.0, -18.0}};
            break;
        case 12:
            polygon = regularPolygon(10, 41.0, -90.0);
            break;
        case 20:
        default:
            polygon = regularPolygon(6, 42.0, -90.0);
            break;
        }
        path.addPolygon(polygon);
        path.closeSubpath();
        return path;
    }

    void drawFacets(QPainter *painter, const QPainterPath &diePath, const QColor &faceColor) const
    {
        painter->save();
        painter->setClipPath(diePath);
        QColor facetColor = faceColor.lighter(150);
        facetColor.setAlpha(150);
        painter->setPen(QPen(facetColor, 1.5));
        painter->setBrush(Qt::NoBrush);

        if (m_sides == 4) {
            const QPointF center(0.0, 10.0);
            painter->drawLine(center, QPointF(0.0, -41.0));
            painter->drawLine(center, QPointF(38.0, 31.0));
            painter->drawLine(center, QPointF(-38.0, 31.0));
        } else if (m_sides == 6) {
            const QPointF center(4.0, -6.0);
            painter->drawLine(QPointF(-33.0, -33.0), center);
            painter->drawLine(QPointF(27.0, -38.0), center);
            painter->drawLine(QPointF(39.0, -23.0), center);
            painter->drawLine(QPointF(7.0, 39.0), center);
            painter->drawLine(QPointF(-37.0, 24.0), center);
        } else if (m_sides == 8) {
            painter->drawLine(QPointF(-37.0, 0.0), QPointF(37.0, 0.0));
            painter->drawLine(QPointF(0.0, -42.0), QPointF(0.0, 42.0));
            painter->drawLine(QPointF(-37.0, 0.0), QPointF(0.0, 12.0));
            painter->drawLine(QPointF(37.0, 0.0), QPointF(0.0, 12.0));
        } else if (m_sides == 10) {
            for (const QPointF &point : QPolygonF{
                     {0.0, -42.0}, {31.0, -18.0}, {38.0, 12.0},
                     {0.0, 40.0}, {-38.0, 12.0}, {-31.0, -18.0}}) {
                painter->drawLine(QPointF(0.0, 4.0), point);
            }
        } else if (m_sides == 12) {
            const QPolygonF inner = regularPolygon(5, 22.0, -90.0);
            painter->drawPolygon(inner);
            const QPolygonF outer = regularPolygon(10, 41.0, -90.0);
            for (int index = 0; index < inner.size(); ++index) {
                painter->drawLine(inner.at(index), outer.at(index * 2));
                painter->drawLine(inner.at(index), outer.at((index * 2 + 1) % outer.size()));
            }
        } else if (m_sides == 20) {
            const QPolygonF outer = regularPolygon(6, 42.0, -90.0);
            for (const QPointF &point : outer) {
                painter->drawLine(QPointF(0.0, 0.0), point);
            }
            painter->drawPolygon(regularPolygon(3, 23.0, -90.0));
        } else {
            painter->drawEllipse(QRectF(-18.0, -39.0, 36.0, 78.0));
            painter->drawEllipse(QRectF(-32.0, -39.0, 64.0, 78.0));
            painter->drawLine(QPointF(-39.0, 0.0), QPointF(39.0, 0.0));
        }
        painter->restore();
    }

    int m_sides = 20;
    int m_value = 20;
    double m_phase = 0.0;
    bool m_rolling = false;
};

DiceRollerWidget::DiceRollerWidget(QWidget *parent)
    : QWidget(parent)
    , m_randomIndex([](int upperBound) {
        return static_cast<int>(QRandomGenerator::global()->bounded(
            static_cast<quint32>(upperBound)));
    })
{
    setObjectName(QStringLiteral("diceRollerWidget"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Dice Roller"), this);
    title->setObjectName(QStringLiteral("diceRollerTitle"));
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 5);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_dieButtons = new QButtonGroup(this);
    m_dieButtons->setExclusive(true);
    auto *diceGrid = new QGridLayout;
    diceGrid->setSpacing(6);
    int buttonIndex = 0;
    for (const int sides : {4, 6, 8, 10, 12, 20, 100}) {
        auto *button = new QPushButton(tr("d%1").arg(sides), this);
        button->setObjectName(QStringLiteral("diceTypeD%1Button").arg(sides));
        button->setCheckable(true);
        button->setMinimumHeight(40);
        button->setFlat(sides != 20);
        button->setChecked(sides == 20);
        m_dieButtons->addButton(button, sides);
        m_dieButtonList.append(button);
        diceGrid->addWidget(button, buttonIndex / 4, buttonIndex % 4);
        connect(button, &QPushButton::toggled, this, [button](bool checked) {
            button->setFlat(!checked);
        });
        ++buttonIndex;
    }
    layout->addLayout(diceGrid);

    auto *countLayout = new QHBoxLayout;
    countLayout->addWidget(new QLabel(tr("Count:"), this));
    m_countSelector = new QSpinBox(this);
    m_countSelector->setObjectName(QStringLiteral("diceCountSelector"));
    m_countSelector->setRange(1, 10);
    m_countSelector->setValue(1);
    m_countSelector->setMinimumHeight(36);
    countLayout->addWidget(m_countSelector, 1);
    layout->addLayout(countLayout);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    m_rollButton = new QPushButton(QIcon::fromTheme(QStringLiteral("games-dice")), tr("Roll Dice"), this);
    m_rollButton->setObjectName(QStringLiteral("diceRollButton"));
    m_rollButton->setDefault(true);
    m_rollButton->setMinimumHeight(52);
    QFont rollFont = m_rollButton->font();
    rollFont.setBold(true);
    rollFont.setPointSize(rollFont.pointSize() + 2);
    m_rollButton->setFont(rollFont);
    layout->addWidget(m_rollButton);

    m_resultFrame = new QFrame(this);
    m_resultFrame->setObjectName(QStringLiteral("diceResultCard"));
    m_resultFrame->setMinimumHeight(178);
    auto *resultLayout = new QVBoxLayout(m_resultFrame);
    resultLayout->setContentsMargins(14, 12, 14, 12);
    m_diceFace = new DiceFaceWidget(m_resultFrame);
    m_diceFace->setRoll(20, 20, 0.0, false);
    resultLayout->addWidget(m_diceFace, 0, Qt::AlignHCenter);

    m_totalLabel = new QLabel(tr("Roll to see results"), m_resultFrame);
    m_totalLabel->setObjectName(QStringLiteral("diceTotalLabel"));
    m_totalLabel->setAlignment(Qt::AlignCenter);
    QFont totalFont = m_totalLabel->font();
    totalFont.setBold(true);
    totalFont.setPointSize(totalFont.pointSize() + 8);
    m_totalLabel->setFont(totalFont);
    resultLayout->addWidget(m_totalLabel);

    m_notationLabel = new QLabel(m_resultFrame);
    m_notationLabel->setObjectName(QStringLiteral("diceNotationLabel"));
    m_notationLabel->setAlignment(Qt::AlignCenter);
    resultLayout->addWidget(m_notationLabel);

    m_resultsLabel = new QLabel(m_resultFrame);
    m_resultsLabel->setObjectName(QStringLiteral("diceIndividualResultsLabel"));
    m_resultsLabel->setAlignment(Qt::AlignCenter);
    m_resultsLabel->setWordWrap(true);
    resultLayout->addWidget(m_resultsLabel);

    m_feedbackLabel = new QLabel(m_resultFrame);
    m_feedbackLabel->setObjectName(QStringLiteral("diceCriticalFeedbackLabel"));
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    QFont feedbackFont = m_feedbackLabel->font();
    feedbackFont.setBold(true);
    feedbackFont.setPointSize(feedbackFont.pointSize() + 2);
    m_feedbackLabel->setFont(feedbackFont);
    resultLayout->addWidget(m_feedbackLabel);
    layout->addWidget(m_resultFrame);
    updateResultCard(false, false);

    auto *historyTitle = new QLabel(tr("History:"), this);
    QFont historyFont = historyTitle->font();
    historyFont.setBold(true);
    historyFont.setPointSize(historyFont.pointSize() + 1);
    historyTitle->setFont(historyFont);
    layout->addWidget(historyTitle);

    m_history = new QListWidget(this);
    m_history->setObjectName(QStringLiteral("diceHistoryList"));
    m_history->setAlternatingRowColors(true);
    m_history->setSpacing(2);
    layout->addWidget(m_history, 1);

    auto *clearButton = new QPushButton(tr("Clear History"), this);
    clearButton->setObjectName(QStringLiteral("diceClearHistoryButton"));
    clearButton->setFlat(true);
    layout->addWidget(clearButton);

    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(50);
    connect(m_dieButtons, &QButtonGroup::idClicked, this, [this](int sides) {
        m_countSelector->setValue(1);
        m_diceFace->setRoll(sides, sides, 0.0, false);
    });
    connect(m_rollButton, &QPushButton::clicked, this, &DiceRollerWidget::roll);
    connect(m_animationTimer, &QTimer::timeout, this, [this] {
        m_animationPhase += 38.0;
        showValues(randomValues(m_finalValues.size(), m_finalSides));
    });
    connect(clearButton, &QPushButton::clicked, m_history, &QListWidget::clear);
}

void DiceRollerWidget::setRandomIndexGenerator(RandomIndexGenerator generator)
{
    if (generator) {
        m_randomIndex = std::move(generator);
    }
}

void DiceRollerWidget::setRollAnimationDurationForTesting(int milliseconds)
{
    m_animationDuration = std::max(0, milliseconds);
}

bool DiceRollerWidget::isRolling() const
{
    return m_rolling;
}

void DiceRollerWidget::roll()
{
    if (m_rolling) {
        return;
    }

    m_finalSides = currentSides();
    const int count = m_countSelector->value();
    m_finalValues = randomValues(count, m_finalSides);
    m_notationLabel->setText(QStringLiteral("%1d%2").arg(count).arg(m_finalSides));
    m_feedbackLabel->clear();
    updateResultCard(false, false);

    if (m_animationDuration == 0) {
        finishRoll();
        return;
    }

    m_rolling = true;
    m_animationPhase = 0.0;
    for (QPushButton *button : std::as_const(m_dieButtonList)) {
        button->setEnabled(false);
    }
    m_countSelector->setEnabled(false);
    m_rollButton->setEnabled(false);
    m_rollButton->setText(tr("Rolling..."));
    showValues(randomValues(count, m_finalSides));
    m_animationTimer->start();
    QTimer::singleShot(m_animationDuration, this, &DiceRollerWidget::finishRoll);
}

QVector<int> DiceRollerWidget::randomValues(int count, int sides) const
{
    QVector<int> values;
    values.reserve(count);
    for (int index = 0; index < count; ++index) {
        const int generated = m_randomIndex(sides);
        const int normalized = generated % sides;
        values.append((normalized < 0 ? normalized + sides : normalized) + 1);
    }
    return values;
}

void DiceRollerWidget::showValues(const QVector<int> &values)
{
    QStringList resultTexts;
    resultTexts.reserve(values.size());
    for (const int value : values) {
        resultTexts.append(QString::number(value));
    }
    const int total = std::accumulate(values.cbegin(), values.cend(), 0);
    m_totalLabel->setText(tr("Total: %1").arg(total));
    m_resultsLabel->setText(QStringLiteral("[%1]").arg(resultTexts.join(QStringLiteral(", "))));
    m_diceFace->setRoll(m_finalSides, total, m_animationPhase, m_rolling);
}

void DiceRollerWidget::finishRoll()
{
    m_animationTimer->stop();
    m_rolling = false;
    m_animationPhase = 0.0;
    showValues(m_finalValues);
    const int total = std::accumulate(m_finalValues.cbegin(), m_finalValues.cend(), 0);
    const bool criticalSuccess =
        m_finalValues.size() == 1 && m_finalSides == 20 && total == 20;
    const bool criticalFailure =
        m_finalValues.size() == 1 && m_finalSides == 20 && total == 1;
    if (criticalSuccess) {
        m_feedbackLabel->setText(tr("Natural 20!"));
    } else if (criticalFailure) {
        m_feedbackLabel->setText(tr("Critical failure!"));
    }
    updateResultCard(criticalSuccess, criticalFailure);

    QStringList resultTexts;
    resultTexts.reserve(m_finalValues.size());
    for (const int value : std::as_const(m_finalValues)) {
        resultTexts.append(QString::number(value));
    }
    const QString notation =
        QStringLiteral("%1d%2").arg(m_finalValues.size()).arg(m_finalSides);
    auto *item = new QListWidgetItem(
        tr("%1: %2\n[%3]").arg(notation, QString::number(total), resultTexts.join(QStringLiteral(", "))));
    item->setSizeHint(QSize(item->sizeHint().width(), 46));
    m_history->insertItem(0, item);
    while (m_history->count() > 20) {
        delete m_history->takeItem(m_history->count() - 1);
    }

    for (QPushButton *button : std::as_const(m_dieButtonList)) {
        button->setEnabled(true);
    }
    m_countSelector->setEnabled(true);
    m_rollButton->setEnabled(true);
    m_rollButton->setText(tr("Roll Dice"));
}

int DiceRollerWidget::currentSides() const
{
    const int checked = m_dieButtons->checkedId();
    return checked > 0 ? checked : 20;
}

void DiceRollerWidget::updateResultCard(bool criticalSuccess, bool criticalFailure)
{
    if (criticalSuccess) {
        m_resultFrame->setStyleSheet(QStringLiteral(
            "QFrame#diceResultCard { background-color: rgba(76, 175, 80, 55);"
            " border: 2px solid #4caf50; border-radius: 8px; }"));
        m_feedbackLabel->setStyleSheet(QStringLiteral("color: #4caf50;"));
    } else if (criticalFailure) {
        m_resultFrame->setStyleSheet(QStringLiteral(
            "QFrame#diceResultCard { background-color: rgba(244, 67, 54, 55);"
            " border: 2px solid #f44336; border-radius: 8px; }"));
        m_feedbackLabel->setStyleSheet(QStringLiteral("color: #f44336;"));
    } else {
        m_resultFrame->setStyleSheet(QStringLiteral(
            "QFrame#diceResultCard { border: 2px solid palette(mid); border-radius: 8px; }"));
        m_feedbackLabel->setStyleSheet({});
    }
}
