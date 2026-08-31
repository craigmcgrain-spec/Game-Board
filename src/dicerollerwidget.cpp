#include "dicerollerwidget.h"

#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <numeric>
#include <utility>

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

    auto *title = new QLabel(tr("Dice Roller"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *controls = new QHBoxLayout;
    auto *form = new QFormLayout;
    m_dieSelector = new QComboBox(this);
    m_dieSelector->setObjectName(QStringLiteral("diceTypeSelector"));
    for (const int sides : {4, 6, 8, 10, 12, 20, 100}) {
        m_dieSelector->addItem(tr("d%1").arg(sides), sides);
    }
    m_dieSelector->setCurrentIndex(m_dieSelector->findData(20));
    form->addRow(tr("Die:"), m_dieSelector);

    m_countSelector = new QSpinBox(this);
    m_countSelector->setObjectName(QStringLiteral("diceCountSelector"));
    m_countSelector->setRange(1, 10);
    m_countSelector->setValue(1);
    form->addRow(tr("Count:"), m_countSelector);
    controls->addLayout(form);

    m_rollButton = new QPushButton(tr("Roll"), this);
    m_rollButton->setObjectName(QStringLiteral("diceRollButton"));
    m_rollButton->setDefault(true);
    controls->addWidget(m_rollButton);
    controls->addStretch();
    layout->addLayout(controls);

    auto *resultFrame = new QFrame(this);
    resultFrame->setFrameShape(QFrame::StyledPanel);
    auto *resultLayout = new QVBoxLayout(resultFrame);
    m_totalLabel = new QLabel(QStringLiteral("-"), resultFrame);
    m_totalLabel->setObjectName(QStringLiteral("diceTotalLabel"));
    m_totalLabel->setAlignment(Qt::AlignCenter);
    QFont totalFont = m_totalLabel->font();
    totalFont.setBold(true);
    totalFont.setPointSize(totalFont.pointSize() + 10);
    m_totalLabel->setFont(totalFont);
    resultLayout->addWidget(m_totalLabel);

    m_notationLabel = new QLabel(resultFrame);
    m_notationLabel->setObjectName(QStringLiteral("diceNotationLabel"));
    m_notationLabel->setAlignment(Qt::AlignCenter);
    resultLayout->addWidget(m_notationLabel);

    m_resultsLabel = new QLabel(resultFrame);
    m_resultsLabel->setObjectName(QStringLiteral("diceIndividualResultsLabel"));
    m_resultsLabel->setAlignment(Qt::AlignCenter);
    m_resultsLabel->setWordWrap(true);
    resultLayout->addWidget(m_resultsLabel);

    m_feedbackLabel = new QLabel(resultFrame);
    m_feedbackLabel->setObjectName(QStringLiteral("diceCriticalFeedbackLabel"));
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    QFont feedbackFont = m_feedbackLabel->font();
    feedbackFont.setBold(true);
    m_feedbackLabel->setFont(feedbackFont);
    resultLayout->addWidget(m_feedbackLabel);
    layout->addWidget(resultFrame);

    auto *historyHeader = new QHBoxLayout;
    auto *historyTitle = new QLabel(tr("History"), this);
    QFont historyFont = historyTitle->font();
    historyFont.setBold(true);
    historyTitle->setFont(historyFont);
    historyHeader->addWidget(historyTitle);
    historyHeader->addStretch();
    auto *clearButton = new QPushButton(tr("Clear History"), this);
    clearButton->setObjectName(QStringLiteral("diceClearHistoryButton"));
    historyHeader->addWidget(clearButton);
    layout->addLayout(historyHeader);

    m_history = new QListWidget(this);
    m_history->setObjectName(QStringLiteral("diceHistoryList"));
    m_history->setAlternatingRowColors(true);
    layout->addWidget(m_history, 1);

    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(55);
    connect(m_dieSelector, &QComboBox::currentIndexChanged, this, [this] {
        m_countSelector->setValue(1);
    });
    connect(m_rollButton, &QPushButton::clicked, this, &DiceRollerWidget::roll);
    connect(m_animationTimer, &QTimer::timeout, this, [this] {
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

    m_finalSides = m_dieSelector->currentData().toInt();
    const int count = m_countSelector->value();
    m_finalValues = randomValues(count, m_finalSides);
    m_notationLabel->setText(QStringLiteral("%1d%2").arg(count).arg(m_finalSides));
    m_feedbackLabel->clear();
    m_feedbackLabel->setStyleSheet({});

    if (m_animationDuration == 0) {
        finishRoll();
        return;
    }

    m_rolling = true;
    m_dieSelector->setEnabled(false);
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
    m_totalLabel->setText(QString::number(total));
    m_resultsLabel->setText(tr("Results: %1").arg(resultTexts.join(QStringLiteral(", "))));
}

void DiceRollerWidget::finishRoll()
{
    m_animationTimer->stop();
    showValues(m_finalValues);
    const int total = std::accumulate(m_finalValues.cbegin(), m_finalValues.cend(), 0);
    if (m_finalValues.size() == 1 && m_finalSides == 20 && total == 20) {
        m_feedbackLabel->setText(tr("Natural 20!"));
        m_feedbackLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
    } else if (m_finalValues.size() == 1 && m_finalSides == 20 && total == 1) {
        m_feedbackLabel->setText(tr("Critical failure!"));
        m_feedbackLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
    }

    QStringList resultTexts;
    resultTexts.reserve(m_finalValues.size());
    for (const int value : std::as_const(m_finalValues)) {
        resultTexts.append(QString::number(value));
    }
    const QString notation =
        QStringLiteral("%1d%2").arg(m_finalValues.size()).arg(m_finalSides);
    m_history->insertItem(
        0,
        tr("%1: %2 (%3)").arg(notation, QString::number(total), resultTexts.join(QStringLiteral(", "))));
    while (m_history->count() > 20) {
        delete m_history->takeItem(m_history->count() - 1);
    }

    m_rolling = false;
    m_dieSelector->setEnabled(true);
    m_countSelector->setEnabled(true);
    m_rollButton->setEnabled(true);
    m_rollButton->setText(tr("Roll"));
}
