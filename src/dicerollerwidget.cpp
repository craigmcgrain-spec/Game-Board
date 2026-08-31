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
#include <QVBoxLayout>

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

    auto *rollButton = new QPushButton(tr("Roll"), this);
    rollButton->setObjectName(QStringLiteral("diceRollButton"));
    rollButton->setDefault(true);
    controls->addWidget(rollButton);
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

    connect(m_dieSelector, &QComboBox::currentIndexChanged, this, [this] {
        m_countSelector->setValue(1);
    });
    connect(rollButton, &QPushButton::clicked, this, &DiceRollerWidget::roll);
    connect(clearButton, &QPushButton::clicked, m_history, &QListWidget::clear);
}

void DiceRollerWidget::setRandomIndexGenerator(RandomIndexGenerator generator)
{
    if (generator) {
        m_randomIndex = std::move(generator);
    }
}

void DiceRollerWidget::roll()
{
    const int sides = m_dieSelector->currentData().toInt();
    const int count = m_countSelector->value();
    QStringList resultTexts;
    int total = 0;
    for (int index = 0; index < count; ++index) {
        const int value = m_randomIndex(sides) + 1;
        total += value;
        resultTexts.append(QString::number(value));
    }

    const QString notation = QStringLiteral("%1d%2").arg(count).arg(sides);
    m_totalLabel->setText(QString::number(total));
    m_notationLabel->setText(notation);
    m_resultsLabel->setText(tr("Results: %1").arg(resultTexts.join(QStringLiteral(", "))));

    m_feedbackLabel->clear();
    m_feedbackLabel->setStyleSheet({});
    if (count == 1 && sides == 20 && total == 20) {
        m_feedbackLabel->setText(tr("Natural 20!"));
        m_feedbackLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
    } else if (count == 1 && sides == 20 && total == 1) {
        m_feedbackLabel->setText(tr("Critical failure!"));
        m_feedbackLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
    }

    m_history->insertItem(
        0,
        tr("%1: %2 (%3)").arg(notation, QString::number(total), resultTexts.join(QStringLiteral(", "))));
    while (m_history->count() > 20) {
        delete m_history->takeItem(m_history->count() - 1);
    }
}
