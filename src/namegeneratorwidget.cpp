#include "namegeneratorwidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {
constexpr auto StylesKey = "tools/nameGenerator/styles";
constexpr auto CustomKey = "tools/nameGenerator/customFragment";

QString capitalized(QString value)
{
    if (!value.isEmpty()) {
        value[0] = value.at(0).toUpper();
    }
    return value;
}

NameParts parts(
    std::initializer_list<const char *> first,
    std::initializer_list<const char *> start,
    std::initializer_list<const char *> end)
{
    NameParts result;
    for (const char *value : first) {
        result.first.append(QString::fromLatin1(value));
    }
    for (const char *value : start) {
        result.lastStart.append(QString::fromLatin1(value));
    }
    for (const char *value : end) {
        result.lastEnd.append(QString::fromLatin1(value));
    }
    return result;
}

void appendParts(NameParts *destination, const NameParts &source)
{
    destination->first.append(source.first);
    destination->lastStart.append(source.lastStart);
    destination->lastEnd.append(source.lastEnd);
}
}

NameGeneratorWidget::NameGeneratorWidget(QWidget *parent)
    : QWidget(parent)
    , m_randomIndexGenerator([](int upperBound) {
        return static_cast<int>(QRandomGenerator::global()->bounded(
            static_cast<quint32>(upperBound)));
    })
{
    setObjectName(QStringLiteral("nameGeneratorWidget"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(tr("Name Generator"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    layout->addWidget(title);

    QSettings settings;
    const QStringList validStyles = styleNames();
    QStringList enabled = settings.value(QString::fromLatin1(StylesKey)).toStringList();
    enabled.removeDuplicates();
    enabled.removeIf([&validStyles](const QString &style) {
        return !validStyles.contains(style);
    });
    if (enabled.isEmpty()) {
        enabled = {QStringLiteral("Friendly")};
    }

    auto *styleGrid = new QGridLayout;
    for (int index = 0; index < validStyles.size(); ++index) {
        const QString style = validStyles.at(index);
        auto *check = new QCheckBox(style, this);
        check->setObjectName(QStringLiteral("nameStyle%1").arg(style.simplified().remove(QLatin1Char(' '))));
        check->setChecked(enabled.contains(style));
        styleGrid->addWidget(check, index / 4, index % 4);
        m_styleChecks.append(check);
        connect(check, &QCheckBox::toggled, this, [this, check](bool checked) {
            styleToggled(check, checked);
        });
    }
    layout->addLayout(styleGrid);

    m_customEdit = new QLineEdit(
        settings.value(QString::fromLatin1(CustomKey)).toString(),
        this);
    m_customEdit->setObjectName(QStringLiteral("nameCustomInput"));
    m_customEdit->setPlaceholderText(tr("Custom words"));
    m_customEdit->setEnabled(enabled.contains(QStringLiteral("Custom")));
    layout->addWidget(m_customEdit);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("nameResultCard"));
    card->setFrameShape(QFrame::StyledPanel);
    auto *cardLayout = new QVBoxLayout(card);
    m_resultLabel = new QLabel(card);
    m_resultLabel->setObjectName(QStringLiteral("generatedNameLabel"));
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QFont resultFont = m_resultLabel->font();
    resultFont.setBold(true);
    resultFont.setPointSize(resultFont.pointSize() + 7);
    m_resultLabel->setFont(resultFont);
    cardLayout->addWidget(m_resultLabel);
    layout->addWidget(card, 1);

    auto *buttons = new QHBoxLayout;
    auto *generateButton = new QPushButton(tr("Generate Another"), this);
    generateButton->setObjectName(QStringLiteral("nameGenerateButton"));
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_copyButton->setObjectName(QStringLiteral("nameCopyButton"));
    buttons->addWidget(generateButton);
    buttons->addWidget(m_copyButton);
    layout->addLayout(buttons);

    connect(generateButton, &QPushButton::clicked, this, &NameGeneratorWidget::generate);
    connect(m_copyButton, &QPushButton::clicked, this, &NameGeneratorWidget::copyName);
    connect(m_customEdit, &QLineEdit::textChanged, this, [this] {
        savePreferences();
        generate();
    });

    generate();
}

QStringList NameGeneratorWidget::styleNames()
{
    return {
        QStringLiteral("Friendly"),
        QStringLiteral("NPC"),
        QStringLiteral("Boss"),
        QStringLiteral("Minion"),
        QStringLiteral("Strong"),
        QStringLiteral("Bitch Ass"),
        QStringLiteral("Small"),
        QStringLiteral("Massive"),
        QStringLiteral("Chad"),
        QStringLiteral("Flamboyant"),
        QStringLiteral("Angry"),
        QStringLiteral("Demon"),
        QStringLiteral("Angel"),
        QStringLiteral("Stupid"),
        QStringLiteral("Smart"),
        QStringLiteral("Custom")
    };
}

NameParts NameGeneratorWidget::builtInParts(const QString &style)
{
    if (style == QStringLiteral("Friendly")) {
        return parts({"Milo", "Tessa", "Perrin", "Luma"}, {"Bright", "Meadow", "Good", "Honey"}, {"brook", "bell", "bough", "well"});
    }
    if (style == QStringLiteral("NPC")) {
        return parts({"Arlen", "Mara", "Dovin", "Nessa"}, {"Cobble", "River", "Tallow", "Market"}, {"ford", "mere", "wick", "ton"});
    }
    if (style == QStringLiteral("Boss")) {
        return parts({"Varkos", "Selvara", "Mordren", "Ilyth"}, {"Dread", "Iron", "Night", "Crown"}, {"maw", "spire", "bane", "reign"});
    }
    if (style == QStringLiteral("Minion")) {
        return parts({"Nib", "Grot", "Pik", "Zuzu"}, {"Mud", "Scrap", "Rat", "Crumb"}, {"toe", "snout", "bit", "kin"});
    }
    if (style == QStringLiteral("Strong")) {
        return parts({"Branna", "Torvek", "Hedra", "Kellan"}, {"Stone", "Oak", "Hammer", "Shield"}, {"arm", "ward", "back", "hold"});
    }
    if (style == QStringLiteral("Bitch Ass")) {
        return parts({"Tripp", "Kiki", "Blaine", "Sass"}, {"Side", "Cheap", "Back", "Whine"}, {"eye", "shot", "talk", "step"});
    }
    if (style == QStringLiteral("Small")) {
        return parts({"Pip", "Mimi", "Tock", "Bree"}, {"Pebble", "Thimble", "Acorn", "Button"}, {"tip", "bud", "dot", "kin"});
    }
    if (style == QStringLiteral("Massive")) {
        return parts({"Goram", "Ulga", "Bront", "Maedra"}, {"Mountain", "Titan", "Thunder", "Coloss"}, {"fall", "stride", "peak", "us"});
    }
    if (style == QStringLiteral("Chad")) {
        return parts({"Brock", "Jace", "Troy", "Rex"}, {"Prime", "Bold", "Sun", "Peak"}, {"man", "well", "son", "ridge"});
    }
    if (style == QStringLiteral("Flamboyant")) {
        return parts({"Fiora", "Lazuli", "Cosimo", "Velvet"}, {"Glitter", "Plume", "Velvet", "Rose"}, {"song", "flare", "dance", "bloom"});
    }
    if (style == QStringLiteral("Angry")) {
        return parts({"Raska", "Dorn", "Vexa", "Krag"}, {"Grim", "Rage", "Fury", "Scowl"}, {"fist", "scar", "spit", "lash"});
    }
    if (style == QStringLiteral("Demon")) {
        return parts({"Azrath", "Velzun", "Nereza", "Kharox"}, {"Ash", "Cinder", "Abyss", "Horn"}, {"gore", "vex", "drake", "zul"});
    }
    if (style == QStringLiteral("Angel")) {
        return parts({"Seriel", "Aurea", "Calen", "Lumiel"}, {"Dawn", "Halo", "Grace", "Choir"}, {"light", "wing", "iel", "song"});
    }
    if (style == QStringLiteral("Stupid")) {
        return parts({"Bonk", "Doodle", "Mump", "Oopsie"}, {"Wobble", "Noodle", "Bumble", "Turnip"}, {"head", "sock", "pants", "face"});
    }
    if (style == QStringLiteral("Smart")) {
        return parts({"Quillon", "Adae", "Soren", "Veyra"}, {"Cipher", "Logic", "Quill", "Lumen"}, {"wise", "mark", "lex", "mind"});
    }
    return {};
}

NameParts NameGeneratorWidget::customParts(const QString &text)
{
    NameParts result;
    static const QRegularExpression wordPattern(QStringLiteral("[a-z]+"));
    const QString lower = text.toLower();
    QRegularExpressionMatchIterator matches = wordPattern.globalMatch(lower);
    while (matches.hasNext()) {
        const QString word = matches.next().captured();
        if (word.size() <= 1) {
            continue;
        }
        QString stem = word;
        if (QStringLiteral("aeiou").contains(stem.back())) {
            stem.chop(1);
        }
        if (stem.isEmpty()) {
            continue;
        }
        result.first.append({
            capitalized(word),
            capitalized(stem + QStringLiteral("a")),
            capitalized(stem + QStringLiteral("en"))
        });
        result.lastStart.append({
            capitalized(stem),
            capitalized(stem + QStringLiteral("r")),
            capitalized(stem + QStringLiteral("l"))
        });
        result.lastEnd.append({
            QStringLiteral("en"),
            QStringLiteral("or"),
            QStringLiteral("is")
        });
    }
    return result;
}

NameParts NameGeneratorWidget::pooledParts(
    const QStringList &styles,
    const QString &customText)
{
    NameParts pooled;
    for (const QString &style : styles) {
        if (style == QStringLiteral("Custom")) {
            appendParts(&pooled, customParts(customText));
        } else {
            appendParts(&pooled, builtInParts(style));
        }
    }
    if (pooled.first.isEmpty() || pooled.lastStart.isEmpty() || pooled.lastEnd.isEmpty()) {
        return builtInParts(QStringLiteral("Friendly"));
    }
    return pooled;
}

void NameGeneratorWidget::setRandomIndexGenerator(RandomIndexGenerator generator)
{
    if (generator) {
        m_randomIndexGenerator = std::move(generator);
    }
}

QStringList NameGeneratorWidget::selectedStyles() const
{
    QStringList selected;
    for (const QCheckBox *check : m_styleChecks) {
        if (check->isChecked()) {
            selected.append(check->text());
        }
    }
    return selected;
}

void NameGeneratorWidget::generate()
{
    const NameParts pool = pooledParts(selectedStyles(), m_customEdit->text());
    const QString generated = pool.first.at(randomIndex(pool.first.size()))
        + QLatin1Char(' ')
        + pool.lastStart.at(randomIndex(pool.lastStart.size()))
        + pool.lastEnd.at(randomIndex(pool.lastEnd.size()));
    m_resultLabel->setText(generated);
}

void NameGeneratorWidget::styleToggled(QCheckBox *changed, bool checked)
{
    if (!checked && selectedStyles().isEmpty()) {
        const QSignalBlocker blocker(changed);
        changed->setChecked(true);
    }
    m_customEdit->setEnabled(selectedStyles().contains(QStringLiteral("Custom")));
    savePreferences();
    generate();
}

void NameGeneratorWidget::savePreferences() const
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(StylesKey), selectedStyles());
    settings.setValue(QString::fromLatin1(CustomKey), m_customEdit->text());
}

int NameGeneratorWidget::randomIndex(int upperBound) const
{
    const int generated = m_randomIndexGenerator(upperBound);
    const int normalized = generated % upperBound;
    return normalized < 0 ? normalized + upperBound : normalized;
}

void NameGeneratorWidget::copyName()
{
    const int sequence = ++m_copySequence;
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard) {
        m_copyButton->setText(tr("Copy failed"));
        return;
    }

    const QString name = m_resultLabel->text();
    clipboard->setText(name);
    if (clipboard->text() != name) {
        m_copyButton->setText(tr("Copy failed"));
        return;
    }

    m_copyButton->setText(tr("Copied!"));
    QTimer::singleShot(1500, this, [this, sequence] {
        if (sequence == m_copySequence) {
            m_copyButton->setText(tr("Copy"));
        }
    });
}
