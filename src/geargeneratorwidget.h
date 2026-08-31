#pragma once

#include <QStringList>
#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;

class GearGeneratorWidget final : public QWidget
{
    Q_OBJECT

public:
    using RandomIndexGenerator = std::function<int(int)>;

    explicit GearGeneratorWidget(QWidget *parent = nullptr);
    void setRandomIndexGenerator(RandomIndexGenerator generator);
    QStringList enabledCategories() const;

public slots:
    void generate();

private:
    void categoryToggled(QCheckBox *changed, bool checked);
    void saveCategories() const;
    int randomIndex(int upperBound) const;

    QList<QCheckBox *> m_categoryChecks;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_descriptionLabel = nullptr;
    QLabel *m_valueLabel = nullptr;
    QStringList m_recentEffects;
    RandomIndexGenerator m_randomIndexGenerator;
};
