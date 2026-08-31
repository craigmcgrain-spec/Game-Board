#pragma once

#include <QList>
#include <QVector>
#include <QWidget>

#include <functional>

class DiceFaceWidget;
class QButtonGroup;
class QFrame;
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
    int currentSides() const;
    void updateResultCard(bool criticalSuccess, bool criticalFailure);

    QButtonGroup *m_dieButtons = nullptr;
    QList<QPushButton *> m_dieButtonList;
    QSpinBox *m_countSelector = nullptr;
    QPushButton *m_rollButton = nullptr;
    QFrame *m_resultFrame = nullptr;
    DiceFaceWidget *m_diceFace = nullptr;
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
    double m_animationPhase = 0.0;
    bool m_rolling = false;
};
