#pragma once

#include <QMainWindow>
#include <QStringList>

class BoardWidget;
class QCloseEvent;
class QLabel;
class QMenu;
class PlayersPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    bool maybeSave();
    bool saveSession();
    bool saveSessionAs();
    bool saveSessionTo(const QString &path);
    bool loadSessionFrom(const QString &path);
    void newSession();
    void openSession();
    void addRecentFile(const QString &path);
    void updateRecentMenu();
    void updateWindowTitle();
    void setModified(bool modified);

    BoardWidget *m_board = nullptr;
    PlayersPanel *m_playersPanel = nullptr;
    QLabel *m_tileCountLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QMenu *m_recentMenu = nullptr;
    QString m_currentFile;
    QStringList m_recentFiles;
    bool m_modified = false;
};
