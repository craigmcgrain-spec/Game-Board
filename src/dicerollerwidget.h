#pragma once

#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;

class DiceRollerWidget final : public QWidget
{
    Q_OBJECT

public:
    using RandomIndexGenerator = std::function<int(int)>;

    explicit DiceRollerWidget(QWidget *parent = nullptr);
    void setRandomIndexGenerator(RandomIndexGenerator generator);

public slots:
    void roll();

private:
    QComboBox *m_dieSelector = nullptr;
    QSpinBox *m_countSelector = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_notationLabel = nullptr;
    QLabel *m_resultsLabel = nullptr;
    QLabel *m_feedbackLabel = nullptr;
    QListWidget *m_history = nullptr;
    RandomIndexGenerator m_randomIndex;
};
