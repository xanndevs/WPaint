#include "MainWindow.h"

#include "CanvasView.h"
#include "ColorDialog.h"
#include "CopilotPanel.h"
#include "Commands.h"
#include "FluentSlider.h"
#include "Layer.h"
#include "LayerStack.h"
#include "LayersPanel.h"
#include "ResizeDialog.h"
#include "Settings.h"
#include "SettingsDialog.h"
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
#include <QGridLayout>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QContextMenuEvent>
#include <QCursor>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSlider>
#include <QStatusBar>
#include <QSvgRenderer>
#include <QScrollBar>
#include <QTimer>
#include <QToolBar>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr QSize kDefaultSize(400, 400);
constexpr int kPaletteSize = 18;

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

QString shapeStyleName(ShapeStyle s) {
    switch (s) {
    case ShapeStyle::Outline: return QObject::tr("Outline");
    case ShapeStyle::Fill: return QObject::tr("Fill");
    case ShapeStyle::OutlineFill: return QObject::tr("Outline and fill");
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

// The fill-mode button shows the *current* mode rather than a generic pattern
// glyph, so each mode needs its own variant of the same hatched-square artwork.
QString shapeStyleModeIcon(ShapeStyle s) {
    switch (s) {
    case ShapeStyle::Outline: return "shape-fill-mode-selection-out";
    case ShapeStyle::Fill: return "shape-fill-mode-selection-in";
    case ShapeStyle::OutlineFill: return "shape-fill-mode-selection-out-in";
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

// Draws `zone` with only its outer corners rounded, so it reads as one button
// divided into zones rather than a smaller button sitting on a bigger one.
static void fillZone(QPainter& p, const QRectF& z, bool roundTop, qreal radius) {
    // One path, one fill. Squaring the inner corners by painting a second rect
    // over the first composes the translucent fill twice in that half, which
    // shows up as a hard brightness step halfway down the zone.
    const qreal r = qBound<qreal>(0.0, radius, qMin(z.width(), z.height()) / 2.0);
    QPainterPath path;
    if (roundTop) {
        path.moveTo(z.left() + r, z.top());
        path.lineTo(z.right() - r, z.top());
        path.arcTo(QRectF(z.right() - 2 * r, z.top(), 2 * r, 2 * r), 90, -90);
        path.lineTo(z.right(), z.bottom());
        path.lineTo(z.left(), z.bottom());
        path.lineTo(z.left(), z.top() + r);
        path.arcTo(QRectF(z.left(), z.top(), 2 * r, 2 * r), 180, -90);
    } else {
        path.moveTo(z.left(), z.top());
        path.lineTo(z.right(), z.top());
        path.lineTo(z.right(), z.bottom() - r);
        path.arcTo(QRectF(z.right() - 2 * r, z.bottom() - 2 * r, 2 * r, 2 * r), 0, -90);
        path.lineTo(z.left() + r, z.bottom());
        path.arcTo(QRectF(z.left(), z.bottom() - 2 * r, 2 * r, 2 * r), 270, -90);
    }
    path.closeSubpath();
    p.fillPath(path, p.brush());
}

// A gallery button, optionally split into two stacked zones: the body picks the
// tool, the chevron strip along the bottom opens the menu. Qt's own
// MenuButtonPopup splits left/right, which put the dropdown on a thin right-hand
// sliver. The outer box is fixed by the cluster and never changes -- only how
// the box divides.
//
// With splitting off (a button whose whole surface is just "open this menu")
// the caret is still drawn, but there is no zone hit-testing and no hover
// division, because there is nothing to choose between.
class PopupButton : public QToolButton {
public:
    explicit PopupButton(QWidget* parent = nullptr) : QToolButton(parent) {
        setContextMenuPolicy(Qt::NoContextMenu);
        setSplitEnabled(true);
        // The checked state drives both the fill and the icon colour, and it
        // flips at runtime, so the icon has to be re-tinted here rather than
        // only at theme-change time.
        connect(this, &QToolButton::toggled, this, [this] { Theme::refreshIcon(this); });
    }

    // False makes the entire surface open the menu (InstantPopup), with no
    // separate tool-select zone.
    void setSplitEnabled(bool on) {
        m_split = on;
        setProperty("wpSplit", on ? 1 : 0);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    int caretBand() const { return Theme::tokens().caretBand; }

protected:
    void mousePressEvent(QMouseEvent* ev) override {
        if (!m_split) { // whole button is the menu
            QToolButton::mousePressEvent(ev);
            return;
        }
        if (ev->button() == Qt::LeftButton && isMenuBand(ev->position().toPoint())) {
            if (QMenu* m = menu()) {
                m->popup(mapToGlobal(QPoint(width() / 2, height())));
                ev->accept();
                return;
            }
        }
        if (ev->button() == Qt::LeftButton) {
            // Tool-select zone. Toggle by hand so the button can stay checkable
            // (Qt will not auto-open a popup for a checkable button).
            setChecked(!isChecked());
            emit clicked();
            ev->accept();
            return;
        }
        QToolButton::mousePressEvent(ev);
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (isDown()) {
            ev->accept();
            return;
        }
        if (m_split) {
            // Remember which zone the pointer is over so only that one lights up.
            const bool inMenu = isMenuBand(ev->position().toPoint());
            if (inMenu != m_hoverMenu) {
                m_hoverMenu = inMenu;
                update();
            }
        }
        QToolButton::mouseMoveEvent(ev);
    }

    void enterEvent(QEnterEvent* ev) override {
        m_inWidget = true;
        m_hoverMenu = isMenuBand(ev->position().toPoint());
        QToolButton::enterEvent(ev);
        update();
    }

    void leaveEvent(QEvent* ev) override {
        m_inWidget = false;
        m_hoverMenu = false;
        QToolButton::leaveEvent(ev);
        update();
    }

    void paintEvent(QPaintEvent* e) override {
        Q_UNUSED(e);
        // Every gallery button paints itself, because the highlight has to go
        // *behind* the glyph. The base widget draws background then glyph, and
        // anything we paint afterwards lands on top of the glyph -- so a fill
        // either hides the tool (opaque) or stains it (translucent). Neither is
        // right, and neither is fixable after the fact. Drawing the background
        // first and the glyph over it is the only way to light the button up
        // without touching the artwork. The stylesheet contributes nothing else
        // to these buttons beyond a transparent background, a transparent 1px
        // border and the radius, all three reproduced below.
        const auto& t = Theme::tokens();
        const int band = caretBand();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        if (isChecked()) {
            // A checked button still has to answer the pointer: the plain
            // QToolButton gets accentHover from :checked:hover, and these paint
            // their own fill, so without this they went dead under the mouse.
            p.setPen(Qt::NoPen);
            p.setBrush(m_inWidget ? t.accentHover : t.accent);
            p.drawRoundedRect(QRectF(rect()), t.radiusSm, t.radiusSm);
        } else if (m_inWidget) {
            // controlHover at full strength. This used to be translucent so the
            // glyph could survive being painted over; now the glyph goes on top
            // of the fill, so the highlight can be the real colour.
            p.setPen(Qt::NoPen);
            p.setBrush(t.controlHover);
            // A split button lights up one zone at a time, and fillZone rounds
            // only the corners that zone owns. A whole-surface button has no
            // inner edge, so it needs all four corners rounded -- the old
            // QSS-driven path did that for free, the painter has to ask.
            if (!m_split)
                p.drawRoundedRect(QRectF(rect()), t.radiusSm, t.radiusSm);
            else if (m_hoverMenu)
                fillZone(p, QRectF(0, height() - band, width(), band),
                         /*roundTop=*/false, t.radiusSm);
            else
                fillZone(p, QRectF(0, 0, width(), height() - band),
                         /*roundTop=*/true, t.radiusSm);
        }

        // The glyph is centred on the whole box, exactly like every other
        // toolbar button, then lifted by glyphLift: geometric centring reads as
        // sitting low next to the plain big buttons, and the chevron strip owns
        // the space the lift frees. Centring on the box minus the band instead
        // (what this used to do) put the glyph 7px high, and that was worse.
        const QSize is = iconSize();
        if (!icon().isNull() && is.width() > 0 && is.height() > 0) {
            const qreal dpr = qMax<qreal>(2.0, devicePixelRatioF());
            const QPixmap pm = icon().pixmap(is, dpr);
            if (!pm.isNull())
                p.drawPixmap(QPoint(width() / 2 - is.width() / 2,
                                    height() / 2 - is.height() / 2 - t.glyphLift),
                             pm);
        }

        if (!menu()) return;
        paintCaret(p, height() - band / 2);
    }

    void paintCaret(QPainter& p, int cy) {
        const auto& t = Theme::tokens();
        const int glyph = t.caretGlyph;
        // Theme::icon renders at 2x and hands back a pixmap whose
        // devicePixelRatio is already 2, so asking for .pixmap(glyph, glyph)
        // makes Qt downscale it and a 10px caret comes back as a faint smear
        // with no solid pixel in it. Ask at the native ratio instead and let
        // the painter scale it down.
        const qreal dpr = qMax<qreal>(2.0, devicePixelRatioF());
        const QPixmap pm = Theme::icon("chevron-down", glyph,
                                       isChecked() ? t.iconOnAccent : t.icon)
                               .pixmap(QSize(glyph, glyph), dpr);
        if (pm.isNull()) return;
        p.drawPixmap((width() - glyph) / 2, cy - glyph / 2, pm);
    }

private:
    bool isMenuBand(const QPoint& p) const {
        return p.y() >= height() - caretBand();
    }
    bool m_split = true;
    bool m_inWidget = false;
    bool m_hoverMenu = false;
};

} // namespace

// --------------------------------------------------------------------------
// ColorWellButton
// --------------------------------------------------------------------------

ColorWellButton::ColorWellButton(QWidget* parent)
    : QWidget(parent) {
    setObjectName("ColorWell");
    setFixedSize(40, 40);
    setToolTip(ColorWellButton::tr("Left-click: Primary color, right-click: Secondary color"));
    // Right-click picks colour 2 in mousePressEvent; without this the context
    // menu event is left unconsumed and reaches QMainWindow, popping the stock
    // widget menu over the top of it.
    setContextMenuPolicy(Qt::NoContextMenu);
}

void ColorWellButton::setColors(const QColor& p, const QColor& s) {
    m_primary = p;
    m_secondary = s;
    update();
}

void ColorWellButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const int edge = width() / 3;
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

PaletteButton::PaletteButton(const QColor& col, int size, QWidget* parent)
    : QWidget(parent), m_col(col) {
    setFixedSize(size, size);
    setToolTip(col.name());
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    // The right button is handled in mousePressEvent, which picks colour 2.
    // Leaving the default policy made Qt *also* raise a context-menu event
    // that nothing consumed, so it walked up to QMainWindow and popped the
    // stock widget menu ("Layers", "App toolbar") on top of the colour pick.
    setContextMenuPolicy(Qt::NoContextMenu);
}

void PaletteButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(m_hover ? Theme::tokens().accent : Theme::tokens().divider,
                  m_hover ? 2 : 1));
    p.setBrush(m_col);
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 2, 2);
}

void PaletteButton::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton)
        emit clicked();
    else if (ev->button() == Qt::RightButton)
        emit customContextMenuRequested(ev->pos());
    ev->accept();
}

void PaletteButton::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void PaletteButton::leaveEvent(QEvent*) { m_hover = false; update(); }

// --------------------------------------------------------------------------
// MainWindow
// --------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("WPaint — Untitled"));
    resize(1280, 720);
    setMinimumSize(890, 600);

    m_stack = new LayerStack(this);
    m_undo = new QUndoStack(this);

    Layer layer;
    layer.name = tr("Layer 1");
    layer.image = QImage(kDefaultSize, QImage::Format_ARGB32_Premultiplied);
    // Transparent: the background layer below supplies the white backdrop, so
    // "clear" leaves the backdrop rather than a white plate that hides it.
    layer.image.fill(Qt::transparent);
    m_stack->replaceAll({layer}, 0);
    m_stack->addBackgroundLayer(Qt::white);

    m_canvas = new CanvasView(m_stack, m_undo, this);
    m_canvas->setColors(QColor("#000000"), QColor("#FFFFFF"));
    // A display preference is applied by repainting; nothing about the document
    // itself has to be rebuilt. What the changed-callback has to cover grows as
    // the table does, so it is one function rather than a lambda that only knows
    // about the first setting.
    Settings::setChangedCallback([this] { applySettings(); });
    applySettings();

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
    connect(m_canvas, &CanvasView::selectionContextRequested, this,
            &MainWindow::showSelectionContextMenu);
    connect(m_stack, &LayerStack::changed, this, &MainWindow::syncStatusSize);
    connect(m_undo, &QUndoStack::cleanChanged, this,
            [this](bool clean) {
                Q_UNUSED(clean);
                updateWindowTitle();
                updateEditActions();
            });

    connect(m_sizePanel, &SizeSliderPanel::sizeChanged, m_canvas,
            &CanvasView::setBrushSize);
    // Space+wheel changes the size from the canvas side; the strip is a view of
    // the canvas value, not the other way round, so it has to follow.
    connect(m_canvas, &CanvasView::brushSizeChanged, m_sizePanel,
            &SizeSliderPanel::setSize);

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
    connect(m_layersPanel, &LayersPanel::backgroundEditRequested, this,
            &MainWindow::editBackgroundColor);
    // Keep the thumbnails in step with drawing. LayerStack::changed only fires
    // for structural edits, so without this a layer preview would not update
    // until something resized, rotated or flipped the document.
    connect(m_canvas, &CanvasView::pixelsChanged, m_layersPanel, &LayersPanel::updateThumbnails);

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
    Theme::setActionIcon(newAct, "new");
    connect(newAct, &QAction::triggered, this, &MainWindow::newDocument);

    QAction* openAct = new QAction(tr("Open..."), this);
    overrideShortcut(openAct, QKeySequence::Open);
    Theme::setActionIcon(openAct, "open");
    connect(openAct, &QAction::triggered, this, &MainWindow::openDocument);

    QAction* saveAct = new QAction(tr("Save"), this);
    overrideShortcut(saveAct, QKeySequence::Save);
    Theme::setActionIcon(saveAct, "save");
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveDocument);

    QAction* saveAsAct = new QAction(tr("Save As..."), this);
    overrideShortcut(saveAsAct, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    Theme::setActionIcon(saveAsAct, "save");
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveDocumentAs);

    QAction* exitAct = new QAction(tr("Exit"), this);
    exitAct->setMenuRole(QAction::QuitRole);
    overrideShortcut(exitAct, QKeySequence::Quit);
    Theme::setActionIcon(exitAct, "close");
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);

    // Edit
    m_undoAction = new QAction(tr("Undo"), this);
    overrideShortcut(m_undoAction, QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::doUndo);
    Theme::setActionIcon(m_undoAction, "undo");
    m_redoAction = new QAction(tr("Redo"), this);
    overrideShortcut(m_redoAction, QKeySequence::Redo);
    Theme::setActionIcon(m_redoAction, "redo");
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::doRedo);
    QAction* redoAlt = new QAction(tr("Redo (alt)"), this);
    overrideShortcut(redoAlt, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    Theme::setActionIcon(redoAlt, "redo");
    connect(redoAlt, &QAction::triggered, this, &MainWindow::doRedo);
    connect(m_undo, &QUndoStack::canUndoChanged, m_undoAction, &QAction::setEnabled);
    connect(m_undo, &QUndoStack::canRedoChanged, m_redoAction, &QAction::setEnabled);
    m_redoAction->setEnabled(false);
    m_undoAction->setEnabled(false);

    m_cutAction = new QAction(tr("Cut"), this);
    overrideShortcut(m_cutAction, QKeySequence::Cut);
    Theme::setActionIcon(m_cutAction, "cut");
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::doCut);

    m_copyAction = new QAction(tr("Copy"), this);
    overrideShortcut(m_copyAction, QKeySequence::Copy);
    Theme::setActionIcon(m_copyAction, "copy");
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::doCopy);

    m_pasteAction = new QAction(tr("Paste"), this);
    overrideShortcut(m_pasteAction, QKeySequence::Paste);
    Theme::setActionIcon(m_pasteAction, "paste");
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::doPaste);

    m_selectAllAction = new QAction(tr("Select All"), this);
    overrideShortcut(m_selectAllAction, QKeySequence::SelectAll);
    Theme::setActionIcon(m_selectAllAction, "select");
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::doSelectAll);

    m_deleteAction = new QAction(tr("Delete"), this);
    overrideShortcut(m_deleteAction, QKeySequence::Delete);
    Theme::setActionIcon(m_deleteAction, "delete");
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::doDelete);

    QAction* resizeAct = new QAction(tr("Resize and Rotate..."), this);
    overrideShortcut(resizeAct, QKeySequence(Qt::CTRL | Qt::Key_E));
    Theme::setActionIcon(resizeAct, "resize");
    connect(resizeAct, &QAction::triggered, this, [this] { openResizeDialog(); });

    QAction* prefsAct = new QAction(tr("Preferences..."), this);
    overrideShortcut(prefsAct, QKeySequence(Qt::CTRL | Qt::Key_Comma));
    Theme::setActionIcon(prefsAct, "settings");
    connect(prefsAct, &QAction::triggered, this, [this] {
        SettingsDialog dlg(this);
        dlg.exec();
    });

    // View
    QAction* zoomIn = new QAction(tr("Zoom In"), this);
    overrideShortcut(zoomIn, QKeySequence::ZoomIn);
    Theme::setActionIcon(zoomIn, "zoom-in");
    connect(zoomIn, &QAction::triggered, m_canvas, &CanvasView::zoomIn);

    QAction* zoomOut = new QAction(tr("Zoom Out"), this);
    overrideShortcut(zoomOut, QKeySequence::ZoomOut);
    Theme::setActionIcon(zoomOut, "zoom-out");
    connect(zoomOut, &QAction::triggered, m_canvas, &CanvasView::zoomOut);

    QAction* zoomActual = new QAction(tr("Actual Size"), this);
    overrideShortcut(zoomActual, QKeySequence("Ctrl+0"));
    Theme::setActionIcon(zoomActual, "zoom-actual");
    connect(zoomActual, &QAction::triggered, m_canvas, &CanvasView::zoomActual);

    QAction* zoomFit = new QAction(tr("Fit to Window"), this);
    overrideShortcut(zoomFit, QKeySequence("Ctrl+9"));
    Theme::setActionIcon(zoomFit, "zoom-fit");
    connect(zoomFit, &QAction::triggered, m_canvas, &CanvasView::zoomFit);

    QAction* boundaryHandles = new QAction(tr("Show Canvas Resize Handles"), this);
    boundaryHandles->setCheckable(true);
    boundaryHandles->setChecked(Settings::showBoundaryHandles());
    m_boundaryHandlesAction = boundaryHandles;
    Theme::setActionIcon(boundaryHandles, "canvas-handles");
    connect(boundaryHandles, &QAction::toggled, m_canvas,
            &CanvasView::setBoundaryHandlesEnabled);

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
    editMenu->addSeparator();
    editMenu->addAction(prefsAct);

    QMenu* viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(zoomIn);
    viewMenu->addAction(zoomOut);
    viewMenu->addAction(zoomActual);
    viewMenu->addAction(zoomFit);
    viewMenu->addSeparator();
    viewMenu->addAction(boundaryHandles);
    viewMenu->addSeparator();

    QMenu* themeMenu = viewMenu->addMenu(Theme::icon("theme", 18), tr("Theme Preference"));
    Theme::setActionIcon(themeMenu->menuAction(), "theme");
    QActionGroup* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    auto* sysThemeAct = themeMenu->addAction(tr("Use System Theme"));
    sysThemeAct->setCheckable(true);
    auto* darkThemeAct = themeMenu->addAction(tr("Dark Mode"));
    darkThemeAct->setCheckable(true);
    auto* lightThemeAct = themeMenu->addAction(tr("Light Mode"));
    lightThemeAct->setCheckable(true);
    themeGroup->addAction(sysThemeAct);
    themeGroup->addAction(darkThemeAct);
    themeGroup->addAction(lightThemeAct);

    const Theme::Pref pref = Theme::preference();
    QAction* currentTheme =
        pref == Theme::Pref::System ? sysThemeAct
        : pref == Theme::Pref::Dark ? darkThemeAct
                                    : lightThemeAct;
    currentTheme->setChecked(true);

    connect(sysThemeAct, &QAction::triggered, this, [this] {
        setThemePreference(Theme::Pref::System);
    });
    connect(darkThemeAct, &QAction::triggered, this, [this] {
        setThemePreference(Theme::Pref::Dark);
    });
    connect(lightThemeAct, &QAction::triggered, this, [this] {
        setThemePreference(Theme::Pref::Light);
    });

    overrideShortcut(themeMenu->menuAction(),
                     QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    Theme::setModeChangedCallback([this] { syncColorWell(); });
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

    // Selection: large standalone 2x button on the far left.
    {
        QList<QWidget*> c;
auto* selBtn = toolButtonFor(ToolId::Select);
        const int big = 2 * Theme::tokens().toolbarBtn;
        const int selIcon = 2 * 20;
        selBtn->setProperty("wpBig", 1);
        selBtn->setFixedSize(big, big);
        Theme::setIcon(selBtn, ToolRegistry::spec(ToolId::Select).icon, selIcon);
        selBtn->setIconSize(QSize(selIcon, selIcon));
        c << selBtn;
        bar->addWidget(toolCluster(tr("Selection"), c));
    }
    bar->addWidget(divider());

    // Image: two rows of three (crop / flips on top, magnify / rotates below)
    // with the big resize button keeping its own spanning column.
    {
        QList<QWidget*> c;
        c << toolButtonFor(ToolId::Crop);

        auto* flipH = new QToolButton(bar);
        flipH->setToolTip(tr("Flip horizontal"));
        Theme::setIcon(flipH, "flip-horizontal");
        connect(flipH, &QToolButton::clicked, this, [this] {
            if (m_canvas->hasSelection())
                m_canvas->flipSelection(Qt::Horizontal);
            else
                m_canvas->flipCanvas(Qt::Horizontal);
        });
        c << flipH;

        auto* flipV = new QToolButton(bar);
        flipV->setToolTip(tr("Flip vertical"));
        Theme::setIcon(flipV, "flip-vertical");
        connect(flipV, &QToolButton::clicked, this, [this] {
            if (m_canvas->hasSelection())
                m_canvas->flipSelection(Qt::Vertical);
            else
                m_canvas->flipCanvas(Qt::Vertical);
        });
        c << flipV;

        auto* magnifyBtn = toolButtonFor(ToolId::Magnify);
        magnifyBtn->setToolTip(tr("Magnify (right-click zooms out)"));
        c << magnifyBtn;

        auto* rotL = new QToolButton(bar);
        rotL->setToolTip(tr("Rotate left"));
        Theme::setIcon(rotL, "rotate-left");
        connect(rotL, &QToolButton::clicked, this, [this] {
            if (m_canvas->hasSelection())
                m_canvas->rotateSelection(-90);
            else
                m_canvas->rotateCanvas(-90);
        });
        c << rotL;

        auto* rotR = new QToolButton(bar);
        rotR->setToolTip(tr("Rotate right"));
        Theme::setIcon(rotR, "rotate-right");
        connect(rotR, &QToolButton::clicked, this, [this] {
            if (m_canvas->hasSelection())
                m_canvas->rotateSelection(90);
            else
                m_canvas->rotateCanvas(90);
        });
        c << rotR;

        // The resize button belongs to the Image cluster rather than a cluster
        // of its own: splitting it off left the "Image" caption centred over
        // only the small buttons, with the resize button hanging outside
        // the text. Keeping it here lets the caption centre over the whole
        // group, and the grid gives a wpBig control a column of its own
        // spanning both rows.
        auto* resizeBtn = new QToolButton(this);
        const int big = 2 * Theme::tokens().toolbarBtn;
        const int iconPx = 2 * 20;
        resizeBtn->setProperty("wpBig", 1);
        resizeBtn->setFixedSize(big, big);
        resizeBtn->setToolTip(tr("Resize and rotate"));
        Theme::setIcon(resizeBtn, "resize", iconPx);
        resizeBtn->setIconSize(QSize(iconPx, iconPx));
        connect(resizeBtn, &QToolButton::clicked, this,
                [this] { openResizeDialog(); });
        c << resizeBtn;

        bar->addWidget(toolCluster(tr("Image"), c, 3));
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
        const int big = 2 * Theme::tokens().toolbarBtn;
        const int iconPx = 2 * 20;
        m_brushButton = new PopupButton(bar);
        m_brushButton->setObjectName("MenuButtonPopup");
        m_brushButton->setProperty("wpBig", 1);
        m_brushButton->setCheckable(true);
        m_brushButton->setPopupMode(QToolButton::DelayedPopup);
        m_brushButton->setToolTip(tr("Brush"));
        m_brushButton->setFixedSize(big, big);
        Theme::setIcon(m_brushButton, "brush", iconPx);
        m_brushButton->setIconSize(QSize(iconPx, iconPx));
        connect(m_brushButton, &QToolButton::clicked, this,
                [this] { selectTool(ToolId::Brush); });
        QMenu* brushMenu = new QMenu(m_brushButton);
        for (BrushStyle s : kBrushStyles) {
            QAction* a = brushMenu->addAction(brushStyleName(s));
            connect(a, &QAction::triggered, this, [this, s] { applyBrushStyle(s); });
        }
        m_brushButton->setMenu(brushMenu);
        bar->addWidget(toolCluster(tr("Brushes"), {m_brushButton}));
    }
    bar->addWidget(divider());

    // Shapes
    {
        const int big = 2 * Theme::tokens().toolbarBtn;
        const int iconPx = 2 * 20;
        m_shapeButton = new PopupButton(bar);
        m_shapeButton->setObjectName("MenuButtonPopup");
        m_shapeButton->setProperty("wpBig", 1);
        m_shapeButton->setCheckable(true);
        // The body re-activates the current shape; only the chevron strip opens
        // the gallery, which PopupButton handles from the pointer position.
        m_shapeButton->setPopupMode(QToolButton::DelayedPopup);
        m_shapeButton->setToolTip(tr("Shapes"));
        m_shapeButton->setFixedSize(big, big);
        Theme::setIcon(m_shapeButton, "shape-rect", iconPx);
        m_shapeButton->setIconSize(QSize(iconPx, iconPx));
        connect(m_shapeButton, &QToolButton::clicked, this, [this] {
            selectTool(m_canvas->currentShape());
        });
        QMenu* shapeMenu = new QMenu(m_shapeButton);
        QActionGroup* grp = new QActionGroup(shapeMenu);
        grp->setExclusive(true);
        for (const auto& s : specs) {
            if (!s.inShapes) continue;
            QAction* a = shapeMenu->addAction(Theme::icon(s.icon, 18), s.name);
            grp->addAction(a);
            const ToolId id = s.id;
            connect(a, &QAction::triggered, this, [this, id, a, iconPx] {
                // Through Theme::setIcon, not setIcon: the property is what the
                // checked-state re-tint reads, so bypassing it left the button
                // showing the previous shape after the next repaint.
                Theme::setIcon(m_shapeButton, ToolRegistry::spec(id).icon, iconPx);
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

        auto* styleBtn = new PopupButton(bar);
        styleBtn->setObjectName("MenuButtonPopup");
        styleBtn->setProperty("wpBig", 1);
        // No secondary function, so no split: the whole surface is one target
        // and opening the menu is all it does. The caret still shows there is
        // more to open.
        styleBtn->setSplitEnabled(false);
        styleBtn->setPopupMode(QToolButton::InstantPopup);
        styleBtn->setToolTip(tr("Shape fill pattern"));
        m_shapeStyleButton = styleBtn;
        m_shapeStyleButton->setFixedSize(big, big);
        const int styleIconPx = iconPx;
        Theme::setIcon(m_shapeStyleButton,
                       shapeStyleModeIcon(m_canvas->currentShapeStyle()), styleIconPx);
        m_shapeStyleButton->setIconSize(QSize(styleIconPx, styleIconPx));
        QMenu* styleMenu = new QMenu(m_shapeStyleButton);
        QActionGroup* styleGrp = new QActionGroup(styleMenu);
        styleGrp->setExclusive(true);
        for (ShapeStyle st : kShapeStyles) {
            QAction* a = styleMenu->addAction(Theme::icon(shapeStyleIcon(st), 18),
                                              shapeStyleName(st));
            styleGrp->addAction(a);
            connect(a, &QAction::triggered, this, [this, st, styleIconPx] {
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
        paletteHost->setFixedHeight(2 * kPaletteSize + 2);
        auto* grid = new QGridLayout(paletteHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(2);
        for (int i = 0; i < kPalette.size(); ++i) {
            const QColor col = kPalette.at(i);
            auto* b = new PaletteButton(col, kPaletteSize, paletteHost);
            connect(b, &PaletteButton::clicked, this, [this, col] {
                m_canvas->setColors(col, m_canvas->secondary());
                syncColorWell();
            });
            connect(b, &QWidget::customContextMenuRequested, this,
                    [this, col](const QPoint&) {
                        m_canvas->setColors(m_canvas->primary(), col);
                        syncColorWell();
                    });
            grid->addWidget(b, i / 10, i % 10);
        }

        // auto* wheelBtn = new QToolButton(this);
        // wheelBtn->setFixedSize(Theme::tokens().toolbarBtn, Theme::tokens().toolbarBtn);
        // wheelBtn->setToolTip(tr("Edit colors"));
        // Theme::setIcon(wheelBtn, "colorwheel", Theme::tokens().toolbarBtn - 10);
        // connect(wheelBtn, &QToolButton::clicked, this, [this] { editColor(true); });

        QList<QWidget*> colorsWidgets;
        m_well->setProperty("wpSpanRows", true);
        colorsWidgets << m_well << paletteHost;
        bar->addWidget(toolCluster(tr("Colors"), colorsWidgets));
    }
    bar->addWidget(divider());

    // // Copilot
    // {
    //     auto* copilotBtn = new QToolButton(bar);
    //     copilotBtn->setToolTip(tr("Show Copilot"));
    //     copilotBtn->setCheckable(true);
    //     copilotBtn->setChecked(false);
    //     copilotBtn->setIcon(renderedIcon("copilot", 20));
    //     connect(copilotBtn, &QToolButton::toggled, this, [this](bool on) {
    //         m_copilotDock->setVisible(on);
    //     });
    //     bar->addWidget(toolCluster(tr("Copilot"), {copilotBtn}));
    // }
    // bar->addWidget(divider());

    // Layers
    {
        auto* layersToggle = new QToolButton(bar);
        const int big = 2 * Theme::tokens().toolbarBtn;
        const int iconPx = 2 * 20;
        layersToggle->setToolTip(tr("Show layers panel"));
        layersToggle->setCheckable(true);
        layersToggle->setProperty("wpBig", 1);
        layersToggle->setContextMenuPolicy(Qt::NoContextMenu);
        connect(layersToggle, &QToolButton::toggled, this,
                [layersToggle](bool) { Theme::refreshIcon(layersToggle); });

        layersToggle->sizePolicy().setHorizontalPolicy(QSizePolicy::Fixed);
        layersToggle->setFixedSize(big, big);
        Theme::setIcon(layersToggle, "layer-stack", iconPx);
        layersToggle->setIconSize(QSize(iconPx, iconPx));
        layersToggle->setChecked(true);

        connect(layersToggle, &QToolButton::toggled, this, [this](bool on) {
            m_layersDock->setVisible(on);
        });
        // auto* addBtn = new QToolButton(bar);
        // addBtn->setToolTip(tr("Add layer"));
        // Theme::setIcon(addBtn, "layer-add");
        // connect(addBtn, &QToolButton::clicked, this, &MainWindow::addLayer);
        // auto* delBtn = new QToolButton(bar);
        // delBtn->setToolTip(tr("Delete layer"));
        // Theme::setIcon(delBtn, "layer-delete");
        //connect(delBtn, &QToolButton::clicked, this, [this] {
        //    removeLayer(m_stack->activeIndex());
        //});
        bar->addWidget(toolCluster(tr("Layers"), {layersToggle}));
    }
}

QWidget* MainWindow::toolCluster(const QString& caption,
                                 const QList<QWidget*>& controls, int cols) {
    const auto& t = Theme::tokens();
    auto* host = new QWidget(this);
    auto* v = new QVBoxLayout(host);
    v->setContentsMargins(6, 4, 6, 4);
    v->setSpacing(2);

    auto itemWidth = [](QWidget* w) {
        if (w->minimumWidth() == w->maximumWidth() && w->minimumWidth() > 0)
            return w->minimumWidth();
        return w->sizeHint().width();
    };

    // Two-row grid: the top row is `cols` standard buttons wide; a control that
    // no longer fits wraps onto the second row. Buttons render wider than the
    // toolbarBtn token (QSS padding + border), so measure a real control.
    auto* rows = new QWidget(host);
    auto* grid = new QGridLayout(rows);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(2);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    const int btn = controls.isEmpty() ? t.toolbarBtn : itemWidth(controls.first());
    const int budget = cols * btn + (cols - 1) * grid->horizontalSpacing();
    const int big = 2 * t.toolbarBtn;

    int row = 0;
    int used = 0;
    int col[2] = {0, 0};
    for (QWidget* c : controls) {
        const int w = itemWidth(c);
        if (w >= big || c->property("wpSpanRows").toBool()) {
            grid->addWidget(c, 0, col[0]++, 2, 1, Qt::AlignCenter);
            used = budget;
            continue;
        }
        if (used > 0 && (used + w > budget || col[row] >= cols)) {
            row = 1 - row;
            used = 0;
        }
        grid->addWidget(c, row, col[row]++);
        used += w;
    }

    auto* cap = new QLabel(caption, host);
    cap->setObjectName("ClusterCaption");
    cap->setAlignment(Qt::AlignHCenter);

    // Reserve a uniform two-row slot so single-item clusters match the rest.
    host->setMinimumHeight(2 * t.toolbarBtn + 2 + t.captionH + 2 + 8);

    v->addWidget(rows);
    v->addWidget(cap);
    return host;
}

QWidget* MainWindow::divider() {
    auto* d = new QWidget(this);
    d->setObjectName("ToolbarDivider");
    d->setFixedWidth(1);
    d->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return d;
}

QToolButton* MainWindow::toolButtonFor(ToolId id, int size) {
    if (QToolButton* existing = m_toolButtons.value(id, nullptr))
        return existing;
    const auto& s = ToolRegistry::spec(id);
    auto* b = new QToolButton(this);
    b->setCheckable(true);
    b->setToolTip(s.name);
    if (size > 0) {
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        b->setFixedSize(size, size);
    }
    Theme::setIcon(b, s.icon);
    b->setContextMenuPolicy(Qt::NoContextMenu);
    // Re-tint whenever the checked state flips, so the glyph follows the
    // accent fill immediately rather than only on the next theme change.
    connect(b, &QToolButton::toggled, this, [b] { Theme::refreshIcon(b); });
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

    m_scrollArea = new QScrollArea(host);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("CanvasScrollArea");
    m_scrollArea->setWidget(m_canvas);
    m_canvas->attachScrollArea(m_scrollArea);
    top->addWidget(m_scrollArea, 1);

    QWidget* viewport = m_scrollArea->viewport();
    m_sizePanel = new SizeSliderPanel(viewport);
    m_sizePanel->raise();
    viewport->installEventFilter(this);

    setCentralWidget(host);
    placeSizePanel();
}

MainWindow::~MainWindow() {
    Theme::setModeChangedCallback(nullptr);
}

void MainWindow::placeSizePanel() {
    if (!m_sizePanel || !m_scrollArea)
        return;
    const auto& t = Theme::tokens();
    const QSize vp = m_sizePanel->parentWidget()->size();
    m_sizePanel->move(t.sizePanelGap,
                      qMax(0, (vp.height() - m_sizePanel->height()) / 2));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* ev) {
    if (m_sizePanel && watched == m_scrollArea->viewport() &&
        ev->type() == QEvent::Resize)
        placeSizePanel();
    return QMainWindow::eventFilter(watched, ev);
}

void MainWindow::contextMenuEvent(QContextMenuEvent* ev) {
    // The canvas raises its own selection menu and handles that event itself.
    // Anything reaching the window is the stock widget/dock list, which offers
    // nothing useful, so it is dropped rather than shown.
    ev->ignore();
}

void MainWindow::resizeEvent(QResizeEvent* ev) {
    QMainWindow::resizeEvent(ev);
    placeSizePanel();
}

// -------------------------------------------------------------- docks -----

void MainWindow::buildDocks() {
    m_layersPanel = new LayersPanel(m_stack, this);
    m_layersDock = new QDockWidget(tr("Layers"), this);
    m_layersDock->setObjectName("LayersDock");
    m_layersDock->setWidget(m_layersPanel);
    // The panel's own caption strip is the title bar: Qt stops painting its
    // own "Layers" caption, which used to stack on top of the panel's.
    m_layersDock->setTitleBarWidget(m_layersPanel->headerWidget());
    m_layersDock->setAllowedAreas(Qt::RightDockWidgetArea |
                                  Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);

    // m_copilotPanel = new CopilotPanel(this);
    // m_copilotDock = new QDockWidget(tr("Copilot"), this);
    // m_copilotDock->setObjectName("CopilotDock");
    // m_copilotDock->setWidget(m_copilotPanel);
    // m_copilotDock->setAllowedAreas(Qt::RightDockWidgetArea);
    // addDockWidget(Qt::RightDockWidgetArea, m_copilotDock);
    // m_copilotDock->hide();
    // // stack below the layers panel
    resizeDocks({m_layersDock, /**m_copilotDock */}, {200,}, Qt::Vertical);
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
    m_zoomLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_zoomSlider = new FluentSlider(Qt::Horizontal);
    m_zoomSlider->setRange(20, 800);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(140);
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

    zoomHost->setFixedSize(h->sizeHint());
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
    for (auto it = m_toolButtons.constBegin(); it != m_toolButtons.constEnd(); ++it) {
        QToolButton* b = it.value();
        b->setChecked(it.key() == id);
        // A checked tool button sits on the accent fill, so its glyph has to
        // switch to the on-accent colour or it stays dark-on-blue and reads as
        // "nothing happened". This runs for every tool, not just the galleries.
        Theme::refreshIcon(b);
    }
    // The brush and shape galleries are not in m_toolButtons (they are split
    // buttons that pick a family, not a single tool), so they are synced by
    // hand. Both go accent while their family owns the tool, so it is obvious
    // which one is live.
    if (m_brushButton) {
        m_brushButton->setChecked(id == ToolId::Brush);
        Theme::refreshIcon(m_brushButton);
    }
    if (m_shapeButton) {
        m_shapeButton->setChecked(id >= ToolId::ShapeLine);
        Theme::refreshIcon(m_shapeButton);
    }
}



void MainWindow::applyShape(ShapeKit::Shape shape) {
    m_canvas->setShape(shape);
}

void MainWindow::applyShapeStyle(ShapeStyle style) {
    m_canvas->setShapeStyle(style);
    // Through Theme::setIcon, not setIcon: the property is what the theme's
    // re-tint pass reads, so bypassing it left the button on the old glyph.
    if (m_shapeStyleButton)
        Theme::setIcon(m_shapeStyleButton, shapeStyleModeIcon(style),
                       m_shapeStyleButton->iconSize().width());
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
    m_zoomSlider->setValue(qBound(20, qRound(zoom * 100), 800));
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
    m_canvas->bakeActiveObject();
    if (m_undo->isClean() || !Settings::confirmDiscard())
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

void MainWindow::setThemePreference(Theme::Pref pref) {
    Theme::setPreference(pref);
    // The choice is a preference, not view state: it has to survive a restart,
    // or the View menu is a three-way radio that forgets where you put it.
    SettingsValues v = Settings::values();
    v.themePreference = static_cast<int>(pref);
    Settings::apply(v);
}

void MainWindow::applySettings() {
    m_canvas->setBoundaryHandlesEnabled(Settings::showBoundaryHandles());
    m_undo->setUndoLimit(Settings::undoLimit());
    m_canvas->update();
    if (m_boundaryHandlesAction)
        m_boundaryHandlesAction->setChecked(Settings::showBoundaryHandles());
}

QSize MainWindow::documentSizeFromDefaults() const {
    const QString spec = Settings::defaultCanvasSize();
    const QStringList parts = spec.split(QLatin1Char('x'), Qt::SkipEmptyParts);
    if (parts.size() != 2)
        return kDefaultSize;
    bool okW = false, okH = false;
    const int w = parts.at(0).toInt(&okW);
    const int h = parts.at(1).toInt(&okH);
    if (!okW || !okH || w < 1 || h < 1)
        return kDefaultSize;
    return QSize(w, h);
}

void MainWindow::newDocument() {
    if (!confirmDiscard())
        return;
    m_currentPath.clear();
    m_undo->clear();
    m_stack->clear();
    Layer l;
    l.name = tr("Layer 1");
    l.image = QImage(documentSizeFromDefaults(), QImage::Format_ARGB32_Premultiplied);
    l.image.fill(Qt::transparent);
    m_stack->replaceAll({l}, 0);
    m_stack->addBackgroundLayer(Settings::defaultBackground());
    m_canvas->setColors(Settings::defaultPrimary(), Settings::defaultSecondary());
    m_canvas->setBrushSize(Settings::defaultBrushSize());
    m_canvas->setBrushStyle(static_cast<BrushStyle>(Settings::defaultBrushStyle()));
    m_canvas->setShapeStyle(static_cast<ShapeStyle>(Settings::defaultShapeStyle()));
    if (Settings::defaultShape() > 0) {
        int seen = 0;
        for (const auto& s : ToolRegistry::specs()) {
            if (!s.inShapes) continue;
            if (seen++ == Settings::defaultShape()) {
                m_canvas->setShape(static_cast<ShapeKit::Shape>(s.id));
                break;
            }
        }
    }
    syncColorWell();
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
    m_canvas->bakeActiveObject();
    m_canvas->cancelFloating();
    m_undo->clear();
    m_stack->replaceAll(loaded.layers(), loaded.activeIndex());
    // Flat images and v1 projects arrive with no backdrop of their own; give
    // them one so every document has the same shape.
    if (!m_stack->hasBackground())
        m_stack->addBackgroundLayer(Qt::white);
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
    m_canvas->bakeActiveObject();
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
        // JPEG/BMP/GIF have no alpha: composite onto the document's background
        // colour first, falling back to white when it is hidden.
        out = QImage(flat.size(), QImage::Format_RGB32);
        out.fill(m_stack->exportBackdrop());
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
    m_canvas->bakeActiveObject(); // a pending shape becomes the top command
    m_canvas->weldFloating();
    m_undo->undo();
}

void MainWindow::doRedo() {
    m_canvas->bakeActiveObject();
    m_canvas->weldFloating();
    m_undo->redo();
}

void MainWindow::doCut() {
    if (!m_canvas->hasSelection()) return;
    m_canvas->liftSelection();
    m_copied = m_canvas->copySelection();
    m_copiedTopLeft = m_canvas->selection().topLeft();
    QGuiApplication::clipboard()->setImage(m_copied);
    // Drop the floating overlay too. liftSelection erases the pixels but keeps
    // them floating at the same spot, so without this the cut is visually
    // indistinguishable from a copy -- the region just gets redrawn.
    m_canvas->commitFloatingRemoval(tr("Cut selection"));
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

void MainWindow::showSelectionContextMenu() {
    if (!m_canvas->hasSelection()) return;
    QMenu menu(this);
    menu.addAction(m_cutAction);
    menu.addAction(m_copyAction);
    menu.addAction(m_pasteAction);
    menu.addSeparator();

    const auto addIconItem = [this, &menu](const QString& text, const char* iconName,
                                            Qt::Orientation orient) {
        QAction* a = menu.addAction(Theme::icon(QLatin1String(iconName), 18), text);
        Theme::setActionIcon(a, QLatin1String(iconName), 18);
        connect(a, &QAction::triggered, this, [this, orient] {
            m_canvas->flipSelection(orient);
        });
    };
    const auto addRotateItem = [this, &menu](const QString& text, const char* iconName,
                                              qreal degrees) {
        QAction* a = menu.addAction(Theme::icon(QLatin1String(iconName), 18), text);
        Theme::setActionIcon(a, QLatin1String(iconName), 18);
        connect(a, &QAction::triggered, this, [this, degrees] {
            m_canvas->rotateSelection(degrees);
        });
    };

    addIconItem(tr("Flip horizontal"), "flip-horizontal", Qt::Horizontal);
    addIconItem(tr("Flip vertical"), "flip-vertical", Qt::Vertical);
    menu.addSeparator();
    addRotateItem(tr("Rotate 90° clockwise"), "rotate-right", 90);
    addRotateItem(tr("Rotate 90° counter-clockwise"), "rotate-left", -90);
    addRotateItem(tr("Rotate 180°"), "rotate-180", 180);
    menu.addSeparator();
    menu.addAction(m_deleteAction);
    menu.exec(QCursor::pos());
}

// ------------------------------------------------------- layer helpers ----

void MainWindow::runLayerCommand(const QString& text, std::function<void()> mutate) {
    m_canvas->bakeActiveObject();
    const QList<Layer> before = m_stack->layers();
    // Sample the active index before the mutation: addLayer() moves it to the
    // new top layer, so reading it afterwards would restore the wrong layer as
    // active when this command is undone.
    const int beforeActive = m_stack->activeIndex();
    mutate();
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, text));
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

void MainWindow::editBackgroundColor(int index) {
    if (index < 0 || index >= m_stack->count() ||
        !m_stack->layerAt(index).isBackground)
        return;
    ColorDialog dlg(m_stack->layerAt(index).backgroundColor, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QColor chosen = dlg.chosenColor();
    if (chosen == m_stack->layerAt(index).backgroundColor)
        return;
    // Structural, so it flows through runLayerCommand and is undoable.
    runLayerCommand(tr("Background color"),
                    [this, index, chosen] { m_stack->setBackgroundColor(index, chosen); });
}

void MainWindow::closeEvent(QCloseEvent* ev) {
    if (!confirmDiscard()) {
        ev->ignore();
        return;
    }
    ev->accept();
}