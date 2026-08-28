#include "boardwidget.h"

#include <QBuffer>
#include <QJsonArray>
#include <QtTest>

#include <algorithm>

namespace {
QString testImage()
{
    QImage image(12, 10, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(QStringLiteral("#4287f5")));

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(bytes.toBase64());
}

QJsonObject validSession()
{
    return {
        {QStringLiteral("backgroundColor"), QStringLiteral("#ff20252b")},
        {QStringLiteral("backgroundImage"), QString()},
        {QStringLiteral("tileBackgrounds"), QJsonArray{
            QJsonObject{
                {QStringLiteral("q"), 2},
                {QStringLiteral("r"), -3},
                {QStringLiteral("image"), testImage()}
            }
        }},
        {QStringLiteral("pieces"), QJsonArray{
            QJsonObject{
                {QStringLiteral("q"), 2},
                {QStringLiteral("r"), -3},
                {QStringLiteral("image"), testImage()},
                {QStringLiteral("scale"), 1.5},
                {QStringLiteral("name"), QStringLiteral("Scout")}
            },
            QJsonObject{
                {QStringLiteral("q"), 2},
                {QStringLiteral("r"), -3},
                {QStringLiteral("image"), testImage()}
            }
        }},
        {QStringLiteral("links"), QJsonArray{
            QJsonObject{
                {QStringLiteral("startQ"), 2},
                {QStringLiteral("startR"), -3},
                {QStringLiteral("endQ"), 5},
                {QStringLiteral("endR"), 1},
                {QStringLiteral("color"), QStringLiteral("#ffe35d6a")},
                {QStringLiteral("width"), 4.0},
                {QStringLiteral("arrows"), static_cast<int>(ArrowStyle::Both)}
            }
        }},
        {QStringLiteral("panX"), 125.5},
        {QStringLiteral("panY"), -48.25},
        {QStringLiteral("zoom"), 1.75}
    };
}
}

class SessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripPreservesBoard()
    {
        BoardWidget board;
        QString error;
        QVERIFY2(board.loadSession(validSession(), &error), qPrintable(error));

        QJsonObject saved;
        QVERIFY2(board.saveSessionData(&saved, &error), qPrintable(error));
        QCOMPARE(saved.value(QStringLiteral("backgroundColor")).toString(), QStringLiteral("#ff20252b"));
        QCOMPARE(saved.value(QStringLiteral("tileBackgrounds")).toArray().size(), 1);
        QCOMPARE(saved.value(QStringLiteral("pieces")).toArray().size(), 2);
        QCOMPARE(saved.value(QStringLiteral("links")).toArray().size(), 1);
        QCOMPARE(saved.value(QStringLiteral("panX")).toDouble(), 125.5);
        QCOMPARE(saved.value(QStringLiteral("panY")).toDouble(), -48.25);
        QCOMPARE(saved.value(QStringLiteral("zoom")).toDouble(), 1.75);

        const QJsonObject tile = saved.value(QStringLiteral("pieces")).toArray().first().toObject();
        QCOMPARE(tile.value(QStringLiteral("scale")).toDouble(), 1.5);
        QCOMPARE(tile.value(QStringLiteral("name")).toString(), QStringLiteral("Scout"));
        QVERIFY(!QImage::fromData(QByteArray::fromBase64(
            tile.value(QStringLiteral("image")).toString().toLatin1())).isNull());
    }

    void invalidSessionDoesNotReplaceBoard()
    {
        BoardWidget board;
        QString error;
        QVERIFY2(board.loadSession(validSession(), &error), qPrintable(error));

        QJsonObject before;
        QVERIFY(board.saveSessionData(&before, &error));

        QJsonObject invalid = validSession();
        QJsonArray pieces = invalid.value(QStringLiteral("pieces")).toArray();
        QJsonObject piece = pieces.first().toObject();
        piece.insert(QStringLiteral("scale"), 10.25);
        pieces.replace(0, piece);
        invalid.insert(QStringLiteral("pieces"), pieces);
        QVERIFY(!board.loadSession(invalid, &error));

        QJsonObject after;
        QVERIFY(board.saveSessionData(&after, &error));
        QCOMPARE(after, before);
    }

    void extremeViewportIsRejected()
    {
        QJsonObject session = validSession();
        session.insert(QStringLiteral("panX"), 1.0e100);

        BoardWidget board;
        QString error;
        QVERIFY(!board.loadSession(session, &error));
        QVERIFY(error.contains(QStringLiteral("invalid viewport")));
    }

    void legacyTilesLoadAsPieces()
    {
        QJsonObject legacy = validSession();
        legacy.insert(QStringLiteral("tiles"), legacy.take(QStringLiteral("pieces")));
        legacy.remove(QStringLiteral("tileBackgrounds"));
        QJsonArray legacyPieces = legacy.value(QStringLiteral("tiles")).toArray();
        for (int index = 0; index < legacyPieces.size(); ++index) {
            QJsonObject piece = legacyPieces.at(index).toObject();
            piece.remove(QStringLiteral("scale"));
            legacyPieces.replace(index, piece);
        }
        legacy.insert(QStringLiteral("tiles"), legacyPieces);

        BoardWidget board;
        QString error;
        QVERIFY2(board.loadSession(legacy, &error), qPrintable(error));

        QJsonObject saved;
        QVERIFY(board.saveSessionData(&saved, &error));
        QCOMPARE(saved.value(QStringLiteral("pieces")).toArray().size(), 2);
        QCOMPARE(
            saved.value(QStringLiteral("pieces")).toArray().first()
                .toObject().value(QStringLiteral("scale")).toDouble(),
            1.0);
        QCOMPARE(saved.value(QStringLiteral("tileBackgrounds")).toArray().size(), 0);
        QVERIFY(!saved.contains(QStringLiteral("tiles")));
    }

    void overflowPieceCanDragFromOutsideHex()
    {
        QJsonObject session = validSession();
        QJsonArray pieces = session.value(QStringLiteral("pieces")).toArray();
        QJsonObject piece = pieces.first().toObject();
        piece.insert(QStringLiteral("q"), 0);
        piece.insert(QStringLiteral("r"), 0);
        piece.insert(QStringLiteral("scale"), 2.0);
        pieces = {piece};
        session.insert(QStringLiteral("pieces"), pieces);
        session.insert(QStringLiteral("panX"), 0.0);
        session.insert(QStringLiteral("panY"), 0.0);
        session.insert(QStringLiteral("zoom"), 1.0);

        BoardWidget board;
        board.resize(640, 512);
        board.show();
        QString error;
        QVERIFY2(board.loadSession(session, &error), qPrintable(error));

        QTest::mousePress(&board, Qt::LeftButton, Qt::NoModifier, QPoint(235, 256));
        QTest::mouseMove(&board, QPoint(416, 320));
        QTest::mouseRelease(&board, Qt::LeftButton, Qt::NoModifier, QPoint(416, 320));

        QJsonObject saved;
        QVERIFY(board.saveSessionData(&saved, &error));
        const QJsonObject movedPiece =
            saved.value(QStringLiteral("pieces")).toArray().first().toObject();
        QCOMPARE(movedPiece.value(QStringLiteral("q")).toInt(), 1);
        QCOMPARE(movedPiece.value(QStringLiteral("r")).toInt(), 0);
    }

    void linkedEquipmentPersistsAndMovesWithOwner()
    {
        QJsonObject session = validSession();
        QJsonArray pieces{
            QJsonObject{
                {QStringLiteral("q"), 0},
                {QStringLiteral("r"), 0},
                {QStringLiteral("id"), QStringLiteral("owner")},
                {QStringLiteral("image"), testImage()},
                {QStringLiteral("scale"), 1.5},
                {QStringLiteral("equipment"), false},
                {QStringLiteral("ownerId"), QString()}
            },
            QJsonObject{
                {QStringLiteral("q"), 0},
                {QStringLiteral("r"), 0},
                {QStringLiteral("id"), QStringLiteral("equipment")},
                {QStringLiteral("image"), testImage()},
                {QStringLiteral("scale"), 1.0},
                {QStringLiteral("equipment"), true},
                {QStringLiteral("ownerId"), QStringLiteral("owner")}
            }
        };
        session.insert(QStringLiteral("pieces"), pieces);
        session.insert(QStringLiteral("panX"), 0.0);
        session.insert(QStringLiteral("panY"), 0.0);
        session.insert(QStringLiteral("zoom"), 1.0);

        BoardWidget board;
        board.resize(640, 512);
        board.show();
        QString error;
        QVERIFY2(board.loadSession(session, &error), qPrintable(error));

        QTest::mousePress(&board, Qt::LeftButton, Qt::NoModifier, QPoint(240, 256));
        QTest::mouseMove(&board, QPoint(416, 320));
        QTest::mouseRelease(&board, Qt::LeftButton, Qt::NoModifier, QPoint(416, 320));

        QJsonObject saved;
        QVERIFY(board.saveSessionData(&saved, &error));
        const QJsonArray savedPieces = saved.value(QStringLiteral("pieces")).toArray();
        QCOMPARE(savedPieces.size(), 2);
        for (const QJsonValue &value : savedPieces) {
            const QJsonObject savedPiece = value.toObject();
            QCOMPARE(savedPiece.value(QStringLiteral("q")).toInt(), 1);
            QCOMPARE(savedPiece.value(QStringLiteral("r")).toInt(), 0);
            if (savedPiece.value(QStringLiteral("id")).toString() == QStringLiteral("equipment")) {
                QVERIFY(savedPiece.value(QStringLiteral("equipment")).toBool());
                QCOMPARE(
                    savedPiece.value(QStringLiteral("ownerId")).toString(),
                    QStringLiteral("owner"));
                QVERIFY(savedPiece.value(QStringLiteral("equipped")).toBool());
            }
        }
    }

    void invalidEquipmentOwnerIsRejected()
    {
        QJsonObject session = validSession();
        QJsonArray pieces = session.value(QStringLiteral("pieces")).toArray();
        QJsonObject equipment = pieces.first().toObject();
        equipment.insert(QStringLiteral("id"), QStringLiteral("equipment"));
        equipment.insert(QStringLiteral("equipment"), true);
        equipment.insert(QStringLiteral("ownerId"), QStringLiteral("missing"));
        pieces = {equipment};
        session.insert(QStringLiteral("pieces"), pieces);

        BoardWidget board;
        QString error;
        QVERIFY(!board.loadSession(session, &error));
        QVERIFY(error.contains(QStringLiteral("missing owner")));
    }

    void playersAndHeartsPersist()
    {
        QJsonObject session = validSession();
        QJsonArray pieces = session.value(QStringLiteral("pieces")).toArray();
        QJsonObject owner = pieces.first().toObject();
        owner.insert(QStringLiteral("id"), QStringLiteral("hero"));
        owner.insert(QStringLiteral("name"), QStringLiteral("Aria"));
        pieces = {owner};
        session.insert(QStringLiteral("pieces"), pieces);
        session.insert(QStringLiteral("players"), QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("player-one")},
                {QStringLiteral("name"), QStringLiteral("Craig")},
                {QStringLiteral("pieceId"), QStringLiteral("hero")},
                {QStringLiteral("totalHearts"), 8},
                {QStringLiteral("currentHearts"), 5},
                {QStringLiteral("notes"), QStringLiteral("Carries the moon key.")}
            }
        });

        BoardWidget board;
        QString error;
        QVERIFY2(board.loadSession(session, &error), qPrintable(error));
        QCOMPARE(board.players().size(), 1);
        QCOMPARE(board.players().first().name, QStringLiteral("Craig"));
        QCOMPARE(board.players().first().totalHearts, 8);
        QCOMPARE(board.players().first().currentHearts, 5);
        QCOMPARE(
            board.players().first().notes,
            QStringLiteral("Carries the moon key."));

        QJsonObject saved;
        QVERIFY(board.saveSessionData(&saved, &error));
        const QJsonObject savedPlayer =
            saved.value(QStringLiteral("players")).toArray().first().toObject();
        QCOMPARE(
            savedPlayer.value(QStringLiteral("pieceId")).toString(),
            QStringLiteral("hero"));
        QCOMPARE(savedPlayer.value(QStringLiteral("currentHearts")).toInt(), 5);
        QCOMPARE(
            savedPlayer.value(QStringLiteral("notes")).toString(),
            QStringLiteral("Carries the moon key."));
    }

    void playerCannotOwnMissingPiece()
    {
        QJsonObject session = validSession();
        session.insert(QStringLiteral("players"), QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("player-one")},
                {QStringLiteral("name"), QStringLiteral("Craig")},
                {QStringLiteral("pieceId"), QStringLiteral("missing")},
                {QStringLiteral("totalHearts"), 5},
                {QStringLiteral("currentHearts"), 5}
            }
        });

        BoardWidget board;
        QString error;
        QVERIFY(!board.loadSession(session, &error));
        QVERIFY(error.contains(QStringLiteral("missing game piece")));
    }

    void unassigningPiecePreservesPlayer()
    {
        QJsonObject session = validSession();
        QJsonArray pieces = session.value(QStringLiteral("pieces")).toArray();
        QJsonObject owner = pieces.first().toObject();
        owner.insert(QStringLiteral("id"), QStringLiteral("hero"));
        pieces = {owner};
        session.insert(QStringLiteral("pieces"), pieces);
        session.insert(QStringLiteral("players"), QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("player-one")},
                {QStringLiteral("name"), QStringLiteral("Craig")},
                {QStringLiteral("pieceId"), QStringLiteral("hero")},
                {QStringLiteral("totalHearts"), 8},
                {QStringLiteral("currentHearts"), 3}
            }
        });

        BoardWidget board;
        QString error;
        QVERIFY2(board.loadSession(session, &error), qPrintable(error));
        QVERIFY(board.setPlayerPiece(QStringLiteral("player-one"), {}));
        QCOMPARE(board.players().size(), 1);
        QCOMPARE(board.players().first().pieceId, QString());
        QCOMPARE(board.players().first().totalHearts, 8);
        QCOMPARE(board.players().first().currentHearts, 3);
    }

    void equipmentSelectionPersists()
    {
        QJsonObject session = validSession();
        session.insert(QStringLiteral("pieces"), QJsonArray{
            QJsonObject{
                {QStringLiteral("q"), 0},
                {QStringLiteral("r"), 0},
                {QStringLiteral("id"), QStringLiteral("owner")},
                {QStringLiteral("image"), testImage()},
                {QStringLiteral("equipment"), false},
                {QStringLiteral("ownerId"), QString()}
            },
            QJsonObject{
                {QStringLiteral("q"), 0},
                {QStringLiteral("r"), 0},
                {QStringLiteral("id"), QStringLiteral("shield")},
                {QStringLiteral("image"), testImage()},
                {QStringLiteral("equipment"), true},
                {QStringLiteral("ownerId"), QStringLiteral("owner")},
                {QStringLiteral("equipped"), false}
            }
        });

        BoardWidget board;
        QString error;
        QVERIFY2(board.loadSession(session, &error), qPrintable(error));
        QVERIFY(board.setEquipmentEquipped(QStringLiteral("shield"), true));

        QJsonObject saved;
        QVERIFY(board.saveSessionData(&saved, &error));
        const QJsonArray pieces = saved.value(QStringLiteral("pieces")).toArray();
        const auto shield = std::ranges::find_if(
            pieces,
            [](const QJsonValue &value) {
                return value.toObject().value(QStringLiteral("id")).toString()
                    == QStringLiteral("shield");
            });
        QVERIFY(shield != pieces.cend());
        QVERIFY(shield->toObject().value(QStringLiteral("equipped")).toBool());
    }
};

QTEST_MAIN(SessionTest)
#include "sessiontest.moc"
