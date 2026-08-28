#pragma once

#include <QSet>
#include <QWidget>

class BoardWidget;
class QVBoxLayout;

class PlayersPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PlayersPanel(BoardWidget *board, QWidget *parent = nullptr);

private:
    void rebuild();

    BoardWidget *m_board = nullptr;
    QVBoxLayout *m_playersLayout = nullptr;
    QSet<QString> m_expandedPlayers;
};
