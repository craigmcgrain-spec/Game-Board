#pragma once

#include <QList>
#include <QMainWindow>
#include <QStringList>

#include <functional>

class BoardWidget;
class QAction;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QMenu;
class QStackedWidget;
class QToolBar;
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
    void setToolsPanelCompact(bool compact);
    void toggleToolPanel(
        const QString &objectName,
        QAction *action,
        const std::function<QWidget *()> &factory);
    void closeActiveTool();

    QDockWidget *m_toolsDock = nullptr;
    QStackedWidget *m_toolStack = nullptr;
    QToolBar *m_toolLauncher = nullptr;
    QList<QAction *> m_toolActions;
    QWidget *m_activeTool = nullptr;
    BoardWidget *m_board = nullptr;
    PlayersPanel *m_playersPanel = nullptr;
    QLabel *m_tileCountLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QMenu *m_recentMenu = nullptr;
    QString m_currentFile;
    QStringList m_recentFiles;
    int m_expandedToolsWidth = 360;
    bool m_toolsPanelCompact = false;
    bool m_modified = false;
};
