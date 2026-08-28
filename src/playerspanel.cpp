#include "playerspanel.h"

#include "boardwidget.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
class AutoResizingPlainTextEdit final : public QPlainTextEdit
{
public:
    explicit AutoResizingPlainTextEdit(const QString &text, QWidget *parent)
        : QPlainTextEdit(text, parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(
            document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged,
            this,
            [this] {
                updateHeight();
            });
        QTimer::singleShot(0, this, [this] {
            updateHeight();
        });
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        updateHeight();
    }

private:
    void updateHeight()
    {
        const int contentHeight = std::ceil(
            document()->documentLayout()->documentSize().height());
        const QMargins margins = contentsMargins();
        const int desiredHeight = std::clamp(
            contentHeight
                + margins.top()
                + margins.bottom()
                + frameWidth() * 2
                + 8,
            54,
            320);
        if (height() != desiredHeight) {
            setFixedHeight(desiredHeight);
        }
    }
};

QString heartsMarkup(int current, int total)
{
    QString markup = QStringLiteral("<span style=\"font-size:20px\">");
    for (int index = 0; index < total; ++index) {
        markup += index < current
            ? QStringLiteral("<span style=\"color:#e53935\">&#x2665;</span>")
            : QStringLiteral("<span style=\"color:#777777\">&#x2665;</span>");
    }
    return markup + QStringLiteral("</span>");
}

QString pieceLabel(const GamePiece &piece)
{
    if (!piece.name.isEmpty()) {
        return piece.name;
    }
    return PlayersPanel::tr("Unnamed piece (%1)").arg(piece.id.left(8));
}
}

PlayersPanel::PlayersPanel(BoardWidget *board, QWidget *parent)
    : QWidget(parent)
    , m_board(board)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *layout = new QVBoxLayout(this);
    auto *addButton = new QPushButton(tr("Add player"), this);
    addButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    layout->addWidget(addButton);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *contents = new QWidget(scrollArea);
    m_playersLayout = new QVBoxLayout(contents);
    m_playersLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(contents);
    layout->addWidget(scrollArea);

    connect(addButton, &QPushButton::clicked, m_board, [this] {
        const QString playerId = m_board->addPlayer();
        if (!playerId.isEmpty()) {
            m_expandedPlayers.insert(playerId);
        }
    });
    connect(
        m_board,
        &BoardWidget::playersChanged,
        this,
        &PlayersPanel::rebuild,
        Qt::QueuedConnection);
    connect(
        m_board,
        &BoardWidget::piecesChanged,
        this,
        &PlayersPanel::rebuild,
        Qt::QueuedConnection);
    rebuild();
}

void PlayersPanel::rebuild()
{
    while (QLayoutItem *item = m_playersLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QVector<Player> players = m_board->players();
    const QVector<GamePiece> pieces = m_board->gamePieces();
    if (players.isEmpty()) {
        auto *emptyLabel = new QLabel(
            tr("Add a player to track health and equipment."),
            this);
        emptyLabel->setWordWrap(true);
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_playersLayout->addWidget(emptyLabel);
        return;
    }

    for (const Player &player : players) {
        auto *card = new QGroupBox(this);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(6, 6, 6, 6);

        auto *pieceButton = new QToolButton(card);
        pieceButton->setPopupMode(QToolButton::InstantPopup);
        pieceButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        pieceButton->setIconSize(QSize(52, 52));
        pieceButton->setFixedSize(62, 62);
        pieceButton->setToolTip(tr("Select this player's game piece"));
        auto *pieceMenu = new QMenu(pieceButton);
        QAction *noPieceAction = pieceMenu->addAction(
            QIcon::fromTheme(QStringLiteral("edit-clear")),
            tr("No game piece"));
        noPieceAction->setCheckable(true);
        noPieceAction->setChecked(player.pieceId.isEmpty());
        connect(noPieceAction, &QAction::triggered, m_board, [this, player] {
            m_board->setPlayerPiece(player.id, {});
        });

        bool selectedPieceFound = false;
        for (const GamePiece &piece : pieces) {
            if (piece.equipment) {
                continue;
            }
            const bool assignedElsewhere = std::ranges::any_of(
                players,
                [&player, &piece](const Player &candidate) {
                    return candidate.id != player.id
                        && candidate.pieceId == piece.id;
                });
            if (assignedElsewhere && piece.id != player.pieceId) {
                continue;
            }
            const QIcon icon(QPixmap::fromImage(piece.image));
            QAction *pieceAction = pieceMenu->addAction(
                icon,
                pieceLabel(piece));
            pieceAction->setCheckable(true);
            pieceAction->setChecked(piece.id == player.pieceId);
            connect(
                pieceAction,
                &QAction::triggered,
                m_board,
                [this, player, piece] {
                    m_board->setPlayerPiece(player.id, piece.id);
                });
            if (piece.id == player.pieceId) {
                pieceButton->setIcon(icon);
                pieceButton->setToolTip(
                    tr("Game piece: %1").arg(pieceLabel(piece)));
                selectedPieceFound = true;
            }
        }
        if (!selectedPieceFound) {
            pieceButton->setIcon(
                QIcon::fromTheme(QStringLiteral("user-identity")));
        }
        pieceButton->setMenu(pieceMenu);

        auto *nameEdit = new QLineEdit(player.name, card);
        nameEdit->setMaxLength(100);
        nameEdit->setPlaceholderText(tr("Player name"));
        QFont nameFont = nameEdit->font();
        nameFont.setBold(true);
        nameFont.setPointSize(nameFont.pointSize() + 2);
        nameEdit->setFont(nameFont);

        auto *hearts = new QLabel(
            heartsMarkup(player.currentHearts, player.totalHearts),
            card);
        hearts->setWordWrap(true);
        hearts->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto *identityLayout = new QVBoxLayout;
        identityLayout->setContentsMargins(0, 0, 0, 0);
        identityLayout->setSpacing(2);
        identityLayout->addWidget(nameEdit);
        identityLayout->addWidget(hearts);

        auto *expandButton = new QToolButton(card);
        expandButton->setCheckable(true);
        expandButton->setChecked(m_expandedPlayers.contains(player.id));
        expandButton->setArrowType(
            expandButton->isChecked() ? Qt::DownArrow : Qt::RightArrow);
        expandButton->setToolTip(
            expandButton->isChecked()
                ? tr("Collapse player details")
                : tr("Expand player details"));

        auto *playerHeader = new QHBoxLayout;
        playerHeader->addWidget(pieceButton, 0, Qt::AlignTop);
        playerHeader->addLayout(identityLayout, 1);
        playerHeader->addWidget(expandButton, 0, Qt::AlignTop);
        cardLayout->addLayout(playerHeader);

        auto *details = new QWidget(card);
        auto *detailsLayout = new QVBoxLayout(details);
        detailsLayout->setContentsMargins(0, 4, 0, 0);
        auto *form = new QFormLayout;
        auto *heartControls = new QWidget(card);
        auto *heartLayout = new QHBoxLayout(heartControls);
        heartLayout->setContentsMargins(0, 0, 0, 0);
        auto *currentHearts = new QSpinBox(heartControls);
        currentHearts->setRange(0, player.totalHearts);
        currentHearts->setValue(player.currentHearts);
        currentHearts->setToolTip(tr("Current hearts"));
        auto *separator = new QLabel(tr("of"), heartControls);
        auto *totalHearts = new QSpinBox(heartControls);
        totalHearts->setRange(1, 100);
        totalHearts->setValue(player.totalHearts);
        totalHearts->setToolTip(tr("Total hearts"));
        heartLayout->addWidget(currentHearts);
        heartLayout->addWidget(separator);
        heartLayout->addWidget(totalHearts);
        form->addRow(tr("Health:"), heartControls);
        detailsLayout->addLayout(form);

        detailsLayout->addWidget(new QLabel(tr("Equipment inventory"), card));
        auto *inventory = new QListWidget(card);
        inventory->setViewMode(QListView::IconMode);
        inventory->setMovement(QListView::Static);
        inventory->setResizeMode(QListView::Adjust);
        inventory->setIconSize(QSize(48, 48));
        inventory->setGridSize(QSize(76, 76));
        inventory->setMinimumHeight(82);
        inventory->setMaximumHeight(170);
        for (const GamePiece &piece : pieces) {
            if (piece.equipment
                && !player.pieceId.isEmpty()
                && piece.ownerId == player.pieceId) {
                auto *item = new QListWidgetItem(
                    QIcon(QPixmap::fromImage(piece.image)),
                    piece.name.isEmpty() ? tr("Equipment") : piece.name,
                    inventory);
                item->setToolTip(item->text());
                item->setData(Qt::UserRole, piece.id);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(
                    piece.equipped ? Qt::Checked : Qt::Unchecked);
            }
        }
        if (inventory->count() == 0) {
            inventory->addItem(tr("No linked equipment"));
        }
        connect(
            inventory,
            &QListWidget::itemChanged,
            m_board,
            [this](QListWidgetItem *item) {
                const QString pieceId = item->data(Qt::UserRole).toString();
                if (!pieceId.isEmpty()) {
                    m_board->setEquipmentEquipped(
                        pieceId,
                        item->checkState() == Qt::Checked);
                }
            });
        detailsLayout->addWidget(inventory);

        detailsLayout->addWidget(new QLabel(tr("Player notes"), card));
        auto *notesEdit = new AutoResizingPlainTextEdit(player.notes, card);
        notesEdit->setPlaceholderText(
            tr("Add character details, conditions, or other notes..."));
        detailsLayout->addWidget(notesEdit);

        auto *removeButton = new QPushButton(tr("Remove player"), card);
        removeButton->setIcon(
            QIcon::fromTheme(QStringLiteral("list-remove")));
        detailsLayout->addWidget(removeButton);
        details->setVisible(expandButton->isChecked());
        cardLayout->addWidget(details);
        m_playersLayout->addWidget(card);

        connect(
            expandButton,
            &QToolButton::toggled,
            this,
            [this, details, expandButton, player](bool expanded) {
                details->setVisible(expanded);
                expandButton->setArrowType(
                    expanded ? Qt::DownArrow : Qt::RightArrow);
                expandButton->setToolTip(
                    expanded
                        ? tr("Collapse player details")
                        : tr("Expand player details"));
                if (expanded) {
                    m_expandedPlayers.insert(player.id);
                } else {
                    m_expandedPlayers.remove(player.id);
                }
            });
        connect(
            nameEdit,
            &QLineEdit::editingFinished,
            m_board,
            [this, nameEdit, player] {
                const QString name = nameEdit->text().trimmed();
                m_board->setPlayerName(player.id, name);
            });
        connect(
            totalHearts,
            &QSpinBox::valueChanged,
            m_board,
            [this, player, totalHearts, currentHearts, hearts](int total) {
                if (!m_board->setPlayerTotalHearts(player.id, total)) {
                    return;
                }
                const QSignalBlocker blocker(currentHearts);
                currentHearts->setMaximum(total);
                currentHearts->setValue(
                    std::min(currentHearts->value(), total));
                hearts->setText(heartsMarkup(
                    currentHearts->value(),
                    totalHearts->value()));
            });
        connect(
            currentHearts,
            &QSpinBox::valueChanged,
            m_board,
            [this, player, totalHearts, hearts](int current) {
                if (m_board->setPlayerCurrentHearts(player.id, current)) {
                    hearts->setText(
                        heartsMarkup(current, totalHearts->value()));
                }
            });
        connect(
            notesEdit,
            &QPlainTextEdit::textChanged,
            m_board,
            [this, notesEdit, player] {
                QString notes = notesEdit->toPlainText();
                if (notes.size() > BoardWidget::MaxPlayerNotesLength) {
                    notes.truncate(BoardWidget::MaxPlayerNotesLength);
                    const QSignalBlocker blocker(notesEdit);
                    notesEdit->setPlainText(notes);
                    QTextCursor cursor = notesEdit->textCursor();
                    cursor.movePosition(QTextCursor::End);
                    notesEdit->setTextCursor(cursor);
                }
                m_board->setPlayerNotes(player.id, notes);
            });
        connect(
            removeButton,
            &QPushButton::clicked,
            m_board,
            [this, player] {
                m_board->removePlayer(player.id);
            });
    }
}
