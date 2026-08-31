#include "chancewheelwidget.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace {
constexpr auto SettingsRoot = "tools/chanceWheel/segments";

double normalizedDegrees(double degrees)
{
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double scaledWeightTotal(const QVector<WheelSegment> &segments, double *scale)
{
    if (segments.isEmpty()) {
        *scale = 1.0;
        return 0.0;
    }
    *scale = std::max_element(
        segments.cbegin(),
        segments.cend(),
        [](const WheelSegment &left, const WheelSegment &right) {
            return left.weight < right.weight;
        })->weight;
    return std::accumulate(
        segments.cbegin(),
        segments.cend(),
        0.0,
        [scale](double total, const WheelSegment &segment) {
            return total + segment.weight / *scale;
        });
}

QStringList colorNames()
{
    return {
        QStringLiteral("#ef6c57"),
        QStringLiteral("#f5a04b"),
        QStringLiteral("#e8ca50"),
        QStringLiteral("#8cc665"),
        QStringLiteral("#4db6ac"),
        QStringLiteral("#59a9e8"),
        QStringLiteral("#8e7cc3"),
        QStringLiteral("#cf72aa")
    };
}
}

ChanceWheelCanvas::ChanceWheelCanvas(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("chanceWheelCanvas"));
    setMinimumSize(240, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);
}

double ChanceWheelCanvas::rotation() const
{
    return m_rotation;
}

void ChanceWheelCanvas::setRotation(double rotation)
{
    m_rotation = rotation;
    update();
}

void ChanceWheelCanvas::setSegments(const QVector<WheelSegment> &segments)
{
    m_segments = segments;
    update();
}

void ChanceWheelCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const double side = std::min(width(), height()) - 24.0;
        const QPointF center(width() / 2.0, height() / 2.0 + 4.0);
        const QPointF offset = event->position() - center;
        if (side > 0.0
            && QPointF::dotProduct(offset, offset) <= side * side / 4.0) {
            emit activated();
        }
    }
    QWidget::mousePressEvent(event);
}

void ChanceWheelCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int side = std::min(width(), height()) - 24;
    if (side <= 0) {
        return;
    }
    const QPointF center(width() / 2.0, height() / 2.0 + 4.0);
    const QRectF wheelRect(
        center.x() - side / 2.0,
        center.y() - side / 2.0,
        side,
        side);

    if (m_segments.isEmpty()) {
        painter.setPen(palette().color(QPalette::Mid));
        painter.setBrush(palette().color(QPalette::AlternateBase));
        painter.drawEllipse(wheelRect);
        painter.drawText(wheelRect, Qt::AlignCenter | Qt::TextWordWrap, tr("Add segments to spin"));
        return;
    }

    double weightScale = 1.0;
    const double totalWeight = scaledWeightTotal(m_segments, &weightScale);

    painter.save();
    painter.translate(center);
    painter.rotate(m_rotation);
    painter.translate(-center);

    double startDegrees = 0.0;
    for (const WheelSegment &segment : m_segments) {
        const double sweepDegrees = (segment.weight / weightScale) / totalWeight * 360.0;
        painter.setPen(QPen(Qt::white, 2.0));
        painter.setBrush(segment.color);
        painter.drawPie(
            wheelRect,
            qRound((90.0 - startDegrees) * 16.0),
            qRound(-sweepDegrees * 16.0));

        const double middleDegrees = startDegrees + sweepDegrees / 2.0;
        painter.save();
        painter.translate(center);
        painter.rotate(middleDegrees);
        QFont labelFont = painter.font();
        labelFont.setBold(true);
        labelFont.setPointSize(std::max(7, labelFont.pointSize() - (m_segments.size() > 8 ? 2 : 0)));
        painter.setFont(labelFont);
        painter.setPen(Qt::white);
        const int radialStart = std::max(22, side / 10);
        const int radialLength = std::max(30, side / 2 - radialStart - 12);
        const QRectF labelRect(-side / 8.0, -radialStart - radialLength, side / 4.0, radialLength);
        painter.drawText(
            labelRect,
            Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
            segment.name);
        painter.restore();
        startDegrees += sweepDegrees;
    }
    painter.restore();

    painter.setPen(QPen(palette().color(QPalette::Shadow), 2.0));
    painter.setBrush(palette().color(QPalette::Window));
    painter.drawEllipse(center, side * 0.055, side * 0.055);

    QPainterPath pointer;
    pointer.moveTo(center.x(), wheelRect.top() + 18.0);
    pointer.lineTo(center.x() - 13.0, wheelRect.top() - 7.0);
    pointer.lineTo(center.x() + 13.0, wheelRect.top() - 7.0);
    pointer.closeSubpath();
    painter.setPen(QPen(palette().color(QPalette::Shadow), 1.5));
    painter.setBrush(palette().color(QPalette::Text));
    painter.drawPath(pointer);
}

ChanceWheelWidget::ChanceWheelWidget(QWidget *parent)
    : QWidget(parent)
    , m_randomAngle([] {
        return QRandomGenerator::global()->generateDouble() * 360.0;
    })
{
    setObjectName(QStringLiteral("chanceWheelWidget"));
    m_segments = loadSegments();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *title = new QLabel(tr("Chance Wheel"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_canvas = new ChanceWheelCanvas(this);
    m_canvas->setSegments(m_segments);
    layout->addWidget(m_canvas, 1);

    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setObjectName(QStringLiteral("chanceWheelStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_spinButton = new QPushButton(tr("Spin"), this);
    m_spinButton->setObjectName(QStringLiteral("chanceWheelSpinButton"));
    m_spinButton->setEnabled(!m_segments.isEmpty());
    layout->addWidget(m_spinButton);

    m_editToggle = new QToolButton(this);
    m_editToggle->setObjectName(QStringLiteral("chanceWheelEditToggle"));
    m_editToggle->setText(tr("Edit segments"));
    m_editToggle->setCheckable(true);
    m_editToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_editToggle->setArrowType(Qt::RightArrow);
    layout->addWidget(m_editToggle);

    m_editor = new QWidget(this);
    m_editor->setObjectName(QStringLiteral("chanceWheelEditor"));
    auto *editorLayout = new QVBoxLayout(m_editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(m_editor);
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(130);
    auto *rows = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(rows);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->addStretch();
    scroll->setWidget(rows);
    editorLayout->addWidget(scroll);

    auto *editorButtons = new QHBoxLayout;
    auto *addButton = new QPushButton(tr("Add"), m_editor);
    addButton->setObjectName(QStringLiteral("chanceWheelAddButton"));
    auto *applyButton = new QPushButton(tr("Apply"), m_editor);
    applyButton->setObjectName(QStringLiteral("chanceWheelApplyButton"));
    editorButtons->addWidget(addButton);
    editorButtons->addStretch();
    editorButtons->addWidget(applyButton);
    editorLayout->addLayout(editorButtons);
    layout->addWidget(m_editor);
    m_editor->hide();
    rebuildEditorRows();

    m_animation = new QPropertyAnimation(m_canvas, "rotation", this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_spinButton, &QPushButton::clicked, this, &ChanceWheelWidget::spin);
    connect(m_canvas, &ChanceWheelCanvas::activated, this, &ChanceWheelWidget::spin);
    connect(m_editToggle, &QToolButton::toggled, this, [this](bool open) {
        m_editToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        m_editor->setVisible(open);
    });
    connect(addButton, &QPushButton::clicked, this, [this] {
        addEditorRow();
    });
    connect(applyButton, &QPushButton::clicked, this, &ChanceWheelWidget::applyEditorRows);
    connect(m_animation, &QPropertyAnimation::finished, this, [this] {
        const double normalizedRotation = normalizedDegrees(m_canvas->rotation());
        m_canvas->setRotation(normalizedRotation);
        m_spinning = false;
        setInputsEnabled(true);
        const int selected = segmentIndexAtAngle(m_segments, normalizedDegrees(360.0 - normalizedRotation));
        m_statusLabel->setText(
            selected >= 0 ? tr("Selected: %1").arg(m_segments.at(selected).name) : tr("No segments"));
    });
}

QVector<WheelSegment> ChanceWheelWidget::defaultSegments()
{
    const QStringList colors = colorNames();
    return {
        {tr("Coffee"), 1.0, QColor(colors.at(0))},
        {tr("Tea"), 1.0, QColor(colors.at(1))},
        {tr("Lunch"), 2.0, QColor(colors.at(2))},
        {tr("Take a walk"), 1.0, QColor(colors.at(3))}
    };
}

bool ChanceWheelWidget::validateSegments(const QVector<WheelSegment> &segments, QString *error)
{
    for (int index = 0; index < segments.size(); ++index) {
        const WheelSegment &segment = segments.at(index);
        if (segment.name.trimmed().isEmpty()) {
            if (error) {
                *error = tr("Segment %1 needs a name.").arg(index + 1);
            }
            return false;
        }
        if (!std::isfinite(segment.weight) || segment.weight <= 0.0) {
            if (error) {
                *error = tr("Segment %1 needs a finite weight greater than zero.").arg(index + 1);
            }
            return false;
        }
    }
    return true;
}

int ChanceWheelWidget::segmentIndexAtAngle(
    const QVector<WheelSegment> &segments,
    double angleDegrees)
{
    if (segments.isEmpty() || !std::isfinite(angleDegrees)) {
        return -1;
    }
    if (!validateSegments(segments)) {
        return -1;
    }
    double weightScale = 1.0;
    const double totalWeight = scaledWeightTotal(segments, &weightScale);
    if (!std::isfinite(totalWeight) || totalWeight <= 0.0) {
        return -1;
    }

    const double angle = normalizedDegrees(angleDegrees);
    double boundary = 0.0;
    for (int index = 0; index < segments.size(); ++index) {
        boundary += (segments.at(index).weight / weightScale) / totalWeight * 360.0;
        if (angle < boundary || index == segments.size() - 1) {
            return index;
        }
    }
    return -1;
}

void ChanceWheelWidget::setRandomAngleGenerator(RandomAngleGenerator generator)
{
    if (generator) {
        m_randomAngle = std::move(generator);
    }
}

void ChanceWheelWidget::setSpinDurationForTesting(int milliseconds)
{
    m_spinDuration = std::max(1, milliseconds);
}

void ChanceWheelWidget::setSegmentsForTesting(const QVector<WheelSegment> &segments)
{
    QString error;
    if (!validateSegments(segments, &error)) {
        return;
    }
    m_segments = segments;
    m_canvas->setSegments(m_segments);
    m_spinButton->setEnabled(!m_segments.isEmpty());
    rebuildEditorRows();
}

bool ChanceWheelWidget::isSpinning() const
{
    return m_spinning;
}

void ChanceWheelWidget::spin()
{
    if (m_spinning || m_segments.isEmpty()) {
        return;
    }

    m_spinning = true;
    setInputsEnabled(false);
    m_statusLabel->setText(tr("Spinning..."));
    const double start = m_canvas->rotation();
    const double target = start + 6.0 * 360.0 + normalizedDegrees(m_randomAngle());
    m_animation->stop();
    m_animation->setDuration(m_spinDuration);
    m_animation->setStartValue(start);
    m_animation->setEndValue(target);
    m_animation->start();

    QTimer::singleShot(m_spinDuration * 3 / 4, this, [this] {
        if (m_spinning) {
            m_statusLabel->setText(tr("Selecting..."));
        }
    });
}

void ChanceWheelWidget::addEditorRow(const QString &name, double weight)
{
    auto *row = new QWidget(m_editor);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *nameEdit = new QLineEdit(name, row);
    nameEdit->setObjectName(QStringLiteral("chanceWheelSegmentName"));
    nameEdit->setPlaceholderText(tr("Name"));
    auto *weightEdit = new QLineEdit(QString::number(weight, 'g', 12), row);
    weightEdit->setObjectName(QStringLiteral("chanceWheelSegmentWeight"));
    weightEdit->setPlaceholderText(tr("Weight"));
    weightEdit->setMaximumWidth(100);
    auto *removeButton = new QPushButton(tr("Remove"), row);
    removeButton->setObjectName(QStringLiteral("chanceWheelRemoveButton"));

    layout->addWidget(nameEdit, 1);
    layout->addWidget(weightEdit);
    layout->addWidget(removeButton);
    m_rows.append({row, nameEdit, weightEdit});
    m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, row);

    connect(removeButton, &QPushButton::clicked, this, [this, row] {
        removeEditorRow(row);
    });
}

void ChanceWheelWidget::removeEditorRow(QWidget *rowWidget)
{
    const auto iterator = std::find_if(
        m_rows.begin(),
        m_rows.end(),
        [rowWidget](const EditorRow &row) {
            return row.widget == rowWidget;
        });
    if (iterator == m_rows.end()) {
        return;
    }
    m_rows.erase(iterator);
    rowWidget->deleteLater();
}

void ChanceWheelWidget::rebuildEditorRows()
{
    for (const EditorRow &row : std::as_const(m_rows)) {
        delete row.widget;
    }
    m_rows.clear();
    for (const WheelSegment &segment : std::as_const(m_segments)) {
        addEditorRow(segment.name, segment.weight);
    }
}

void ChanceWheelWidget::applyEditorRows()
{
    const QStringList colors = colorNames();
    QVector<WheelSegment> candidate;
    candidate.reserve(m_rows.size());
    for (int index = 0; index < m_rows.size(); ++index) {
        const EditorRow &row = m_rows.at(index);
        bool converted = false;
        const double weight = row.weight->text().trimmed().toDouble(&converted);
        candidate.append({
            row.name->text().trimmed(),
            converted ? weight : std::numeric_limits<double>::quiet_NaN(),
            QColor(colors.at(index % colors.size()))
        });
    }

    QString error;
    if (!validateSegments(candidate, &error)) {
        m_statusLabel->setText(error);
        return;
    }

    m_segments = candidate;
    m_canvas->setSegments(m_segments);
    m_spinButton->setEnabled(!m_segments.isEmpty());
    m_statusLabel->setText(m_segments.isEmpty() ? tr("No segments") : tr("Ready"));
    saveSegments();
}

void ChanceWheelWidget::setInputsEnabled(bool enabled)
{
    m_spinButton->setEnabled(enabled && !m_segments.isEmpty());
    m_editToggle->setEnabled(enabled);
    m_editor->setEnabled(enabled);
}

void ChanceWheelWidget::saveSegments() const
{
    QSettings settings;
    settings.beginWriteArray(QString::fromLatin1(SettingsRoot), m_segments.size());
    for (int index = 0; index < m_segments.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("name"), m_segments.at(index).name);
        settings.setValue(QStringLiteral("weight"), m_segments.at(index).weight);
    }
    settings.endArray();
}

QVector<WheelSegment> ChanceWheelWidget::loadSegments() const
{
    QSettings settings;
    const QString sizeKey = QString::fromLatin1(SettingsRoot) + QStringLiteral("/size");
    if (!settings.contains(sizeKey)) {
        return defaultSegments();
    }

    const QStringList colors = colorNames();
    QVector<WheelSegment> loaded;
    const int size = settings.beginReadArray(QString::fromLatin1(SettingsRoot));
    for (int index = 0; index < size; ++index) {
        settings.setArrayIndex(index);
        const QString name = settings.value(QStringLiteral("name")).toString().trimmed();
        bool converted = false;
        const double weight = settings.value(QStringLiteral("weight")).toString().toDouble(&converted);
        WheelSegment segment{name, weight, QColor(colors.at(index % colors.size()))};
        if (converted && validateSegments({segment})) {
            loaded.append(segment);
        }
    }
    settings.endArray();
    return loaded;
}
