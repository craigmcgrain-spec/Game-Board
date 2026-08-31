#include "mainwindow.h"

#include "boardwidget.h"
#include "chancewheelwidget.h"
#include "dicerollerwidget.h"
#include "geargeneratorwidget.h"
#include "namegeneratorwidget.h"
#include "playerspanel.h"
#include "tileassetpicker.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSpinBox>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QWindow>

#include <cmath>
#include <numbers>

namespace {
constexpr int SessionVersion = 6;
constexpr qint64 MaxSessionBytes = 512LL * 1024 * 1024;
constexpr int MaxRecentFiles = 8;

enum class ToolIcon
{
    D20,
    Wheel,
    Gear,
    NameTag
};

QIcon toolIcon(ToolIcon type, const QColor &color)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QColor fill = color;
    fill.setAlpha(45);
    painter.setBrush(fill);

    if (type == ToolIcon::D20) {
        const QPolygonF outline{
            {16.0, 3.0}, {28.0, 10.0}, {28.0, 23.0},
            {16.0, 29.0}, {4.0, 23.0}, {4.0, 10.0}
        };
        painter.drawPolygon(outline);
        for (const QPointF &point : outline) {
            painter.drawLine(QPointF(16.0, 16.0), point);
        }
        painter.drawPolygon(QPolygonF{{16.0, 7.0}, {23.0, 20.0}, {9.0, 20.0}});
    } else if (type == ToolIcon::Wheel) {
        painter.drawEllipse(QRectF(4.0, 4.0, 24.0, 24.0));
        painter.drawLine(QPointF(16.0, 16.0), QPointF(16.0, 4.0));
        painter.drawLine(QPointF(16.0, 16.0), QPointF(27.0, 20.0));
        painter.drawLine(QPointF(16.0, 16.0), QPointF(7.0, 24.0));
        painter.setBrush(color);
        painter.drawEllipse(QPointF(16.0, 16.0), 2.5, 2.5);
        painter.drawPolygon(QPolygonF{{16.0, 1.0}, {12.5, 6.0}, {19.5, 6.0}});
    } else if (type == ToolIcon::Gear) {
        QPainterPath gear;
        gear.setFillRule(Qt::OddEvenFill);
        for (int index = 0; index < 24; ++index) {
            const double radius = index % 3 == 0 ? 14.0 : (index % 3 == 1 ? 11.0 : 12.0);
            const double angle = -std::numbers::pi / 2.0
                + index * 2.0 * std::numbers::pi / 24.0;
            const QPointF point(
                16.0 + std::cos(angle) * radius,
                16.0 + std::sin(angle) * radius);
            if (index == 0) {
                gear.moveTo(point);
            } else {
                gear.lineTo(point);
            }
        }
        gear.closeSubpath();
        gear.addEllipse(QRectF(12.0, 12.0, 8.0, 8.0));
        painter.drawPath(gear);
    } else {
        QPainterPath tag;
        tag.moveTo(3.0, 8.0);
        tag.lineTo(17.0, 3.0);
        tag.lineTo(29.0, 15.0);
        tag.lineTo(16.0, 28.0);
        tag.lineTo(3.0, 16.0);
        tag.closeSubpath();
        painter.drawPath(tag);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(10.0, 11.0), 2.5, 2.5);
    }
    return QIcon(pixmap);
}

class FloatingDockTitleBar final : public QWidget
{
public:
    explicit FloatingDockTitleBar(QDockWidget *dock)
        : QWidget(dock)
        , m_dock(dock)
    {
        setObjectName(QStringLiteral("toolsFloatingTitleBar"));
        setAttribute(Qt::WA_StyledBackground, true);
        setAutoFillBackground(true);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 3, 3, 3);
        layout->setSpacing(4);
        auto *title = new QLabel(dock->windowTitle(), this);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(title, 1);

        auto *dockButton = new QToolButton(this);
        dockButton->setObjectName(QStringLiteral("dockToolsPanelButton"));
        dockButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
        dockButton->setToolTip(tr("Dock Tabletop Tools"));
        layout->addWidget(dockButton);

        auto *closeButton = new QToolButton(this);
        closeButton->setObjectName(QStringLiteral("closeToolsPanelButton"));
        closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        closeButton->setToolTip(tr("Hide Tabletop Tools"));
        layout->addWidget(closeButton);

        connect(dockButton, &QToolButton::clicked, dock, [dock] {
            dock->setFloating(false);
        });
        connect(closeButton, &QToolButton::clicked, dock, &QWidget::hide);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_dock->isFloating()) {
            if (QWindow *window = m_dock->windowHandle()) {
                window->startSystemMove();
            }
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_dock->isFloating()) {
            m_dock->setFloating(false);
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    QDockWidget *m_dock = nullptr;
};

QIcon colorIcon(const QColor &color)
{
    QPixmap pixmap(20, 20);
    pixmap.fill(color);
    return QIcon(pixmap);
}

QString sessionFilter()
{
    return MainWindow::tr("Hexboard sessions (*.hexboard);;All files (*)");
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_toolsDock(new QDockWidget(tr("Tabletop Tools"), this))
    , m_toolStack(new QStackedWidget(m_toolsDock))
    , m_board(new BoardWidget(this))
    , m_tileCountLabel(new QLabel(tr("0 pieces"), this))
    , m_zoomLabel(new QLabel(tr("100%"), this))
{
    resize(1200, 800);
    setCentralWidget(m_board);

    m_toolsDock->setObjectName(QStringLiteral("toolsDock"));
    m_toolsDock->setAttribute(Qt::WA_StyledBackground, true);
    m_toolsDock->setAutoFillBackground(true);
    m_toolsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_toolsDock->setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    m_toolsDock->setMinimumWidth(300);
    auto *toolsPanel = new QWidget(m_toolsDock);
    toolsPanel->setObjectName(QStringLiteral("toolsPanel"));
    toolsPanel->setAttribute(Qt::WA_StyledBackground, true);
    toolsPanel->setAutoFillBackground(true);
    auto *toolsPanelLayout = new QHBoxLayout(toolsPanel);
    toolsPanelLayout->setContentsMargins(0, 0, 0, 0);
    toolsPanelLayout->setSpacing(0);
    m_toolLauncher = new QToolBar(tr("Tabletop Tools"), toolsPanel);
    m_toolLauncher->setObjectName(QStringLiteral("toolPanelLauncher"));
    m_toolLauncher->setMovable(false);
    m_toolLauncher->setIconSize(QSize(26, 26));
    m_toolLauncher->setOrientation(Qt::Vertical);
    m_toolLauncher->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolLauncher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    toolsPanelLayout->addWidget(m_toolLauncher);
    m_toolStack->setObjectName(QStringLiteral("toolContentStack"));
    m_toolStack->setAttribute(Qt::WA_StyledBackground, true);
    m_toolStack->setAutoFillBackground(true);
    toolsPanelLayout->addWidget(m_toolStack, 1);
    m_toolsDock->setWidget(toolsPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolsDock);
    auto *floatingToolsTitle = new FloatingDockTitleBar(m_toolsDock);
    connect(m_toolsDock, &QDockWidget::topLevelChanged, this, [this, floatingToolsTitle](bool floating) {
        m_toolsDock->setTitleBarWidget(floating ? floatingToolsTitle : nullptr);
    });

    auto *playersDock = new QDockWidget(tr("Players"), this);
    playersDock->setObjectName(QStringLiteral("playersDock"));
    playersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    playersDock->setMinimumWidth(200);
    m_playersPanel = new PlayersPanel(m_board, playersDock);
    playersDock->setWidget(m_playersPanel);
    addDockWidget(Qt::RightDockWidgetArea, playersDock);
    resizeDocks({playersDock}, {320}, Qt::Horizontal);
    m_recentFiles = QSettings().value(QStringLiteral("files/recent")).toStringList();
    updateWindowTitle();

    auto *interactionTools = new QActionGroup(this);
    interactionTools->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

    auto *toolbar = addToolBar(tr("Board"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction *backgroundColor = toolbar->addAction(
        QIcon::fromTheme(QStringLiteral("color-picker")),
        tr("Background color"));
    QAction *backgroundImage = toolbar->addAction(
        QIcon::fromTheme(QStringLiteral("insert-image")),
        tr("Background image"));
    toolbar->addSeparator();
    QAction *showHexGrid = toolbar->addAction(
        QIcon::fromTheme(QStringLiteral("view-grid")),
        tr("Show hex grid"));
    showHexGrid->setCheckable(true);
    showHexGrid->setShortcut(QKeySequence(Qt::Key_G));
    toolbar->addSeparator();
    QAction *navigate = toolbar->addAction(
        QIcon::fromTheme(QStringLiteral("transform-move")),
        tr("Navigate"));
    navigate->setCheckable(true);
    navigate->setShortcut(QKeySequence(Qt::Key_N));
    interactionTools->addAction(navigate);
    toolbar->addSeparator();
    QAction *clearBoard = toolbar->addAction(
        QIcon::fromTheme(QStringLiteral("edit-clear-all")),
        tr("Clear board"));

    auto *tileToolbar = addToolBar(tr("Tile backgrounds"));
    tileToolbar->setMovable(false);
    tileToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QAction *paintTiles = tileToolbar->addAction(
        QIcon::fromTheme(QStringLiteral("draw-brush")),
        tr("Paint tiles"));
    paintTiles->setCheckable(true);
    paintTiles->setShortcut(QKeySequence(Qt::Key_P));
    interactionTools->addAction(paintTiles);
    QAction *eraseTiles = tileToolbar->addAction(
        QIcon::fromTheme(QStringLiteral("draw-eraser")),
        tr("Erase tiles"));
    eraseTiles->setCheckable(true);
    eraseTiles->setShortcut(QKeySequence(Qt::Key_E));
    interactionTools->addAction(eraseTiles);

    auto *tilePicker = new TileAssetPicker(tileToolbar);
    tileToolbar->addWidget(tilePicker);
    QAction *tileFolder = tileToolbar->addAction(
        QIcon::fromTheme(QStringLiteral("folder-pictures")),
        tr("Tile folder"));

    auto *linkToolbar = addToolBar(tr("Links"));
    linkToolbar->setMovable(false);
    linkToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction *linkMode = linkToolbar->addAction(
        QIcon::fromTheme(QStringLiteral("draw-line")),
        tr("Link hexes"));
    linkMode->setCheckable(true);
    linkMode->setShortcut(QKeySequence(Qt::Key_L));
    interactionTools->addAction(linkMode);

    linkToolbar->addSeparator();
    linkToolbar->addWidget(new QLabel(tr("Thickness:"), linkToolbar));
    auto *lineWidth = new QSpinBox(linkToolbar);
    lineWidth->setRange(1, 12);
    lineWidth->setValue(6);
    lineWidth->setSuffix(tr(" px"));
    linkToolbar->addWidget(lineWidth);

    QColor linkColor(Qt::black);
    QAction *lineColor = linkToolbar->addAction(colorIcon(linkColor), tr("Line color"));

    linkToolbar->addWidget(new QLabel(tr("Arrows:"), linkToolbar));
    auto *arrowStyle = new QComboBox(linkToolbar);
    arrowStyle->addItem(tr("None"), static_cast<int>(ArrowStyle::None));
    arrowStyle->addItem(tr("End"), static_cast<int>(ArrowStyle::End));
    arrowStyle->addItem(tr("Both ends"), static_cast<int>(ArrowStyle::Both));
    arrowStyle->setCurrentIndex(1);
    linkToolbar->addWidget(arrowStyle);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *newAction = fileMenu->addAction(tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    QAction *openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    fileMenu->addSeparator();
    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    QAction *saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    updateRecentMenu();

    QMenu *boardMenu = menuBar()->addMenu(tr("&Board"));
    boardMenu->addAction(backgroundColor);
    boardMenu->addAction(backgroundImage);
    QAction *removeBackgroundImage = boardMenu->addAction(tr("Use solid color"));
    boardMenu->addSeparator();
    boardMenu->addAction(clearBoard);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *diceRoller = toolsMenu->addAction(tr("Dice Roller"));
    diceRoller->setObjectName(QStringLiteral("openDiceRollerAction"));
    diceRoller->setCheckable(true);
    diceRoller->setIcon(toolIcon(ToolIcon::D20, palette().color(QPalette::ButtonText)));
    QAction *chanceWheel = toolsMenu->addAction(tr("Chance Wheel"));
    chanceWheel->setObjectName(QStringLiteral("openChanceWheelAction"));
    chanceWheel->setCheckable(true);
    chanceWheel->setIcon(toolIcon(ToolIcon::Wheel, palette().color(QPalette::ButtonText)));
    QAction *gearGenerator = toolsMenu->addAction(tr("Gear Generator"));
    gearGenerator->setObjectName(QStringLiteral("openGearGeneratorAction"));
    gearGenerator->setCheckable(true);
    gearGenerator->setIcon(toolIcon(ToolIcon::Gear, palette().color(QPalette::ButtonText)));
    QAction *nameGenerator = toolsMenu->addAction(tr("Name Generator"));
    nameGenerator->setObjectName(QStringLiteral("openNameGeneratorAction"));
    nameGenerator->setCheckable(true);
    nameGenerator->setIcon(toolIcon(ToolIcon::NameTag, palette().color(QPalette::ButtonText)));
    m_toolActions = {diceRoller, chanceWheel, gearGenerator, nameGenerator};
    m_toolLauncher->addActions({diceRoller, chanceWheel, gearGenerator, nameGenerator});
    setToolsPanelCompact(true);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *toggleToolsPanel = m_toolsDock->toggleViewAction();
    toggleToolsPanel->setObjectName(QStringLiteral("toggleToolsPanelAction"));
    toggleToolsPanel->setText(tr("Tabletop Tools Panel"));
    toggleToolsPanel->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    toggleToolsPanel->setStatusTip(tr("Show or hide the Tabletop Tools panel"));
    viewMenu->addAction(toggleToolsPanel);
    viewMenu->addAction(playersDock->toggleViewAction());

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *instructions = helpMenu->addAction(tr("How to use Hexboard"));
    QAction *about = helpMenu->addAction(tr("About Hexboard"));

    statusBar()->showMessage(
        tr("Drop game-piece images onto hexes. Drag empty space to pan and scroll to zoom."));
    statusBar()->addPermanentWidget(m_tileCountLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);

    connect(backgroundColor, &QAction::triggered, m_board, &BoardWidget::chooseBackgroundColor);
    connect(backgroundImage, &QAction::triggered, m_board, &BoardWidget::chooseBackgroundImage);
    connect(removeBackgroundImage, &QAction::triggered, m_board, &BoardWidget::clearBackgroundImage);
    connect(navigate, &QAction::toggled, m_board, &BoardWidget::setNavigationMode);
    connect(showHexGrid, &QAction::toggled, this, [this](bool visible) {
        m_board->setHexGridVisible(visible);
        QSettings().setValue(QStringLiteral("view/hexGridVisible"), visible);
    });
    connect(clearBoard, &QAction::triggered, this, [this] {
        if (QMessageBox::question(
                this,
                tr("Clear board"),
                tr("Remove every tile background, game piece, and link from the board?"))
                == QMessageBox::Yes) {
            m_board->clearBoard();
        }
    });
    connect(diceRoller, &QAction::triggered, this, [this] {
        toggleToolPanel(QStringLiteral("diceRollerWidget"), m_toolActions.at(0), [this] {
            return new DiceRollerWidget(m_toolStack);
        });
    });
    connect(chanceWheel, &QAction::triggered, this, [this] {
        toggleToolPanel(QStringLiteral("chanceWheelWidget"), m_toolActions.at(1), [this] {
            return new ChanceWheelWidget(m_toolStack);
        });
    });
    connect(gearGenerator, &QAction::triggered, this, [this] {
        toggleToolPanel(QStringLiteral("gearGeneratorWidget"), m_toolActions.at(2), [this] {
            return new GearGeneratorWidget(m_toolStack);
        });
    });
    connect(nameGenerator, &QAction::triggered, this, [this] {
        toggleToolPanel(QStringLiteral("nameGeneratorWidget"), m_toolActions.at(3), [this] {
            return new NameGeneratorWidget(m_toolStack);
        });
    });
    connect(paintTiles, &QAction::toggled, m_board, &BoardWidget::setTilePaintMode);
    connect(eraseTiles, &QAction::toggled, m_board, &BoardWidget::setTileEraseMode);
    connect(
        tilePicker,
        &TileAssetPicker::assetSelected,
        this,
        [this, paintTiles](const QImage &image, bool activatePaintTool) {
        m_board->setTileBrushImage(image);
        if (activatePaintTool) {
            paintTiles->setChecked(true);
        }
        });
    connect(tilePicker, &TileAssetPicker::collectionLoaded, this, [this, paintTiles](int count) {
        paintTiles->setEnabled(count > 0);
        if (count == 0) {
            paintTiles->setChecked(false);
            m_board->setTileBrushImage({});
        }
        statusBar()->showMessage(tr("Loaded %n individual tile(s).", nullptr, count), 5000);
    });
    connect(tileFolder, &QAction::triggered, this, [this, tilePicker] {
        const QString current = QSettings().value(
            QStringLiteral("tiles/collectionDirectory"),
            QStringLiteral("/home/lyco/Documents/Hexboard Tiles")).toString();
        const QString directory = QFileDialog::getExistingDirectory(
            this,
            tr("Choose tile image collection"),
            current);
        if (directory.isEmpty()) {
            return;
        }
        QSettings().setValue(QStringLiteral("tiles/collectionDirectory"), directory);
        tilePicker->loadDirectory(directory);
    });
    connect(newAction, &QAction::triggered, this, &MainWindow::newSession);
    connect(openAction, &QAction::triggered, this, &MainWindow::openSession);
    connect(saveAction, &QAction::triggered, this, [this] {
        saveSession();
    });
    connect(saveAsAction, &QAction::triggered, this, [this] {
        saveSessionAs();
    });
    connect(linkMode, &QAction::toggled, m_board, &BoardWidget::setLinkMode);
    connect(lineWidth, &QSpinBox::valueChanged, m_board, &BoardWidget::setLinkWidth);
    connect(lineColor, &QAction::triggered, this, [this, lineColor, linkColor]() mutable {
        const QColor selected = QColorDialog::getColor(linkColor, this, tr("Choose line color"));
        if (!selected.isValid()) {
            return;
        }
        linkColor = selected;
        lineColor->setIcon(colorIcon(selected));
        m_board->setLinkColor(selected);
    });
    connect(arrowStyle, &QComboBox::currentIndexChanged, this, [this, arrowStyle] {
        m_board->setArrowStyle(static_cast<ArrowStyle>(arrowStyle->currentData().toInt()));
    });
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    connect(instructions, &QAction::triggered, this, [this] {
        QMessageBox::information(
            this,
            tr("How to use Hexboard"),
            tr("<b>Add:</b> Drag game-piece images from Dolphin onto the board.<br>"
               "<b>Move:</b> Drag an individual piece from one hex to another.<br>"
               "<b>Resize:</b> Right-click a piece and use the Game piece size menu.<br>"
               "<b>Name:</b> Right-click a piece and choose Rename game piece.<br>"
               "<b>Player:</b> Right-click a piece and enable Player to add it to the Players panel.<br>"
               "<b>Equipment:</b> Mark a piece as Equipment, choose Link to game piece, "
               "then click its owner.<br>"
               "<b>Players:</b> Add a player in the right panel, assign a game piece, "
               "track hearts, equipment, and free-text notes.<br>"
               "<b>Tiles:</b> Choose a tile image, enable Paint tiles, then click or drag.<br>"
               "<b>Link:</b> Enable Link hexes, then click the start and end hex.<br>"
               "<b>Navigate:</b> Enable Navigate to drag from anywhere without moving pieces.<br>"
               "<b>Sessions:</b> Use the File menu to save or open a complete board.<br>"
               "<b>Navigate:</b> Drag empty space to pan and use the wheel to zoom.<br>"
               "<b>Remove:</b> Right-click a piece or hex to remove content."));
    });
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this,
            tr("About Hexboard"),
            tr("<b>Hexboard 1.0</b><br>An infinite hexagonal canvas for tabletop games."));
    });
    connect(m_board, &BoardWidget::pieceCountChanged, this, [this](int count) {
        m_tileCountLabel->setText(tr("%n piece(s)", nullptr, count));
    });
    connect(m_board, &BoardWidget::zoomChanged, this, [this](int percent) {
        m_zoomLabel->setText(tr("%1%").arg(percent));
    });
    connect(m_board, &BoardWidget::interactionHintChanged, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });
    connect(m_board, &BoardWidget::boardChanged, this, [this] {
        setModified(true);
    });
    showHexGrid->setChecked(
        QSettings().value(QStringLiteral("view/hexGridVisible"), false).toBool());
    tilePicker->loadDirectory(QSettings().value(
        QStringLiteral("tiles/collectionDirectory"),
        QStringLiteral("/home/lyco/Documents/Hexboard Tiles")).toString());
}

void MainWindow::toggleToolPanel(
    const QString &objectName,
    QAction *action,
    const std::function<QWidget *()> &factory)
{
    const bool sameTool =
        m_activeTool && m_activeTool->objectName() == objectName;
    if (sameTool && !m_toolsDock->isHidden() && !m_toolStack->isHidden()) {
        closeActiveTool();
        setToolsPanelCompact(true);
        return;
    }

    if (!sameTool) {
        closeActiveTool();
        m_activeTool = factory();
        m_toolStack->addWidget(m_activeTool);
        m_toolStack->setCurrentWidget(m_activeTool);
    }
    for (QAction *toolAction : std::as_const(m_toolActions)) {
        const QSignalBlocker blocker(toolAction);
        toolAction->setChecked(toolAction == action);
    }
    setToolsPanelCompact(false);
    m_toolsDock->show();
    m_toolsDock->raise();
}

void MainWindow::closeActiveTool()
{
    if (m_activeTool) {
        m_toolStack->removeWidget(m_activeTool);
        m_activeTool->deleteLater();
        m_activeTool = nullptr;
    }
    for (QAction *action : std::as_const(m_toolActions)) {
        const QSignalBlocker blocker(action);
        action->setChecked(false);
    }
}

void MainWindow::setToolsPanelCompact(bool compact)
{
    m_toolStack->setVisible(!compact);
    m_toolLauncher->setOrientation(Qt::Vertical);
    m_toolLauncher->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolLauncher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    if (compact) {
        m_toolsDock->setMinimumWidth(54);
        m_toolsDock->setMaximumWidth(64);
        if (m_toolsDock->isFloating()) {
            m_toolsDock->resize(64, m_toolsDock->height());
        } else {
            resizeDocks({m_toolsDock}, {64}, Qt::Horizontal);
        }
    } else {
        m_toolsDock->setMaximumWidth(QWIDGETSIZE_MAX);
        m_toolsDock->setMinimumWidth(300);
        if (m_toolsDock->isFloating()) {
            m_toolsDock->resize(360, m_toolsDock->height());
        } else {
            resizeDocks({m_toolsDock}, {360}, Qt::Horizontal);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (!m_modified) {
        return true;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this,
        tr("Unsaved session"),
        tr("The current gameboard has unsaved changes."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Save) {
        return saveSession();
    }
    return choice == QMessageBox::Discard;
}

bool MainWindow::saveSession()
{
    return m_currentFile.isEmpty() ? saveSessionAs() : saveSessionTo(m_currentFile);
}

bool MainWindow::saveSessionAs()
{
    const QString suggestion = m_currentFile.isEmpty()
        ? QDir::home().filePath(tr("Untitled.hexboard"))
        : m_currentFile;
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Hexboard session"),
        suggestion,
        sessionFilter());
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".hexboard"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".hexboard");
    }
    return saveSessionTo(path);
}

bool MainWindow::saveSessionTo(const QString &path)
{
    QJsonObject boardData;
    QString boardError;
    if (!m_board->saveSessionData(&boardData, &boardError)) {
        QMessageBox::critical(this, tr("Could not save session"), boardError);
        return false;
    }

    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("hexboard-session")},
        {QStringLiteral("version"), SessionVersion},
        {QStringLiteral("board"), boardData}
    };
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (contents.size() > MaxSessionBytes) {
        QMessageBox::critical(
            this,
            tr("Could not save session"),
            tr("The session exceeds the supported 512 MB file limit."));
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(
            this,
            tr("Could not save session"),
            tr("Could not open %1 for writing:\n%2").arg(path, file.errorString()));
        return false;
    }
    if (file.write(contents) != contents.size()) {
        const QString error = file.errorString();
        file.cancelWriting();
        QMessageBox::critical(
            this,
            tr("Could not save session"),
            tr("Could not write %1:\n%2").arg(path, error));
        return false;
    }
    if (!file.commit()) {
        QMessageBox::critical(
            this,
            tr("Could not save session"),
            tr("Could not finish writing %1:\n%2").arg(path, file.errorString()));
        return false;
    }

    m_currentFile = QFileInfo(path).absoluteFilePath();
    addRecentFile(m_currentFile);
    setModified(false);
    statusBar()->showMessage(tr("Session saved to %1").arg(m_currentFile), 5000);
    return true;
}

bool MainWindow::loadSessionFrom(const QString &path)
{
    QFile file(path);
    const QFileInfo info(file);
    if (!info.exists() || !file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(
            this,
            tr("Could not open session"),
            tr("Could not open %1:\n%2").arg(path, file.errorString()));
        return false;
    }
    if (file.size() > MaxSessionBytes) {
        QMessageBox::critical(
            this,
            tr("Could not open session"),
            tr("%1 is larger than the supported 512 MB session limit.").arg(path));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::critical(
            this,
            tr("Could not open session"),
            tr("%1 is not a valid Hexboard session:\n%2").arg(path, parseError.errorString()));
        return false;
    }

    const QJsonObject root = document.object();
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("hexboard-session")
        || version < 1
        || version > SessionVersion
        || !root.value(QStringLiteral("board")).isObject()) {
        QMessageBox::critical(
            this,
            tr("Could not open session"),
            tr("%1 uses an unsupported or invalid Hexboard session format.").arg(path));
        return false;
    }

    QString boardError;
    if (!m_board->loadSession(root.value(QStringLiteral("board")).toObject(), &boardError)) {
        QMessageBox::critical(
            this,
            tr("Could not open session"),
            tr("Could not load %1:\n%2").arg(path, boardError));
        return false;
    }

    m_currentFile = info.absoluteFilePath();
    addRecentFile(m_currentFile);
    setModified(false);
    statusBar()->showMessage(tr("Session opened from %1").arg(m_currentFile), 5000);
    return true;
}

void MainWindow::newSession()
{
    if (!maybeSave()) {
        return;
    }
    m_board->resetSession();
    m_currentFile.clear();
    setModified(false);
}

void MainWindow::openSession()
{
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Hexboard session"),
        m_currentFile.isEmpty() ? QDir::homePath() : m_currentFile,
        sessionFilter());
    if (!path.isEmpty()) {
        loadSessionFrom(path);
    }
}

void MainWindow::addRecentFile(const QString &path)
{
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    m_recentFiles.removeAll(absolutePath);
    m_recentFiles.prepend(absolutePath);
    while (m_recentFiles.size() > MaxRecentFiles) {
        m_recentFiles.removeLast();
    }
    QSettings().setValue(QStringLiteral("files/recent"), m_recentFiles);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    m_recentMenu->clear();
    m_recentFiles.removeIf([](const QString &path) {
        return !QFileInfo::exists(path);
    });

    if (m_recentFiles.isEmpty()) {
        QAction *emptyAction = m_recentMenu->addAction(tr("No recent sessions"));
        emptyAction->setEnabled(false);
        return;
    }

    for (int index = 0; index < m_recentFiles.size(); ++index) {
        const QString path = m_recentFiles.at(index);
        QString fileName = QFileInfo(path).fileName();
        fileName.replace(QLatin1Char('&'), QStringLiteral("&&"));
        QAction *action = m_recentMenu->addAction(
            tr("&%1 %2").arg(index + 1).arg(fileName));
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path] {
            if (maybeSave()) {
                loadSessionFrom(path);
            }
        });
    }
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_currentFile.isEmpty()
        ? tr("Untitled")
        : QFileInfo(m_currentFile).fileName();
    setWindowTitle(tr("%1[*] — Hexboard").arg(name));
    setWindowModified(m_modified);
}

void MainWindow::setModified(bool modified)
{
    m_modified = modified;
    updateWindowTitle();
}
