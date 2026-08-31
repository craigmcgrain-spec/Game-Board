#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QPropertyAnimation;
class QToolButton;
class QVBoxLayout;

struct WheelSegment
{
    QString name;
    double weight = 1.0;
    QColor color;
};

class ChanceWheelCanvas final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double rotation READ rotation WRITE setRotation)

public:
    explicit ChanceWheelCanvas(QWidget *parent = nullptr);

    double rotation() const;
    void setRotation(double rotation);
    void setSegments(const QVector<WheelSegment> &segments);

signals:
    void activated();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<WheelSegment> m_segments;
    double m_rotation = 0.0;
};

class ChanceWheelWidget final : public QWidget
{
    Q_OBJECT

public:
    using RandomAngleGenerator = std::function<double()>;

    explicit ChanceWheelWidget(QWidget *parent = nullptr);

    static QVector<WheelSegment> defaultSegments();
    static bool validateSegments(const QVector<WheelSegment> &segments, QString *error = nullptr);
    static int segmentIndexAtAngle(const QVector<WheelSegment> &segments, double angleDegrees);

    void setRandomAngleGenerator(RandomAngleGenerator generator);
    void setSpinDurationForTesting(int milliseconds);
    void setSegmentsForTesting(const QVector<WheelSegment> &segments);
    bool isSpinning() const;

public slots:
    void spin();

private:
    struct EditorRow
    {
        QWidget *widget = nullptr;
        QLineEdit *name = nullptr;
        QLineEdit *weight = nullptr;
    };

    void addEditorRow(const QString &name = {}, double weight = 1.0);
    void removeEditorRow(QWidget *rowWidget);
    void rebuildEditorRows();
    void applyEditorRows();
    void setInputsEnabled(bool enabled);
    void saveSegments() const;
    QVector<WheelSegment> loadSegments() const;

    ChanceWheelCanvas *m_canvas = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_spinButton = nullptr;
    QToolButton *m_editToggle = nullptr;
    QWidget *m_editor = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    QVector<WheelSegment> m_segments;
    QVector<EditorRow> m_rows;
    RandomAngleGenerator m_randomAngle;
    int m_spinDuration = 3200;
    bool m_spinning = false;
};
