#include "MainWindow.h"

#include "CanvasView.h"
#include "ColorDialog.h"
#include "CopilotPanel.h"
#include "Commands.h"
#include "DrawingUtils.h"
#include "FileDropDialog.h"
#include "FluentSlider.h"
#include "FluentToast.h"
#include "Layer.h"
#include "LayerStack.h"
#include "LayersPanel.h"
#include "PalettePresets.h"
#include "ResizeDialog.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Shortcuts.h"
#include "SizeSliderPanel.h"
#include "Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <algorithm>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
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
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
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
// tinting used for monochrome icons. Its only caller is the Copilot cluster,
// which is commented out in the shell, so the build would otherwise be the one
// warning in a -Wall -Wextra tree.
[[maybe_unused]] QIcon renderedIcon(const QString& svgName, int px) {
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
    // Explicit rather than inherited: the rail turns this on for its own drag, and
    // a window that only accepts drops because a dock asked for it is a window
    // that stops accepting them if the dock is ever rebuilt without it.
    setAcceptDrops(true);

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
    //
    // The callback list outlives the window that registered into it, and with a
    // second window open either of them can be closed while the other lives on.
    // Hence the guarded pointer rather than `this`.
    QPointer<MainWindow> settingsGuard(this);
    Settings::setChangedCallback([settingsGuard] {
        if (!settingsGuard)
            return;
        settingsGuard->applySettings();
        // A rebind has to take effect with the dialog still open, or the page
        // would only be showing you what the next launch would look like.
        settingsGuard->applyShortcuts();
    });
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
    // Pristine is "no command has ever been pushed", which is the undo index
    // rather than the clean index: a document with edits in it can be undone
    // back to untouched, and it is untouched again at that point. The clean index
    // answers a different question -- "is this what is on disk" -- which is not
    // what anything here is asking.
    connect(m_undo, &QUndoStack::indexChanged, this,
            [this](int index) { m_documentPristine = index <= 0; });
    // The clipboard belongs to whatever is on screen when you look, and the image
    // sources that matter are other applications -- a PNG copied in a file
    // manager, "Copy image" on a page -- so the Edit menu has to be told when
    // something new arrives rather than only when this window copies something.
    if (QClipboard* board = QGuiApplication::clipboard()) {
        connect(board, &QClipboard::dataChanged, this,
                [this] { updateEditActions(); });
        connect(board, &QClipboard::changed, this, [this] { updateEditActions(); });
    }

    connect(m_sizePanel, &SizeSliderPanel::sizeChanged, m_canvas,
            &CanvasView::setBrushSize);
    // Space+wheel changes the size from the canvas side; the strip is a view of
    // the canvas value, not the other way round, so it has to follow.
    connect(m_canvas, &CanvasView::brushSizeChanged, m_sizePanel,
            &SizeSliderPanel::setSize);
    connect(m_canvas, &CanvasView::editableLayerRequired, this, [this] {
        showToast(tr("No editable layer is selected. A group holds layers, it is "
                     "not one you can draw on."));
    });

    connect(m_layersPanel, &LayersPanel::activeRequested, this,
            [this](int index) { m_canvas->setActiveLayer(index); });
    connect(m_layersPanel, &LayersPanel::visibilityRequested, this,
            &MainWindow::toggleLayerVisibility);
    connect(m_layersPanel, &LayersPanel::moveRequested, this,
            [this](int from, int to, bool intoFolder) {
                if (!m_stack->canMoveSpan(from, to, intoFolder)) {
                    showToast(tr("A layer cannot be moved there."));
                    return;
                }
                runLayerCommand(intoFolder ? tr("Move into group") : tr("Move layer"),
                                [this, from, to, intoFolder] {
                                    m_stack->moveSpan(from, to, intoFolder);
                                });
            });
    connect(m_layersPanel, &LayersPanel::moveRefused, this,
            [this] { showToast(tr("Only a block of neighbouring layers can be moved.")); });
    connect(m_layersPanel, &LayersPanel::addRequested, this, &MainWindow::addLayer);
    connect(m_layersPanel, &LayersPanel::removeSelectionRequested, this,
            [this](QList<int> sel) { removeLayers(sel); });
    connect(m_layersPanel, &LayersPanel::renameFinished, this,
            &MainWindow::renameLayers);
    connect(m_layersPanel, &LayersPanel::folderRequested, this,
            [this](QList<int> sel) { groupLayers(sel); });
    connect(m_layersPanel, &LayersPanel::foldedRequested, this,
            [this](int index, bool folded) {
                runLayerCommand(tr("Collapse group"),
                                [this, index, folded] { m_stack->setFolded(index, folded); });
            });
    connect(m_layersPanel, &LayersPanel::backgroundEditRequested, this,
            &MainWindow::editBackgroundColor);
    connect(m_layersPanel, &LayersPanel::contextRequested, this,
            [this](QList<int> sel, const QPoint& at) { showLayerContextMenu(sel, at); });
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
    // Every shortcut in the app is bound through here, by id rather than by
    // sequence: the Shortcuts page stores an override against the id, so a
    // binding can be changed at runtime without the page holding a QAction, and
    // a menu-less action -- the flip buttons, the shape gallery -- gets a
    // binding for free. WindowShortcut is deliberate: it makes a *disabled*
    // action block the key rather than letting it fall through to the focused
    // child, which is what makes Cut work on a selection that has nothing to
    // cut in the usual sense.
    auto overrideShortcut = [this](QAction* a, const QKeySequence& seq) {
        a->setShortcut(seq);
        a->setShortcutContext(Qt::WindowShortcut);
    };
    m_shortcutActions = new QHash<QString, QAction*>;
    auto bind = [this, &overrideShortcut](QAction* a, const QString& id) {
        if (const Shortcuts::Entry* e = Shortcuts::find(id)) {
            overrideShortcut(a, Shortcuts::effective(*e));
            m_shortcutActions->insert(id, a);
        }
        return a;
    };

    // File
    QAction* newAct = new QAction(tr("New"), this);
    bind(newAct, QStringLiteral("file.new"));
    Theme::setActionIcon(newAct, "new");
    connect(newAct, &QAction::triggered, this, &MainWindow::newDocument);

    QAction* openAct = new QAction(tr("Open..."), this);
    bind(openAct, QStringLiteral("file.open"));
    Theme::setActionIcon(openAct, "open");
    connect(openAct, &QAction::triggered, this, &MainWindow::openDocument);

    QAction* saveAct = new QAction(tr("Save"), this);
    bind(saveAct, QStringLiteral("file.save"));
    Theme::setActionIcon(saveAct, "save");
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveDocument);

    QAction* saveAsAct = new QAction(tr("Save As..."), this);
    bind(saveAsAct, QStringLiteral("file.saveAs"));
    Theme::setActionIcon(saveAsAct, "save");
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveDocumentAs);

    QAction* exitAct = new QAction(tr("Exit"), this);
    exitAct->setMenuRole(QAction::QuitRole);
    bind(exitAct, QStringLiteral("file.exit"));
    Theme::setActionIcon(exitAct, "close");
    connect(exitAct, &QAction::triggered, this, &MainWindow::close);

    // Edit
    m_undoAction = new QAction(tr("Undo"), this);
    bind(m_undoAction, QStringLiteral("edit.undo"));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::doUndo);
    Theme::setActionIcon(m_undoAction, "undo");
    m_redoAction = new QAction(tr("Redo"), this);
    bind(m_redoAction, QStringLiteral("edit.redo"));
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
    bind(m_cutAction, QStringLiteral("edit.cut"));
    Theme::setActionIcon(m_cutAction, "cut");
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::doCut);

    m_copyAction = new QAction(tr("Copy"), this);
    bind(m_copyAction, QStringLiteral("edit.copy"));
    Theme::setActionIcon(m_copyAction, "copy");
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::doCopy);

    m_pasteAction = new QAction(tr("Paste"), this);
    bind(m_pasteAction, QStringLiteral("edit.paste"));
    Theme::setActionIcon(m_pasteAction, "paste");
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::doPaste);

    m_selectAllAction = new QAction(tr("Select All"), this);
    bind(m_selectAllAction, QStringLiteral("edit.selectAll"));
    Theme::setActionIcon(m_selectAllAction, "select");
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::doSelectAll);

    m_deleteAction = new QAction(tr("Delete"), this);
    bind(m_deleteAction, QStringLiteral("edit.delete"));
    Theme::setActionIcon(m_deleteAction, "delete");
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::doDelete);

    QAction* resizeAct = new QAction(tr("Resize and Rotate..."), this);
    bind(resizeAct, QStringLiteral("image.resize"));
    Theme::setActionIcon(resizeAct, "resize");
    connect(resizeAct, &QAction::triggered, this, [this] { openResizeDialog(); });

    QAction* prefsAct = new QAction(tr("Preferences..."), this);
    bind(prefsAct, QStringLiteral("edit.preferences"));
    Theme::setActionIcon(prefsAct, "settings");
    connect(prefsAct, &QAction::triggered, this, [this] {
        SettingsDialog dlg(this);
        dlg.exec();
    });

    // ---- key-only actions ----
    // These have no menu row of their own: the toolbar already shows them, and a
    // menu entry that only exists to carry a letter is clutter. They still get an
    // id, so they appear on the Shortcuts page and can be rebound.
    auto toolShortcut = [this, &bind](ToolId tool, const char* id) {
        auto* a = new QAction(tr("Select the %1 tool").arg(ToolRegistry::spec(tool).name), this);
        connect(a, &QAction::triggered, this, [this, tool] { selectTool(tool); });
        bind(a, QString::fromLatin1(id));
    };
    toolShortcut(ToolId::Select, "tool.select");
    toolShortcut(ToolId::Pencil, "tool.pencil");
    toolShortcut(ToolId::Fill, "tool.fill");
    toolShortcut(ToolId::Text, "tool.text");
    toolShortcut(ToolId::Eraser, "tool.eraser");
    toolShortcut(ToolId::Eyedropper, "tool.eyedropper");
    toolShortcut(ToolId::Crop, "image.crop");
    toolShortcut(ToolId::Magnify, "image.magnify");
    toolShortcut(ToolId::Brush, "brush.gallery");

    auto galleryShortcut = [this, &bind](const QString& label, const char* id,
                                         const std::function<void()>& fn) {
        auto* a = new QAction(label, this);
        connect(a, &QAction::triggered, this, fn);
        bind(a, QString::fromLatin1(id));
    };
    galleryShortcut(tr("Shapes"), "shape.gallery",
                    [this] { selectTool(m_canvas->currentShape()); });

    // ---- image transforms, shared by the toolbar and the keyboard ----
    // The toolbar buttons used to own these lambdas; the actions own them now so
    // a shortcut and a click cannot drift apart.
    auto transform = [this, &bind](const QString& label, const char* id,
                                   const std::function<void()>& fn) {
        auto* a = new QAction(label, this);
        connect(a, &QAction::triggered, this, fn);
        m_imageTransforms.insert(QString::fromLatin1(id), fn);
        bind(a, QString::fromLatin1(id));
        return a;
    };
    auto flip = [this](Qt::Orientation o) {
        if (m_canvas->hasSelection())
            m_canvas->flipSelection(o);
        else
            m_canvas->flipCanvas(o);
    };
    transform(tr("Flip horizontal"), "image.flipH",
               [this, flip] { flip(Qt::Horizontal); });
    transform(tr("Flip vertical"), "image.flipV",
               [this, flip] { flip(Qt::Vertical); });
    transform(tr("Rotate left 90 degrees"), "image.rotateLeft", [this] {
        if (m_canvas->hasSelection())
            m_canvas->rotateSelection(-90);
        else
            m_canvas->rotateCanvas(-90);
    });
    transform(tr("Rotate right 90 degrees"), "image.rotateRight", [this] {
        if (m_canvas->hasSelection())
            m_canvas->rotateSelection(90);
        else
            m_canvas->rotateCanvas(90);
    });
    // Deselect has no toolbar button but is a normal Edit-menu command.
    {
        auto* a = new QAction(tr("Deselect"), this);
        connect(a, &QAction::triggered, this, [this] {
            m_canvas->clearSelection();
            updateEditActions();
        });
        bind(a, QStringLiteral("edit.deselect"));
    }
    {
        auto* a = new QAction(tr("Swap color 1 and color 2"), this);
        connect(a, &QAction::triggered, this, [this] {
            m_canvas->setColors(m_canvas->secondary(), m_canvas->primary());
            syncColorWell();
        });
        bind(a, QStringLiteral("color.swap"));
    }
    // The fill mode cycles forward from wherever it is and wraps, so the key
    // always means "the next one" rather than a fixed target.
    auto cycleFillMode = [this, &bind](const char* id, int step) {
        auto* a = new QAction(step > 0 ? tr("Shape fill mode: next")
                                       : tr("Shape fill mode: previous"),
                              this);
        connect(a, &QAction::triggered, this, [this, step] {
            const int count = int(kShapeStyles.size());
            const int next = (int(m_canvas->currentShapeStyle()) + step + count) % count;
            applyShapeStyle(kShapeStyles.at(next));
            if (m_shapeStyleMenu)
                m_shapeStyleMenu->actions().at(next)->setChecked(true);
        });
        bind(a, QString::fromLatin1(id));
    };
    cycleFillMode("shape.fillModeNext", 1);
    cycleFillMode("shape.fillModePrev", -1);

    auto stepBrush = [this, &bind](const char* id, int delta) {
        auto* a = new QAction(delta > 0 ? tr("Brush size: larger")
                                        : tr("Brush size: smaller"),
                              this);
        connect(a, &QAction::triggered, this,
                [this, delta] { m_canvas->setBrushSize(m_canvas->brushSize() + delta); });
        bind(a, QString::fromLatin1(id));
    };
    stepBrush("brush.larger", 1);
    stepBrush("brush.smaller", -1);

    auto stepShape = [this, &bind](const char* id, int step) {
        auto* a = new QAction(step > 0 ? tr("Next shape") : tr("Previous shape"), this);
        connect(a, &QAction::triggered, this, [this, step] {
            QList<ToolId> shapes;
            for (const auto& sp : ToolRegistry::specs())
                if (sp.inShapes)
                    shapes << sp.id;
            if (shapes.isEmpty()) return;
            int at = shapes.indexOf(m_canvas->currentShape());
            if (at < 0) at = 0;
            const int next = (at + step + shapes.size()) % shapes.size();
            applyShape(static_cast<ShapeKit::Shape>(shapes.at(next)));
            selectTool(shapes.at(next));
        });
        bind(a, QString::fromLatin1(id));
    };
    stepShape("shape.next", 1);
    stepShape("shape.previous", -1);

    buildLayerActions(bind);

    // View
    QAction* zoomIn = new QAction(tr("Zoom In"), this);
    bind(zoomIn, QStringLiteral("view.zoomIn"));
    Theme::setActionIcon(zoomIn, "zoom-in");
    connect(zoomIn, &QAction::triggered, m_canvas, &CanvasView::zoomIn);

    QAction* zoomOut = new QAction(tr("Zoom Out"), this);
    bind(zoomOut, QStringLiteral("view.zoomOut"));
    Theme::setActionIcon(zoomOut, "zoom-out");
    connect(zoomOut, &QAction::triggered, m_canvas, &CanvasView::zoomOut);

    QAction* zoomActual = new QAction(tr("Actual Size"), this);
    bind(zoomActual, QStringLiteral("view.actualSize"));
    Theme::setActionIcon(zoomActual, "zoom-actual");
    connect(zoomActual, &QAction::triggered, m_canvas, &CanvasView::zoomActual);

    QAction* zoomFit = new QAction(tr("Fit to Window"), this);
    bind(zoomFit, QStringLiteral("view.fit"));
    Theme::setActionIcon(zoomFit, "zoom-fit");
    connect(zoomFit, &QAction::triggered, m_canvas, &CanvasView::zoomFit);

    QAction* boundaryHandles = new QAction(tr("Show Canvas Resize Handles"), this);
    boundaryHandles->setCheckable(true);
    boundaryHandles->setChecked(Settings::showBoundaryHandles());
    m_boundaryHandlesAction = boundaryHandles;
    bind(boundaryHandles, QStringLiteral("view.boundaryHandles"));
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

    bind(themeMenu->menuAction(), QStringLiteral("view.theme"));
    QPointer<MainWindow> modeGuard(this);
    Theme::setModeChangedCallback([modeGuard] {
        if (modeGuard)
            modeGuard->syncColorWell();
    });
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
        connect(flipH, &QToolButton::clicked, this,
                [this] { m_imageTransforms.value(QStringLiteral("image.flipH"))(); });
        c << flipH;

        auto* flipV = new QToolButton(bar);
        flipV->setToolTip(tr("Flip vertical"));
        Theme::setIcon(flipV, "flip-vertical");
        connect(flipV, &QToolButton::clicked, this,
                [this] { m_imageTransforms.value(QStringLiteral("image.flipV"))(); });
        c << flipV;

        auto* magnifyBtn = toolButtonFor(ToolId::Magnify);
        magnifyBtn->setToolTip(tr("Magnify (right-click zooms out)"));
        c << magnifyBtn;

        auto* rotL = new QToolButton(bar);
        rotL->setToolTip(tr("Rotate left"));
        Theme::setIcon(rotL, "rotate-left");
        connect(rotL, &QToolButton::clicked, this,
                [this] { m_imageTransforms.value(QStringLiteral("image.rotateLeft"))(); });
        c << rotL;

        auto* rotR = new QToolButton(bar);
        rotR->setToolTip(tr("Rotate right"));
        Theme::setIcon(rotR, "rotate-right");
        connect(rotR, &QToolButton::clicked, this,
                [this] { m_imageTransforms.value(QStringLiteral("image.rotateRight"))(); });
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
        m_shapeStyleMenu = styleMenu;

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

        m_paletteHost = new QWidget(this);
        m_paletteHost->setObjectName("PaletteHost");
        m_paletteHost->setFixedHeight(2 * kPaletteSize + 2);
        buildPaletteGrid();

        // auto* wheelBtn = new QToolButton(this);
        // wheelBtn->setFixedSize(Theme::tokens().toolbarBtn, Theme::tokens().toolbarBtn);
        // wheelBtn->setToolTip(tr("Edit colors"));
        // Theme::setIcon(wheelBtn, "colorwheel", Theme::tokens().toolbarBtn - 10);
        // connect(wheelBtn, &QToolButton::clicked, this, [this] { editColor(true); });

        QList<QWidget*> colorsWidgets;
        m_well->setProperty("wpSpanRows", true);
        colorsWidgets << m_well << m_paletteHost;
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
        m_layersToggle = layersToggle;

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

// The QPointer guards in the two callbacks make this unnecessary, and it was
// never right anyway: it cleared the *one* callback slot, which with a list
// means clearing the first entry rather than this window's.
MainWindow::~MainWindow() = default;

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

// ------------------------------------------------------------- drops ------

// What a drop is carrying, if it is something this app can use.
//
// A file on disk comes first, over pixels the drag is carrying: a file can be
// reopened, put in a project, or copied somewhere else, and pixels cannot. The
// app's own layer drag is the one payload to refuse outright -- it is aimed at
// the rail, and letting the window answer for it would move layers every time
// somebody dropped one on empty space.
static const char kLayerDragFormat[] = "application/x-wpaint-layers";

struct DroppedImage {
    QImage image;
    QString path; // empty when the drag carried pixels rather than a file
};

static bool imageFromDrop(const QMimeData* mime, DroppedImage* out) {
    if (!mime || mime->hasFormat(QLatin1String(kLayerDragFormat)))
        return false;
    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            const QString path = url.toLocalFile();
            if (path.isEmpty())
                continue;
            QImageReader reader(path);
            const QImage img = reader.read();
            if (img.isNull())
                continue;
            out->path = path;
            out->image = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            return true;
        }
    }
    if (mime->hasImage()) {
        const QImage img = mime->imageData().value<QImage>();
        if (!img.isNull()) {
            out->image = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            return true;
        }
    }
    return false;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* ev) {
    DroppedImage probe;
    if (imageFromDrop(ev->mimeData(), &probe))
        ev->acceptProposedAction();
    else
        ev->ignore();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* ev) {
    DroppedImage probe;
    if (imageFromDrop(ev->mimeData(), &probe))
        ev->acceptProposedAction();
    else
        ev->ignore();
}

void MainWindow::dropEvent(QDropEvent* ev) {
    if (handleDroppedImage(ev->mimeData(), ev->position()))
        ev->acceptProposedAction();
    else
        ev->ignore();
}

// Everything a drop of `mime` at `windowPos` (window coordinates) comes to.
// Public because Qt delivers real drops through the drag manager and will not
// route a synthetic one to a widget, so the routing -- which is all of the
// decision-making here -- would otherwise not be reachable by a test at all.
bool MainWindow::handleDroppedImage(const QMimeData* mime, const QPointF& windowPos) {
    DroppedImage dropped;
    if (!imageFromDrop(mime, &dropped))
        return false;

    // Where the pointer is over the document, in image coordinates. The canvas
    // widget is larger than the viewport on purpose -- it can be panned
    // off-centre -- so "over the document" is a question and not an assumption.
    const QPointF atCanvas = QPointF(m_canvas->mapFrom(this, windowPos.toPoint()));
    const QSize doc = m_stack->size();
    const bool inside = !doc.isEmpty() && m_canvas->rect().contains(atCanvas.toPoint()) &&
                        m_canvas->toImage(atCanvas).x() >= 0 &&
                        m_canvas->toImage(atCanvas).y() >= 0 &&
                        m_canvas->toImage(atCanvas).x() < doc.width() &&
                        m_canvas->toImage(atCanvas).y() < doc.height();

    // A canvas nobody has drawn on needs no question: the preference decides.
    if (isDocumentPristine()) {
        if (Settings::fileDropAction() ==
            static_cast<int>(FileDropAction::OpenAsNew)) {
            if (dropped.path.isEmpty()) {
                // Pixels with no file behind them cannot become a document, so
                // the other answer is the only one there is.
                placeImageOnCanvas(dropped.image, atCanvas, inside);
                return true;
            }
            return openFile(dropped.path, /*hideBackground=*/true);
        }
        placeImageOnCanvas(dropped.image, atCanvas, inside);
        return true;
    }

    // Something is on the canvas. Ask, because the three answers are not
    // variations on one: one of them throws the work away.
    FileDropDialog dlg(dropped.path.isEmpty() ? tr("The dropped image") : dropped.path, this);
    dlg.exec();
    switch (dlg.choice()) {
    case FileDropDialog::Choice::None:
        return false; // cancelled: the drop does nothing, and that is an answer
    case FileDropDialog::Choice::DiscardAndOpen:
        if (dropped.path.isEmpty()) {
            // Nothing to open. Placing it is the closest thing to what was
            // asked for that does not lose the canvas.
            placeImageOnLayer(dropped.image);
            return true;
        }
        return openFile(dropped.path, /*hideBackground=*/true);
    case FileDropDialog::Choice::NewWindow:
        if (dropped.path.isEmpty()) {
            showToast(tr("An image with no file behind it cannot open in a new window."));
            return false;
        }
        openInNewWindow(dropped.path);
        return true;
    case FileDropDialog::Choice::PlaceToLayer:
        placeImageOnLayer(dropped.image);
        return true;
    }
    return false;
}

void MainWindow::placeImageOnCanvas(const QImage& img, const QPointF& atCanvas,
                                    bool inside) {
    if (img.isNull())
        return;
    const QSize doc = m_stack->size();
    const QPointF atImage = m_canvas->toImage(atCanvas);
    const Draw::DropPlacement where = Draw::placeDroppedImage(doc, img.size(), atImage, inside);
    // Whatever was half-drawn belongs to the image underneath, and the canvas has
    // to be its old size before the placement is measured against it.
    m_canvas->bakeActiveObject();
    if (where.canvas != doc)
        m_canvas->growCanvasTo(where.canvas);
    m_canvas->pasteFloating(img, where.topLeft, tr("Place image"));
    // The Select tool is what makes a floating object draggable and scalable, so
    // a dropped image arrives with it -- selected, and ready to be moved.
    selectTool(ToolId::Select);
    updateEditActions();
}

void MainWindow::placeImageOnLayer(const QImage& img) {
    if (img.isNull())
        return;
    // Through the same rule as Add layer, so a dropped image lands where a new
    // layer would rather than in a place of its own.
    Layer l;
    l.name = m_stack->nextName(tr("Layer"));
    l.image = img;
    int at = -1;
    runLayerCommand(tr("Add image layer"), [this, &l, &at] {
        at = m_stack->addLayer(m_stack->newEntryIndex(
                 static_cast<NewLayerPlacement>(Settings::newLayerPlacement()),
                 m_layersPanel->selectedIndices().isEmpty()
                     ? m_stack->activeIndex()
                     : m_layersPanel->selectedIndices().first(),
                 Settings::newLayersStayInGroup()),
             l);
        if (at >= 0)
            m_stack->setActiveIndex(at);
    });
    if (at < 0)
        return;
    m_layersPanel->setActiveLayer(at);
    m_layersPanel->setSelection({at});
    updateWindowTitle();
}

void MainWindow::openInNewWindow(const QString& file) {
    if (file.isEmpty())
        return;
    // A second window, not a second process: it has its own layer stack and its
    // own undo history, which is the whole point of asking.
    auto* win = new MainWindow;
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->resize(size());
    win->show();
    // Loaded after it is on screen. openFile() asks about unsaved work first, and
    // in a window that was created a moment ago there is none to ask about.
    win->openFile(file);
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
    buildLayerClipboardActions();

    // The toast floats over the whole window rather than the canvas: it is
    // about the document, not about where the pointer is.
    m_toast = new FluentToast(this);
    m_toast->hide();
    m_toast->lower();
}

// The rail's own Ctrl+C / X / V. Scoped to the panel so they cannot fight the
// image clipboard: the same three keys mean "copy the picture" everywhere else
// in the app, and a shortcut that changed meaning depending on which widget
// last had focus would be worse than no shortcut at all.
void MainWindow::buildLayerClipboardActions() {
    auto scoped = [this](const QString& text, const QKeySequence& keys,
                         const std::function<void()>& fn) {
        auto* a = new QAction(text, this);
        a->setShortcut(keys);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, fn);
        m_layersPanel->addAction(a);
        return a;
    };
    scoped(tr("Copy layers"), QKeySequence(QKeySequence::Copy),
           [this] { copyLayers(m_layersPanel->selectedIndices(), false); });
    scoped(tr("Cut layers"), QKeySequence(QKeySequence::Cut),
           [this] { copyLayers(m_layersPanel->selectedIndices(), true); });
    m_pasteAboveAction =
        scoped(tr("Paste layers above"), QKeySequence(QKeySequence::Paste),
               [this] { pasteLayers(true); });
    m_pasteBelowAction =
        scoped(tr("Paste layers below"), QKeySequence(QKeySequence::Paste),
               [this] { pasteLayers(false); });
    m_pasteAboveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_pasteBelowAction->setEnabled(false);
}

// The rail's context menu. One menu for both cases, because the operations are
// the same list; what differs is which of them make sense, and a menu that
// greys out three entries to say "not now" is a worse answer than a shorter one.
void MainWindow::showLayerContextMenu(const QList<int>& selection, const QPoint& at) {
    if (selection.isEmpty()) return;
    const bool one = selection.size() == 1;
    const bool hasClipboard = !m_layerClipboard.isEmpty();
    const bool canPaste = hasClipboard && one;
    const bool canGroup = !one && m_stack->resolveSelection(selection).size() == 1;
    const bool canDelete = m_stack->canRemoveAny(selection);

    QMenu menu(this);
    QAction* copyA = menu.addAction(tr("Copy"));
    copyA->setShortcut(QKeySequence(QKeySequence::Copy));
    QAction* cutA = menu.addAction(tr("Cut"));
    cutA->setShortcut(QKeySequence(QKeySequence::Cut));
    menu.addSeparator();
    QAction* pasteAbove = menu.addAction(tr("Paste Above"));
    QAction* pasteBelow = menu.addAction(tr("Paste Below"));
    menu.addSeparator();
    QAction* renameA = menu.addAction(tr("Rename"));
    renameA->setShortcut(Qt::Key_F2);
    QAction* groupA = menu.addAction(tr("Group Layers"));
    menu.addSeparator();
    QAction* mergeDownA = menu.addAction(tr("Merge Down"));
    QAction* mergeSelectedA = menu.addAction(tr("Merge Selected"));
    menu.addSeparator();
    QAction* deleteA = menu.addAction(tr("Delete"));

    // With several layers picked there is no single row for a paste to anchor
    // to -- above which one? -- so those two are refused rather than guessed at.
    pasteAbove->setEnabled(canPaste);
    pasteBelow->setEnabled(canPaste);
    groupA->setEnabled(canGroup);
    // Merge Down is a question about one layer, and Merge Selected about a
    // block, so each is only ever offered to the selection it can answer for.
    const bool canMergeDown = one && m_stack->canMergeDown(selection.first());
    mergeDownA->setEnabled(canMergeDown);
    mergeSelectedA->setEnabled(!one && m_stack->canMergeSelection(selection));
    if (!canMergeDown && one)
        mergeDownA->setToolTip(tr("There is nothing below this layer to merge into"));
    deleteA->setEnabled(canDelete);
    if (!canDelete)
        deleteA->setToolTip(tr("A document keeps at least one layer"));

    QAction* chosen = menu.exec(at);
    if (!chosen) return;

    if (chosen == copyA) copyLayers(selection, false);
    else if (chosen == cutA) copyLayers(selection, true);
    else if (chosen == pasteAbove) pasteLayers(true);
    else if (chosen == pasteBelow) pasteLayers(false);
    else if (chosen == deleteA) removeLayers(selection);
    else if (chosen == groupA) groupLayers(selection);
    else if (chosen == mergeDownA) mergeLayerDown(selection.first());
    else if (chosen == mergeSelectedA) mergeLayers(selection);
    else if (chosen == renameA) {
        // Rename acts on the last row the user touched, then carries the name
        // across the rest of the selection.
        const int last = m_layersPanel->lastSelectedIndex();
        const int target = last >= 0 && selection.contains(last) ? last : selection.first();
        m_layersPanel->beginRenameAt(target);
    }
}

void MainWindow::showToast(const QString& message) {
    if (!m_toast) {
        m_toast = new FluentToast(this);
        m_toast->hide();
    }
    m_toast->raise();
    m_toast->show(message);
}

void MainWindow::copyLayers(const QList<int>& selection, bool cut) {
    const QList<QList<int>> runs = m_stack->resolveSelection(selection);
    if (runs.isEmpty())
        return;
    // Topmost first: the rail reads down, and a paste that comes back in the
    // order it was copied is the one people expect.
    m_layerClipboard.clear();
    for (const QList<int>& run : runs) {
        for (int index : run)
            m_layerClipboard.append(m_stack->layerAt(index));
    }
    // A copied background would arrive as a second backdrop, which the model
    // refuses and the user cannot see. It is not a layer, so it does not copy.
    m_layerClipboard.removeIf([](const Layer& l) { return l.isBackground; });
    m_layerClipboard.removeIf([](const Layer& l) { return l.isFolder; });
    if (m_pasteAboveAction) {
        m_pasteAboveAction->setEnabled(!m_layerClipboard.isEmpty());
        m_pasteBelowAction->setEnabled(!m_layerClipboard.isEmpty());
    }
    if (cut)
        removeLayers(selection);
}

void MainWindow::pasteLayers(bool above) {
    if (m_layerClipboard.isEmpty())
        return;
    const QList<int> selection = m_layersPanel->selectedIndices();
    if (selection.isEmpty())
        return;
    const int top = selection.first();
    const int bottom = selection.last();
    // Above the selection's top, below its bottom. Both anchor to the
    // selection rather than to the active row, so a five-layer paste lands
    // where the five rows the user picked are.
    const int at = above ? top : bottom + 1;

    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    m_canvas->bakeActiveObject();
    QList<int> inserted;
    for (int k = 0; k < m_layerClipboard.size(); ++k) {
        Layer copy = m_layerClipboard.at(k);
        copy.isBackground = false;
        copy.name = m_stack->nextName(copy.name);
        m_stack->addLayer(at + k, copy);
        inserted << at + k;
    }
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive,
                                         tr(above ? "Paste layers above"
                                                  : "Paste layers below")));
    m_layersPanel->setSelection(inserted);
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

// The image on the clipboard, or a null one.
//
// QClipboard::image() is the accessor, and it is deliberately the only one: it
// decodes whatever the owner published, so a PNG copied out of a file manager
// and "Copy image" on a web page both come through it without the app knowing or
// caring which mime types they used. Asking mimeData() what is on the clipboard
// would be more explicit and less portable -- some platform plugins hand back a
// mime object that cannot answer -- and the answer would not change what happens
// next. The pixmap fallback is for an owner that publishes a pixmap and nothing
// else, which some toolkits still do.
static QImage imageFromClipboard() {
    const QClipboard* board = QGuiApplication::clipboard();
    const QImage img = board->image();
    if (!img.isNull())
        return img;
    const QPixmap pm = board->pixmap();
    return pm.isNull() ? QImage() : pm.toImage();
}

void MainWindow::updateEditActions() {
    const bool sel = m_canvas->hasSelection();
    m_cutAction->setEnabled(sel);
    m_copyAction->setEnabled(sel);
    m_deleteAction->setEnabled(sel || m_canvas->floatingActive());
    // From the clipboard rather than always on. A paste action that is live with
    // an empty clipboard is a menu item that does nothing when you click it, and
    // the image sources that matter are other applications: a PNG copied out of
    // a file manager, or "Copy image" on a page.
    m_pasteAction->setEnabled(!m_copied.isNull() || !imageFromClipboard().isNull());
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
    if (r == QMessageBox::Save && !saveDocument())
        // The Save As dialog was cancelled, so nothing was written. Returning
        // true here would then go on to throw the document away, which is the
        // one thing the question was asking about.
        return false;
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

// The toolbar palette is a preference, so the grid is built from the table and
// rebuilt when the table changes. It is twenty small widgets rather than one
// painted strip because each swatch already knows how to hover, show its hex and
// take a right-click as "set color 2".
void MainWindow::buildPaletteGrid() {
    if (!m_paletteHost)
        return;
    if (auto* old = m_paletteHost->layout()) {
        while (old->count() > 0) {
            QLayoutItem* item = old->takeAt(0);
            if (QWidget* w = item->widget())
                w->deleteLater();
            delete item;
        }
        delete old;
    }
    auto* grid = new QGridLayout(m_paletteHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    const QVector<QColor> colors = PalettePresets::effective(
        Settings::palettePreset(), Settings::paletteCustom());
    for (int i = 0; i < colors.size(); ++i) {
        const QColor col = colors.at(i);
        auto* b = new PaletteButton(col, kPaletteSize, m_paletteHost);
        connect(b, &PaletteButton::clicked, this, [this, col] {
            m_canvas->setColors(col, m_canvas->secondary());
            syncColorWell();
        });
        connect(b, &QWidget::customContextMenuRequested, this, [this, col](const QPoint&) {
            m_canvas->setColors(m_canvas->primary(), col);
            syncColorWell();
        });
        grid->addWidget(b, i / PalettePresets::kColumns, i % PalettePresets::kColumns);
    }
}

void MainWindow::applyShortcuts() {
    if (!m_shortcutActions)
        return;
    for (auto it = m_shortcutActions->constBegin(); it != m_shortcutActions->constEnd();
         ++it) {
        const Shortcuts::Entry* e = Shortcuts::find(it.key());
        if (!e)
            continue;
        it.value()->setShortcut(Shortcuts::effective(*e));
    }
}

void MainWindow::applySettings() {
    buildPaletteGrid();
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
    // Whatever was floating, selected or half-drawn belongs to the document being
    // left. It used to survive: a new image could open with a selection, and with
    // the pixels of a pasted object still floating over it from the last one.
    m_canvas->bakeActiveObject();
    m_canvas->cancelFloating();
    m_canvas->clearSelection();
    Layer l;
    l.name = tr("Layer 1");
    l.image = QImage(documentSizeFromDefaults(), QImage::Format_ARGB32_Premultiplied);
    l.image.fill(Qt::transparent);
    m_stack->replaceAll({l}, 0);
    m_stack->addBackgroundLayer(Settings::defaultBackground());
    // The one path that makes a document pristine: never saved, nothing done to it.
    m_documentPristine = true;
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
    // The save question comes after the file is chosen, not before: asking
    // "save your work?" and then having the Open dialog cancelled leaves the
    // question hanging over a document nothing happened to.
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif);;"
           "WPaint Project (*.wpa);;All files (*)"));
    if (file.isEmpty())
        return;
    openFile(file);
}

// Opening is one thing with three callers -- the File menu, a file dropped on the
// window, and a second window being told what to open -- so it is one function
// and not three copies of the same twenty lines.
//
// Returns false when the file could not be opened, which the drop path needs: a
// dropped file that turns out not to be an image has to say so rather than
// silently do nothing.
bool MainWindow::openFile(const QString& file, bool hideBackground) {
    if (file.isEmpty())
        return false;
    if (!confirmDiscard())
        return false;

    if (file.endsWith(QLatin1String(".wpa"), Qt::CaseInsensitive)) {
        LayerStack loaded;
        const auto res = loaded.loadProject(file);
        if (!res.ok) {
            QMessageBox::critical(this, tr("WPaint"),
                                  tr("Could not open project: %1").arg(res.error));
            return false;
        }
        installDocument(loaded, file);
        return true;
    }

    QImageReader reader(file);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::critical(this, tr("WPaint"),
                              tr("Could not load image:\n%1").arg(reader.errorString()));
        return false;
    }
    LayerStack loaded;
    Layer l;
    l.name = tr("Background");
    l.image = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    loaded.addLayer(0, l);
    installDocument(loaded, file);
    // An image dropped on a blank canvas is the whole document, not a drawing
    // with a backdrop under it. A flat file has no background of its own, so
    // installDocument() adds a white one -- and under a PNG with transparency
    // that turns the transparent parts white, which is the opposite of what the
    // file says. So the backdrop is hidden and the checkerboard shows through,
    // which is what the image actually is.
    if (hideBackground) {
        const int bg = m_stack->backgroundIndex();
        if (bg >= 0)
            m_stack->setLayerVisible(bg, false);
    }
    return true;
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
    // A document from a file is not pristine, whatever the undo stack says: it
    // has a history, and a file dropped onto it is a question rather than a
    // decision. newDocument() is the only path that sets this.
    m_documentPristine = false;
    updateWindowTitle();
    m_canvas->zoomFit();
    syncStatusSize();
}

bool MainWindow::saveDocument() {
    if (m_currentPath.isEmpty() || m_currentPath.endsWith(QLatin1String(".wpa")))
        return saveDocumentAs();
    return saveTo(m_currentPath);
}

bool MainWindow::saveDocumentAs() {
    QString filter;
    const QString file = QFileDialog::getSaveFileName(
        this, tr("Save As"), m_currentPath.isEmpty() ? tr("untitled") : m_currentPath,
        tr("PNG Image (*.png);;JPEG Image (*.jpg);;Bitmap (*.bmp);;"
           "GIF Image (*.gif);;WPaint Project (*.wpa)"),
        &filter);
    // Cancelled: nothing was written, and the caller has to hear about it.
    if (file.isEmpty())
        return false;
    m_currentPath = file;
    return saveTo(file);
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
    // Our own copy first: it is the same pixels the clipboard holds, but it does
    // not go through a decode, and it is what the rail's copy put there.
    QImage img = !m_copied.isNull() ? m_copied : imageFromClipboard();
    if (img.isNull())
        return;
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // The middle of what the user can see, in image coordinates.
    //
    // The scroll bars and the canvas are two different coordinate systems and
    // the canvas is not even at the viewport's origin -- it is centred in it
    // when the document is smaller than the view. Going straight from a scroll
    // offset to image coordinates assumed all three lined up, and the paste
    // landed off the edge of the document whenever they did not, which for a
    // small image in a large window is most of the time.
    const QRect vp = m_scrollArea->viewport()->rect();
    const QPoint viewportCentre(m_scrollArea->horizontalScrollBar()->value() +
                                    vp.width() / 2,
                                m_scrollArea->verticalScrollBar()->value() +
                                    vp.height() / 2);
    const QPointF centerImg =
        m_canvas->toImage(QPointF(m_canvas->mapFrom(m_scrollArea->viewport(), viewportCentre)));
    const QPointF topLeft =
        m_canvas->clampToDocument(QPointF(centerImg.x() - img.width() / 2.0,
                                          centerImg.y() - img.height() / 2.0),
                                  img.size());

    m_canvas->pasteFloating(img, topLeft, tr("Paste image"));
    // The Select tool is what makes a floating object draggable and scalable, so
    // a paste has to arrive with it -- otherwise the paste lands selected and
    // nothing can be done with it until the user picks the right tool.
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
    // The rail's selection if there is one, the canvas's active layer otherwise:
    // "above the selected layer" has to mean something when the selection is
    // empty, and the active layer is what the user was last working on.
    int at = -1;
    runLayerCommand(tr("Add layer"), [this, &at] {
        Layer l;
        l.name = m_stack->nextName(tr("Layer"));
        l.image = QImage(m_stack->size(), QImage::Format_ARGB32_Premultiplied);
        l.image.fill(Qt::transparent);
        at = m_stack->addLayer(newLayerInsertIndex(), l);
        m_stack->setActiveIndex(at);
    });
    if (at >= 0)
        m_layersPanel->setActiveLayer(at);
}

// The index a new layer or group goes at, from the preference and the current
// selection. Both callers go through here so a layer added from the rail and a
// group added from the button cannot end up in different places.
int MainWindow::newLayerInsertIndex() const {
    const QList<int> sel = m_layersPanel->selectedIndices();
    const int anchor = sel.isEmpty() ? m_stack->activeIndex() : sel.first();
    return m_stack->newEntryIndex(static_cast<NewLayerPlacement>(Settings::newLayerPlacement()),
                                  anchor, Settings::newLayersStayInGroup());
}

// The layer keys are QActions rather than key handling on the rail, so they
// resolve as WindowShortcuts -- which is what makes Alt+Up beat the canvas's own
// arrow-key nudging instead of both firing.
void MainWindow::buildLayerActions(
    const std::function<QAction*(QAction*, const QString&)>& bind) {
    auto add = [this, &bind](const QString& label, const char* id,
                             const std::function<void()>& fn) {
        auto* a = new QAction(label, this);
        connect(a, &QAction::triggered, this, fn);
        bind(a, QString::fromLatin1(id));
    };
    add(tr("Add a layer"), "layer.add", [this] { addLayer(); });
    add(tr("Duplicate the layer"), "layer.duplicate", [this] {
        const int at = m_stack->activeIndex();
        if (at < 0 || at >= m_stack->count()) return;
        runLayerCommand(tr("Duplicate layer"), [this, at] {
            Layer copy = m_stack->layerAt(at);
            copy.name = m_stack->nextName(copy.name);
            m_stack->addLayer(at, copy);
            m_stack->setActiveIndex(at);
        });
        m_layersPanel->setActiveLayer(at);
    });
    // "Above" and "below" mean the same thing in the rail (index 0 is topmost)
    // and the opposite on the canvas (y grows downwards), so the two steps are
    // spelled out rather than derived from a delta that would be wrong once.
    auto focus = [this](int delta) {
        const int at = m_stack->activeIndex();
        const int next = at + delta;
        if (at < 0 || next < 0 || next >= m_stack->count())
            return;
        m_stack->setActiveIndex(next);
        m_layersPanel->setActiveLayer(next);
    };
    add(tr("Focus the layer above"), "layer.above", [this, focus] { focus(-1); });
    add(tr("Focus the layer below"), "layer.below", [this, focus] { focus(1); });
    auto move = [this](int delta) {
        const int at = m_stack->activeIndex();
        const int next = at + delta;
        if (at < 0 || next < 0 || next >= m_stack->count())
            return;
        runLayerCommand(tr("Move layer"), [this, at, next] { m_stack->moveLayer(at, next); });
        m_layersPanel->setActiveLayer(next);
    };
    add(tr("Move the layer up"), "layer.moveUp", [this, move] { move(-1); });
    add(tr("Move the layer down"), "layer.moveDown", [this, move] { move(1); });
    add(tr("Show or hide the layers panel"), "layers.panel", [this] {
        // Routed through the toolbar button so the key and the click cannot
        // leave the button showing the wrong state.
        m_layersToggle->click();
    });
}

void MainWindow::removeLayer(int index) { removeLayers({index}); }

void MainWindow::removeLayers(const QList<int>& selection) {
    // Every removal is one undo entry, taken from the bottom up so the indices
    // above the ones already handled stay valid. LayerStack::removeLayer refuses
    // the last real layer, so the "keep one" rule lives in the model rather than
    // in every caller that could forget it.
    if (!m_stack->canRemoveAny(selection))
        return;
    const int doomedCount = m_stack->resolveSelection(selection).size();
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    m_canvas->bakeActiveObject();
    if (!m_stack->removeSpans(selection))
        return;
    // Land on something that can still be painted on.
    const int active = m_stack->drawableIndex();
    m_stack->setActiveIndex(active >= 0 ? active : 0);
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive,
                                         tr(doomedCount == 1 ? "Delete layer"
                                                             : "Delete layers")));
    m_layersPanel->setSelection({m_stack->activeIndex()});
}

void MainWindow::renameLayer(int index, const QString& name) {
    renameLayers(index, name, {});
}

// Renaming a selection types the name once. The layer the user actually edited
// keeps it, and the rest get a numbered suffix -- the same name, not a wildcard:
// three layers called "Paint" are a bug, and "Paint (1)", "Paint (2)" is the
// cheapest thing that stops it.
void MainWindow::renameLayers(int index, const QString& name,
                              const QList<int>& alsoSelected) {
    const QString base = name.trimmed();
    if (base.isEmpty() || index < 0 || index >= m_stack->count())
        return;
    if (m_stack->layerAt(index).isBackground)
        return;
    QList<int> others = alsoSelected;
    others.removeAll(index);
    if (others.isEmpty()) {
        runLayerCommand(tr("Rename layer"),
                        [this, index, base] { m_stack->renameLayer(index, base); });
        return;
    }
    // Topmost first, so the suffixes count down the rail the way it reads.
    std::sort(others.begin(), others.end());
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    m_canvas->bakeActiveObject();
    m_stack->renameLayer(index, base);
    int serial = 1;
    for (int other : others) {
        if (other < 0 || other >= m_stack->count() || m_stack->layerAt(other).isBackground)
            continue;
        m_stack->renameLayer(other, QStringLiteral("%1 (%2)").arg(base).arg(serial++));
    }
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Rename layers")));
}

void MainWindow::groupLayers(const QList<int>& selection) {
    if (selection.size() < 2) {
        // Nothing to wrap, so make the group and let the layers arrive later --
        // a folder button that only groups what is already selected is a button
        // that does nothing for most of a document's life.
        const QList<Layer> before = m_stack->layers();
        const int beforeActive = m_stack->activeIndex();
        m_canvas->bakeActiveObject();
        const int at = m_stack->addFolder(
            Settings::newFoldersFolded(),
            static_cast<NewLayerPlacement>(Settings::newLayerPlacement()),
            selection.isEmpty() ? m_stack->activeIndex() : selection.first(),
            Settings::newLayersStayInGroup());
        if (at < 0)
            return;
        m_undo->push(
            Commands::makeLayerList(m_stack, before, beforeActive, tr("Add group")));
        m_layersPanel->setSelection({at});
        m_layersPanel->setActiveLayer(at);
        return;
    }
    // A folder owns a contiguous run, so a selection with a hole in it is
    // refused rather than quietly widened -- widening would swallow layers the
    // user did not pick, and losing one to a group is not undoable by renaming.
    if (m_stack->resolveSelection(selection).size() != 1) {
        showToast(tr("Only a block of neighbouring layers can be grouped."));
        return;
    }
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    m_canvas->bakeActiveObject();
    // New groups start folded by default: a group of nine layers arriving as
    // nine expanded rows has not tidied anything.
    const int at = m_stack->groupInto(selection, Settings::newFoldersFolded());
    if (at < 0)
        return;
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Group layers")));
    m_layersPanel->setSelection({at});
}

void MainWindow::mergeLayerDown(int index) {
    if (!m_stack->canMergeDown(index)) {
        showToast(tr("There is nothing below this layer to merge into."));
        return;
    }
    int at = -1;
    runLayerCommand(tr("Merge down"), [this, index, &at] { at = m_stack->mergeDown(index); });
    if (at >= 0)
        m_layersPanel->setSelection({at});
}

void MainWindow::mergeLayers(const QList<int>& selection) {
    if (!m_stack->canMergeSelection(selection)) {
        showToast(tr("Only a block of neighbouring layers can be merged."));
        return;
    }
    int at = -1;
    runLayerCommand(tr("Merge layers"),
                    [this, selection, &at] { at = m_stack->mergeSelected(selection); });
    if (at >= 0)
        m_layersPanel->setSelection({at});
}

void MainWindow::toggleLayerVisibility(int index, bool visible) {
    runLayerCommand(tr(visible ? "Show layer" : "Hide layer"),
                    [this, index, visible] { m_stack->setLayerVisible(index, visible); });
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