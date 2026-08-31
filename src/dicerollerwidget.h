#pragma once

#include <QVector>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;

class DiceRollerWidget final : public QWidget
{
    Q_OBJECT

public:
    using RandomIndexGenerator = std::function<int(int)>;

    explicit DiceRollerWidget(QWidget *parent = nullptr);
    void setRandomIndexGenerator(RandomIndexGenerator generator);
    void setRollAnimationDurationForTesting(int milliseconds);
    bool isRolling() const;

public slots:
    void roll();

private:
    QVector<int> randomValues(int count, int sides) const;
    void showValues(const QVector<int> &values);
    void finishRoll();

    QComboBox *m_dieSelector = nullptr;
    QSpinBox *m_countSelector = nullptr;
    QPushButton *m_rollButton = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_notationLabel = nullptr;
    QLabel *m_resultsLabel = nullptr;
    QLabel *m_feedbackLabel = nullptr;
    QListWidget *m_history = nullptr;
    QTimer *m_animationTimer = nullptr;
    RandomIndexGenerator m_randomIndex;
    QVector<int> m_finalValues;
    int m_finalSides = 20;
    int m_animationDuration = 650;
    bool m_rolling = false;
};
