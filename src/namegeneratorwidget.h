#pragma once

#include <QStringList>
#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

struct NameParts
{
    QStringList first;
    QStringList lastStart;
    QStringList lastEnd;
};

class NameGeneratorWidget final : public QWidget
{
    Q_OBJECT

public:
    using RandomIndexGenerator = std::function<int(int)>;

    explicit NameGeneratorWidget(QWidget *parent = nullptr);

    static QStringList styleNames();
    static NameParts builtInParts(const QString &style);
    static NameParts customParts(const QString &text);
    static NameParts pooledParts(const QStringList &styles, const QString &customText);

    void setRandomIndexGenerator(RandomIndexGenerator generator);
    QStringList selectedStyles() const;

public slots:
    void generate();

private:
    void styleToggled(QCheckBox *changed, bool checked);
    void savePreferences() const;
    int randomIndex(int upperBound) const;
    void copyName();

    QList<QCheckBox *> m_styleChecks;
    QLineEdit *m_customEdit = nullptr;
    QLabel *m_resultLabel = nullptr;
    QPushButton *m_copyButton = nullptr;
    RandomIndexGenerator m_randomIndexGenerator;
    int m_copySequence = 0;
};
