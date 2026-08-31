#include "geargeneratorwidget.h"

#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
constexpr auto CategoriesKey = "tools/gearGenerator/enabledCategories";

const QStringList &categoryNames()
{
    static const QStringList names{
        QStringLiteral("Sword"),
        QStringLiteral("Staff"),
        QStringLiteral("Bow"),
        QStringLiteral("Armor"),
        QStringLiteral("Potion")
    };
    return names;
}

struct GearVocabulary
{
    QStringList bases;
    QStringList materials;
    QStringList powers;
    QStringList prefixes;
    QStringList owners;
};

GearVocabulary vocabularyFor(const QString &category)
{
    if (category == QStringLiteral("Sword")) {
        return {
            {QStringLiteral("blade"), QStringLiteral("sabre"), QStringLiteral("falchion"), QStringLiteral("longsword")},
            {QStringLiteral("star-iron"), QStringLiteral("black steel"), QStringLiteral("sun-bronze"), QStringLiteral("frosted silver")},
            {QStringLiteral("casts a brief ward after a parry"), QStringLiteral("glows near hidden doors"), QStringLiteral("rings when danger approaches"), QStringLiteral("leaves a trail of harmless sparks"), QStringLiteral("steadies its wielder against fear"), QStringLiteral("cuts spectral bindings")},
            {QStringLiteral("Vigilant"), QStringLiteral("Emberbound"), QStringLiteral("Wayfarer's"), QStringLiteral("Moonlit")},
            {QStringLiteral("the Lantern Guard"), QStringLiteral("Captain Orren"), QStringLiteral("the Hollow Court"), QStringLiteral("a nameless pilgrim")}
        };
    }
    if (category == QStringLiteral("Staff")) {
        return {
            {QStringLiteral("crook"), QStringLiteral("rod"), QStringLiteral("branch"), QStringLiteral("spire-staff")},
            {QStringLiteral("storm ash"), QStringLiteral("riverglass"), QStringLiteral("red cedar"), QStringLiteral("cloud crystal")},
            {QStringLiteral("summons a palm-sized rain cloud"), QStringLiteral("stores one whispered message"), QStringLiteral("warms a cold campsite"), QStringLiteral("reveals fresh tracks at dusk"), QStringLiteral("calms restless animals"), QStringLiteral("bends candlelight toward magic")},
            {QStringLiteral("Whispering"), QStringLiteral("Rainwise"), QStringLiteral("Elder"), QStringLiteral("Far-Seer's")},
            {QStringLiteral("the Moss Collegium"), QStringLiteral("Sage Pell"), QStringLiteral("the North Observatory"), QStringLiteral("a hedge magician")}
        };
    }
    if (category == QStringLiteral("Bow")) {
        return {
            {QStringLiteral("shortbow"), QStringLiteral("longbow"), QStringLiteral("recurve"), QStringLiteral("war bow")},
            {QStringLiteral("thornwood"), QStringLiteral("pale yew"), QStringLiteral("sea oak"), QStringLiteral("ironbark")},
            {QStringLiteral("guides arrows through strong wind"), QStringLiteral("marks a struck target with blue light"), QStringLiteral("muffles the snap of its string"), QStringLiteral("never frays in rain"), QStringLiteral("points toward the last fired arrow"), QStringLiteral("scatters harmless petals on a miss")},
            {QStringLiteral("Horizon"), QStringLiteral("Quiet"), QStringLiteral("Gale-Touched"), QStringLiteral("Greenwatch")},
            {QStringLiteral("the Reed Rangers"), QStringLiteral("Warden Siva"), QStringLiteral("the Western March"), QStringLiteral("an exiled scout")}
        };
    }
    if (category == QStringLiteral("Armor")) {
        return {
            {QStringLiteral("mail"), QStringLiteral("brigandine"), QStringLiteral("cuirass"), QStringLiteral("traveling coat")},
            {QStringLiteral("lacquered iron"), QStringLiteral("wyrmhide"), QStringLiteral("mirror brass"), QStringLiteral("woven slate")},
            {QStringLiteral("softens the sound of heavy footsteps"), QStringLiteral("sheds mud with a shake"), QStringLiteral("cools its wearer at noon"), QStringLiteral("hardens briefly against falling stone"), QStringLiteral("displays a faint map of nearby shelter"), QStringLiteral("dims when an ambush is near")},
            {QStringLiteral("Bastion"), QStringLiteral("Roadwarden"), QStringLiteral("Gleaming"), QStringLiteral("Stonewake")},
            {QStringLiteral("the Ninth Company"), QStringLiteral("Dame Corren"), QStringLiteral("the Bridge Keepers"), QStringLiteral("a royal courier")}
        };
    }
    return {
        {QStringLiteral("elixir"), QStringLiteral("tonic"), QStringLiteral("draught"), QStringLiteral("cordial")},
        {QStringLiteral("amber resin"), QStringLiteral("moonmint"), QStringLiteral("blue salt"), QStringLiteral("phoenix pepper")},
        {QStringLiteral("sharpens hearing for a few minutes"), QStringLiteral("makes the drinker's voice carry clearly"), QStringLiteral("restores warmth to numb hands"), QStringLiteral("turns briefly luminous near poison"), QStringLiteral("grants dreamless sleep"), QStringLiteral("removes the taste of spoiled food")},
        {QStringLiteral("Bright"), QStringLiteral("Wayfarer's"), QStringLiteral("Stillwater"), QStringLiteral("Coppercap")},
        {QStringLiteral("the Glass Apothecary"), QStringLiteral("Doctor Venn"), QStringLiteral("the Dawn Market"), QStringLiteral("a traveling herbalist")}
    };
}
}

GearGeneratorWidget::GearGeneratorWidget(QWidget *parent)
    : QWidget(parent)
    , m_randomIndexGenerator([](int upperBound) {
        return static_cast<int>(QRandomGenerator::global()->bounded(
            static_cast<quint32>(upperBound)));
    })
{
    setObjectName(QStringLiteral("gearGeneratorWidget"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(tr("Gear Generator"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *categoryGrid = new QGridLayout;
    QSettings settings;
    QStringList enabled = settings.value(QString::fromLatin1(CategoriesKey)).toStringList();
    enabled.removeDuplicates();
    const QStringList validNames = categoryNames();
    enabled.removeIf([&validNames](const QString &name) {
        return !validNames.contains(name);
    });
    if (enabled.isEmpty()) {
        enabled = validNames;
    }

    for (int index = 0; index < validNames.size(); ++index) {
        const QString &category = validNames.at(index);
        auto *check = new QCheckBox(category, this);
        check->setObjectName(QStringLiteral("gearCategory%1").arg(category));
        check->setChecked(enabled.contains(category));
        categoryGrid->addWidget(check, index / 3, index % 3);
        m_categoryChecks.append(check);
        connect(check, &QCheckBox::toggled, this, [this, check](bool checked) {
            categoryToggled(check, checked);
        });
    }
    layout->addLayout(categoryGrid);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("gearResultCard"));
    card->setFrameShape(QFrame::StyledPanel);
    auto *cardLayout = new QVBoxLayout(card);
    m_typeLabel = new QLabel(card);
    m_typeLabel->setObjectName(QStringLiteral("gearTypeLabel"));
    m_nameLabel = new QLabel(card);
    m_nameLabel->setObjectName(QStringLiteral("gearNameLabel"));
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 4);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setWordWrap(true);
    m_descriptionLabel = new QLabel(card);
    m_descriptionLabel->setObjectName(QStringLiteral("gearDescriptionLabel"));
    m_descriptionLabel->setWordWrap(true);
    m_valueLabel = new QLabel(card);
    m_valueLabel->setObjectName(QStringLiteral("gearValueLabel"));
    cardLayout->addWidget(m_typeLabel);
    cardLayout->addWidget(m_nameLabel);
    cardLayout->addWidget(m_descriptionLabel);
    cardLayout->addWidget(m_valueLabel);
    layout->addWidget(card, 1);

    auto *generateButton = new QPushButton(tr("Generate"), this);
    generateButton->setObjectName(QStringLiteral("gearGenerateButton"));
    layout->addWidget(generateButton);
    connect(generateButton, &QPushButton::clicked, this, &GearGeneratorWidget::generate);

    generate();
}

void GearGeneratorWidget::setRandomIndexGenerator(RandomIndexGenerator generator)
{
    if (generator) {
        m_randomIndexGenerator = std::move(generator);
    }
}

QStringList GearGeneratorWidget::enabledCategories() const
{
    QStringList enabled;
    for (const QCheckBox *check : m_categoryChecks) {
        if (check->isChecked()) {
            enabled.append(check->text());
        }
    }
    return enabled;
}

void GearGeneratorWidget::generate()
{
    const QStringList enabled = enabledCategories();
    if (enabled.isEmpty()) {
        return;
    }
    const QString category = enabled.at(randomIndex(enabled.size()));
    const GearVocabulary vocabulary = vocabularyFor(category);
    const QString base = vocabulary.bases.at(randomIndex(vocabulary.bases.size()));
    const QString material = vocabulary.materials.at(randomIndex(vocabulary.materials.size()));
    const QString prefix = vocabulary.prefixes.at(randomIndex(vocabulary.prefixes.size()));
    const QString owner = vocabulary.owners.at(randomIndex(vocabulary.owners.size()));

    QStringList availableEffects;
    for (const QString &effect : vocabulary.powers) {
        if (!m_recentEffects.contains(effect)) {
            availableEffects.append(effect);
        }
    }
    if (availableEffects.isEmpty()) {
        availableEffects = vocabulary.powers;
    }
    const QString effect = availableEffects.at(randomIndex(availableEffects.size()));
    m_recentEffects.prepend(effect);
    while (m_recentEffects.size() > 10) {
        m_recentEffects.removeLast();
    }

    static const QStringList rarities{
        QStringLiteral("Common"),
        QStringLiteral("Uncommon"),
        QStringLiteral("Rare"),
        QStringLiteral("Very Rare")
    };
    static const QStringList values{
        QStringLiteral("25 gp"),
        QStringLiteral("100 gp"),
        QStringLiteral("750 gp"),
        QStringLiteral("3,500 gp")
    };
    const int rarityIndex = randomIndex(rarities.size());

    m_typeLabel->setText(category);
    m_nameLabel->setText(tr("%1 %2 %3").arg(prefix, material, base));
    m_descriptionLabel->setText(
        tr("A %1 %2 once carried by %3; it %4.").arg(material, base, owner, effect));
    m_valueLabel->setText(tr("%1 - %2").arg(rarities.at(rarityIndex), values.at(rarityIndex)));
}

void GearGeneratorWidget::categoryToggled(QCheckBox *changed, bool checked)
{
    if (!checked && enabledCategories().isEmpty()) {
        const QSignalBlocker blocker(changed);
        changed->setChecked(true);
    }
    saveCategories();
}

void GearGeneratorWidget::saveCategories() const
{
    QSettings().setValue(QString::fromLatin1(CategoriesKey), enabledCategories());
}

int GearGeneratorWidget::randomIndex(int upperBound) const
{
    const int generated = m_randomIndexGenerator(upperBound);
    const int normalized = generated % upperBound;
    return normalized < 0 ? normalized + upperBound : normalized;
}
