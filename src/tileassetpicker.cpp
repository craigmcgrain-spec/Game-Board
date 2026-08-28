#include "tileassetpicker.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMenu>
#include <QPixmap>
#include <QQueue>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QToolButton>

#include <cstdlib>
#include <utility>

namespace {
constexpr int MaxSourceDimension = 8192;
constexpr qint64 MaxSourcePixels = 16LL * 1024 * 1024;
constexpr int MaxTilesPerSheet = 4096;
constexpr int MaxAssets = 10000;
constexpr qint64 MaxAssetBytes = 512LL * 1024 * 1024;

bool dimensionsWithinLimits(const QSize &dimensions)
{
    return dimensions.isValid()
        && dimensions.width() <= MaxSourceDimension
        && dimensions.height() <= MaxSourceDimension
        && static_cast<qint64>(dimensions.width()) * dimensions.height()
            <= MaxSourcePixels;
}

QImage tileWithTransparency(QImage tile)
{
    const bool hasAlpha = tile.hasAlphaChannel();
    tile = tile.convertToFormat(QImage::Format_ARGB32);
    if (hasAlpha || tile.isNull()) {
        return tile;
    }

    const QColor keyColor = tile.pixelColor(0, 0);
    QQueue<QPoint> pending;
    const auto clearBackgroundPixel = [&](int x, int y) {
        QRgb &pixel = reinterpret_cast<QRgb *>(tile.scanLine(y))[x];
        if (qAlpha(pixel) == 0) {
            return;
        }
        if (std::abs(qRed(pixel) - keyColor.red()) > 16
            || std::abs(qGreen(pixel) - keyColor.green()) > 16
            || std::abs(qBlue(pixel) - keyColor.blue()) > 16) {
            return;
        }
        pixel = qRgba(0, 0, 0, 0);
        pending.enqueue({x, y});
    };

    for (int x = 0; x < tile.width(); ++x) {
        clearBackgroundPixel(x, 0);
        clearBackgroundPixel(x, tile.height() - 1);
    }
    for (int y = 0; y < tile.height(); ++y) {
        clearBackgroundPixel(0, y);
        clearBackgroundPixel(tile.width() - 1, y);
    }
    while (!pending.isEmpty()) {
        const QPoint point = pending.dequeue();
        if (point.x() > 0) {
            clearBackgroundPixel(point.x() - 1, point.y());
        }
        if (point.x() + 1 < tile.width()) {
            clearBackgroundPixel(point.x() + 1, point.y());
        }
        if (point.y() > 0) {
            clearBackgroundPixel(point.x(), point.y() - 1);
        }
        if (point.y() + 1 < tile.height()) {
            clearBackgroundPixel(point.x(), point.y() + 1);
        }
    }
    return tile;
}
}

TileAssetPicker::TileAssetPicker(QWidget *parent)
    : QWidget(parent)
    , m_favorites(QSettings().value(QStringLiteral("tiles/favorites")).toStringList())
    , m_selectButton(new QToolButton(this))
    , m_favoriteButton(new QToolButton(this))
    , m_menu(new QMenu(this))
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_selectButton->setPopupMode(QToolButton::InstantPopup);
    m_selectButton->setObjectName(QStringLiteral("tileAssetSelectButton"));
    m_selectButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_selectButton->setIconSize({42, 42});
    m_selectButton->setText(tr("Choose tile"));
    m_selectButton->setMenu(m_menu);
    m_selectButton->setMinimumWidth(240);
    layout->addWidget(m_selectButton);

    m_favoriteButton->setCheckable(true);
    m_favoriteButton->setObjectName(QStringLiteral("tileAssetFavoriteButton"));
    m_favoriteButton->setText(tr("Favorite"));
    m_favoriteButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_favoriteButton->setToolTip(tr("Add or remove the selected tile from Favorites"));
    layout->addWidget(m_favoriteButton);
    updateFavoriteButton();

    connect(m_favoriteButton, &QToolButton::toggled, this, [this](bool favorite) {
        if (m_currentIndex < 0 || m_currentIndex >= m_assets.size()) {
            return;
        }
        const QString id = m_assets.at(m_currentIndex).id;
        m_favorites.removeAll(id);
        if (favorite) {
            m_favorites.prepend(id);
        }
        QSettings().setValue(QStringLiteral("tiles/favorites"), m_favorites);
        updateFavoriteButton();
        rebuildMenu();
    });
}

int TileAssetPicker::loadDirectory(const QString &directory)
{
    m_assets.clear();
    m_currentIndex = -1;

    QStringList paths;
    QDirIterator iterator(
        directory,
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
         QStringLiteral("*.webp"), QStringLiteral("*.bmp"), QStringLiteral("*.gif")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        paths.append(iterator.next());
    }
    paths.sort(Qt::CaseInsensitive);

    const QDir root(directory);
    const QRegularExpression dimensionsPattern(
        QStringLiteral("([0-9]+)x([0-9]+)(?=\\.[^.]+$)"),
        QRegularExpression::CaseInsensitiveOption);
    qint64 assetBytes = 0;
    bool limitReached = false;
    for (const QString &path : paths) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        if (!dimensionsWithinLimits(reader.size())) {
            continue;
        }
        const QImage image = reader.read();
        if (image.isNull()) {
            continue;
        }

        const QString relativePath = root.relativeFilePath(path);
        QString relativeDirectory = root.relativeFilePath(QFileInfo(path).absolutePath());
        if (relativeDirectory == QStringLiteral(".")) {
            relativeDirectory = tr("Root");
        }
        const QRegularExpressionMatch match = dimensionsPattern.match(QFileInfo(path).fileName());
        int tileWidth = image.width();
        int tileHeight = image.height();
        if (match.hasMatch()) {
            tileWidth = match.captured(1).toInt();
            tileHeight = match.captured(2).toInt();
        }
        if (tileWidth <= 0 || tileHeight <= 0
            || image.width() % tileWidth != 0
            || image.height() % tileHeight != 0) {
            tileWidth = image.width();
            tileHeight = image.height();
        }

        const int columns = image.width() / tileWidth;
        const int rows = image.height() / tileHeight;
        const qint64 tileCount =
            static_cast<qint64>(columns) * rows;
        if (tileCount > MaxTilesPerSheet) {
            continue;
        }
        const bool isSheet = columns * rows > 1;
        const QString group = isSheet
            ? relativeDirectory + QLatin1Char('/') + QFileInfo(path).completeBaseName()
            : relativeDirectory;
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                if (m_assets.size() >= MaxAssets) {
                    limitReached = true;
                    break;
                }
                const QImage tile = tileWithTransparency(image.copy(
                    column * tileWidth,
                    row * tileHeight,
                    tileWidth,
                    tileHeight));
                const qint64 tileBytes = tile.sizeInBytes();
                if (tileBytes < 0
                    || assetBytes > MaxAssetBytes - tileBytes) {
                    limitReached = true;
                    break;
                }
                assetBytes += tileBytes;
                const int tileNumber = row * columns + column + 1;
                const QString name = isSheet
                    ? tr("Tile %1").arg(tileNumber)
                    : QFileInfo(path).completeBaseName();
                const QPixmap preview = QPixmap::fromImage(
                    tile.scaled(42, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                m_assets.append({
                    relativePath + QStringLiteral("::") + QString::number(tileNumber),
                    group,
                    name,
                    isSheet
                        ? tr("%1, tile %2 (row %3, column %4)")
                              .arg(relativePath)
                              .arg(tileNumber)
                              .arg(row + 1)
                              .arg(column + 1)
                        : relativePath,
                    tile,
                    QIcon(preview)
                });
            }
            if (limitReached) {
                break;
            }
        }
        if (limitReached) {
            break;
        }
    }

    rebuildMenu();
    m_selectButton->setEnabled(!m_assets.isEmpty());
    m_favoriteButton->setEnabled(!m_assets.isEmpty());
    if (!m_assets.isEmpty()) {
        selectAsset(0, false);
    } else {
        m_selectButton->setText(tr("No tile assets"));
        m_selectButton->setIcon({});
        updateFavoriteButton();
    }
    emit collectionLoaded(m_assets.size());
    return m_assets.size();
}

void TileAssetPicker::rebuildMenu()
{
    m_menu->clear();

    bool addedFavorite = false;
    if (!m_favorites.isEmpty()) {
        for (const QString &favoriteId : std::as_const(m_favorites)) {
            for (int index = 0; index < m_assets.size(); ++index) {
                const Asset &asset = m_assets.at(index);
                if (asset.id != favoriteId) {
                    continue;
                }
                if (!addedFavorite) {
                    m_menu->addSection(
                        QIcon::fromTheme(QStringLiteral("rating")),
                        tr("Favorites"));
                }
                QAction *action = m_menu->addAction(
                    asset.icon,
                    asset.group + QStringLiteral(" / ") + asset.name);
                action->setToolTip(asset.description);
                connect(action, &QAction::triggered, this, [this, index] {
                    selectAsset(index, true);
                });
                addedFavorite = true;
                break;
            }
        }
    }
    if (addedFavorite) {
        m_menu->addSeparator();
    }

    QHash<QString, QMenu *> groupMenus;
    groupMenus.insert(QString(), m_menu);
    for (int index = 0; index < m_assets.size(); ++index) {
        const Asset &asset = m_assets.at(index);
        QString parentPath;
        for (const QString &segment : asset.group.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            const QString path = parentPath.isEmpty()
                ? segment
                : parentPath + QLatin1Char('/') + segment;
            if (!groupMenus.contains(path)) {
                QMenu *parentMenu = groupMenus.value(parentPath, m_menu);
                groupMenus.insert(
                    path,
                    parentMenu->addMenu(
                        QIcon::fromTheme(QStringLiteral("folder-pictures")),
                        segment));
            }
            parentPath = path;
        }

        QMenu *groupMenu = groupMenus.value(asset.group, m_menu);
        QAction *action = groupMenu->addAction(asset.icon, asset.name);
        action->setToolTip(asset.description);
        connect(action, &QAction::triggered, this, [this, index] {
            selectAsset(index, true);
        });
    }
}

void TileAssetPicker::selectAsset(int index, bool activatePaintTool)
{
    if (index < 0 || index >= m_assets.size()) {
        return;
    }
    m_currentIndex = index;
    const Asset &asset = m_assets.at(index);
    m_selectButton->setText(asset.name);
    m_selectButton->setIcon(asset.icon);
    m_selectButton->setToolTip(asset.description);
    {
        const QSignalBlocker blocker(m_favoriteButton);
        m_favoriteButton->setChecked(m_favorites.contains(asset.id));
    }
    updateFavoriteButton();
    emit assetSelected(asset.image, activatePaintTool);
}

void TileAssetPicker::updateFavoriteButton()
{
    const bool favorite = m_currentIndex >= 0
        && m_currentIndex < m_assets.size()
        && m_favorites.contains(m_assets.at(m_currentIndex).id);
    m_favoriteButton->setIcon(QIcon::fromTheme(
        favorite ? QStringLiteral("rating") : QStringLiteral("rating-unrated")));
}
