#include "boardwidget.h"

#include <QBuffer>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QImageReader>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QSet>
#include <QSlider>
#include <QUrl>
#include <QUuid>
#include <QWidgetAction>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr int MaxPieces = 10000;
constexpr int MaxLinks = 50000;
constexpr int MaxTileBackgrounds = 50000;
constexpr int MaxPlayers = 100;
constexpr int MaxNameLength = 100;
constexpr int MaxHearts = 100;
constexpr int MaxImageDimension = 8192;
constexpr qint64 MaxImagePixels = 16LL * 1024 * 1024;
constexpr qint64 MaxDecodedImageBytes = 512LL * 1024 * 1024;
constexpr double MaxPanOffset = 100000000.0;

QPointF boundedPan(const QPointF &pan)
{
    return {
        std::clamp(pan.x(), -MaxPanOffset, MaxPanOffset),
        std::clamp(pan.y(), -MaxPanOffset, MaxPanOffset)
    };
}

bool dimensionsWithinLimits(const QSize &dimensions)
{
    return dimensions.isValid()
        && dimensions.width() <= MaxImageDimension
        && dimensions.height() <= MaxImageDimension
        && static_cast<qint64>(dimensions.width()) * dimensions.height()
            <= MaxImagePixels;
}

HexCoord roundedAxial(double q, double r)
{
    const double s = -q - r;
    int roundedQ = std::lround(q);
    int roundedR = std::lround(r);
    int roundedS = std::lround(s);

    const double qDifference = std::abs(roundedQ - q);
    const double rDifference = std::abs(roundedR - r);
    const double sDifference = std::abs(roundedS - s);

    if (qDifference > rDifference && qDifference > sDifference) {
        roundedQ = -roundedR - roundedS;
    } else if (rDifference > sDifference) {
        roundedR = -roundedQ - roundedS;
    }

    return {roundedQ, roundedR};
}

QVector<HexCoord> hexLine(const HexCoord &start, const HexCoord &end)
{
    const int deltaQ = end.q - start.q;
    const int deltaR = end.r - start.r;
    const int deltaS = (-end.q - end.r) - (-start.q - start.r);
    const int distance = std::max({std::abs(deltaQ), std::abs(deltaR), std::abs(deltaS)});
    QVector<HexCoord> result;
    result.reserve(distance);
    for (int step = 1; step <= distance; ++step) {
        const double progress = static_cast<double>(step) / distance;
        result.append(roundedAxial(
            start.q + deltaQ * progress,
            start.r + deltaR * progress));
    }
    return result;
}

std::optional<QString> encodedImage(const QImage &image)
{
    if (image.isNull()) {
        return QString();
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return std::nullopt;
    }
    return QString::fromLatin1(bytes.toBase64());
}

std::optional<QImage> decodedImage(const QJsonValue &value)
{
    if (!value.isString() || value.toString().isEmpty()) {
        return std::nullopt;
    }

    const QByteArray bytes = QByteArray::fromBase64(
        value.toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty()) {
        return std::nullopt;
    }

    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QImageReader reader(&buffer);
    const QSize dimensions = reader.size();
    if (!dimensionsWithinLimits(dimensions)) {
        return std::nullopt;
    }
    const QImage image = reader.read();
    return image.isNull() ? std::nullopt : std::optional<QImage>(image);
}

bool reserveDecodedImage(const QImage &image, qint64 *totalBytes)
{
    const qint64 bytes = image.sizeInBytes();
    if (bytes < 0
        || *totalBytes > MaxDecodedImageBytes - bytes) {
        return false;
    }
    *totalBytes += bytes;
    return true;
}

bool imageWithinLimits(const QImage &image)
{
    return !image.isNull()
        && image.width() <= MaxImageDimension
        && image.height() <= MaxImageDimension
        && static_cast<qint64>(image.width()) * image.height()
            <= MaxImagePixels;
}
}

size_t qHash(const HexCoord &coord, size_t seed) noexcept
{
    return qHashMulti(seed, coord.q, coord.r);
}

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(640, 420);

    QSettings settings;
    m_backgroundColor = settings.value(QStringLiteral("board/backgroundColor"), m_backgroundColor).value<QColor>();
    const QString imagePath = settings.value(QStringLiteral("board/backgroundImage")).toString();
    if (!imagePath.isEmpty()) {
        QImageReader reader(imagePath);
        reader.setAutoTransform(true);
        if (dimensionsWithinLimits(reader.size())) {
            const QImage image = reader.read();
            if (imageWithinLimits(image)) {
                m_backgroundImage = image;
            }
        }
    }
}

void BoardWidget::chooseBackgroundColor()
{
    const QColor color = QColorDialog::getColor(m_backgroundColor, this, tr("Choose board background"));
    if (!color.isValid()) {
        return;
    }

    m_backgroundColor = color;
    m_backgroundImage = {};
    QSettings settings;
    settings.setValue(QStringLiteral("board/backgroundColor"), color);
    settings.remove(QStringLiteral("board/backgroundImage"));
    emit boardChanged();
    update();
}

void BoardWidget::chooseBackgroundImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Choose board background"),
        {},
        tr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!dimensionsWithinLimits(reader.size())) {
        emit interactionHintChanged(tr("The selected background image is too large."));
        return;
    }
    const QImage image = reader.read();
    if (!imageWithinLimits(image)) {
        emit interactionHintChanged(tr("Could not load the selected background image."));
        return;
    }

    m_backgroundImage = image;
    QSettings settings;
    settings.setValue(QStringLiteral("board/backgroundImage"), path);
    emit boardChanged();
    update();
}

void BoardWidget::clearBackgroundImage()
{
    if (m_backgroundImage.isNull()) {
        return;
    }

    m_backgroundImage = {};
    QSettings settings;
    settings.remove(QStringLiteral("board/backgroundImage"));
    emit boardChanged();
    update();
}

void BoardWidget::clearBoard()
{
    if (m_tileBackgrounds.isEmpty() && m_pieces.isEmpty() && m_links.isEmpty()) {
        return;
    }

    m_tileBackgrounds.clear();
    m_pieces.clear();
    m_links.clear();
    m_linkStart.reset();
    m_sourceTile.reset();
    m_sourcePieceIndex.reset();
    m_pendingEquipmentId.clear();
    m_dropTile.reset();
    bool playersUpdated = false;
    for (Player &player : m_players) {
        if (!player.pieceId.isEmpty()) {
            player.pieceId.clear();
            playersUpdated = true;
        }
    }
    emit pieceCountChanged(0);
    emit piecesChanged();
    if (playersUpdated) {
        emit playersChanged();
    }
    emit boardChanged();
    update();
}

void BoardWidget::resetSession()
{
    m_tileBackgrounds.clear();
    m_pieces.clear();
    m_players.clear();
    m_links.clear();
    m_backgroundColor = QColor(QStringLiteral("#20252b"));
    m_backgroundImage = {};
    m_pan = {};
    m_zoom = 1.0;
    m_linkStart.reset();
    m_linkHover.reset();
    m_dropTile.reset();
    m_sourceTile.reset();
    m_sourcePieceIndex.reset();
    m_pendingEquipmentId.clear();
    emit pieceCountChanged(0);
    emit piecesChanged();
    emit playersChanged();
    emit zoomChanged(100);
    update();
}

bool BoardWidget::saveSessionData(QJsonObject *data, QString *errorMessage) const
{
    if (!data) {
        if (errorMessage) {
            *errorMessage = tr("No session destination was provided.");
        }
        return false;
    }
    if (pieceCount() > MaxPieces
        || m_links.size() > MaxLinks
        || m_tileBackgrounds.size() > MaxTileBackgrounds
        || m_players.size() > MaxPlayers) {
        if (errorMessage) {
            *errorMessage = tr("The board exceeds the supported session limits.");
        }
        return false;
    }
    qint64 decodedImageBytes = 0;
    if ((!m_backgroundImage.isNull()
         && (!imageWithinLimits(m_backgroundImage)
             || !reserveDecodedImage(m_backgroundImage, &decodedImageBytes)))) {
        if (errorMessage) {
            *errorMessage = tr("The board images exceed the supported size limit.");
        }
        return false;
    }
    for (const QImage &image : m_tileBackgrounds) {
        if (!imageWithinLimits(image)
            || !reserveDecodedImage(image, &decodedImageBytes)) {
            if (errorMessage) {
                *errorMessage = tr("The board images exceed the supported size limit.");
            }
            return false;
        }
    }
    for (const QVector<GamePiece> &pieces : m_pieces) {
        for (const GamePiece &piece : pieces) {
            if (!imageWithinLimits(piece.image)
                || !reserveDecodedImage(piece.image, &decodedImageBytes)) {
                if (errorMessage) {
                    *errorMessage = tr("The board images exceed the supported size limit.");
                }
                return false;
            }
        }
    }

    QJsonArray pieces;
    for (auto iterator = m_pieces.cbegin(); iterator != m_pieces.cend(); ++iterator) {
        for (const GamePiece &piece : iterator.value()) {
            const auto image = encodedImage(piece.image);
            if (!image) {
                if (errorMessage) {
                    *errorMessage = tr("A game piece could not be encoded.");
                }
                return false;
            }
            pieces.append(QJsonObject{
                {QStringLiteral("q"), iterator.key().q},
                {QStringLiteral("r"), iterator.key().r},
                {QStringLiteral("id"), piece.id},
                {QStringLiteral("image"), *image},
                {QStringLiteral("scale"), piece.scale},
                {QStringLiteral("equipment"), piece.equipment},
                {QStringLiteral("ownerId"), piece.ownerId},
                {QStringLiteral("name"), piece.name},
                {QStringLiteral("equipped"), piece.equipped}
            });
        }
    }

    QJsonArray tileBackgrounds;
    for (auto iterator = m_tileBackgrounds.cbegin(); iterator != m_tileBackgrounds.cend(); ++iterator) {
        const auto image = encodedImage(iterator.value());
        if (!image) {
            if (errorMessage) {
                *errorMessage = tr("A tile background could not be encoded.");
            }
            return false;
        }
        tileBackgrounds.append(QJsonObject{
            {QStringLiteral("q"), iterator.key().q},
            {QStringLiteral("r"), iterator.key().r},
            {QStringLiteral("image"), *image}
        });
    }

    QJsonArray links;
    for (const HexLink &link : m_links) {
        links.append(QJsonObject{
            {QStringLiteral("startQ"), link.start.q},
            {QStringLiteral("startR"), link.start.r},
            {QStringLiteral("endQ"), link.end.q},
            {QStringLiteral("endR"), link.end.r},
            {QStringLiteral("color"), link.color.name(QColor::HexArgb)},
            {QStringLiteral("width"), link.width},
            {QStringLiteral("arrows"), static_cast<int>(link.arrows)}
        });
    }

    QJsonArray players;
    for (const Player &player : m_players) {
        players.append(QJsonObject{
            {QStringLiteral("id"), player.id},
            {QStringLiteral("name"), player.name},
            {QStringLiteral("pieceId"), player.pieceId},
            {QStringLiteral("totalHearts"), player.totalHearts},
            {QStringLiteral("currentHearts"), player.currentHearts},
            {QStringLiteral("notes"), player.notes}
        });
    }

    const auto backgroundImage = encodedImage(m_backgroundImage);
    if (!backgroundImage) {
        if (errorMessage) {
            *errorMessage = tr("The background image could not be encoded.");
        }
        return false;
    }

    *data = {
        {QStringLiteral("backgroundColor"), m_backgroundColor.name(QColor::HexArgb)},
        {QStringLiteral("backgroundImage"), *backgroundImage},
        {QStringLiteral("tileBackgrounds"), tileBackgrounds},
        {QStringLiteral("pieces"), pieces},
        {QStringLiteral("players"), players},
        {QStringLiteral("links"), links},
        {QStringLiteral("panX"), m_pan.x()},
        {QStringLiteral("panY"), m_pan.y()},
        {QStringLiteral("zoom"), m_zoom}
    };
    return true;
}

bool BoardWidget::loadSession(const QJsonObject &data, QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };
    const auto coordinate = [](const QJsonObject &object, const QString &key, int *result) {
        const QJsonValue value = object.value(key);
        if (!value.isDouble()) {
            return false;
        }
        const double number = value.toDouble();
        if (number < std::numeric_limits<int>::min()
            || number > std::numeric_limits<int>::max()
            || std::floor(number) != number) {
            return false;
        }
        *result = static_cast<int>(number);
        return true;
    };
    qint64 decodedImageBytes = 0;

    const QColor backgroundColor(data.value(QStringLiteral("backgroundColor")).toString());
    if (!backgroundColor.isValid()) {
        return fail(tr("The session has an invalid background color."));
    }

    QImage backgroundImage;
    const QJsonValue backgroundValue = data.value(QStringLiteral("backgroundImage"));
    if (!backgroundValue.isString()) {
        return fail(tr("The session has an invalid background image value."));
    }
    if (!backgroundValue.toString().isEmpty()) {
        const auto image = decodedImage(backgroundValue);
        if (!image || !reserveDecodedImage(*image, &decodedImageBytes)) {
            return fail(tr("The session background image is invalid."));
        }
        backgroundImage = *image;
    }

    QHash<HexCoord, QImage> tileBackgrounds;
    const QJsonValue tileBackgroundsValue = data.value(QStringLiteral("tileBackgrounds"));
    if (!tileBackgroundsValue.isUndefined()) {
        if (!tileBackgroundsValue.isArray()
            || tileBackgroundsValue.toArray().size() > MaxTileBackgrounds) {
            return fail(tr("The session contains an invalid number of tile backgrounds."));
        }
        for (const QJsonValue &value : tileBackgroundsValue.toArray()) {
            if (!value.isObject()) {
                return fail(tr("The session contains an invalid tile background."));
            }
            const QJsonObject object = value.toObject();
            HexCoord coord;
            const auto image = decodedImage(object.value(QStringLiteral("image")));
            if (!coordinate(object, QStringLiteral("q"), &coord.q)
                || !coordinate(object, QStringLiteral("r"), &coord.r)
                || !image) {
                return fail(tr("The session contains an invalid tile background."));
            }
            if (!reserveDecodedImage(*image, &decodedImageBytes)) {
                return fail(tr("The session images exceed the supported memory limit."));
            }
            tileBackgrounds.insert(coord, *image);
        }
    }

    const QString piecesKey = data.contains(QStringLiteral("pieces"))
        ? QStringLiteral("pieces")
        : QStringLiteral("tiles");
    const QJsonValue piecesValue = data.value(piecesKey);
    if (!piecesValue.isArray() || piecesValue.toArray().size() > MaxPieces) {
        return fail(tr("The session contains an invalid number of game pieces."));
    }
    QHash<HexCoord, QVector<GamePiece>> pieces;
    QSet<QString> pieceIds;
    for (const QJsonValue &value : piecesValue.toArray()) {
        if (!value.isObject()) {
            return fail(tr("The session contains an invalid game piece."));
        }
        const QJsonObject object = value.toObject();
        HexCoord coord;
        const auto image = decodedImage(object.value(QStringLiteral("image")));
        const QJsonValue scaleValue = object.value(QStringLiteral("scale"));
        const double scale = scaleValue.isUndefined() ? 1.0 : scaleValue.toDouble(-1.0);
        QString id = object.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        const bool equipment = object.value(QStringLiteral("equipment")).toBool(false);
        const QString ownerId = object.value(QStringLiteral("ownerId")).toString();
        const QJsonValue nameValue = object.value(QStringLiteral("name"));
        const QString name = nameValue.toString();
        const QJsonValue equippedValue = object.value(QStringLiteral("equipped"));
        const bool equipped = equippedValue.isUndefined()
            ? true
            : equippedValue.toBool();
        if (!coordinate(object, QStringLiteral("q"), &coord.q)
            || !coordinate(object, QStringLiteral("r"), &coord.r)
            || !image
            || scale < MinPieceScale
            || scale > MaxPieceScale
            || !std::isfinite(scale)
            || pieceIds.contains(id)
            || (!nameValue.isUndefined()
                && (!nameValue.isString() || name.size() > MaxNameLength))
            || (!equippedValue.isUndefined() && !equippedValue.isBool())
            || (!equipment && !ownerId.isEmpty())) {
            return fail(tr("The session contains an invalid game piece."));
        }
        if (!reserveDecodedImage(*image, &decodedImageBytes)) {
            return fail(tr("The session images exceed the supported memory limit."));
        }
        pieceIds.insert(id);
        pieces[coord].append({
            id,
            *image,
            scale,
            equipment,
            ownerId,
            name,
            equipped
        });
    }
    for (const QVector<GamePiece> &tilePieces : std::as_const(pieces)) {
        for (const GamePiece &piece : tilePieces) {
            if (piece.ownerId.isEmpty()) {
                continue;
            }
            bool validOwner = false;
            for (const QVector<GamePiece> &possibleOwners : std::as_const(pieces)) {
                validOwner = std::ranges::any_of(
                    possibleOwners,
                    [&piece](const GamePiece &candidate) {
                        return candidate.id == piece.ownerId && !candidate.equipment;
                    });
                if (validOwner) {
                    break;
                }
            }

            if (!validOwner || piece.ownerId == piece.id) {
                return fail(tr("The session contains equipment with a missing owner."));
            }
        }
    }

    QVector<Player> players;
    QSet<QString> playerIds;
    QSet<QString> assignedPieceIds;
    const QJsonValue playersValue = data.value(QStringLiteral("players"));
    if (!playersValue.isUndefined()) {
        if (!playersValue.isArray() || playersValue.toArray().size() > MaxPlayers) {
            return fail(tr("The session contains an invalid number of players."));
        }
        for (const QJsonValue &value : playersValue.toArray()) {
            if (!value.isObject()) {
                return fail(tr("The session contains an invalid player."));
            }
            const QJsonObject object = value.toObject();
            const QString id = object.value(QStringLiteral("id")).toString();
            const QString name = object.value(QStringLiteral("name")).toString();
            const QString pieceId = object.value(QStringLiteral("pieceId")).toString();
            const QJsonValue notesValue = object.value(QStringLiteral("notes"));
            const QString notes = notesValue.toString();
            int totalHearts = 0;
            int currentHearts = 0;
            if (id.isEmpty()
                || playerIds.contains(id)
                || !object.value(QStringLiteral("name")).isString()
                || name.size() > MaxNameLength
                || !object.value(QStringLiteral("pieceId")).isString()
                || (!notesValue.isUndefined()
                    && (!notesValue.isString()
                        || notes.size() > MaxPlayerNotesLength))
                || !coordinate(object, QStringLiteral("totalHearts"), &totalHearts)
                || !coordinate(object, QStringLiteral("currentHearts"), &currentHearts)
                || totalHearts < 1
                || totalHearts > MaxHearts
                || currentHearts < 0
                || currentHearts > totalHearts
                || (!pieceId.isEmpty() && assignedPieceIds.contains(pieceId))) {
                return fail(tr("The session contains an invalid player."));
            }
            if (!pieceId.isEmpty()) {
                bool validPiece = false;
                for (const QVector<GamePiece> &tilePieces : std::as_const(pieces)) {
                    validPiece = std::ranges::any_of(
                        tilePieces,
                        [&pieceId](const GamePiece &piece) {
                            return piece.id == pieceId && !piece.equipment;
                        });
                    if (validPiece) {
                        break;
                    }
                }
                if (!validPiece) {
                    return fail(tr("The session contains a player with a missing game piece."));
                }
                assignedPieceIds.insert(pieceId);
            }
            playerIds.insert(id);
            players.append({
                id,
                name,
                pieceId,
                totalHearts,
                currentHearts,
                notes
            });
        }
    }

    const QJsonValue linksValue = data.value(QStringLiteral("links"));
    if (!linksValue.isArray() || linksValue.toArray().size() > MaxLinks) {
        return fail(tr("The session contains an invalid number of links."));
    }
    QVector<HexLink> links;
    links.reserve(linksValue.toArray().size());
    for (const QJsonValue &value : linksValue.toArray()) {
        if (!value.isObject()) {
            return fail(tr("The session contains an invalid link."));
        }
        const QJsonObject object = value.toObject();
        HexLink link;
        int arrows = 0;
        const QColor color(object.value(QStringLiteral("color")).toString());
        const QJsonValue widthValue = object.value(QStringLiteral("width"));
        if (!coordinate(object, QStringLiteral("startQ"), &link.start.q)
            || !coordinate(object, QStringLiteral("startR"), &link.start.r)
            || !coordinate(object, QStringLiteral("endQ"), &link.end.q)
            || !coordinate(object, QStringLiteral("endR"), &link.end.r)
            || !coordinate(object, QStringLiteral("arrows"), &arrows)
            || !color.isValid()
            || !widthValue.isDouble()
            || widthValue.toDouble() < 1.0
            || widthValue.toDouble() > 12.0
            || arrows < static_cast<int>(ArrowStyle::None)
            || arrows > static_cast<int>(ArrowStyle::Both)) {
            return fail(tr("The session contains an invalid link."));
        }
        link.color = color;
        link.width = widthValue.toDouble();
        link.arrows = static_cast<ArrowStyle>(arrows);
        links.append(link);
    }

    const QJsonValue panX = data.value(QStringLiteral("panX"));
    const QJsonValue panY = data.value(QStringLiteral("panY"));
    const QJsonValue zoom = data.value(QStringLiteral("zoom"));
    if (!panX.isDouble() || !panY.isDouble() || !zoom.isDouble()
        || !std::isfinite(panX.toDouble()) || !std::isfinite(panY.toDouble())
        || !std::isfinite(zoom.toDouble())
        || std::abs(panX.toDouble()) > MaxPanOffset
        || std::abs(panY.toDouble()) > MaxPanOffset
        || zoom.toDouble() < MinZoom || zoom.toDouble() > MaxZoom) {
        return fail(tr("The session contains an invalid viewport."));
    }

    m_tileBackgrounds = std::move(tileBackgrounds);
    m_pieces = std::move(pieces);
    m_players = std::move(players);
    m_links = std::move(links);
    m_backgroundColor = backgroundColor;
    m_backgroundImage = backgroundImage;
    m_pan = {panX.toDouble(), panY.toDouble()};
    m_zoom = zoom.toDouble();
    m_linkStart.reset();
    m_linkHover.reset();
    m_dropTile.reset();
    m_sourceTile.reset();
    m_sourcePieceIndex.reset();
    m_pendingEquipmentId.clear();
    emit pieceCountChanged(pieceCount());
    emit piecesChanged();
    emit playersChanged();
    emit zoomChanged(std::lround(m_zoom * 100.0));
    update();
    return true;
}

void BoardWidget::setLinkMode(bool enabled)
{
    m_linkMode = enabled;
    m_linkStart.reset();
    m_linkHover.reset();
    const bool toolActive = m_linkMode || m_tilePaintMode || m_tileEraseMode;
    setMouseTracking(toolActive);
    setCursor(toolActive ? Qt::CrossCursor : Qt::ArrowCursor);
    emit interactionHintChanged(
        enabled
            ? tr("Link mode: click a start hex, then click an end hex. Press Escape to cancel.")
            : tr("Link mode disabled."));
    update();
}

void BoardWidget::setLinkWidth(double width)
{
    m_linkWidth = width;
}

void BoardWidget::setLinkColor(const QColor &color)
{
    if (color.isValid()) {
        m_linkColor = color;
    }
}

void BoardWidget::setArrowStyle(ArrowStyle arrows)
{
    m_arrowStyle = arrows;
}

void BoardWidget::setHexGridVisible(bool visible)
{
    m_hexGridVisible = visible;
    update();
}

void BoardWidget::setTilePaintMode(bool enabled)
{
    m_tilePaintMode = enabled;
    m_paintingTiles = false;
    m_lastPaintedTile.reset();
    setDropTile(std::nullopt);
    const bool toolActive = m_linkMode || m_tilePaintMode || m_tileEraseMode;
    setMouseTracking(toolActive);
    setCursor(toolActive ? Qt::CrossCursor : Qt::ArrowCursor);
    if (enabled) {
        emit interactionHintChanged(tr("Tile paint mode: click or drag across hexes to paint them."));
    }
}

void BoardWidget::setTileEraseMode(bool enabled)
{
    m_tileEraseMode = enabled;
    m_paintingTiles = false;
    m_lastPaintedTile.reset();
    setDropTile(std::nullopt);
    const bool toolActive = m_linkMode || m_tilePaintMode || m_tileEraseMode;
    setMouseTracking(toolActive);
    setCursor(toolActive ? Qt::CrossCursor : Qt::ArrowCursor);
    if (enabled) {
        emit interactionHintChanged(tr("Tile eraser mode: click or drag across hexes to clear them."));
    }
}

void BoardWidget::setTileBrushImage(const QImage &image)
{
    m_tileBrushImage = imageWithinLimits(image) ? image : QImage();
    if (!image.isNull() && m_tileBrushImage.isNull()) {
        emit interactionHintChanged(tr("The selected tile image is too large."));
    }
}

void BoardWidget::setNavigationMode(bool enabled)
{
    m_navigationMode = enabled;
    m_panning = false;
    if (enabled) {
        setCursor(Qt::OpenHandCursor);
        emit interactionHintChanged(tr("Navigation mode: drag anywhere to move the game board."));
    } else {
        const bool editing = m_linkMode || m_tilePaintMode || m_tileEraseMode;
        setCursor(editing ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

QVector<GamePiece> BoardWidget::gamePieces() const
{
    QVector<GamePiece> result;
    for (const QVector<GamePiece> &pieces : m_pieces) {
        result.append(pieces);
    }
    std::ranges::sort(result, [](const GamePiece &left, const GamePiece &right) {
        const int byName = QString::localeAwareCompare(left.name, right.name);
        return byName == 0 ? left.id < right.id : byName < 0;
    });
    return result;
}

QVector<Player> BoardWidget::players() const
{
    return m_players;
}

QString BoardWidget::addPlayer()
{
    if (m_players.size() >= MaxPlayers) {
        emit interactionHintChanged(
            tr("A session can contain at most %n player(s).", nullptr, MaxPlayers));
        return {};
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_players.append({
        id,
        tr("Player %1").arg(m_players.size() + 1),
        {},
        5,
        5,
        {}
    });
    emit playersChanged();
    emit boardChanged();
    return id;
}

bool BoardWidget::removePlayer(const QString &playerId)
{
    const qsizetype removed = m_players.removeIf([&playerId](const Player &player) {
        return player.id == playerId;
    });
    if (removed == 0) {
        return false;
    }
    emit playersChanged();
    emit boardChanged();
    update();
    return true;
}

bool BoardWidget::setPlayerName(const QString &playerId, const QString &name)
{
    if (name.size() > MaxNameLength) {
        return false;
    }
    const auto player = std::ranges::find(m_players, playerId, &Player::id);
    if (player == m_players.end() || player->name == name) {
        return player != m_players.end();
    }
    player->name = name;
    emit boardChanged();
    update();
    return true;
}

bool BoardWidget::setPlayerPiece(const QString &playerId, const QString &pieceId)
{
    const auto player = std::ranges::find(m_players, playerId, &Player::id);
    if (player == m_players.end()) {
        return false;
    }
    if (!pieceId.isEmpty()) {
        const QVector<GamePiece> pieces = gamePieces();
        const auto piece = std::ranges::find(pieces, pieceId, &GamePiece::id);
        if (piece == pieces.end()
            || piece->equipment
            || std::ranges::any_of(
                m_players,
                [&playerId, &pieceId](const Player &candidate) {
                    return candidate.id != playerId && candidate.pieceId == pieceId;
                })) {
            return false;
        }
    }
    if (player->pieceId == pieceId) {
        return true;
    }
    player->pieceId = pieceId;
    emit playersChanged();
    emit boardChanged();
    update();
    return true;
}

bool BoardWidget::setPlayerTotalHearts(const QString &playerId, int totalHearts)
{
    const auto player = std::ranges::find(m_players, playerId, &Player::id);
    if (player == m_players.end() || totalHearts < 1 || totalHearts > MaxHearts) {
        return false;
    }
    if (player->totalHearts == totalHearts) {
        return true;
    }
    player->totalHearts = totalHearts;
    player->currentHearts = std::min(player->currentHearts, totalHearts);
    emit boardChanged();
    update();
    return true;
}

bool BoardWidget::setPlayerCurrentHearts(const QString &playerId, int currentHearts)
{
    const auto player = std::ranges::find(m_players, playerId, &Player::id);
    if (player == m_players.end()
        || currentHearts < 0
        || currentHearts > player->totalHearts) {
        return false;
    }
    if (player->currentHearts == currentHearts) {
        return true;
    }
    player->currentHearts = currentHearts;
    emit boardChanged();
    update();
    return true;
}

bool BoardWidget::setPlayerNotes(const QString &playerId, const QString &notes)
{
    if (notes.size() > MaxPlayerNotesLength) {
        return false;
    }
    const auto player = std::ranges::find(m_players, playerId, &Player::id);
    if (player == m_players.end()) {
        return false;
    }
    if (player->notes == notes) {
        return true;
    }
    player->notes = notes;
    emit boardChanged();
    return true;
}

bool BoardWidget::setEquipmentEquipped(const QString &pieceId, bool equipped)
{
    const auto pieceRef = pieceById(pieceId);
    if (!pieceRef) {
        return false;
    }
    GamePiece &piece = m_pieces[pieceRef->tile][pieceRef->index];
    if (!piece.equipment || piece.ownerId.isEmpty()) {
        return false;
    }
    if (piece.equipped == equipped) {
        return true;
    }
    piece.equipped = equipped;
    emit piecesChanged();
    emit boardChanged();
    update();
    return true;
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    drawBackground(painter);

    QList<HexCoord> backgroundTiles = m_tileBackgrounds.keys();
    std::ranges::sort(backgroundTiles, [this](const HexCoord &left, const HexCoord &right) {
        const QPointF leftCenter = hexCenter(left);
        const QPointF rightCenter = hexCenter(right);
        return leftCenter.y() == rightCenter.y()
            ? leftCenter.x() < rightCenter.x()
            : leftCenter.y() < rightCenter.y();
    });
    for (const HexCoord &coord : backgroundTiles) {
        drawTileBackground(painter, coord, m_tileBackgrounds.value(coord));
    }

    if (m_hexGridVisible) {
        drawHexGrid(painter);
    }

    for (const HexLink &link : m_links) {
        drawLink(painter, link);
    }
    if (m_linkStart && m_linkHover && *m_linkStart != *m_linkHover) {
        drawLink(
            painter,
            {*m_linkStart, *m_linkHover, m_linkColor, m_linkWidth, m_arrowStyle},
            true);
    }

    for (const HexCoord &coord : pieceTilesInPaintOrder()) {
        const QVector<GamePiece> &pieces = m_pieces.constFind(coord).value();
        for (int index : unlinkedPieceIndices(coord)) {
            const PieceRef ownerRef{coord, index};
            for (const PieceRef &equipmentRef : equipmentFor(pieces.at(index).id, true)) {
                drawGamePiece(painter, equipmentRef);
            }
            drawGamePiece(painter, ownerRef);
        }
    }
    for (const HexCoord &coord : pieceTilesInPaintOrder()) {
        const QVector<GamePiece> &pieces = m_pieces.constFind(coord).value();
        for (int index : unlinkedPieceIndices(coord)) {
            const PieceRef ownerRef{coord, index};
            for (const PieceRef &equipmentRef : equipmentFor(pieces.at(index).id, true)) {
                drawGamePieceName(painter, equipmentRef);
            }
            drawGamePieceName(painter, ownerRef);
        }
    }

    if (m_linkMode && m_linkHover) {
        drawTileIndicator(painter, *m_linkHover, m_linkColor, 2.0);
    }
    if (m_linkStart) {
        drawTileIndicator(painter, *m_linkStart, QColor(QStringLiteral("#f6c85f")), 3.0);
    }
    if (m_sourceTile) {
        drawTileIndicator(painter, *m_sourceTile, QColor(QStringLiteral("#f6c85f")), 2.5);
    }
    if (m_dropTile) {
        drawTileIndicator(painter, *m_dropTile, QColor(QStringLiteral("#5ad1a4")), 3.0);
    }
}

void BoardWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (canAcceptDrop(event->mimeData())) {
        event->acceptProposedAction();
    }
}

void BoardWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (!canAcceptDrop(event->mimeData())) {
        return;
    }

    setDropTile(hexAt(event->position()));
    event->acceptProposedAction();
}

void BoardWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropTile(std::nullopt);
    event->accept();
}

void BoardWidget::dropEvent(QDropEvent *event)
{
    if (pieceCount() >= MaxPieces) {
        setDropTile(std::nullopt);
        emit interactionHintChanged(
            tr("The board has reached the maximum number of game pieces."));
        return;
    }
    const auto image = imageFromDrop(event->mimeData());
    if (!image) {
        return;
    }

    const HexCoord target = hexAt(event->position());
    m_pieces[target].append({
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        *image,
        1.0,
        false,
        {},
        {},
        true
    });
    setDropTile(std::nullopt);
    emit pieceCountChanged(pieceCount());
    emit piecesChanged();
    emit boardChanged();
    emit interactionHintChanged(tr("Game piece placed. Drag it to move it to another tile."));
    event->acceptProposedAction();
    update();
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton) {
        return;
    }

    m_lastPointerPosition = event->position();
    const HexCoord pressedTile = hexAt(event->position());
    if (!m_pendingEquipmentId.isEmpty() && event->button() == Qt::LeftButton) {
        const auto equipmentRef = pieceById(m_pendingEquipmentId);
        const auto ownerRef = pieceAt(event->position());
        if (!equipmentRef) {
            m_pendingEquipmentId.clear();
            emit interactionHintChanged(tr("The equipment is no longer available."));
        } else if (!ownerRef) {
            emit interactionHintChanged(tr("Click a game piece to link the equipment."));
        } else {
            const GamePiece &target =
                m_pieces.constFind(ownerRef->tile)->at(ownerRef->index);
            if (target.equipment || target.id == m_pendingEquipmentId) {
                emit interactionHintChanged(tr("Equipment must be linked to a regular game piece."));
            } else {
                linkEquipment(*equipmentRef, *ownerRef);
                m_pendingEquipmentId.clear();
                emit interactionHintChanged(tr("Equipment linked to the game piece."));
            }
        }
        update();
        return;
    }

    if (m_navigationMode && event->button() == Qt::LeftButton) {
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if ((m_tilePaintMode || m_tileEraseMode) && event->button() == Qt::LeftButton) {
        if (m_tilePaintMode && m_tileBrushImage.isNull()) {
            emit interactionHintChanged(tr("Choose a tile image before painting."));
            return;
        }
        m_paintingTiles = true;
        m_lastPaintedTile = pressedTile;
        setDropTile(pressedTile);
        applyTileBrush(pressedTile);
        return;
    }

    if (m_linkMode && event->button() == Qt::LeftButton) {
        if (!m_linkStart) {
            m_linkStart = pressedTile;
            emit interactionHintChanged(tr("Start hex selected. Click the destination hex."));
        } else if (*m_linkStart == pressedTile) {
            m_linkStart.reset();
            emit interactionHintChanged(tr("Link selection canceled."));
        } else {
            if (m_links.size() >= MaxLinks) {
                emit interactionHintChanged(
                    tr("The board has reached the maximum number of links."));
                return;
            }
            m_links.append({*m_linkStart, pressedTile, m_linkColor, m_linkWidth, m_arrowStyle});
            m_linkStart.reset();
            emit boardChanged();
            emit interactionHintChanged(tr("Hexes linked. Click another start hex to continue."));
        }
        m_linkHover = pressedTile;
        update();
        return;
    }

    auto pressedPiece = pieceAt(event->position());
    if (event->button() == Qt::LeftButton && pressedPiece) {
        const GamePiece &selected =
            m_pieces.constFind(pressedPiece->tile)->at(pressedPiece->index);
        if (!selected.ownerId.isEmpty()) {
            pressedPiece = pieceById(selected.ownerId);
        }
        if (!pressedPiece) {
            return;
        }
        m_movingTile = true;
        m_sourceTile = pressedPiece->tile;
        m_sourcePieceIndex = pressedPiece->index;
        m_dropTile = pressedPiece->tile;
        setCursor(Qt::ClosedHandCursor);
    } else {
        m_panning = true;
        setCursor(Qt::ClosedHandCursor);
    }
    update();
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPointF delta = event->position() - m_lastPointerPosition;
        const QPointF previousPan = m_pan;
        m_pan = boundedPan(m_pan + delta);
        m_lastPointerPosition = event->position();
        if (m_pan != previousPan) {
            emit boardChanged();
        }
        update();
        return;
    }

    if (m_tilePaintMode || m_tileEraseMode) {
        const HexCoord tile = hexAt(event->position());
        setDropTile(tile);
        if (m_paintingTiles && m_lastPaintedTile != tile) {
            for (const HexCoord &intermediate : hexLine(*m_lastPaintedTile, tile)) {
                applyTileBrush(intermediate);
            }
            m_lastPaintedTile = tile;
        }
        return;
    }

    if (m_linkMode) {
        const HexCoord hover = hexAt(event->position());
        if (m_linkHover != hover) {
            m_linkHover = hover;
            update();
        }
        return;
    }

    if (m_movingTile) {
        setDropTile(hexAt(event->position()));
        return;
    }

}

void BoardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton) {
        return;
    }

    if ((m_tilePaintMode || m_tileEraseMode) && event->button() == Qt::LeftButton) {
        m_paintingTiles = false;
        m_lastPaintedTile.reset();
        setDropTile(std::nullopt);
        return;
    }

    if (m_linkMode && event->button() == Qt::LeftButton) {
        return;
    }

    if (m_movingTile && m_sourceTile && m_sourcePieceIndex && m_dropTile
        && *m_sourceTile != *m_dropTile) {
        movePieceGroup({*m_sourceTile, *m_sourcePieceIndex}, *m_dropTile);
    }

    m_panning = false;
    m_movingTile = false;
    m_sourceTile.reset();
    m_sourcePieceIndex.reset();
    m_dropTile.reset();
    if (m_navigationMode) {
        setCursor(Qt::OpenHandCursor);
    } else {
        setCursor(
            (m_linkMode || m_tilePaintMode || m_tileEraseMode)
                ? Qt::CrossCursor
                : Qt::ArrowCursor);
    }
    update();
}

void BoardWidget::wheelEvent(QWheelEvent *event)
{
    const QPointF position = event->position();
    const QPointF worldBeforeZoom = screenToWorld(position);
    const double previousZoom = m_zoom;
    const QPointF previousPan = m_pan;
    const double factor = std::pow(1.0015, event->angleDelta().y());
    m_zoom = std::clamp(m_zoom * factor, MinZoom, MaxZoom);
    m_pan = boundedPan(
        position
        - QPointF(width() / 2.0, height() / 2.0)
        - worldBeforeZoom * m_zoom);
    emit zoomChanged(std::lround(m_zoom * 100.0));
    if (m_zoom != previousZoom || m_pan != previousPan) {
        emit boardChanged();
    }
    update();
    event->accept();
}

void BoardWidget::contextMenuEvent(QContextMenuEvent *event)
{
    const HexCoord coord = hexAt(event->pos());
    const auto piece = pieceAt(event->pos());
    QMenu menu(this);
    double currentScale = 1.0;
    bool isEquipment = false;
    QString ownerId;
    QString selectedPieceId;
    QString selectedPieceName;
    bool assignedToPlayer = false;
    QString assignedPlayerId;
    if (piece) {
        const auto pieces = m_pieces.constFind(piece->tile);
        if (pieces != m_pieces.cend() && piece->index >= 0 && piece->index < pieces->size()) {
            const GamePiece &selected = pieces->at(piece->index);
            currentScale = selected.scale;
            isEquipment = selected.equipment;
            ownerId = selected.ownerId;
            selectedPieceId = selected.id;
            selectedPieceName = selected.name;
            assignedToPlayer = std::ranges::any_of(
                m_players,
                [&selected, &assignedPlayerId](const Player &player) {
                    if (player.pieceId != selected.id) {
                        return false;
                    }
                    assignedPlayerId = player.id;
                    return true;
                });
        }
    }
    QMenu *sizeMenu = menu.addMenu(tr("Game piece size"));
    sizeMenu->setEnabled(piece.has_value() && ownerId.isEmpty());
    auto *sliderContainer = new QWidget(sizeMenu);
    auto *sliderLayout = new QHBoxLayout(sliderContainer);
    sliderLayout->setContentsMargins(8, 4, 8, 4);
    auto *scaleLabel = new QLabel(
        tr("%1%").arg(std::lround(currentScale * 100.0)),
        sliderContainer);
    auto *scaleSlider = new QSlider(Qt::Horizontal, sliderContainer);
    scaleSlider->setRange(
        std::lround(MinPieceScale / PieceScaleStep),
        std::lround(MaxPieceScale / PieceScaleStep));
    scaleSlider->setValue(std::lround(currentScale / PieceScaleStep));
    scaleSlider->setSingleStep(1);
    scaleSlider->setPageStep(4);
    scaleSlider->setMinimumWidth(220);
    scaleSlider->setTickPosition(QSlider::TicksBelow);
    scaleSlider->setTickInterval(4);
    sliderLayout->addWidget(scaleSlider);
    sliderLayout->addWidget(scaleLabel);
    auto *sliderAction = new QWidgetAction(sizeMenu);
    sliderAction->setDefaultWidget(sliderContainer);
    sizeMenu->addAction(sliderAction);
    QAction *resetSizeAction = sizeMenu->addAction(tr("Reset to 100%"));
    resetSizeAction->setEnabled(currentScale != 1.0);
    connect(
        scaleSlider,
        &QSlider::valueChanged,
        this,
        [this, piece, scaleLabel, resetSizeAction](int step) {
            if (!piece) {
                return;
            }
            auto pieces = m_pieces.find(piece->tile);
            if (pieces == m_pieces.end()
                || piece->index < 0
                || piece->index >= pieces->size()) {
                return;
            }
            const double scale = step * PieceScaleStep;
            scaleLabel->setText(tr("%1%").arg(std::lround(scale * 100.0)));
            resetSizeAction->setEnabled(scale != 1.0);
            if ((*pieces)[piece->index].scale == scale) {
                return;
            }
            (*pieces)[piece->index].scale = scale;
            emit boardChanged();
            update();
        });
    QAction *renameAction = menu.addAction(tr("Rename game piece..."));
    renameAction->setEnabled(piece.has_value());
    menu.addSeparator();
    QAction *playerAction = menu.addAction(tr("Player"));
    playerAction->setCheckable(true);
    playerAction->setChecked(assignedToPlayer);
    playerAction->setEnabled(piece.has_value() && !isEquipment);
    QAction *equipmentAction = menu.addAction(tr("Equipment"));
    equipmentAction->setCheckable(true);
    equipmentAction->setChecked(isEquipment);
    equipmentAction->setEnabled(
        piece.has_value()
        && !assignedToPlayer
        && (isEquipment || equipmentFor(selectedPieceId).isEmpty()));
    QAction *linkEquipmentAction = menu.addAction(tr("Link to game piece"));
    linkEquipmentAction->setEnabled(piece.has_value() && isEquipment && ownerId.isEmpty());
    QAction *unlinkEquipmentAction = menu.addAction(tr("Unlink from game piece"));
    unlinkEquipmentAction->setEnabled(piece.has_value() && !ownerId.isEmpty());
    menu.addSeparator();
    QAction *removeAction = menu.addAction(tr("Remove game piece"));
    removeAction->setEnabled(piece.has_value());
    QAction *removeTileBackgroundAction = menu.addAction(tr("Remove tile background"));
    removeTileBackgroundAction->setEnabled(m_tileBackgrounds.contains(coord));
    QAction *removeLinksAction = menu.addAction(tr("Remove links from this hex"));
    removeLinksAction->setEnabled(std::ranges::any_of(m_links, [coord](const HexLink &link) {
        return link.start == coord || link.end == coord;
    }));

    QAction *selectedAction = menu.exec(event->globalPos());
    if (selectedAction == playerAction && piece) {
        if (playerAction->isChecked()) {
            const QString playerId = addPlayer();
            if (playerId.isEmpty() || !setPlayerPiece(playerId, selectedPieceId)) {
                if (!playerId.isEmpty()) {
                    removePlayer(playerId);
                }
                emit interactionHintChanged(tr("Could not identify this game piece as a player."));
                return;
            }
            if (!selectedPieceName.isEmpty()) {
                setPlayerName(playerId, selectedPieceName);
            }
            emit interactionHintChanged(
                tr("Game piece identified as a player. Edit the player name in the Players panel."));
        } else if (!assignedPlayerId.isEmpty()) {
            setPlayerPiece(assignedPlayerId, {});
            emit interactionHintChanged(
                tr("Player identifier removed. The player entry and health were preserved."));
        }
        update();
    } else if (selectedAction == renameAction && piece) {
        bool accepted = false;
        const QString name = QInputDialog::getText(
            this,
            tr("Rename game piece"),
            tr("Name:"),
            QLineEdit::Normal,
            selectedPieceName,
            &accepted).trimmed();
        if (!accepted || name.size() > MaxNameLength) {
            return;
        }
        auto pieces = m_pieces.find(piece->tile);
        if (pieces == m_pieces.end() || piece->index < 0 || piece->index >= pieces->size()) {
            return;
        }
        (*pieces)[piece->index].name = name;
        emit piecesChanged();
        emit boardChanged();
        update();
    } else if (selectedAction == equipmentAction && piece) {
        auto pieces = m_pieces.find(piece->tile);
        if (pieces == m_pieces.end() || piece->index < 0 || piece->index >= pieces->size()) {
            return;
        }
        GamePiece &selected = (*pieces)[piece->index];
        selected.equipment = equipmentAction->isChecked();
        if (!selected.equipment) {
            selected.ownerId.clear();
        }
        emit piecesChanged();
        emit boardChanged();
        update();
    } else if (selectedAction == linkEquipmentAction && piece) {
        m_pendingEquipmentId = selectedPieceId;
        setCursor(Qt::CrossCursor);
        emit interactionHintChanged(tr("Click the game piece that should own this equipment."));
    } else if (selectedAction == unlinkEquipmentAction && piece) {
        auto pieces = m_pieces.find(piece->tile);
        if (pieces == m_pieces.end() || piece->index < 0 || piece->index >= pieces->size()) {
            return;
        }
        (*pieces)[piece->index].ownerId.clear();
        emit piecesChanged();
        emit boardChanged();
        update();
    } else if (selectedAction == resetSizeAction && piece) {
        auto pieces = m_pieces.find(piece->tile);
        if (pieces == m_pieces.end() || piece->index < 0 || piece->index >= pieces->size()) {
            return;
        }
        GamePiece &selectedPiece = (*pieces)[piece->index];
        selectedPiece.scale = 1.0;
        emit boardChanged();
        update();
    } else if (selectedAction == removeAction && piece) {
        auto pieces = m_pieces.find(piece->tile);
        if (pieces == m_pieces.end() || piece->index < 0 || piece->index >= pieces->size()) {
            return;
        }
        const QString removedId = pieces->at(piece->index).id;
        pieces->removeAt(piece->index);
        if (pieces->isEmpty()) {
            m_pieces.erase(pieces);
        }
        for (auto iterator = m_pieces.begin(); iterator != m_pieces.end(); ++iterator) {
            for (GamePiece &candidate : iterator.value()) {
                if (candidate.ownerId == removedId) {
                    candidate.ownerId.clear();
                }
            }
        }
        bool playersUpdated = false;
        for (Player &player : m_players) {
            if (player.pieceId == removedId) {
                player.pieceId.clear();
                playersUpdated = true;
            }
        }
        emit pieceCountChanged(pieceCount());
        emit piecesChanged();
        if (playersUpdated) {
            emit playersChanged();
        }
        emit boardChanged();
        update();
    } else if (selectedAction == removeTileBackgroundAction) {
        m_tileBackgrounds.remove(coord);
        emit boardChanged();
        update();
    } else if (selectedAction == removeLinksAction) {
        m_links.removeIf([coord](const HexLink &link) {
            return link.start == coord || link.end == coord;
        });
        emit boardChanged();
        update();
    }
}

void BoardWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && !m_pendingEquipmentId.isEmpty()) {
        m_pendingEquipmentId.clear();
        setCursor(Qt::ArrowCursor);
        emit interactionHintChanged(tr("Equipment linking canceled."));
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_linkStart) {
        m_linkStart.reset();
        emit interactionHintChanged(tr("Link selection canceled."));
        update();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

QPointF BoardWidget::hexCenter(const HexCoord &coord) const
{
    return {
        HexWidth * 0.75 * coord.q,
        HexHeight * (coord.r + coord.q / 2.0)
    };
}

HexCoord BoardWidget::hexAt(const QPointF &screenPosition) const
{
    const QPointF world = screenToWorld(screenPosition);
    const double q = world.x() / (HexWidth * 0.75);
    const double r = world.y() / HexHeight - q / 2.0;
    return roundedAxial(q, r);
}

QPolygonF BoardWidget::hexPolygon(const HexCoord &coord) const
{
    const QPointF center = worldToScreen(hexCenter(coord));
    const double halfWidth = HexWidth * m_zoom / 2.0;
    const double halfHeight = HexHeight * m_zoom / 2.0;
    return {
        center + QPointF(-halfWidth / 2.0, -halfHeight),
        center + QPointF(halfWidth / 2.0, -halfHeight),
        center + QPointF(halfWidth, 0.0),
        center + QPointF(halfWidth / 2.0, halfHeight),
        center + QPointF(-halfWidth / 2.0, halfHeight),
        center + QPointF(-halfWidth, 0.0)
    };
}

QPointF BoardWidget::worldToScreen(const QPointF &worldPosition) const
{
    return worldPosition * m_zoom + QPointF(width() / 2.0, height() / 2.0) + m_pan;
}

QPointF BoardWidget::screenToWorld(const QPointF &screenPosition) const
{
    return (screenPosition - QPointF(width() / 2.0, height() / 2.0) - m_pan) / m_zoom;
}

QRectF BoardWidget::pieceSlot(const HexCoord &coord, int index, int count) const
{
    const QPointF center = worldToScreen(hexCenter(coord));
    if (count <= 1) {
        return hexPolygon(coord).boundingRect();
    }

    const int columns = std::ceil(std::sqrt(static_cast<double>(count)));
    const int rows = (count + columns - 1) / columns;
    const int row = index / columns;
    const int column = index % columns;
    const int itemsInRow = std::min(columns, count - row * columns);
    const double available = std::min(HexWidth, HexHeight) * m_zoom * 0.66;
    const double cellWidth = available / columns;
    const double cellHeight = available / rows;
    const QPointF slotCenter(
        center.x() + (column - (itemsInRow - 1) / 2.0) * cellWidth,
        center.y() + (row - (rows - 1) / 2.0) * cellHeight);
    const QSizeF slotSize(cellWidth * 0.88, cellHeight * 0.88);
    return {slotCenter - QPointF(slotSize.width() / 2.0, slotSize.height() / 2.0), slotSize};
}

QRectF BoardWidget::pieceRenderRect(
    const PieceRef &pieceRef) const
{
    const auto tilePieces = m_pieces.constFind(pieceRef.tile);
    if (tilePieces == m_pieces.cend()
        || pieceRef.index < 0
        || pieceRef.index >= tilePieces->size()) {
        return {};
    }
    const GamePiece &piece = tilePieces->at(pieceRef.index);
    if (!piece.ownerId.isEmpty()) {
        const auto ownerRef = pieceById(piece.ownerId);
        return ownerRef ? equipmentRenderRect(pieceRef, *ownerRef) : QRectF();
    }

    const QVector<int> visibleIndices = unlinkedPieceIndices(pieceRef.tile);
    const int layoutIndex = visibleIndices.indexOf(pieceRef.index);
    if (layoutIndex < 0) {
        return {};
    }
    const QRectF slot = pieceSlot(pieceRef.tile, layoutIndex, visibleIndices.size());
    QSizeF size;
    if (visibleIndices.size() <= 1) {
        const double widthScale = HexWidth * m_zoom
            / (piece.image.width() + piece.image.height() / 2.0);
        const double heightScale = HexHeight * m_zoom / piece.image.height();
        const double imageScale = std::min(widthScale, heightScale);
        size = QSizeF(
            piece.image.width() * imageScale,
            piece.image.height() * imageScale);
    } else {
        size = piece.image.size().scaled(slot.size().toSize(), Qt::KeepAspectRatio);
    }
    size *= piece.scale;
    return {
        slot.center().x() - size.width() / 2.0,
        slot.center().y() - size.height() / 2.0,
        size.width(),
        size.height()
    };
}

QRectF BoardWidget::equipmentRenderRect(
    const PieceRef &equipmentRef,
    const PieceRef &ownerRef) const
{
    const auto equipmentPieces = m_pieces.constFind(equipmentRef.tile);
    if (equipmentPieces == m_pieces.cend()
        || equipmentRef.index < 0
        || equipmentRef.index >= equipmentPieces->size()) {
        return {};
    }
    const GamePiece &equipment = equipmentPieces->at(equipmentRef.index);
    const QRectF ownerBounds = pieceRenderRect(ownerRef);
    const QVector<PieceRef> attachments = equipmentFor(equipment.ownerId, true);
    const int attachmentIndex =
        std::max(0, static_cast<int>(attachments.indexOf(equipmentRef)));
    const int attachmentCount = std::max(1, static_cast<int>(attachments.size()));
    const double equipmentHeight =
        ownerBounds.height() * std::min(0.48, 0.9 / attachmentCount);
    const double equipmentWidth =
        equipmentHeight * equipment.image.width() / equipment.image.height();
    const double availableHeight = ownerBounds.height() * 0.9;
    const double step = availableHeight / attachmentCount;
    const double centerY = ownerBounds.center().y()
        - availableHeight / 2.0
        + step * (attachmentIndex + 0.5);
    return {
        ownerBounds.left() - equipmentWidth * 0.78,
        centerY - equipmentHeight / 2.0,
        equipmentWidth,
        equipmentHeight
    };
}

QVector<int> BoardWidget::unlinkedPieceIndices(const HexCoord &coord) const
{
    QVector<int> indices;
    const auto pieces = m_pieces.constFind(coord);
    if (pieces == m_pieces.cend()) {
        return indices;
    }
    for (int index = 0; index < pieces->size(); ++index) {
        if (pieces->at(index).ownerId.isEmpty()) {
            indices.append(index);
        }
    }
    return indices;
}

QVector<BoardWidget::PieceRef> BoardWidget::equipmentFor(
    const QString &ownerId,
    bool equippedOnly) const
{
    QVector<PieceRef> attachments;
    for (auto iterator = m_pieces.cbegin(); iterator != m_pieces.cend(); ++iterator) {
        for (int index = 0; index < iterator->size(); ++index) {
            if (iterator->at(index).ownerId == ownerId
                && (!equippedOnly || iterator->at(index).equipped)) {
                attachments.append({iterator.key(), index});
            }
        }
    }
    return attachments;
}

std::optional<BoardWidget::PieceRef> BoardWidget::pieceById(const QString &id) const
{
    if (id.isEmpty()) {
        return std::nullopt;
    }
    for (auto iterator = m_pieces.cbegin(); iterator != m_pieces.cend(); ++iterator) {
        for (int index = 0; index < iterator->size(); ++index) {
            if (iterator->at(index).id == id) {
                return PieceRef{iterator.key(), index};
            }
        }
    }
    return std::nullopt;
}

QList<HexCoord> BoardWidget::pieceTilesInPaintOrder() const
{
    QList<HexCoord> tiles = m_pieces.keys();
    std::ranges::sort(tiles, [this](const HexCoord &left, const HexCoord &right) {
        const QPointF leftCenter = hexCenter(left);
        const QPointF rightCenter = hexCenter(right);
        return leftCenter.y() == rightCenter.y()
            ? leftCenter.x() < rightCenter.x()
            : leftCenter.y() < rightCenter.y();
    });
    return tiles;
}

bool BoardWidget::pieceContainsPoint(
    const QRectF &bounds,
    const GamePiece &piece,
    const QPointF &screenPosition) const
{
    if (!bounds.contains(screenPosition)
        || bounds.width() <= 0.0
        || bounds.height() <= 0.0
        || piece.image.isNull()) {
        return false;
    }

    const int imageX = std::clamp(
        static_cast<int>((screenPosition.x() - bounds.left()) / bounds.width() * piece.image.width()),
        0,
        piece.image.width() - 1);
    const int imageY = std::clamp(
        static_cast<int>((screenPosition.y() - bounds.top()) / bounds.height() * piece.image.height()),
        0,
        piece.image.height() - 1);
    return qAlpha(piece.image.pixel(imageX, imageY)) > 16;
}

std::optional<BoardWidget::PieceRef> BoardWidget::pieceAt(const QPointF &screenPosition) const
{
    const QList<HexCoord> tiles = pieceTilesInPaintOrder();
    for (int tileIndex = tiles.size() - 1; tileIndex >= 0; --tileIndex) {
        const HexCoord tile = tiles.at(tileIndex);
        const QVector<GamePiece> &pieces = m_pieces.constFind(tile).value();
        const QVector<int> visibleIndices = unlinkedPieceIndices(tile);
        for (int visibleIndex = visibleIndices.size() - 1; visibleIndex >= 0; --visibleIndex) {
            const int pieceIndex = visibleIndices.at(visibleIndex);
            const GamePiece &piece = pieces.at(pieceIndex);
            const PieceRef ownerRef{tile, pieceIndex};
            const QRectF bounds = pieceRenderRect(ownerRef);
            if (pieceContainsPoint(bounds, piece, screenPosition)) {
                return ownerRef;
            }
            const QVector<PieceRef> attachments = equipmentFor(piece.id, true);
            for (int attachmentIndex = attachments.size() - 1;
                 attachmentIndex >= 0;
                 --attachmentIndex) {
                const PieceRef attachmentRef = attachments.at(attachmentIndex);
                const GamePiece &attachment =
                    m_pieces.constFind(attachmentRef.tile)->at(attachmentRef.index);
                if (pieceContainsPoint(
                        pieceRenderRect(attachmentRef),
                        attachment,
                        screenPosition)) {
                    return attachmentRef;
                }
            }
        }
    }
    return std::nullopt;
}

int BoardWidget::pieceCount() const
{
    int count = 0;
    for (const QVector<GamePiece> &pieces : m_pieces) {
        count += pieces.size();
    }
    return count;
}

bool BoardWidget::canAcceptDrop(const QMimeData *mimeData) const
{
    if (mimeData->hasImage()) {
        return true;
    }

    return std::ranges::any_of(mimeData->urls(), [](const QUrl &url) {
        return url.isLocalFile();
    });
}

std::optional<QImage> BoardWidget::imageFromDrop(const QMimeData *mimeData) const
{
    if (mimeData->hasImage()) {
        const QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (imageWithinLimits(image)) {
            return image;
        }
    }

    for (const QUrl &url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        QImageReader reader(url.toLocalFile());
        reader.setAutoTransform(true);
        if (!dimensionsWithinLimits(reader.size())) {
            continue;
        }
        const QImage image = reader.read();
        if (imageWithinLimits(image)) {
            return image;
        }
    }
    return std::nullopt;
}

void BoardWidget::drawBackground(QPainter &painter)
{
    painter.fillRect(rect(), m_backgroundColor);
    if (m_backgroundImage.isNull()) {
        return;
    }

    const QSize scaledSize = m_backgroundImage.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
    const QRect target(
        (width() - scaledSize.width()) / 2,
        (height() - scaledSize.height()) / 2,
        scaledSize.width(),
        scaledSize.height());
    painter.drawImage(target, m_backgroundImage);
}

void BoardWidget::drawTileBackground(
    QPainter &painter,
    const HexCoord &coord,
    const QImage &image)
{
    const QPointF center = worldToScreen(hexCenter(coord));
    const double renderedWidth = HexWidth * m_zoom;
    const double scale = renderedWidth / image.width();
    const QSizeF renderedSize(renderedWidth, image.height() * scale);
    const QRectF target(
        center.x() - renderedSize.width() / 2.0,
        center.y() - HexHeight * m_zoom / 2.0,
        renderedSize.width(),
        renderedSize.height());
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, image);
    painter.restore();
}

void BoardWidget::drawHexGrid(QPainter &painter)
{
    const std::array<HexCoord, 4> corners{
        hexAt(QPointF(0.0, 0.0)),
        hexAt(QPointF(width(), 0.0)),
        hexAt(QPointF(0.0, height())),
        hexAt(QPointF(width(), height()))
    };
    const auto [minimumQ, maximumQ] = std::minmax_element(
        corners.cbegin(),
        corners.cend(),
        [](const HexCoord &left, const HexCoord &right) {
            return left.q < right.q;
        });
    const auto [minimumR, maximumR] = std::minmax_element(
        corners.cbegin(),
        corners.cend(),
        [](const HexCoord &left, const HexCoord &right) {
            return left.r < right.r;
        });

    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (int q = minimumQ->q - 2; q <= maximumQ->q + 2; ++q) {
        for (int r = minimumR->r - 2; r <= maximumR->r + 2; ++r) {
            const QPolygonF polygon = hexPolygon({q, r});
            if (!polygon.boundingRect().intersects(rect())) {
                continue;
            }
            painter.setPen(QPen(QColor(0, 0, 0, 145), 2.5));
            painter.drawPolygon(polygon);
            painter.setPen(QPen(QColor(255, 255, 255, 165), 1.0));
            painter.drawPolygon(polygon);
        }
    }
    painter.restore();
}

void BoardWidget::drawLink(QPainter &painter, const HexLink &link, bool preview)
{
    const QPointF startCenter = worldToScreen(hexCenter(link.start));
    const QPointF endCenter = worldToScreen(hexCenter(link.end));
    const QPointF offset = endCenter - startCenter;
    const double distance = std::hypot(offset.x(), offset.y());
    if (distance < 1.0) {
        return;
    }

    const QPointF direction = offset / distance;
    const double penWidth = std::max(1.0, link.width * m_zoom);

    painter.save();
    QPen pen(link.color, penWidth, preview ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(link.color);
    painter.drawLine(startCenter, endCenter);

    const auto drawArrow = [&](const QPointF &tip, const QPointF &arrowDirection) {
        const double arrowLength = (9.0 + link.width * 1.5) * m_zoom;
        const double arrowWidth = arrowLength * 0.55;
        const QPointF normal(-arrowDirection.y(), arrowDirection.x());
        const QPointF base = tip - arrowDirection * arrowLength;
        painter.drawPolygon(QPolygonF{
            tip,
            base + normal * arrowWidth,
            base - normal * arrowWidth
        });
    };

    if (link.arrows == ArrowStyle::End || link.arrows == ArrowStyle::Both) {
        drawArrow(endCenter, direction);
    }
    if (link.arrows == ArrowStyle::Both) {
        drawArrow(startCenter, -direction);
    }
    painter.restore();
}

void BoardWidget::drawGamePiece(
    QPainter &painter,
    const PieceRef &pieceRef)
{
    const auto pieces = m_pieces.constFind(pieceRef.tile);
    if (pieces == m_pieces.cend()
        || pieceRef.index < 0
        || pieceRef.index >= pieces->size()) {
        return;
    }
    const GamePiece &piece = pieces->at(pieceRef.index);
    const QRectF target = pieceRenderRect(pieceRef);
    const bool defeatedPlayer = std::ranges::any_of(
        m_players,
        [&piece](const Player &player) {
            return player.pieceId == piece.id && player.currentHearts == 0;
        });
    painter.save();
    if (defeatedPlayer) {
        painter.setOpacity(0.35);
    }
    painter.drawImage(target, piece.image);
    painter.restore();
}

void BoardWidget::drawGamePieceName(
    QPainter &painter,
    const PieceRef &pieceRef)
{
    const auto pieces = m_pieces.constFind(pieceRef.tile);
    if (pieces == m_pieces.cend()
        || pieceRef.index < 0
        || pieceRef.index >= pieces->size()) {
        return;
    }
    const GamePiece &piece = pieces->at(pieceRef.index);
    QString displayName = piece.name;
    const auto player = std::ranges::find(m_players, piece.id, &Player::pieceId);
    if (player != m_players.cend()) {
        displayName = player->name;
    }
    if (!displayName.isEmpty()) {
        const QRectF target = pieceRenderRect(pieceRef);
        painter.save();
        QFont labelFont = font();
        labelFont.setBold(true);
        labelFont.setPixelSize(12);
        painter.setFont(labelFont);
        const QFontMetrics metrics(labelFont);
        const QSize labelSize =
            metrics.size(Qt::TextSingleLine, displayName) + QSize(10, 4);
        const QRectF labelBounds(
            target.center().x() - labelSize.width() / 2.0,
            target.top() - labelSize.height() - 3.0,
            labelSize.width(),
            labelSize.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 190));
        painter.drawRoundedRect(labelBounds, 4.0, 4.0);
        painter.setPen(Qt::white);
        painter.drawText(labelBounds, Qt::AlignCenter, displayName);
        painter.restore();
    }
}

void BoardWidget::movePieceGroup(const PieceRef &ownerRef, const HexCoord &destination)
{
    const auto ownerPieces = m_pieces.constFind(ownerRef.tile);
    if (ownerPieces == m_pieces.cend()
        || ownerRef.index < 0
        || ownerRef.index >= ownerPieces->size()) {
        return;
    }
    const QString ownerId = ownerPieces->at(ownerRef.index).id;
    QSet<QString> groupIds{ownerId};
    for (const PieceRef &equipmentRef : equipmentFor(ownerId)) {
        groupIds.insert(m_pieces.constFind(equipmentRef.tile)->at(equipmentRef.index).id);
    }

    QVector<GamePiece> group;
    for (auto iterator = m_pieces.begin(); iterator != m_pieces.end();) {
        QVector<GamePiece> &tilePieces = iterator.value();
        for (int index = tilePieces.size() - 1; index >= 0; --index) {
            if (groupIds.contains(tilePieces.at(index).id)) {
                group.prepend(tilePieces.takeAt(index));
            }
        }
        if (tilePieces.isEmpty()) {
            iterator = m_pieces.erase(iterator);
        } else {
            ++iterator;
        }
    }
    std::stable_partition(
        group.begin(),
        group.end(),
        [&ownerId](const GamePiece &piece) {
            return piece.id == ownerId;
        });
    m_pieces[destination].append(group);
    emit pieceCountChanged(pieceCount());
    emit boardChanged();
}

void BoardWidget::linkEquipment(
    const PieceRef &equipmentRef,
    const PieceRef &ownerRef)
{
    const auto equipmentPieces = m_pieces.find(equipmentRef.tile);
    const auto ownerPieces = m_pieces.constFind(ownerRef.tile);
    if (equipmentPieces == m_pieces.end()
        || ownerPieces == m_pieces.cend()
        || equipmentRef.index < 0
        || equipmentRef.index >= equipmentPieces->size()
        || ownerRef.index < 0
        || ownerRef.index >= ownerPieces->size()) {
        return;
    }
    const QString ownerId = ownerPieces->at(ownerRef.index).id;
    GamePiece equipment = equipmentPieces->takeAt(equipmentRef.index);
    equipment.equipment = true;
    equipment.ownerId = ownerId;
    if (equipmentPieces->isEmpty()) {
        m_pieces.erase(equipmentPieces);
    }
    m_pieces[ownerRef.tile].append(equipment);
    emit piecesChanged();
    emit boardChanged();
}

void BoardWidget::drawTileIndicator(
    QPainter &painter,
    const HexCoord &coord,
    const QColor &color,
    double width)
{
    painter.save();
    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 28));
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolygon(hexPolygon(coord));
    painter.restore();
}

void BoardWidget::applyTileBrush(const HexCoord &coord)
{
    if (m_tileEraseMode) {
        if (m_tileBackgrounds.remove(coord) == 0) {
            return;
        }
    } else {
        if (m_tileBrushImage.isNull()) {
            return;
        }
        if (!m_tileBackgrounds.contains(coord)
            && m_tileBackgrounds.size() >= MaxTileBackgrounds) {
            emit interactionHintChanged(
                tr("The board has reached the maximum number of tile backgrounds."));
            return;
        }
        m_tileBackgrounds.insert(coord, m_tileBrushImage);
    }
    emit boardChanged();
    update();
}

void BoardWidget::setDropTile(std::optional<HexCoord> coord)
{
    if (m_dropTile == coord) {
        return;
    }
    m_dropTile = coord;
    update();
}
