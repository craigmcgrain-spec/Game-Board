#include "tileassetpicker.h"

#include <QDir>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

class TileAssetPickerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("HexboardTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TileAssetPickerTests"));
        QSettings().clear();
    }

    void foldersAreCollapsedAndFavoritesPersist()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("Forest"))));
        QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("Water"))));

        QImage forest(72, 72, QImage::Format_ARGB32);
        forest.fill(Qt::green);
        QImage water(72, 72, QImage::Format_ARGB32);
        water.fill(Qt::blue);
        QVERIFY(forest.save(directory.filePath(QStringLiteral("Forest/Oak.png"))));
        QVERIFY(water.save(directory.filePath(QStringLiteral("Water/Lake.png"))));

        TileAssetPicker picker;
        QCOMPARE(picker.loadDirectory(directory.path()), 2);
        auto *selectButton =
            picker.findChild<QToolButton *>(QStringLiteral("tileAssetSelectButton"));
        auto *favoriteButton =
            picker.findChild<QToolButton *>(QStringLiteral("tileAssetFavoriteButton"));
        QVERIFY(selectButton);
        QVERIFY(favoriteButton);

        int folderCount = 0;
        int directAssetCount = 0;
        for (QAction *action : selectButton->menu()->actions()) {
            folderCount += action->menu() != nullptr;
            directAssetCount += !action->isSeparator() && action->menu() == nullptr;
        }
        QCOMPARE(folderCount, 2);
        QCOMPARE(directAssetCount, 0);

        QAction *folder = selectButton->menu()->actions().first();
        QVERIFY(folder->menu());
        folder->menu()->actions().first()->trigger();
        favoriteButton->click();
        QVERIFY(favoriteButton->isChecked());

        directAssetCount = 0;
        for (QAction *action : selectButton->menu()->actions()) {
            directAssetCount += !action->isSeparator() && action->menu() == nullptr;
        }
        QCOMPARE(directAssetCount, 1);

        TileAssetPicker restoredPicker;
        QCOMPARE(restoredPicker.loadDirectory(directory.path()), 2);
        auto *restoredButton =
            restoredPicker.findChild<QToolButton *>(QStringLiteral("tileAssetSelectButton"));
        QVERIFY(restoredButton);
        directAssetCount = 0;
        for (QAction *action : restoredButton->menu()->actions()) {
            directAssetCount += !action->isSeparator() && action->menu() == nullptr;
        }
        QCOMPARE(directAssetCount, 1);
    }

    void excessiveSpriteSheetIsRejected()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage sheet(65, 65, QImage::Format_ARGB32);
        sheet.fill(Qt::transparent);
        QVERIFY(sheet.save(directory.filePath(QStringLiteral("Tiles-1x1.png"))));

        TileAssetPicker picker;
        QCOMPARE(picker.loadDirectory(directory.path()), 0);
    }

    void cleanupTestCase()
    {
        QSettings().clear();
    }
};

QTEST_MAIN(TileAssetPickerTest)
#include "tileassetpickertest.moc"
