#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include <QWidget>

#include <optional>

class QMimeData;
class QPainter;

struct HexCoord {
    int q = 0;
    int r = 0;

    bool operator==(const HexCoord &) const = default;
};

enum class ArrowStyle {
    None,
    End,
    Both
};

struct HexLink {
    HexCoord start;
    HexCoord end;
    QColor color;
    double width = 3.0;
    ArrowStyle arrows = ArrowStyle::End;
};

struct GamePiece {
    QString id;
    QImage image;
    double scale = 1.0;
    bool equipment = false;
    QString ownerId;
    QString name;
    bool equipped = true;
};

struct Player {
    QString id;
    QString name;
    QString pieceId;
    int totalHearts = 5;
    int currentHearts = 5;
    QString notes;
};

size_t qHash(const HexCoord &coord, size_t seed = 0) noexcept;

class BoardWidget final : public QWidget
{
    Q_OBJECT

public:
    static constexpr int MaxPlayerNotesLength = 10000;

    explicit BoardWidget(QWidget *parent = nullptr);

    void chooseBackgroundColor();
    void chooseBackgroundImage();
    void clearBackgroundImage();
    void clearBoard();
    void resetSession();
    bool saveSessionData(QJsonObject *data, QString *errorMessage) const;
    bool loadSession(const QJsonObject &data, QString *errorMessage);
    void setLinkMode(bool enabled);
    void setLinkWidth(double width);
    void setLinkColor(const QColor &color);
    void setArrowStyle(ArrowStyle arrows);
    void setHexGridVisible(bool visible);
    void setTilePaintMode(bool enabled);
    void setTileEraseMode(bool enabled);
    void setTileBrushImage(const QImage &image);
    void setNavigationMode(bool enabled);
    [[nodiscard]] QVector<GamePiece> gamePieces() const;
    [[nodiscard]] QVector<Player> players() const;
    [[nodiscard]] QString addPlayer();
    bool removePlayer(const QString &playerId);
    bool setPlayerName(const QString &playerId, const QString &name);
    bool setPlayerPiece(const QString &playerId, const QString &pieceId);
    bool setPlayerTotalHearts(const QString &playerId, int totalHearts);
    bool setPlayerCurrentHearts(const QString &playerId, int currentHearts);
    bool setPlayerNotes(const QString &playerId, const QString &notes);
    bool setEquipmentEquipped(const QString &pieceId, bool equipped);

signals:
    void pieceCountChanged(int count);
    void zoomChanged(int percent);
    void interactionHintChanged(const QString &hint);
    void boardChanged();
    void piecesChanged();
    void playersChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct PieceRef {
        HexCoord tile;
        int index = 0;

        bool operator==(const PieceRef &) const = default;
    };

    static constexpr double HexWidth = 128.0;
    static constexpr double HexHeight = 128.0;
    static constexpr double MinZoom = 0.2;
    static constexpr double MaxZoom = 4.0;
    static constexpr double MinPieceScale = 0.5;
    static constexpr double MaxPieceScale = 10.0;
    static constexpr double PieceScaleStep = 0.25;

    [[nodiscard]] QPointF hexCenter(const HexCoord &coord) const;
    [[nodiscard]] HexCoord hexAt(const QPointF &screenPosition) const;
    [[nodiscard]] QPolygonF hexPolygon(const HexCoord &coord) const;
    [[nodiscard]] QPointF worldToScreen(const QPointF &worldPosition) const;
    [[nodiscard]] QPointF screenToWorld(const QPointF &screenPosition) const;
    [[nodiscard]] QRectF pieceSlot(const HexCoord &coord, int index, int count) const;
    [[nodiscard]] QRectF pieceRenderRect(
        const PieceRef &pieceRef) const;
    [[nodiscard]] QRectF equipmentRenderRect(
        const PieceRef &equipmentRef,
        const PieceRef &ownerRef) const;
    [[nodiscard]] QVector<int> unlinkedPieceIndices(const HexCoord &coord) const;
    [[nodiscard]] QVector<PieceRef> equipmentFor(
        const QString &ownerId,
        bool equippedOnly = false) const;
    [[nodiscard]] std::optional<PieceRef> pieceById(const QString &id) const;
    [[nodiscard]] QList<HexCoord> pieceTilesInPaintOrder() const;
    [[nodiscard]] bool pieceContainsPoint(
        const QRectF &bounds,
        const GamePiece &piece,
        const QPointF &screenPosition) const;
    [[nodiscard]] std::optional<PieceRef> pieceAt(const QPointF &screenPosition) const;
    [[nodiscard]] int pieceCount() const;
    [[nodiscard]] bool canAcceptDrop(const QMimeData *mimeData) const;
    [[nodiscard]] std::optional<QImage> imageFromDrop(const QMimeData *mimeData) const;
    void drawBackground(QPainter &painter);
    void drawTileBackground(QPainter &painter, const HexCoord &coord, const QImage &image);
    void drawHexGrid(QPainter &painter);
    void drawLink(QPainter &painter, const HexLink &link, bool preview = false);
    void drawGamePiece(
        QPainter &painter,
        const PieceRef &pieceRef);
    void drawGamePieceName(
        QPainter &painter,
        const PieceRef &pieceRef);
    void movePieceGroup(const PieceRef &ownerRef, const HexCoord &destination);
    void linkEquipment(const PieceRef &equipmentRef, const PieceRef &ownerRef);
    void drawTileIndicator(QPainter &painter, const HexCoord &coord, const QColor &color, double width);
    void applyTileBrush(const HexCoord &coord);
    void setDropTile(std::optional<HexCoord> coord);

    QHash<HexCoord, QImage> m_tileBackgrounds;
    QHash<HexCoord, QVector<GamePiece>> m_pieces;
    QVector<Player> m_players;
    QVector<HexLink> m_links;
    QColor m_backgroundColor{QStringLiteral("#20252b")};
    QImage m_backgroundImage;
    QImage m_tileBrushImage;
    QColor m_linkColor{Qt::black};
    QPointF m_pan;
    QPointF m_lastPointerPosition;
    double m_zoom = 1.0;
    double m_linkWidth = 6.0;
    ArrowStyle m_arrowStyle = ArrowStyle::End;
    bool m_linkMode = false;
    bool m_hexGridVisible = false;
    bool m_tilePaintMode = false;
    bool m_tileEraseMode = false;
    bool m_navigationMode = false;
    bool m_paintingTiles = false;
    bool m_panning = false;
    bool m_movingTile = false;
    std::optional<HexCoord> m_linkStart;
    std::optional<HexCoord> m_linkHover;
    std::optional<HexCoord> m_dropTile;
    std::optional<HexCoord> m_sourceTile;
    std::optional<HexCoord> m_lastPaintedTile;
    std::optional<int> m_sourcePieceIndex;
    QString m_pendingEquipmentId;
};
