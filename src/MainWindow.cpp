#include "MainWindow.h"

#include "CanvasView.h"
#include "ColorDialog.h"
#include "CopilotPanel.h"
#include "Commands.h"
#include "Layer.h"
#include "LayerStack.h"
#include "LayersPanel.h"
#include "ResizeDialog.h"
#include "SizeSliderPanel.h"
#include "Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollArea>
#include <QSlider>
#include <QStatusBar>
#include <QSvgRenderer>
#include <QScrollBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr QSize kDefaultSize(816, 461);
constexpr int kPaletteSize = 16;

const QList<QColor> kPalette = {
    QColor("#FFFFFF"), QColor("#000000"), QColor("#888888"), QColor("#A3867A"),
    QColor("#E7C69F"), QColor("#F9CB9C"), QColor("#C19171"), QColor("#8E5E3E"),
    QColor("#6E382E"), QColor("#4C301A"), QColor("#255E6B"), QColor("#5A3562"),
    QColor("#7AC7E0"), QColor("#2A7AB6"), QColor("#144E73"), QColor("#4B4B5C"),
    QColor("#90C978"), QColor("#D48AAD"), QColor("#60B04C"), QColor("#B95E2A"),
};

const QList<BrushStyle> kBrushStyles = {BrushStyle::Round, BrushStyle::Square,
                                        BrushStyle::Spray, BrushStyle::Calligraphy};

const QList<ShapeStyle> kShapeStyles = {ShapeStyle::Outline, ShapeStyle::Fill,
                                        ShapeStyle::OutlineFill};

QString brushStyleName(BrushStyle s) {
    switch (s) {
    case BrushStyle::Round: return QObject::tr("Round");
    case BrushStyle::Square: return QObject::tr("Square");
    case BrushStyle::Spray: return QObject::tr("Spray");
    case BrushStyle::Calligraphy: return QObject::tr("Calligraphy");
    }
    return "";
}

QString shapeStyleIcon(ShapeStyle s) {
    switch (s) {
    case ShapeStyle::Outline: return "shape-outline";
    case ShapeStyle::Fill: return "shape-fill";
    case ShapeStyle::OutlineFill: return "shape-outline-fill";
    }
    return "";
}

// Renders an SVG with its baked-in colors (gradients), bypassing the mask
// tinting used for monochrome icons.
QIcon renderedIcon(const QString& svgName, int px) {
    QFile f(QStringLiteral(":/assets/icons/%1.svg").arg(svgName));
    QIcon ic;
    if (f.open(QIODevice::ReadOnly)) {
        QSvgRenderer r(f.readAll());
        QPixmap pm(QSize(px * 2, px * 2));
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(2.0);
        QPainter p(&pm);
        r.render(&p, QRectF(0, 0, px, px));
        p.end();
        ic = QIcon(pm);
    }
    return ic;
}

} // namespace

// --------------------------------------------------------------------------
// ColorWellButton
// --------------------------------------------------------------------------

ColorWellButton::ColorWellButton(QWidget* parent)
    : QWidget(parent) {
    setObjectName("ColorWell");
    setFixedSize(48, 38);
    setToolTip(ColorWellButton::tr("Left-click: edit color 1, right-click: edit color 2"));
}

void ColorWellButton::setColors(const QColor& p, const QColor& s) {
    m_primary = p;
    m_secondary = s;
    update();
}

void ColorWellButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const int edge = 8;
    const QRect front(edge, edge, width() - edge, height() - edge);
    const QRect back(0, 0, width() - edge, height() - edge);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 0, 0, 70), 1));
    p.setBrush(m_secondary);
    p.drawRoundedRect(back, 3, 3);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1));
    p.setBrush(m_primary);
    p.drawRoundedRect(front, 3, 3);
}

void ColorWellButton::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton)
        emit primaryClicked();
    else if (ev->button() == Qt::RightButton)
        emit secondaryClicked();
    ev->accept();
}

// --------------------------------------------------------------------------
// MainWindow
// --------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("WPaint — Untitled"));
    resize(1240, 800);
    setMinimumSize(960, 640);

    m_stack = new LayerStack(this);
    m_undo = new QUndoStack(this);

    Layer layer;
    layer.name = tr("Layer 1");
    layer.image = QImage(kDefaultSize, QImage::Format_ARGB32_Premultiplied);
    layer.image.fill(Qt::white);
    m_stack->replaceAll({layer}, 0);

    m_canvas = new CanvasView(m_stack, m_undo, this);
    m_canvas->setColors(QColor("#000000"), QColor("#FFFFFF"));

    buildActions();
    buildMenuBar();
    buildToolbar();
    buildCentral();
    buildDocks();
    buildStatusBar();

    // connections ------------------------------------------------------
    connect(m_canvas, &CanvasView::colorPicked, this,
            [this](const QColor& color, bool primary) {
                if (primary)
                    m_canvas->setColors(color, m_canvas->secondary());
                else
                    m_canvas->setColors(m_canvas->primary(), color);
                syncColorWell();
            });

    connect(m_canvas, &CanvasView::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvas, &CanvasView::selectionChanged, this,
            &MainWindow::updateEditActions);
    connect(m_stack, &LayerStack::changed, this, &MainWindow::syncStatusSize);
    connect(m_undo, &QUndoStack::cleanChanged, this,
            [this](bool clean) {
                Q_UNUSED(clean);
                updateWindowTitle();
                updateEditActions();
            });

    connect(m_sizePanel, &SizeSliderPanel::sizeChanged, m_canvas,
            &CanvasView::setBrushSize);

    connect(m_layersPanel, &LayersPanel::activeRequested, this,
            [this](int index) { m_canvas->setActiveLayer(index); });
    connect(m_layersPanel, &LayersPanel::visibilityRequested, this,
            &MainWindow::toggleLayerVisibility);
    connect(m_layersPanel, &LayersPanel::moveRequested, this,
            [this](int from, int to) {
                runLayerCommand(tr("Move layer"),
                                [this, from, to] { m_stack->moveLayer(from, to); });
            });
    connect(m_layersPanel, &LayersPanel::addRequested, this, &MainWindow::addLayer);
    connect(m_layersPanel, &LayersPanel::removeRequested, this,
            &MainWindow::removeLayer);
    connect(m_layersPanel, &LayersPanel::renameRequested, this,
            &MainWindow::renameLayer);

    selectTool(ToolId::Pencil);
    syncStatusSize();
    updateEditActions();
    // The scroll area has no real size until we're shown; fit once the
    // window is laid out so the canvas doesn't sit at minimum zoom.
    QTimer::singleShot(0, this, [this] { m_canvas->zoomFit(); });
}

// ------------------------------------------------------------ actions -----

void MainWindow::buildActions() {
    auto overrideShortcut = [this](QAction* a, const QKeySequence& seq) {
        a->setShortcut(seq);
        a->setShortcutContext(Qt::WindowShortcut);
    };

    // File
    QAction* newAct = new QAction(tr("New"), this);
    overrideShortcut(newAct, QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newDocument);

    QAction* openAct = new QAction(tr("Open..."), this);
    overrideShortcut(openAct, QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openDocument);

    QAction* saveAct = new QAction(tr("Save"), this);
    overrideShortcut(saveAct, QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveDocument);

    QAction* saveAsAct = new QAction(tr("Save As..."), this);
    overrideShortcut(saveAsAct, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveDocumentAs);

    QAction* exitAct = new QAction(tr("Exit"), this);
    exitAct->setMenuRole(QAction::QuitRole);
    overrideShortcut(exitAct, QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);

    // Edit
    m_undoAction = m_undo->createUndoAction(this, tr("Undo"));
    overrideShortcut(m_undoAction, QKeySequence::Undo);
    m_redoAction = m_undo->createRedoAction(this, tr("Redo"));
    overrideShortcut(m_redoAction, QKeySequence::Redo);
    QAction* redoAlt = new QAction(tr("Redo (alt)"), this);
    overrideShortcut(redoAlt, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    connect(redoAlt, &QAction::triggered, m_undo, &QUndoStack::redo);

    m_cutAction = new QAction(tr("Cut"), this);
    overrideShortcut(m_cutAction, QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::doCut);

    m_copyAction = new QAction(tr("Copy"), this);
    overrideShortcut(m_copyAction, QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::doCopy);

    m_pasteAction = new QAction(tr("Paste"), this);
    overrideShortcut(m_pasteAction, QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::doPaste);

    m_selectAllAction = new QAction(tr("Select All"), this);
    overrideShortcut(m_selectAllAction, QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::doSelectAll);

    m_deleteAction = new QAction(tr("Delete"), this);
    overrideShortcut(m_deleteAction, QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::doDelete);

    QAction* resizeAct = new QAction(tr("Resize and Rotate..."), this);
    overrideShortcut(resizeAct, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(resizeAct, &QAction::triggered, this, [this] { openResizeDialog(); });

    // View
    QAction* zoomIn = new QAction(tr("Zoom In"), this);
    overrideShortcut(zoomIn, QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, m_canvas, &CanvasView::zoomIn);

    QAction* zoomOut = new QAction(tr("Zoom Out"), this);
    overrideShortcut(zoomOut, QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, m_canvas, &CanvasView::zoomOut);

    QAction* zoomActual = new QAction(tr("Actual Size"), this);
    overrideShortcut(zoomActual, QKeySequence("Ctrl+0"));
    connect(zoomActual, &QAction::triggered, m_canvas, &CanvasView::zoomActual);

    QAction* zoomFit = new QAction(tr("Fit to Window"), this);
    overrideShortcut(zoomFit, QKeySequence("Ctrl+9"));
    connect(zoomFit, &QAction::triggered, m_canvas, &CanvasView::zoomFit);

    QMenu* fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    QMenu* editMenu = menuBar()->addMenu(tr("Edit"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addAction(m_selectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(m_deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(resizeAct);

    QMenu* viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(zoomIn);
    viewMenu->addAction(zoomOut);
    viewMenu->addAction(zoomActual);
    viewMenu->addAction(zoomFit);
    viewMenu->addSeparator();

    QAction* themeToggle = new QAction(tr("Toggle Dark Mode"), this);
    themeToggle->setCheckable(true);
    themeToggle->setChecked(Theme::mode() == Theme::Mode::Dark);
    overrideShortcut(themeToggle, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(themeToggle, &QAction::toggled, this, [this](bool on) {
        Theme::setMode(on ? Theme::Mode::Dark : Theme::Mode::Light);
        syncColorWell();
    });
    viewMenu->addAction(themeToggle);
}

void MainWindow::openResizeDialog() {
    const QSize doc = m_stack->size();
    const QSize sel = m_canvas->selectionPixelRect().size();
    ResizeDialog dlg(doc, sel.isValid() ? sel : QSize(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const ResizeParams p = dlg.params();
    if (p.selection) {
        m_canvas->transformSelection(QSize(p.width, p.height), p.rotateDegrees);
        m_canvas->clearSelection();
    } else {
        if (p.width > 0 && p.height > 0)
            m_canvas->setCanvasSize(QSize(p.width, p.height));
        if (p.rotateDegrees != 0)
            m_canvas->rotateCanvas(p.rotateDegrees);
    }
}

// ----------------------------------------------------------- menu bar -----

void MainWindow::buildMenuBar() {
    // Menu building happens in buildActions(); here we only make sure the
    // menu bar is present.
    menuBar();
}

void MainWindow::buildToolbar() {
    QToolBar* bar = new QToolBar(tr("App toolbar"), this);
    bar->setObjectName("AppToolbar");
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setIconSize(QSize(20, 20));
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bar->toggleViewAction()->setVisible(false);
    addToolBar(Qt::TopToolBarArea, bar);

    const auto& specs = ToolRegistry::specs();

    // Selection
    {
        QList<QWidget*> c;
        c << toolButtonFor(ToolId::Select);
        bar->addWidget(toolCluster(tr("Selection"), c));
    }
    bar->addWidget(divider());

    // Image
    {
        QList<QWidget*> c;
        c << toolButtonFor(ToolId::Crop);
        auto* resizeBtn = new QToolButton(bar);
        resizeBtn->setCheckable(false);
        resizeBtn->setToolTip(tr("Resize and rotate"));
        Theme::setIcon(resizeBtn, "resize");
        connect(resizeBtn, &QToolButton::clicked, this, [this] { openResizeDialog(); });
        c << resizeBtn;
        bar->addWidget(toolCluster(tr("Image"), c));
    }
    bar->addWidget(divider());

    // Tools
    {
        QList<QWidget*> c;
        c << toolButtonFor(ToolId::Pencil) << toolButtonFor(ToolId::Fill)
          << toolButtonFor(ToolId::Eraser) << toolButtonFor(ToolId::Eyedropper)
          << toolButtonFor(ToolId::Text);
        bar->addWidget(toolCluster(tr("Tools"), c));
    }
    bar->addWidget(divider());

    // Brushes
    {
        m_brushButton = new QToolButton(bar);
        m_brushButton->setObjectName("MenuButtonPopup");
        m_brushButton->setPopupMode(QToolButton::MenuButtonPopup);
        m_brushButton->setToolTip(tr("Brush"));
        Theme::setIcon(m_brushButton, "brush");
        connect(m_brushButton, &QToolButton::clicked, this,
                [this] { selectTool(ToolId::Brush); });
        QMenu* brushMenu = new QMenu(m_brushButton);
        for (BrushStyle s : kBrushStyles) {
            QAction* a = brushMenu->addAction(brushStyleName(s));
            a->setCheckable(true);
            connect(a, &QAction::triggered, this, [this, s] { applyBrushStyle(s); });
        }
        m_brushButton->setMenu(brushMenu);
        bar->addWidget(toolCluster(tr("Brushes"), {m_brushButton}));
    }
    bar->addWidget(divider());

    // Shapes
    {
        m_shapeButton = new QToolButton(bar);
        m_shapeButton->setObjectName("MenuButtonPopup");
        m_shapeButton->setPopupMode(QToolButton::InstantPopup);
        m_shapeButton->setToolTip(tr("Shapes"));
        Theme::setIcon(m_shapeButton, "shape-rect");
        QMenu* shapeMenu = new QMenu(m_shapeButton);
        QActionGroup* grp = new QActionGroup(shapeMenu);
        grp->setExclusive(true);
        for (const auto& s : specs) {
            if (!s.inShapes) continue;
            QAction* a = shapeMenu->addAction(Theme::icon(s.icon, 18), s.name);
            grp->addAction(a);
            const ToolId id = s.id;
            connect(a, &QAction::triggered, this, [this, id, a] {
                m_shapeButton->setIcon(Theme::icon(ToolRegistry::spec(id).icon, 20));
                applyShape(static_cast<ShapeKit::Shape>(id));
                selectTool(id);
                a->setChecked(true);
            });
        }
        for (QAction* a : grp->actions())
            if (a->text() == QObject::tr("Rectangle")) {
                a->setChecked(true);
                break;
            }
        m_shapeButton->setMenu(shapeMenu);

        m_shapeStyleButton = new QToolButton(bar);
        m_shapeStyleButton->setObjectName("MenuButtonPopup");
        m_shapeStyleButton->setPopupMode(QToolButton::InstantPopup);
        m_shapeStyleButton->setToolTip(tr("Shape fill pattern"));
        Theme::setIcon(m_shapeStyleButton, "shape-outline");
        QMenu* styleMenu = new QMenu(m_shapeStyleButton);
        QActionGroup* styleGrp = new QActionGroup(styleMenu);
        styleGrp->setExclusive(true);
        for (ShapeStyle st : kShapeStyles) {
            QAction* a = styleMenu->addAction(QString());
            a->setIcon(Theme::icon(shapeStyleIcon(st), 18));
            styleGrp->addAction(a);
            connect(a, &QAction::triggered, this, [this, st] {
                applyShapeStyle(st);
            });
        }
        styleGrp->actions().first()->setChecked(true);
        m_shapeStyleButton->setMenu(styleMenu);

        bar->addWidget(toolCluster(tr("Shapes"),
                                   {m_shapeButton, m_shapeStyleButton}));
    }
    bar->addWidget(divider());

    // Colors
    {
        m_well = new ColorWellButton(this);
        connect(m_well, &ColorWellButton::primaryClicked, this,
                [this] { editColor(true); });
        connect(m_well, &ColorWellButton::secondaryClicked, this,
                [this] { editColor(false); });

        QWidget* paletteHost = new QWidget(this);
        paletteHost->setObjectName("PaletteHost");
        auto* grid = new QGridLayout(paletteHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(2);
        m_palette.reserve(kPalette.size());
        for (int i = 0; i < kPalette.size(); ++i) {
            auto* b = new QToolButton(paletteHost);
            b->setObjectName("PaletteButton");
            b->setFixedSize(kPaletteSize, kPaletteSize);
            b->setToolTip(kPalette.at(i).name());
            const QColor col = kPalette.at(i);
            QPixmap pm(kPaletteSize * 2, kPaletteSize * 2);
            pm.fill(col);
            pm.setDevicePixelRatio(2.0);
            b->setIcon(QIcon(pm));
            b->setIconSize(QSize(kPaletteSize, kPaletteSize));
            connect(b, &QToolButton::clicked, this, [this, col] {
                m_canvas->setColors(col, m_canvas->secondary());
                syncColorWell();
            });
            connect(b, &QToolButton::customContextMenuRequested, this,
                    [this, col](const QPoint&) {
                        m_canvas->setColors(m_canvas->primary(), col);
                        syncColorWell();
                    });
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            grid->addWidget(b, i / 10, i % 10);
        }

        auto* wheelBtn = new QToolButton(this);
        wheelBtn->setObjectName("PaletteButton");
        wheelBtn->setFixedSize(kPaletteSize + 4, kPaletteSize + 4);
        wheelBtn->setToolTip(tr("Edit colors"));
        Theme::setIcon(wheelBtn, "colorwheel", kPaletteSize);
        connect(wheelBtn, &QToolButton::clicked, this, [this] { editColor(true); });

        QList<QWidget*> colorsWidgets;
        colorsWidgets << m_well << paletteHost << wheelBtn;
        bar->addWidget(toolCluster(tr("Colors"), colorsWidgets));
    }
    bar->addWidget(divider());

    // Copilot
    {
        auto* copilotBtn = new QToolButton(bar);
        copilotBtn->setToolTip(tr("Show Copilot"));
        copilotBtn->setCheckable(true);
        copilotBtn->setChecked(false);
        copilotBtn->setIcon(renderedIcon("copilot", 20));
        connect(copilotBtn, &QToolButton::toggled, this, [this](bool on) {
            m_copilotDock->setVisible(on);
        });
        bar->addWidget(toolCluster(tr("Copilot"), {copilotBtn}));
    }
    bar->addWidget(divider());

    // Layers
    {
        auto* layersToggle = new QToolButton(bar);
        layersToggle->setToolTip(tr("Show layers panel"));
        layersToggle->setCheckable(true);
        layersToggle->setChecked(true);
        Theme::setIcon(layersToggle, "layer-stack");
        connect(layersToggle, &QToolButton::toggled, this, [this](bool on) {
            m_layersDock->setVisible(on);
        });
        auto* addBtn = new QToolButton(bar);
        addBtn->setToolTip(tr("Add layer"));
        Theme::setIcon(addBtn, "layer-add");
        connect(addBtn, &QToolButton::clicked, this, &MainWindow::addLayer);
        auto* delBtn = new QToolButton(bar);
        delBtn->setToolTip(tr("Delete layer"));
        Theme::setIcon(delBtn, "layer-delete");
        connect(delBtn, &QToolButton::clicked, this, [this] {
            removeLayer(m_stack->activeIndex());
        });
        bar->addWidget(toolCluster(tr("Layers"), {layersToggle, addBtn, delBtn}));
    }
}

QWidget* MainWindow::toolCluster(const QString& caption,
                                 const QList<QWidget*>& controls) {
    auto* host = new QWidget(this);
    auto* v = new QVBoxLayout(host);
    v->setContentsMargins(6, 4, 6, 4);
    v->setSpacing(2);

    auto* row = new QWidget(host);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(2);
    for (QWidget* c : controls)
        h->addWidget(c);
    h->addStretch(1);

    auto* cap = new QLabel(caption, host);
    cap->setObjectName("ClusterCaption");
    cap->setAlignment(Qt::AlignHCenter);

    v->addWidget(row);
    v->addWidget(cap);
    return host;
}

QWidget* MainWindow::divider() {
    auto* d = new QWidget(this);
    d->setObjectName("ToolbarDivider");
    d->setFixedSize(1, 34);
    return d;
}

QToolButton* MainWindow::toolButtonFor(ToolId id) {
    if (QToolButton* existing = m_toolButtons.value(id, nullptr))
        return existing;
    const auto& s = ToolRegistry::spec(id);
    auto* b = new QToolButton(this);
    b->setCheckable(true);
    b->setToolTip(s.name);
    Theme::setIcon(b, s.icon);
    connect(b, &QToolButton::clicked, this, [this, id] { selectTool(id); });
    m_toolButtons.insert(id, b);
    return b;
}

// ------------------------------------------------------------- central -----

void MainWindow::buildCentral() {
    auto* host = new QWidget(this);
    host->setObjectName("canvasHost");
    auto* top = new QHBoxLayout(host);
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(0);

    m_sizePanel = new SizeSliderPanel(host);
    top->addWidget(m_sizePanel);

    m_scrollArea = new QScrollArea(host);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("CanvasScrollArea");
    m_scrollArea->setWidget(m_canvas);
    m_canvas->attachScrollArea(m_scrollArea);
    top->addWidget(m_scrollArea, 1);

    setCentralWidget(host);
}

// -------------------------------------------------------------- docks -----

void MainWindow::buildDocks() {
    m_layersPanel = new LayersPanel(m_stack, this);
    m_layersDock = new QDockWidget(tr("Layers"), this);
    m_layersDock->setObjectName("LayersDock");
    m_layersDock->setWidget(m_layersPanel);
    m_layersDock->setAllowedAreas(Qt::RightDockWidgetArea |
                                  Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);

    m_copilotPanel = new CopilotPanel(this);
    m_copilotDock = new QDockWidget(tr("Copilot"), this);
    m_copilotDock->setObjectName("CopilotDock");
    m_copilotDock->setWidget(m_copilotPanel);
    m_copilotDock->setAllowedAreas(Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_copilotDock);
    m_copilotDock->hide();
    // stack below the layers panel
    resizeDocks({m_layersDock, m_copilotDock}, {120, 300}, Qt::Vertical);
}

// ---------------------------------------------------------- status bar -----

void MainWindow::buildStatusBar() {
    m_sizeStatus = new QLabel(this);
    m_sizeStatus->setObjectName("StatusDim");
    statusBar()->addWidget(m_sizeStatus);

    m_zoomOutBtn = new QToolButton(this);
    m_zoomOutBtn->setAutoRaise(true);
    Theme::setIcon(m_zoomOutBtn, "zoom-out", 16);
    connect(m_zoomOutBtn, &QToolButton::clicked, m_canvas, &CanvasView::zoomOut);

    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setObjectName("StatusZoomLabel");
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(20, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(140);
    m_zoomSlider->setTickInterval(40);
    m_zoomSlider->setTickPosition(QSlider::TicksAbove);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_zooming) return;
        m_canvas->setZoom(v / 100.0);
    });

    m_zoomInBtn = new QToolButton(this);
    m_zoomInBtn->setAutoRaise(true);
    Theme::setIcon(m_zoomInBtn, "zoom-in", 16);
    connect(m_zoomInBtn, &QToolButton::clicked, m_canvas, &CanvasView::zoomIn);

    auto* zoomHost = new QWidget(this);
    auto* h = new QHBoxLayout(zoomHost);
    h->setContentsMargins(4, 2, 8, 2);
    h->setSpacing(4);
    h->addWidget(m_zoomOutBtn);
    h->addWidget(m_zoomSlider);
    h->addWidget(m_zoomLabel);
    h->addWidget(m_zoomInBtn);

    statusBar()->addPermanentWidget(zoomHost);
}

// ----------------------------------------------------------- behavior -----

void MainWindow::selectTool(ToolId id) {
    m_canvas->setTool(ToolRegistry::create(id));
    syncToolButtons();
    m_sizePanel->setEnabledForTool(m_canvas->tool()->supportsBrushSize());
    updateEditActions();
}

void MainWindow::syncToolButtons() {
    const ToolId id = m_canvas->tool() ? m_canvas->tool()->id() : ToolId::Select;
    for (auto it = m_toolButtons.constBegin(); it != m_toolButtons.constEnd(); ++it)
        it.value()->setChecked(it.key() == id);
    if (m_shapeButton && id >= ToolId::ShapeLine)
        m_shapeButton->setChecked(true);
    else if (m_shapeButton)
        m_shapeButton->setChecked(false);
}

void MainWindow::applyShape(ShapeKit::Shape shape) {
    m_canvas->setShape(shape);
}

void MainWindow::applyShapeStyle(ShapeStyle style) {
    m_canvas->setShapeStyle(style);
}

void MainWindow::applyBrushStyle(BrushStyle style) {
    m_canvas->setBrushStyle(style);
    selectTool(ToolId::Brush);
}

void MainWindow::syncColorWell() {
    if (m_well)
        m_well->setColors(m_canvas->primary(), m_canvas->secondary());
}

void MainWindow::onZoomChanged(qreal zoom) {
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
    m_zooming = true;
    m_zoomSlider->setValue(qBound(20, qRound(zoom * 100), 400));
    m_zooming = false;
}

void MainWindow::syncStatusSize() {
    const QSize s = m_stack->size();
    m_sizeStatus->setText(tr("%1 × %2 px").arg(s.width()).arg(s.height()));
    m_sizePanel->setSize(m_canvas->brushSize());
}

void MainWindow::updateEditActions() {
    const bool sel = m_canvas->hasSelection();
    m_cutAction->setEnabled(sel);
    m_copyAction->setEnabled(sel);
    m_deleteAction->setEnabled(sel || m_canvas->floatingActive());
    m_pasteAction->setEnabled(true);
}

void MainWindow::editColor(bool primary) {
    ColorDialog dlg(primary ? m_canvas->primary() : m_canvas->secondary(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (primary)
        m_canvas->setColors(dlg.chosenColor(), m_canvas->secondary());
    else
        m_canvas->setColors(m_canvas->primary(), dlg.chosenColor());
    syncColorWell();
}

// -------------------------------------------------------------- file ------

bool MainWindow::confirmDiscard() {
    if (m_undo->isClean())
        return true;
    const auto r = QMessageBox::warning(
        this, tr("WPaint"),
        tr("Save changes to the current image?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (r == QMessageBox::Cancel)
        return false;
    if (r == QMessageBox::Save)
        saveDocument();
    return true;
}

void MainWindow::newDocument() {
    if (!confirmDiscard())
        return;
    m_currentPath.clear();
    m_undo->clear();
    m_stack->clear();
    Layer l;
    l.name = tr("Layer 1");
    l.image = QImage(kDefaultSize, QImage::Format_ARGB32_Premultiplied);
    l.image.fill(Qt::white);
    m_stack->replaceAll({l}, 0);
    updateWindowTitle();
}

void MainWindow::openDocument() {
    if (!confirmDiscard())
        return;
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif);;"
           "WPaint Project (*.wpa);;All files (*)"));
    if (file.isEmpty())
        return;

    if (file.endsWith(QLatin1String(".wpa"), Qt::CaseInsensitive)) {
        LayerStack loaded;
        const auto res = loaded.loadProject(file);
        if (!res.ok) {
            QMessageBox::critical(this, tr("WPaint"),
                                  tr("Could not open project: %1").arg(res.error));
            return;
        }
        installDocument(loaded, file);
        return;
    }

    QImageReader reader(file);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::critical(this, tr("WPaint"),
                              tr("Could not load image:\n%1").arg(reader.errorString()));
        return;
    }
    LayerStack loaded;
    Layer l;
    l.name = tr("Background");
    l.image = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    loaded.addLayer(0, l);
    installDocument(loaded, file);
}

void MainWindow::installDocument(const LayerStack& loaded, const QString& path) {
    m_canvas->cancelFloating();
    m_undo->clear();
    m_stack->replaceAll(loaded.layers(), loaded.activeIndex());
    m_currentPath = path;
    updateWindowTitle();
    m_canvas->zoomFit();
    syncStatusSize();
}

void MainWindow::saveDocument() {
    if (m_currentPath.isEmpty() || m_currentPath.endsWith(QLatin1String(".wpa")))
        saveDocumentAs();
    else
        saveTo(m_currentPath);
}

void MainWindow::saveDocumentAs() {
    QString filter;
    const QString file = QFileDialog::getSaveFileName(
        this, tr("Save As"), m_currentPath.isEmpty() ? tr("untitled") : m_currentPath,
        tr("PNG Image (*.png);;JPEG Image (*.jpg);;Bitmap (*.bmp);;"
           "GIF Image (*.gif);;WPaint Project (*.wpa)"),
        &filter);
    if (file.isEmpty())
        return;
    m_currentPath = file;
    saveTo(file);
}

bool MainWindow::saveTo(const QString& path) {
    if (path.endsWith(QLatin1String(".wpa"), Qt::CaseInsensitive)) {
        const auto res = m_stack->saveProject(path);
        if (!res.ok) {
            QMessageBox::critical(this, tr("WPaint"),
                                  tr("Could not save project: %1").arg(res.error));
            return false;
        }
        m_undo->setClean();
        updateWindowTitle();
        return true;
    }

    const QImage flat = exportedComposite();
    const bool pngLike = path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive);
    QImage out = flat;
    if (!pngLike) {
        // JPEG/BMP/GIF have no alpha: composite onto white first.
        out = QImage(flat.size(), QImage::Format_RGB32);
        out.fill(Qt::white);
        QPainter p(&out);
        p.drawImage(0, 0, flat);
        p.end();
    }
    QImageWriter writer(path);
    const bool ok = writer.write(out);
    if (!ok)
        QMessageBox::critical(this, tr("WPaint"),
                              tr("Could not save image: %1")
                                  .arg(writer.errorString()));
    else {
        m_undo->setClean();
        updateWindowTitle();
    }
    return ok;
}

QImage MainWindow::exportedComposite() const {
    // The canonical flattened preview paints transparent areas white so
    // JPEG/BMP/GIF never embed transparency we can't honor.
    return m_canvas->composite();
}

void MainWindow::updateWindowTitle() {
    const QString name =
        m_currentPath.isEmpty()
            ? tr("Untitled")
            : QFileInfo(m_currentPath).fileName();
    const bool dirty = !m_undo->isClean();
    setWindowTitle(tr("WPaint — %1%2").arg(name, dirty ? QStringLiteral(" *") : QString()));
}

// ------------------------------------------------------------ edit ops ----

void MainWindow::doUndo() {
    m_canvas->weldFloating();
    m_undo->undo();
}

void MainWindow::doRedo() {
    m_canvas->weldFloating();
    m_undo->redo();
}

void MainWindow::doCut() {
    if (!m_canvas->hasSelection()) return;
    m_canvas->liftSelection();
    m_copied = m_canvas->copySelection();
    m_copiedTopLeft = m_canvas->selection().topLeft();
    QGuiApplication::clipboard()->setImage(m_copied);
    updateEditActions();
}

void MainWindow::doCopy() {
    if (!m_canvas->hasSelection()) return;
    m_copied = m_canvas->copySelection();
    m_copiedTopLeft = m_canvas->selection().topLeft();
    QGuiApplication::clipboard()->setImage(m_copied);
}

void MainWindow::doPaste() {
    m_canvas->weldFloating();
    QImage img = m_copied;
    if (img.isNull() && QGuiApplication::clipboard()->image().isNull()) {
        QPixmap pm = QGuiApplication::clipboard()->pixmap();
        if (!pm.isNull())
            img = pm.toImage();
    }
    if (img.isNull() && !m_copied.isNull())
        img = m_copied;
    if (img.isNull())
        return;
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Place near the center of the visible viewport.
    const QRect vp = m_scrollArea->viewport()->rect();
    const QPointF centerWidget(m_scrollArea->horizontalScrollBar()->value() +
                                   vp.width() / 2.0,
                               m_scrollArea->verticalScrollBar()->value() +
                                   vp.height() / 2.0);
    const QPointF centerImg = m_canvas->toImage(centerWidget);
    const QPointF topLeft(centerImg.x() - img.width() / 2.0,
                          centerImg.y() - img.height() / 2.0);

    m_canvas->pasteFloating(img, topLeft);
    selectTool(ToolId::Select);
    updateEditActions();
}

void MainWindow::doSelectAll() {
    m_canvas->weldFloating();
    m_canvas->setSelection(QRectF(0, 0, m_stack->size().width(),
                                  m_stack->size().height()));
}

void MainWindow::doDelete() {
    if (m_canvas->floatingActive())
        m_canvas->commitFloatingRemoval(tr("Delete selection"));
    else if (m_canvas->hasSelection())
        m_canvas->clearSelectionRegion(tr("Clear selection"));
    updateEditActions();
}

// ------------------------------------------------------- layer helpers ----

void MainWindow::runLayerCommand(const QString& text, std::function<void()> mutate) {
    const QList<Layer> before = m_stack->layers();
    mutate();
    m_undo->push(Commands::makeLayerList(m_stack, before, text));
}

void MainWindow::addLayer() {
    runLayerCommand(tr("Add layer"), [this] {
        Layer l;
        l.name = m_stack->nextName(tr("Layer"));
        l.image = QImage(m_stack->size(), QImage::Format_ARGB32_Premultiplied);
        l.image.fill(Qt::transparent);
        m_stack->addLayer(0, l);
        m_stack->setActiveIndex(0);
    });
    m_layersPanel->setActiveLayer(0);
}

void MainWindow::removeLayer(int index) {
    if (index < 0 || index >= m_stack->count() || m_stack->count() <= 1)
        return;
    runLayerCommand(tr("Delete layer"), [this, index] { m_stack->removeLayer(index); });
}

void MainWindow::toggleLayerVisibility(int index, bool visible) {
    runLayerCommand(tr(visible ? "Show layer" : "Hide layer"),
                    [this, index, visible] { m_stack->setLayerVisible(index, visible); });
}

void MainWindow::renameLayer(int index, const QString& name) {
    if (index < 0 || index >= m_stack->count())
        return;
    runLayerCommand(tr("Rename layer"), [this, index, name] { m_stack->renameLayer(index, name); });
}

void MainWindow::closeEvent(QCloseEvent* ev) {
    if (!confirmDiscard()) {
        ev->ignore();
        return;
    }
    ev->accept();
}