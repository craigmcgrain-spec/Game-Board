#pragma once

#include <QIcon>
#include <QImage>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QMenu;
class QToolButton;

class TileAssetPicker final : public QWidget
{
    Q_OBJECT

public:
    explicit TileAssetPicker(QWidget *parent = nullptr);

    int loadDirectory(const QString &directory);

signals:
    void assetSelected(const QImage &image, bool activatePaintTool);
    void collectionLoaded(int assetCount);

private:
    struct Asset {
        QString id;
        QString group;
        QString name;
        QString description;
        QImage image;
        QIcon icon;
    };

    void rebuildMenu();
    void selectAsset(int index, bool activatePaintTool);
    void updateFavoriteButton();

    QVector<Asset> m_assets;
    QStringList m_favorites;
    QToolButton *m_selectButton = nullptr;
    QToolButton *m_favoriteButton = nullptr;
    QMenu *m_menu = nullptr;
    int m_currentIndex = -1;
};
