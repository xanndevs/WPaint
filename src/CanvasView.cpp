#include "CanvasView.h"

#include "Commands.h"
#include "DrawingUtils.h"
#include "LayerStack.h"
#include "Settings.h"
#include "Theme.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QUndoStack>
#include <QWheelEvent>

namespace {
constexpr int kPad = 20; // room for boundary handles
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 8.0;
constexpr qreal kMinObjectDim = 2.0;

// Grow/shrink a rectangle by dragging one of its 8 handles (0=TL, 1=TC, 2=TR,
// 3=MR, 4=BR, 5=BC, 6=BL, 7=ML). With `mirror` the opposite edge/corner also
// moves, i.e. the rect scales symmetrically about its centre.
QRectF adjustResizeRect(const QRectF& r0, int handle, const QPointF& d,
                        bool mirror, qreal minDim) {
    qreal L = r0.left(), T = r0.top(), R = r0.right(), B = r0.bottom();
    const bool west = handle == 0 || handle == 6 || handle == 7;
    const bool east = handle == 2 || handle == 3 || handle == 4;
    const bool north = handle == 0 || handle == 1 || handle == 2;
    const bool south = handle == 4 || handle == 5 || handle == 6;
    if (west) L += d.x();
    if (east) R += d.x();
    if (north) T += d.y();
    if (south) B += d.y();
    if (mirror) {
        if (west) R -= d.x();
        if (east) L -= d.x();
        if (north) B -= d.y();
        if (south) T -= d.y();
    }
    QRectF out(QPointF(qMin(L, R), qMin(T, B)), QPointF(qMax(L, R), qMax(T, B)));
    if (out.width() < minDim) {
        out.setX(out.center().x() - minDim / 2);
        out.setWidth(minDim);
    }
    if (out.height() < minDim) {
        out.setY(out.center().y() - minDim / 2);
        out.setHeight(minDim);
    }
    return out;
}

Qt::CursorShape resizeCursorForHandle(int handle) {
    switch (handle) {
    case 0: case 4: return Qt::SizeFDiagCursor;
    case 2: case 6: return Qt::SizeBDiagCursor;
    case 1: case 5: return Qt::SizeVerCursor;
    case 3: case 7: return Qt::SizeHorCursor;
    default: return Qt::ArrowCursor;
    }
}
} // namespace

CanvasView::CanvasView(LayerStack* stack, QUndoStack* undo, QWidget* parent)
    : QWidget(parent), m_stack(stack), m_undo(undo) {
    setObjectName("canvasHost");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);

    connect(m_stack, &LayerStack::changed, this, [this]() {
        updateViewSize();
        update();
    });

    m_antsTimer.setInterval(90);
    connect(&m_antsTimer, &QTimer::timeout, this, [this]() {
        m_dashOffset = (m_dashOffset + 1) % 8;
        update();
    });
    m_antsTimer.start();
}

// ------------------------------------------------------------ model ------

void CanvasView::setColors(const QColor& p, const QColor& s) {
    m_primary = p;
    m_secondary = s;
    if (m_hasObject) {
        m_object.penColor = p;
        m_object.fillColor = s;
        update();
    }
}

void CanvasView::setBrushSize(int s) {
    const int nv = qBound(1, s, kMaxBrushSize);
    if (nv == m_brushSize) return;
    m_brushSize = nv;
    // A shape already on the canvas keeps the width it was drawn with until the
    // user picks a new one, which makes the live object disagree with the
    // preview of the shape being drawn right now. Retint it, so the outline
    // under the cursor follows the size strip.
    if (m_hasObject) {
        m_object.penWidth = nv;
        m_objectOrig.penWidth = nv;
    }
    emit brushSizeChanged(nv);
    update();
}

void CanvasView::setBoundaryHandlesEnabled(bool on) {
    if (m_showBoundaryHandles == on) return;
    m_showBoundaryHandles = on;
    update();
}

// ------------------------------------------------ live shape object -------

void CanvasView::attachShapeObject(int layer, ToolId shape, ShapeStyle style,
                                   qreal penWidth, const QColor& penColor,
                                   const QColor& fillColor, const QRectF& rect,
                                   const QPointF& a, const QPointF& b,
                                   const QPointF& c1, const QPointF& c2) {
    clearSelection();
    ShapeObject o;
    o.layer = layer;
    o.shape = shape;
    o.style = style;
    o.penWidth = penWidth;
    o.penColor = penColor;
    o.fillColor = fillColor;
    o.rect = rect.normalized();
    o.a = a;
    o.b = b;
    o.c1 = c1;
    o.c2 = c2;
    m_object = o;
    m_hasObject = true;
    update();
}

void CanvasView::clearActiveObject() {
    m_objectDragging = false;
    m_objectHandle = -1;
    m_selFloatOrig = QImage();
    if (!m_hasObject) return;
    m_hasObject = false;
    m_object = ShapeObject();
    update();
}

// The exact image region a shape would touch once rendered, including the
// pen/AA halo; used to size the undo patch for a bake.
static QRect shapeBakeRect(const ShapeObject& o, const QSize& img) {
    QRectF base = o.rect;
    if (o.shape == ToolId::ShapeCurve) {
        QPainterPath path;
        path.moveTo(o.a);
        QPointF c1, c2;
        const bool a1 = !o.c1.isNull();
        const bool a2 = !o.c2.isNull();
        ShapeKit::cubicControls(o.a, o.b, o.c1, a1, o.c2, a2, c1, c2);
        path.cubicTo(c1, c2, o.b);
        base = path.boundingRect();
    }
    const qreal halo = qCeil(o.penWidth / 2.0) + 1.0;
    return base.adjusted(-halo, -halo, halo, halo)
        .toAlignedRect()
        .intersected(QRect(QPoint(0, 0), img));
}

void CanvasView::bakeActiveObject() {
    if (!m_hasObject) return;
    m_objectDragging = false;
    m_objectHandle = -1;
    const ShapeObject o = m_object;
    m_hasObject = false;
    m_object = ShapeObject();
    if (o.layer < 0 || o.layer >= m_stack->count()) return;
    const QRect region = shapeBakeRect(o, imageSize());
    if (region.isEmpty()) return;

    beginEdit(o.layer);
    QPainter p(&m_stack->layerAt(o.layer).image);
    p.setRenderHint(QPainter::Antialiasing, Settings::smoothShapes());
    paintShapeObject(p, o);
    p.end();
    markDirty(o.layer, region);
    commitEdit(tr("Shape"));
    update();
}

QPointF CanvasView::mapObjectPoint(const QRectF& fromR, const QRectF& toR,
                                   const QPointF& p) const {
    const qreal sx = fromR.width() > 0.001 ? toR.width() / fromR.width() : 1.0;
    const qreal sy = fromR.height() > 0.001 ? toR.height() / fromR.height() : 1.0;
    return toR.topLeft() + QPointF((p.x() - fromR.left()) * sx,
                                   (p.y() - fromR.top()) * sy);
}

void CanvasView::paintShapeObject(QPainter& p, const ShapeObject& o) const {
    if (o.layer < 0) return;
    QPen pen(o.penColor, o.penWidth);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    QPainterPath line;
    if (o.shape == ToolId::ShapeCurve) {
        QPointF c1, c2;
        const bool a1 = !o.c1.isNull();
        const bool a2 = !o.c2.isNull();
        ShapeKit::cubicControls(o.a, o.b, o.c1, a1, o.c2, a2, c1, c2);
        line.moveTo(o.a);
        line.cubicTo(c1, c2, o.b);
    } else if (o.shape == ToolId::ShapeLine ||
               o.shape == ToolId::ShapePointingArrow) {
        line.moveTo(o.a);
        line.lineTo(o.b);
    }
    ShapeKit::draw(p, o.shape, o.rect, o.style, pen, QBrush(o.fillColor), line,
                   o.a, o.b);
}

void CanvasView::beginSelectionResize(int handle, const QPointF& widget) {
    m_selResizing = true;
    m_selHandle = handle;
    m_selOrig = m_selection;
    m_selStartWidget = widget;
    m_resizeCaptured = false;
    if (m_floatingActive && !m_floating.isNull()) {
        m_selFloatOrig = m_floating;
    } else if (!m_spaceDown) {
        // Resizing a freshly made selection should scale what it covers. The
        // box alone has nothing to scale, so the region is lifted first and
        // the drag scales the captured pixels. Holding Space keeps the old
        // box-only resize, for when you just want to move the marquee.
        liftSelection();
        if (!m_floating.isNull()) {
            m_selFloatOrig = m_floating;
            m_resizeCaptured = true;
        }
    }
    update();
}

void CanvasView::updateSelectionResize(const QPointF& widget, bool mirror) {
    if (!m_selResizing) return;
    m_mirrorResize = mirror;
    const QPointF d = toImage(widget) - toImage(m_selStartWidget);
    const QRectF live = adjustResizeRect(m_selOrig, m_selHandle, d, mirror, kMinObjectDim)
                            .intersected(QRectF(QPointF(0, 0), QSizeF(imageSize())));
    m_selection = live.normalized();
    if (m_floatingActive) {
        m_floatingPos = m_selection.topLeft();
        if (!m_selFloatOrig.isNull() && !m_selection.isEmpty())
            m_floating = m_selFloatOrig.scaled(m_selection.size().toSize(),
                                               Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation);
    }
    update();
}

void CanvasView::finishSelectionResize() {
    if (!m_selResizing) return;
    m_selResizing = false;
    m_selHandle = -1;
    m_selFloatOrig = QImage();
    setSelection(m_selection);
    if (m_resizeCaptured) {
        // The drag scaled the captured pixels, so bake them down and label the
        // entry for what it was. Welding under the default label would file a
        // resize under "Move selection" in the undo history.
        m_resizeCaptured = false;
        weldFloating(tr("Resize selection"));
    }
    update();
}

void CanvasView::beginObjectResize(int handle, const QPointF& widget) {
    if (!m_hasObject) return;
    m_objectOrig = m_object;
    m_objectDragging = true;
    m_objectHandle = handle;
    m_objectStartWidget = widget;
    update();
}

void CanvasView::beginObjectMove(const QPointF& widget) {
    if (!m_hasObject) return;
    m_objectOrig = m_object;
    m_objectDragging = true;
    m_objectHandle = -1;
    m_objectStartWidget = widget;
    update();
}

void CanvasView::updateObjectDrag(const QPointF& widget, bool mirror) {
    if (!m_objectDragging || !m_hasObject) return;
    m_mirrorResize = mirror;
    const QPointF d = toImage(widget) - toImage(m_objectStartWidget);
    const QRectF r = (m_objectHandle >= 0)
                         ? adjustResizeRect(m_objectOrig.rect, m_objectHandle, d,
                                            mirror, kMinObjectDim)
                         : m_objectOrig.rect.translated(d);
    m_object.rect = r;
    m_object.a = mapObjectPoint(m_objectOrig.rect, r, m_objectOrig.a);
    m_object.b = mapObjectPoint(m_objectOrig.rect, r, m_objectOrig.b);
    m_object.c1 = mapObjectPoint(m_objectOrig.rect, r, m_objectOrig.c1);
    m_object.c2 = mapObjectPoint(m_objectOrig.rect, r, m_objectOrig.c2);
    update();
}

static bool rectsNearlyEqual(const QRectF& a, const QRectF& b, qreal eps) {
    return qAbs(a.left() - b.left()) < eps && qAbs(a.top() - b.top()) < eps &&
           qAbs(a.width() - b.width()) < eps && qAbs(a.height() - b.height()) < eps;
}

void CanvasView::finishObjectDrag() {
    if (!m_objectDragging || !m_hasObject) return;
    m_objectDragging = false;
    m_objectHandle = -1;
    if (rectsNearlyEqual(m_objectOrig.rect, m_object.rect, 0.5))
        m_object = m_objectOrig;
    update();
}

// --------------------------------------------------------- coordinate ----

QPointF CanvasView::toImage(const QPointF& widget) const {
    return (widget - m_canvasOrigin) / m_zoom;
}

QRectF CanvasView::toImage(const QRectF& widget) const {
    return QRectF(toImage(widget.topLeft()), toImage(widget.bottomRight()));
}

QPointF CanvasView::fromImage(const QPointF& image) const {
    return m_canvasOrigin + image * m_zoom;
}

QRectF CanvasView::fromImage(const QRectF& image) const {
    return QRectF(fromImage(image.topLeft()), fromImage(image.bottomRight()));
}

QSize CanvasView::imageSize() const {
    return m_stack->size();
}

// ------------------------------------------------------------- zoom -----

static qreal snapZoom(qreal z) {
    z = qBound(kMinZoom, z, kMaxZoom);

    // Handle zoom levels under 100% using your original 5% logic
    if (z < 1.0) {
        return qRound(z * 20) / 20.0;
    }

    // Explicitly define your preferred zoom levels above 100%
    static const std::vector<qreal> kZoomSteps = {
        1.0, 1.25, 1.50, 1.75, 2.00, 2.50, 3.00, 4.00, 5.00, 6.00, 7.00, 8.00
    };

    // Find the closest predefined step
    auto it = std::lower_bound(kZoomSteps.begin(), kZoomSteps.end(), z);

    if (it == kZoomSteps.end()) return kZoomSteps.back();
    if (it == kZoomSteps.begin()) return kZoomSteps.front();

    // Snap to the absolute nearest neighbor
    qreal higher = *it;
    qreal lower = *(it - 1);
    return (z - lower < higher - z) ? lower : higher;
}

void CanvasView::setZoom(qreal z) { applyZoom(z, true); }

void CanvasView::applyZoom(qreal z, bool snap) {
    if (snap) z = snapZoom(z);
    else z = qBound(kMinZoom, z, kMaxZoom);
    if (qFuzzyCompare(z, m_zoom)) return;
    m_autoFit = false;
    m_zoom = z;
    updateViewSize();
    if (m_tool) m_tool->onZoomChanged(this); // e.g. text tool commits at new scale
    emit zoomChanged(z);
    update();
}

void CanvasView::zoomIn() { setZoom(m_zoom * 1.25); }
void CanvasView::zoomOut() { setZoom(m_zoom / 1.25); }
void CanvasView::zoomActual() { setZoom(1.0); }

void CanvasView::zoomAt(const QPoint& widgetAnchor, qreal factor) {
    // The image point currently under the cursor. Everything else here is about
    // making this same point land back under the cursor afterwards.
    const QPointF imagePt = toImage(widgetAnchor);
    if (!m_scrollArea) { setZoom(m_zoom * factor); return; }

    QScrollBar* hbar = m_scrollArea->horizontalScrollBar();
    QScrollBar* vbar = m_scrollArea->verticalScrollBar();
    const int oldH = hbar->value();
    const int oldV = vbar->value();

    // Continuous, not snapped to the 25% ladder. Snapping breaks two things: a
    // high-resolution wheel or trackpad sends small deltas whose factor gets
    // snapped straight back to the current step, so the zoom can never escape
    // it, and every notch that does land is a lurch.
    applyZoom(m_zoom * factor, false);

    // setZoom() resized the widget and re-centred the origin, so the anchor
    // now sits at a different widget position. Pin it back to the same viewport
    // pixel. A widget point p appears on screen at (p - scroll), and the cursor
    // was at (widgetAnchor - oldScroll), so:
    //   after - newScroll == widgetAnchor - oldScroll
    const QPointF after = fromImage(imagePt);
    hbar->setValue(qBound(hbar->minimum(), qRound(after.x()) - widgetAnchor.x() + oldH,
                          hbar->maximum()));
    vbar->setValue(qBound(vbar->minimum(), qRound(after.y()) - widgetAnchor.y() + oldV,
                          vbar->maximum()));
}

// Middle-drag pans by scrolling the viewport, the same motion the scroll bars
// and the wheel do: the document itself never moves. The anchor is tracked in
// *global* coordinates on purpose -- CanvasView is the scroll area's widget, so
// scrolling slides the widget itself under a stationary cursor, and a widget-
// space anchor would report the scroll-back as extra cursor travel and run away
// over a long drag. A widget point p is drawn at (p - scroll), so dragging the
// cursor by dx means decreasing the horizontal scroll value by dx.
void CanvasView::beginPan(const QPointF& widgetPos) {
    m_panning = true;
    m_panAnchor = mapToGlobal(widgetPos);
    setCursor(Qt::ClosedHandCursor);
}

void CanvasView::panTo(const QPointF& widgetPos) {
    if (!m_panning) return;
    const QPointF global = mapToGlobal(widgetPos);
    const QPointF delta = global - m_panAnchor;
    m_panAnchor = global;
    if (!m_scrollArea || delta.isNull()) return;
    QScrollBar* hbar = m_scrollArea->horizontalScrollBar();
    QScrollBar* vbar = m_scrollArea->verticalScrollBar();
    if (hbar->maximum() > hbar->minimum()) {
        hbar->setValue(qBound(hbar->minimum(), hbar->value() - qRound(delta.x()),
                              hbar->maximum()));
    }
    if (vbar->maximum() > vbar->minimum()) {
        vbar->setValue(qBound(vbar->minimum(), vbar->value() - qRound(delta.y()),
                              vbar->maximum()));
    }
}

void CanvasView::endPan() {
    if (!m_panning) return;
    m_panning = false;
    setCursor(Qt::ArrowCursor);
}

void CanvasView::zoomFit() {
    const QSize vp = m_scrollArea ? m_scrollArea->viewport()->size() : size();
    const QSize img = imageSize();
    if (img.isEmpty() || vp.isEmpty()) return;
    const qreal z = qMin(qreal(vp.width() - 2 * kPad) / img.width(),
                         qreal(vp.height() - 2 * kPad) / img.height());
    m_autoFit = true;
    m_zoom = snapZoom(qMax(kMinZoom, z));
    updateViewSize();
    if (m_tool) m_tool->onZoomChanged(this);
    emit zoomChanged(m_zoom);
    update();
}

void CanvasView::updateViewSize() {
    const QSize img = imageSize();
    const QSize vp = m_scrollArea ? m_scrollArea->viewport()->size() : size();
    // A viewport-sized margin on every side, on top of the usual padding. The
    // scroll area clamps its scrollbars to [min,max], so the content can only
    // be positioned within that range; without slack, zoomAt() cannot keep a
    // point under the cursor when the cursor is nearer the top-left than the
    // canvas margin allows, and the anchor visibly drifts. The margin
    // guarantees every point can reach every viewport pixel, which is also how
    // Photoshop behaves -- you can pan the canvas off-centre.
    const int needW = qCeil(img.width() * m_zoom) + 2 * kPad + 2 * vp.width();
    const int needH = qCeil(img.height() * m_zoom) + 2 * kPad + 2 * vp.height();
    const int w = qMax(needW, vp.width());
    const int h = qMax(needH, vp.height());
    setFixedSize(w, h);
    m_canvasOrigin = QPointF((w - img.width() * m_zoom) / 2.0,
                             (h - img.height() * m_zoom) / 2.0);

    // Center the view when we opened or auto-fit. Done by computing the
    // offset rather than QScrollArea::ensureVisible(), which centres against
    // whatever scroll range is current and so lands in the wrong place when the
    // widget was just resized.
    if (m_scrollArea && m_autoFit) {
        QScrollBar* hbar = m_scrollArea->horizontalScrollBar();
        QScrollBar* vbar = m_scrollArea->verticalScrollBar();
        const QPointF centre = m_canvasOrigin + QPointF(img.width() * m_zoom,
                                                        img.height() * m_zoom) / 2.0;
        hbar->setValue(qBound(hbar->minimum(), qRound(centre.x() - vp.width() / 2.0),
                              hbar->maximum()));
        vbar->setValue(qBound(vbar->minimum(), qRound(centre.y() - vp.height() / 2.0),
                              vbar->maximum()));
    }
}

void CanvasView::resizeEvent(QResizeEvent* ev) {
    updateViewSize();
    QWidget::resizeEvent(ev);
}

void CanvasView::wheelEvent(QWheelEvent* ev) {
    // Space + wheel is the brush size, and takes priority over both the zoom
    // and the scroll: Space is already the "don't edit, navigate" modifier
    // here, and a size change is the one thing it can usefully add.
    if (m_spaceDown && m_tool && m_tool->supportsBrushSize() &&
        Settings::spaceWheelBrushSize() &&
        !(ev->modifiers() & Qt::ControlModifier)) {
        int steps = 0;
        const int angle = ev->angleDelta().y();
        if (angle != 0) {
            steps = qRound(qreal(angle) / 120.0); // one notch per step
        } else {
            // A trackpad has no notches; ~30px of travel is one step.
            const int px = ev->pixelDelta().y();
            steps = qBound(-4, px / 30, 4);
        }
        if (steps != 0) {
            setBrushSize(m_brushSize + steps);
            ev->accept();
            return;
        }
    }
    if (ev->modifiers() & Qt::ControlModifier) {
        // A mouse wheel reports angleDelta; a trackpad pinch reports pixelDelta
        // with angleDelta zero, and used to be dropped entirely.
        int delta = ev->angleDelta().y();
        if (delta == 0) delta = ev->pixelDelta().y();
        if (delta == 0) { ev->accept(); return; }
        // Scale by the delta rather than treating every event as one notch, so
        // many small trackpad deltas and one coarse wheel notch feel the same.
        const qreal steps = qBound(-8.0, qreal(delta) / 120.0, 8.0);
        zoomAt(ev->position().toPoint(), std::pow(1.2, steps));
        ev->accept();
    } else {
        QWidget::wheelEvent(ev);
    }
}

// --------------------------------------------------------- selection -----

QRect CanvasView::selectionPixelRect() const {
    if (!m_hasSelection) return QRect();
    const QSize img = imageSize();
    return m_selection.normalized().toAlignedRect().intersected(QRect(QPoint(0, 0), img));
}

void CanvasView::setSelection(const QRectF& r) {
    if (r.isEmpty() || r.width() < 1 || r.height() < 1) {
        clearSelection();
        return;
    }
    m_selection = r.normalized().intersected(QRectF(QPointF(0, 0), QSizeF(imageSize())));
    m_hasSelection = !m_selection.isEmpty();
    emit selectionChanged();
    update();
}

void CanvasView::clearSelection() {
    if (!m_hasSelection && !m_makeSelection) return;
    m_hasSelection = false;
    m_makeSelection = false;
    m_selection = QRectF();
    emit selectionChanged();
    update();
}

static QRectF normalizedClamped(const QPointF& a, const QPointF& b, const QSize& img) {
    return QRectF(a, b).normalized().intersected(QRectF(QPointF(0, 0), QSizeF(img)));
}

void CanvasView::beginSelectionDrag(const QPointF& imgAnchor) {
    bakeActiveObject();
    if (m_floatingActive) weldFloating();
    if (m_sessionOpen) commitEdit(tr("Draw"));
    m_makeSelection = true;
    m_selectAnchor = imgAnchor;
    m_selectLive = normalizedClamped(m_selectAnchor, imgAnchor, imageSize());
    update();
}

void CanvasView::updateSelectionDrag(const QPointF& imgCursor) {
    if (!m_makeSelection) return;
    m_selectLive = normalizedClamped(m_selectAnchor, imgCursor, imageSize());
    update();
}

void CanvasView::endSelectionDrag() {
    // If this drag was a clicking without moving, the drag is cancelled.
    if (!m_makeSelection) return;
    m_makeSelection = false;
    setSelection(m_selectLive);
}

QRectF CanvasView::selectionDragLive() const {
    return m_makeSelection ? m_selectLive : QRectF();
}

// ---------------------------------------------- floating / clipboard -----

void CanvasView::liftSelection() {
    if (!m_hasSelection || m_floatingActive) return;
    QRect sel = selectionPixelRect();
    if (sel.isEmpty()) return;
    beginEditList({activeLayerIndex()});
    m_floating = m_stack->layerAt(activeLayerIndex()).image.copy(sel);
    m_floatingPos = m_selection.topLeft();
    m_floatingActive = true;
    eraseRegion(activeLayerIndex(), sel);
    markDirty(activeLayerIndex(), sel);
    update();
}

void CanvasView::pasteFloating(const QImage& img, const QPointF& topLeft) {
    if (img.isNull()) return;
    if (m_floatingActive) weldFloating();
    m_floating = img;
    m_floatingPos = topLeft;
    m_floatingActive = true;
    m_selection = QRectF(m_floatingPos, QSizeF(img.size()))
                      .intersected(QRectF(QPointF(0, 0), QSizeF(imageSize())));
    m_hasSelection = !m_selection.isEmpty();
    emit selectionChanged();
    update();
}

void CanvasView::weldFloating(const QString& text) {
    if (!m_floatingActive) return;
    const int layer = activeLayerIndex();
    const QPoint pos = m_floatingPos.toPoint();
    Draw::blit(m_stack->layerAt(layer).image, m_floating, pos);
    markDirty(layer, QRect(pos, m_floating.size()));
    m_floatingActive = false;
    m_floating = QImage();
    commitEdit(text.isEmpty() ? tr("Move selection") : text);
    update();
}

void CanvasView::cancelFloating() {
    if (!m_floatingActive) return;
    revertActiveEdit();
    m_floatingActive = false;
    m_floating = QImage();
    update();
}

void CanvasView::cancelFloatingLift() { cancelFloating(); }

void CanvasView::commitFloatingRemoval(const QString& text) {
    if (!m_floatingActive) return;
    // A lift already opened an edit session and erased the region, so close
    // that one instead of beginning a second erase. The re-erase would be a
    // no-op that commitEdit discards, but it would still split the cut into
    // two undo entries -- the first mislabelled "Draw".
    if (m_sessionOpen) {
        m_floatingActive = false;
        m_floating = QImage();
        commitEdit(text);
        update();
        return;
    }
    const QRect r = QRect(m_floatingPos.toPoint(), m_floating.size());
    m_floatingActive = false;
    m_floating = QImage();
    setSelection(QRectF(r));
    clearSelectionRegion(text);
    update();
}

void CanvasView::clearSelectionRegion(const QString& text) {
    if (!m_hasSelection) return;
    if (m_floatingActive) weldFloating();
    const QRect sel = selectionPixelRect();
    if (sel.isEmpty()) return;
    beginEditList({activeLayerIndex()});
    eraseRegion(activeLayerIndex(), sel);
    markDirty(activeLayerIndex(), sel);
    commitEdit(text);
    update();
}

QImage CanvasView::copySelection() const {
    // Copy what the user actually sees: the floating overlay wins over the
    // (already-erased) layer composite.
    if (m_floatingActive && !m_floating.isNull())
        return m_floating;
    if (!m_hasSelection) return QImage();
    return compositeRegion(m_selection);
}

QImage CanvasView::compositeRegion(const QRectF& imageRect) const {
    const QRect r = imageRect.normalized().toAlignedRect()
                        .intersected(QRect(QPoint(0, 0), imageSize()));
    if (r.isEmpty()) return QImage();
    return composite().copy(r);
}

QImage CanvasView::composite() const {
    return m_stack->composite();
}

// selection drag helpers used by the drag machinery (float move)
void CanvasView::beginFloatDrag(const QPointF& widgetPos) {
    m_floatMoving = true;
    m_floatDragAnchor = widgetPos;
    m_floatPosAtDrag = m_floatingPos;
}

void CanvasView::moveFloatDrag(const QPointF& widgetPos) {
    if (!m_floatMoving) return;
    const QPointF cur = toImage(widgetPos);
    const QPointF anchor = toImage(m_floatDragAnchor);
    const QPointF delta = cur - anchor;
    QPointF pos = m_floatPosAtDrag + delta;
    const QSize img = imageSize();
    pos.setX(qBound<qreal>(-m_floating.width() + 1, pos.x(), img.width()));
    pos.setY(qBound<qreal>(-m_floating.height() + 1, pos.y(), img.height()));
    m_floatingPos = pos;
    m_selection = QRectF(pos, QSizeF(m_floating.size()));
    update();
}

void CanvasView::endFloatDrag() {
    m_floatMoving = false;
}

bool CanvasView::isFloatDragging() const { return m_floatMoving; }

// ----------------------------------------------------- edit sessions -----

void CanvasView::beginEdit(int layerIndex) {
    beginEditList({layerIndex});
}

void CanvasView::beginEditList(const QList<int>& layers) {
    if (m_floatingActive) weldFloating(); // bake pending selection-move first
    if (m_sessionOpen) commitEdit(tr("Draw"));
    m_sessionOpen = true;
    m_sessionBefore.clear();
    m_sessionDirty.clear();
    for (int i : layers) {
        if (i < 0 || i >= m_stack->count()) continue;
        m_sessionBefore.insert(i, m_stack->layerAt(i).image);
    }
}

void CanvasView::markDirty(int layer, const QRect& imageRect) {
    if (!m_sessionOpen) return;
    if (layer < 0 || layer >= m_stack->count()) return;
    const QRect r = imageRect.normalized().intersected(QRect(QPoint(0, 0), imageSize()));
    if (r.isEmpty()) return;
    m_sessionDirty[layer] = m_sessionDirty.value(layer).united(r);
}

void CanvasView::commitEdit(const QString& text) {
    if (!m_sessionOpen) return;
    m_sessionOpen = false;
    QVector<Commands::PaintPatch> patches;
    for (auto it = m_sessionBefore.constBegin(); it != m_sessionBefore.constEnd(); ++it) {
        const int layer = it.key();
        const QRect d = m_sessionDirty.value(layer);
        if (d.isEmpty()) continue;
        const QImage& before = it.value();
        QImage beforeR = before.copy(d);
        const QImage& after = m_stack->layerAt(layer).image;
        QImage afterR = after.copy(d);
        if (beforeR == afterR) continue; // nothing actually changed
        Commands::PaintPatch patch;
        patch.layer = layer;
        patch.pos = d.topLeft();
        patch.before = beforeR;
        patch.after = afterR;
        patches.append(patch);
    }
    m_sessionBefore.clear();
    m_sessionDirty.clear();
    if (!patches.isEmpty()) {
        m_undo->push(Commands::makePaint(m_stack, patches, text));
        emit pixelsChanged();
    }
    update();
}

void CanvasView::revertActiveEdit() {
    if (!m_sessionOpen) return;
    m_sessionOpen = false;
    for (auto it = m_sessionBefore.constBegin(); it != m_sessionBefore.constEnd(); ++it)
        m_stack->layerAt(it.key()).image = it.value();
    m_sessionBefore.clear();
    m_sessionDirty.clear();
    update();
}

void CanvasView::eraseRegion(int layer, const QRect& rect) {
    QPainter p(&m_stack->layerAt(layer).image);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(rect.intersected(QRect(QPoint(0, 0), imageSize())), Qt::transparent);
}

QImage& CanvasView::layerImage(int i) {
    return m_stack->layerAt(i).image;
}

int CanvasView::activeLayerIndex() const { return m_stack->activeIndex(); }

void CanvasView::setActiveLayer(int i) { m_stack->setActiveIndex(i); }

// ------------------------------------------------------ canvas ops -------

void CanvasView::cropTo(const QRectF& imageRect) {
    bakeActiveObject();
    const QRect r = imageRect.normalized().toAlignedRect()
                        .intersected(QRect(QPoint(0, 0), imageSize()));
    if (r.width() < 1 || r.height() < 1) return;
    if (m_floatingActive) weldFloating();
    clearSelection();
    if (r == QRect(QPoint(0, 0), imageSize())) return;
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    QList<Layer> after;
    for (int i = 0; i < m_stack->count(); ++i) {
        Layer l = m_stack->layerAt(i);
        // The background layer holds no pixels; its colour is reapplied across
        // whatever the new canvas size is, so it rides along unchanged.
        if (!l.isBackground)
            l.image = l.image.copy(r);
        after << l;
    }
    m_stack->replaceAll(after, m_stack->activeIndex());
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Crop")));
}

void CanvasView::setCanvasSize(const QSize& size) {
    bakeActiveObject();
    if (size == imageSize() || size.isEmpty()) return;
    if (m_floatingActive) weldFloating();
    clearSelection();
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    m_stack->setSize(size);
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Resize canvas")));
}

void CanvasView::rotateCanvas(qreal degrees) {
    bakeActiveObject();
    if (m_floatingActive) weldFloating();
    clearSelection();
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    QList<Layer> after;
    for (int i = 0; i < m_stack->count(); ++i) {
        Layer l = m_stack->layerAt(i);
        if (!l.isBackground)
            l.image = Draw::rotateImage(l.image, degrees);
        after << l;
    }
    m_stack->replaceAll(after, m_stack->activeIndex());
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Rotate canvas")));
}

void CanvasView::flipCanvas(Qt::Orientation orientation) {
    bakeActiveObject();
    if (m_floatingActive) weldFloating();
    clearSelection();
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    QList<Layer> after;
    for (int i = 0; i < m_stack->count(); ++i) {
        Layer l = m_stack->layerAt(i);
        if (!l.isBackground)
            l.image = Draw::flipImage(l.image, orientation);
        after << l;
    }
    m_stack->replaceAll(after, m_stack->activeIndex());
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Flip horizontal")));
}

// Rotating/flipping/resizing a selection works on the floating selection, not
// on the pixels under the marquee. Welding first (what this used to do) put
// the result straight back on the layer, so the next 90 degrees re-read the
// *new* selection box -- which, being the bounding box of the rotated content,
// no longer matched what the user picked and swept in whatever surrounded it.
// Lifting and transforming the floating image instead keeps the pixels the user
// selected together for the whole gesture, and the single undo entry is the
// weld, exactly as for a move.
void CanvasView::rotateSelection(qreal degrees) {
    bakeActiveObject();
    if (!m_hasSelection) return;
    if (!m_floatingActive) liftSelection();
    if (!m_floatingActive || m_floating.isNull()) return;

    const QPointF center = QRectF(m_floatingPos, QSizeF(m_floating.size())).center();
    m_floating = Draw::rotateImage(m_floating, degrees);
    m_floatingPos = center - QPointF(m_floating.width(), m_floating.height()) / 2.0;
    setSelection(QRectF(m_floatingPos, QSizeF(m_floating.size())));
    requestRepaint();
}

void CanvasView::flipSelection(Qt::Orientation orientation) {
    bakeActiveObject();
    if (!m_hasSelection) return;
    if (!m_floatingActive) liftSelection();
    if (!m_floatingActive || m_floating.isNull()) return;

    // A mirror about the centre leaves the bounding box where it was, so the
    // selection does not move -- only the content inside it does.
    m_floating = Draw::flipImage(m_floating, orientation);
    requestRepaint();
}

void CanvasView::transformSelection(const QSize& targetSize, int rotateDegrees) {
    bakeActiveObject();
    if (!m_hasSelection) return;
    if (!m_floatingActive) liftSelection();
    if (!m_floatingActive || m_floating.isNull()) return;

    const QPointF center = QRectF(m_floatingPos, QSizeF(m_floating.size())).center();
    const QSize size = targetSize.isEmpty() ? m_floating.size() : targetSize;
    if (size.width() > 0 && size.height() > 0 && size != m_floating.size())
        m_floating = m_floating.scaled(size, Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
    if (rotateDegrees != 0)
        m_floating = Draw::rotateImage(m_floating, rotateDegrees);
    m_floatingPos = center - QPointF(m_floating.width(), m_floating.height()) / 2.0;

    setSelection(QRectF(m_floatingPos, QSizeF(m_floating.size())));
    requestRepaint();
}

// --------------------------------------------------------- tools ---------

void CanvasView::setTool(Tool* tool) {
    if (tool == m_tool) return;
    if (m_tool) m_tool->onDeactivate(this);    // may commit an in-progress gesture
    if (m_hasObject)
        bakeActiveObject(); // letting go by switching tools
    m_tool = tool;
    if (m_tool) m_tool->onActivate(this);
    if (auto* st = dynamic_cast<ShapeTool*>(m_tool))
        m_shapeTool = st;
    setCursor(m_tool ? tool->cursor() : Qt::ArrowCursor);
    emit toolChanged(m_tool);
    update();
}

ShapeKit::Shape CanvasView::currentShape() const {
    return m_shapeTool ? m_shapeTool->shape() : ToolId::ShapeRect;
}

void CanvasView::setShape(ShapeKit::Shape shape) {
    if (m_shapeTool)
        m_shapeTool->setShape(shape);
    else
        m_shapeTool = dynamic_cast<ShapeTool*>(ToolRegistry::create(shape));
}
ShapeStyle CanvasView::currentShapeStyle() const {
    return m_shapeTool ? m_shapeTool->style() : ShapeStyle::Outline;
}

void CanvasView::setShapeStyle(ShapeStyle style) {
    if (m_hasObject)
        m_object.style = style;   // restyle the pending shape too
    if (m_shapeTool)
        m_shapeTool->setStyle(style);
    else
        m_shapeTool = dynamic_cast<ShapeTool*>(ToolRegistry::create(ToolId::ShapeRect));
    if (m_shapeTool)
        m_shapeTool->setStyle(style);
    update();
}

// ----------------------------------------------------- mouse handling ----

void CanvasView::mousePressEvent(QMouseEvent* ev) {
    setFocus();
    // A folder is a real selection and a perfectly good thing to click, it just
    // has no pixels. Every drawing gesture arrives through here, so this is the
    // one place that has to know -- and it refuses before the tool sees the
    // event, so no tool has to grow a "you cannot draw here" branch.
    if (m_tool && m_tool->id() != ToolId::Select && m_tool->id() != ToolId::Magnify &&
        ev->button() != Qt::MiddleButton && m_stack && !m_stack->isDrawable(m_stack->activeIndex())) {
        emit editableLayerRequired();
        ev->accept();
        return;
    }
    if (m_boundaryResize) finishBoundaryResize();
    if (m_selResizing) finishSelectionResize();
    if (m_objectDragging) finishObjectDrag();
    // The middle button pans, so it belongs to the viewport rather than to
    // whatever tool is active, and it must not cancel a tool gesture either.
    if (ev->button() == Qt::MiddleButton) {
        beginPan(ev->position());
        ev->accept();
        return;
    }
    // Pressing the other button abandons a gesture already in progress, so
    // right-dragging a shape and then clicking left throws the shape away, and
    // the same works in reverse for a right-button stroke. This is a local
    // revert rather than an undo, so nothing lands on the undo stack.
    if (m_tool && m_tool->gestureButton() != Qt::NoButton &&
        ev->button() != m_tool->gestureButton()) {
        m_tool->cancelGesture(this);
        ev->accept();
        return;
    }
    const bool left = ev->button() == Qt::LeftButton;
    const bool selectTool = m_tool && m_tool->id() == ToolId::Select;
    const bool shapeTool =
        m_tool && dynamic_cast<ShapeTool*>(m_tool) != nullptr;
    const QPointF img = left ? toImage(ev->position()) : QPointF();

    // live object resize handles (Select or Shape after a drawn shape)
    if (left && (selectTool || shapeTool) && m_hasObject) {
        const int h = handleAtImageRect(m_object.rect, img);
        if (h >= 0) {
            beginObjectResize(h, ev->position());
            ev->accept();
            return;
        }
    }
    // selection free-resize handles (Select tool only)
    if (left && selectTool && m_hasSelection && !m_makeSelection && !m_hasObject) {
        const int h = handleAtImageRect(m_selection, img);
        if (h >= 0) {
            beginSelectionResize(h, ev->position());
            ev->accept();
            return;
        }
    }
    // drag a live object to move it
    if (left && selectTool && m_hasObject && m_object.rect.contains(img)) {
        beginObjectMove(ev->position());
        ev->accept();
        return;
    }
    // canvas boundary handles (any tool that edits, but not the magnifier:
    // a zoom click near an edge should zoom, not resize the canvas)
    if (m_showBoundaryHandles && m_tool && left &&
        m_tool->id() != ToolId::Magnify) {
        const int handle = handleAtWidget(ev->position());
        if (handle >= 0) {
            bakeActiveObject();
            m_boundaryResize = true;
            m_boundaryHandle = handle;
            m_boundaryStartWidget = ev->position();
            m_boundaryLiveRect = QRect(QPoint(0, 0), imageSize());
            update();
            ev->accept();
            return;
        }
    }
    if (m_tool) {
        m_tool->mousePress(this, ev);
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void CanvasView::mouseMoveEvent(QMouseEvent* ev) {
    m_lastWidget = ev->position();
    if (m_panning) {
        panTo(ev->position());
        ev->accept();
        return;
    }
    if (m_objectDragging) {
        updateObjectDrag(ev->position(), ev->modifiers() & Qt::AltModifier);
        ev->accept();
        return;
    }
    if (m_selResizing) {
        updateSelectionResize(ev->position(), ev->modifiers() & Qt::AltModifier);
        ev->accept();
        return;
    }
    if (m_boundaryResize) {
        updateBoundaryResize(ev->position());
        ev->accept();
        return;
    }
    // live resize/move cursor feedback
    Qt::CursorShape cur = Qt::ArrowCursor;
    if (m_tool && (m_tool->id() == ToolId::Select ||
                   dynamic_cast<ShapeTool*>(m_tool) != nullptr)) {
        const QPointF img = toImage(ev->position());
        if (m_hasObject) {
            const int h = handleAtImageRect(m_object.rect, img);
            if (h >= 0) cur = resizeCursorForHandle(h);
            else if (m_tool->id() == ToolId::Select && m_object.rect.contains(img))
                cur = Qt::SizeAllCursor;
        } else if (m_tool->id() == ToolId::Select && m_hasSelection && !m_makeSelection) {
            const int h = handleAtImageRect(m_selection, img);
            if (h >= 0) cur = resizeCursorForHandle(h);
        }
    }
    if (cur == Qt::ArrowCursor && m_showBoundaryHandles && m_tool &&
        m_tool->id() != ToolId::Magnify) {
        const int h = handleAtWidget(ev->position());
        if (h >= 0) cur = resizeCursorForHandle(h);
    }
    setCursor(cur);
    if (m_tool) {
        m_tool->mouseMove(this, ev);
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* ev) {
    if (m_panning) {
        if (ev->button() == Qt::MiddleButton)
            endPan();
        ev->accept();
        return;
    }
    if (m_objectDragging) {
        finishObjectDrag();
        ev->accept();
        return;
    }
    if (m_selResizing) {
        finishSelectionResize();
        ev->accept();
        return;
    }
    if (m_boundaryResize) {
        finishBoundaryResize();
        ev->accept();
        return;
    }
    if (m_tool) {
        m_tool->mouseRelease(this, ev);
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* ev) {
    if (m_tool) {
        m_tool->mouseDoubleClick(this, ev);
        ev->accept();
    }
}

// Maps an arrow key to a 1px step, or 10px with Shift held.
static bool arrowDelta(QKeyEvent* ev, QPoint& out) {
    switch (ev->key()) {
    case Qt::Key_Left:  out = QPoint(-1, 0); break;
    case Qt::Key_Right: out = QPoint(1, 0); break;
    case Qt::Key_Up:    out = QPoint(0, -1); break;
    case Qt::Key_Down:  out = QPoint(0, 1); break;
    default: return false;
    }
    if (ev->modifiers() & Qt::ShiftModifier)
        out *= 10;
    return true;
}

void CanvasView::keyPressEvent(QKeyEvent* ev) {
    if (m_tool) {
        // A KeyPress arrives already accepted, so "still accepted after the
        // tool ran" does not mean the tool wanted it. A tool claims a key only
        // by accepting it from a non-accepted state, so start from ignored and
        // let an explicit accept() count as intent. Without this every tool
        // that does not handle a given key silently ate it -- which is why
        // Space, and with it arrow-nudge and Space+arrows sketching, did
        // nothing while the Select tool was active.
        ev->ignore();
        m_tool->keyPress(this, ev);
        if (ev->isAccepted()) return;
    }
    // Space alone only arms the sketch cursor. It must be consumed either way
    // or the enclosing QScrollArea treats it as a page-scroll.
    if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat()) {
        m_spaceDown = true;
        ev->accept();
        return;
    }
    if (handleArrowKey(ev))
        return;
    QWidget::keyPressEvent(ev);
}

// Arrow keys nudge the live object, the floating selection or the marquee.
// With Space held they instead extend a sketch stroke on the active layer.
bool CanvasView::handleArrowKey(QKeyEvent* ev) {
    QPoint step;
    if (!arrowDelta(ev, step)) return false;

    if (m_spaceDown) {
        if (m_sketching)
            sketchTo(m_sketchLast + QPointF(step));
        else
            sketchTo(toImage(m_lastWidget) + QPointF(step));
        ev->accept();
        return true;
    }

    if (!nudgeBy(step)) return false;
    ev->accept();
    return true;
}

bool CanvasView::nudgeBy(const QPoint& delta) {
    if (m_hasObject) {
        m_object.rect.translate(delta);
        m_object.a += QPointF(delta);
        m_object.b += QPointF(delta);
        m_object.c1 += QPointF(delta);
        m_object.c2 += QPointF(delta);
        m_objectOrig = m_object;
        update();
        return true;
    }
    if (m_floatingActive) {
        // Match the drag clamp: keep at least one pixel of the floating
        // selection overlapping the canvas.
        const QSize img = imageSize();
        QPointF pos = m_floatingPos + QPointF(delta);
        pos.setX(qBound<qreal>(-m_floating.width() + 1, pos.x(), img.width()));
        pos.setY(qBound<qreal>(-m_floating.height() + 1, pos.y(), img.height()));
        if (pos == m_floatingPos) return false;
        m_floatingPos = pos;
        m_selection = QRectF(pos, QSizeF(m_floating.size()));
        update();
        return true;
    }
    if (m_hasSelection) {
        const QRectF r =
            (m_selection.translated(delta))
                .intersected(QRectF(QPointF(0, 0), QSizeF(imageSize())));
        if (r.isEmpty()) return false;
        setSelection(r);
        update();
        return true;
    }
    return false;
}

void CanvasView::sketchTo(const QPointF& imagePt) {
    const QPointF target(
        qBound<qreal>(0, imagePt.x(), imageSize().width()),
        qBound<qreal>(0, imagePt.y(), imageSize().height()));
    if (!m_sketching) {
        if (m_floatingActive) weldFloating();
        m_sketching = true;
        m_sketchLast = target;
        beginEdit(activeLayerIndex());
        return;
    }
    if (target == m_sketchLast) return;

    const int w = qMax(1, m_brushSize);
    QPainter p(&layerImage(activeLayerIndex()));
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(m_primary);
    pen.setWidth(w);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawLine(m_sketchLast, target);

    const int pad = w / 2 + 2;
    const QRect dirty = QRectF(m_sketchLast, target).normalized()
                            .adjusted(-pad, -pad, pad, pad)
                            .toAlignedRect()
                            .intersected(QRect(QPoint(0, 0), imageSize()));
    markDirty(activeLayerIndex(), dirty);
    m_sketchLast = target;
    update();
}

void CanvasView::endSketch() {
    if (!m_sketching) return;
    m_sketching = false;
    commitEdit(tr("Sketch"));
}

void CanvasView::keyReleaseEvent(QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat()) {
        m_spaceDown = false;
        endSketch();
        ev->accept();
        return;
    }
    QWidget::keyReleaseEvent(ev);
}

void CanvasView::contextMenuEvent(QContextMenuEvent* ev) {
    // Only inside a selection, matching how the rest of the app treats the
    // clipboard and transform actions.
    if (m_hasSelection && m_selection.contains(toImage(ev->pos()))) {
        emit selectionContextRequested();
        ev->accept();
        return;
    }
    ev->ignore();
}

// ------------------------------------------------- boundary resize ------

QRect CanvasView::handleWidgetRectFor(const QRectF& imageRect, int index) const {
    const QPointF tl = fromImage(imageRect.topLeft());
    const QPointF br = fromImage(imageRect.bottomRight());
    const QPointF ctr = (tl + br) / 2;
    QPointF center;
    switch (index) {
    case 0: center = tl; break;
    case 1: center = {ctr.x(), tl.y()}; break;
    case 2: center = {br.x(), tl.y()}; break;
    case 3: center = {br.x(), ctr.y()}; break;
    case 4: center = br; break;
    case 5: center = {ctr.x(), br.y()}; break;
    case 6: center = {tl.x(), br.y()}; break;
    case 7: center = {tl.x(), ctr.y()}; break;
    default: center = tl; break;
    }
    const qreal hs = 4;
    return QRect(QPoint(qRound(center.x() - hs), qRound(center.y() - hs)),
                 QPoint(qRound(center.x() + hs), qRound(center.y() + hs)));
}

QRect CanvasView::handleWidgetRect(int index) const {
    return handleWidgetRectFor(QRectF(QPointF(0, 0), QSizeF(imageSize())), index);
}

int CanvasView::handleAtImageRect(const QRectF& imageRect, const QPointF& imagePt) const {
    const QPoint w = fromImage(imagePt).toPoint();
    for (int i = 0; i < 8; ++i)
        if (handleWidgetRectFor(imageRect, i).adjusted(1, 1, -1, -1).contains(w))
            return i;
    return -1;
}

int CanvasView::handleAtWidget(const QPointF& widget) const {
    for (int i = 0; i < 8; ++i)
        if (handleWidgetRect(i).adjusted(1, 1, -1, -1).contains(widget.toPoint()))
            return i;
    return -1;
}

void CanvasView::updateBoundaryResize(const QPointF& widget) {
    const QPointF delta = toImage(widget) - toImage(m_boundaryStartWidget);
    const QSize img = imageSize();
    int l = 0, t = 0, r = img.width(), b = img.height();
    const int h = m_boundaryHandle;
    if (h == 0 || h == 6 || h == 7) l = qRound(delta.x());
    if (h == 0 || h == 1 || h == 2) t = qRound(delta.y());
    if (h == 2 || h == 3 || h == 4) r = img.width() + qRound(delta.x());
    if (h == 4 || h == 5 || h == 6) b = img.height() + qRound(delta.y());
    const int min = m_boundaryMin;
    if (r - l < min) { if (h >= 2 && h <= 4) r = l + min; else l = r - min; }
    if (b - t < min) { if (h >= 4 && h <= 6) b = t + min; else t = b - min; }
    m_boundaryLiveRect = QRect(l, t, r - l, b - t);
    emit boundaryResizePreview(m_boundaryLiveRect);
    update();
}

void CanvasView::finishBoundaryResize() {
    const QRect r = m_boundaryLiveRect;
    m_boundaryResize = false;
    m_boundaryHandle = -1;
    if (r.isEmpty() || r == QRect(QPoint(0, 0), imageSize())) {
        update();
        return;
    }
    applyBoundaryResize(r);
}

void CanvasView::applyBoundaryResize(const QRect& r) {
    if (m_floatingActive) weldFloating();
    clearSelection();
    const QSize newSize(r.width(), r.height());
    const QList<Layer> before = m_stack->layers();
    const int beforeActive = m_stack->activeIndex();
    QList<Layer> after;
    for (int i = 0; i < m_stack->count(); ++i) {
        const Layer& src = m_stack->layerAt(i);
        QImage out(newSize, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.drawImage(-r.left(), -r.top(), src.image);
        p.end();
        Layer l = src;
        l.image = out;
        after << l;
    }
    m_stack->replaceAll(after, m_stack->activeIndex());
    m_undo->push(Commands::makeLayerList(m_stack, before, beforeActive, tr("Resize canvas")));
}

// ------------------------------------------------------------ painting ---

void CanvasView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Theme::tokens().workspace);
    drawWorkspace(p);
    if (m_tool) {
        p.save();
        p.translate(m_canvasOrigin);
        p.scale(m_zoom, m_zoom);
        m_tool->paintOverlay(p, this);
        p.restore();
    }
}

void CanvasView::drawWorkspace(QPainter& p) {
    const QSize img = imageSize();
    const QSizeF c(img.width() * m_zoom, img.height() * m_zoom);

    p.save();
    p.translate(m_canvasOrigin);
    p.scale(m_zoom, m_zoom);

    // canvas content. The background layer holds no pixels, so its colour is
    // filled here; with the background hidden the checkerboard shows through.
    const int bg = m_stack->backgroundIndex();
    if (bg >= 0 && m_stack->layerAt(bg).visible) {
        p.fillRect(QRect(QPoint(0, 0), img), m_stack->layerAt(bg).backgroundColor);
    } else {
        Draw::checkerboard(p, QRect(QPoint(0, 0), img), 8,
                           Theme::tokens().checkerLight, Theme::tokens().checkerDark);
    }
    // Blit quality is a user preference: smooth blends the pixels when the
    // document is scaled, nearest keeps them as hard-edged blocks. Only the
    // screen is affected — the layers themselves are never resampled here.
    const bool smooth = Settings::antialiasCanvas() &&
                        !(Settings::crispPixelsWhenMagnified() && m_zoom > 1.0);
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    for (int i = m_stack->count() - 1; i >= 0; --i) {
        const Layer& l = m_stack->layerAt(i);
        if (!l.visible || l.isBackground || l.image.isNull()) continue;
        p.drawImage(QPointF(0, 0), l.image);
    }
    if (m_floatingActive)
        p.drawImage(m_floatingPos, m_floating);
    p.restore();

    // border
    const QRectF border(m_canvasOrigin, c);
    p.setPen(QPen(Theme::tokens().canvasBorder, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(border);

    drawSelectionOverlay(p);
    drawSelectionHandles(p);
    drawObjectOverlay(p);
    drawObjectHandles(p);
    drawBoundaryPreview(p);
    drawBoundaryHandles(p);
}

void CanvasView::drawSelectionOverlay(QPainter& p) {
    QRectF sel;
    if (m_makeSelection)
        sel = m_selectLive;
    else if (m_hasSelection)
        sel = m_selection;
    else
        return;

    const QRectF w = fromImage(sel).adjusted(0.5, 0.5, -0.5, -0.5);
    QPen pen(Theme::tokens().selectionA, 1);
    pen.setCosmetic(true);
    QVector<qreal> dashes{5, 4};
    pen.setDashPattern(dashes);
    p.setBrush(Qt::NoBrush);

    // underlay
    QPen white(Theme::tokens().selectionB, 1);
    white.setCosmetic(true);
    p.setPen(white);
    p.drawRect(w);
    pen.setDashOffset(m_dashOffset);
    p.setPen(pen);
    p.drawRect(w);

    // corner marks for the selection
    const qreal hs = 4;
    const QColor sc = Theme::tokens().selectionB;
    p.setBrush(sc);
    p.setPen(Qt::NoPen);
    const qreal x0 = w.left(), y0 = w.top(), x1 = w.right(), y1 = w.bottom();
    p.drawRect(QRectF(QPointF(x0, y0), QPointF(x0 + hs, y0 + hs)));
    p.drawRect(QRectF(QPointF(x1 - hs, y0), QPointF(x1, y0 + hs)));
    p.drawRect(QRectF(QPointF(x0, y1 - hs), QPointF(x0 + hs, y1)));
    p.drawRect(QRectF(QPointF(x1 - hs, y1 - hs), QPointF(x1, y1)));
}

void CanvasView::drawSelectionHandles(QPainter& p) {
    if (!m_hasSelection || m_makeSelection || m_hasObject) return;
    if (!m_tool || m_tool->id() != ToolId::Select) return;
    for (int i = 0; i < 8; ++i) {
        const QRect r = handleWidgetRectFor(m_selection, i);
        const bool hover = m_selResizing && m_selHandle == i;
        p.fillRect(r, Theme::tokens().handle);
        p.setPen(QPen(hover ? Theme::tokens().handleHover : Theme::tokens().handleOutline, 1));
        p.drawRect(r);
    }
}

void CanvasView::drawObjectOverlay(QPainter& p) {
    if (!m_hasObject) return;
    if (m_object.layer < 0 || m_object.layer >= m_stack->count()) return;

    // transient vector shape; drawn live until it is baked on let-go
    p.save();
    p.translate(m_canvasOrigin);
    p.scale(m_zoom, m_zoom);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintShapeObject(p, m_object);
    p.restore();

    // marquee around the object bounds
    const QRectF w = fromImage(m_object.rect).adjusted(0.5, 0.5, -0.5, -0.5);
    QPen a(Theme::tokens().selectionA, 1);
    a.setCosmetic(true);
    QVector<qreal> dashes{5, 4};
    a.setDashPattern(dashes);
    p.setBrush(Qt::NoBrush);
    QPen b(Theme::tokens().selectionB, 1);
    b.setCosmetic(true);
    p.setPen(b);
    p.drawRect(w);
    a.setDashOffset(m_dashOffset);
    p.setPen(a);
    p.drawRect(w);
}

void CanvasView::drawObjectHandles(QPainter& p) {
    if (!m_hasObject) return;
    if (!m_tool || (m_tool->id() != ToolId::Select &&
                    dynamic_cast<ShapeTool*>(m_tool) == nullptr))
        return;
    for (int i = 0; i < 8; ++i) {
        const QRect r = handleWidgetRectFor(m_object.rect, i);
        const bool hover = m_objectDragging && m_objectHandle == i;
        p.fillRect(r, Theme::tokens().handle);
        p.setPen(QPen(hover ? Theme::tokens().handleHover : Theme::tokens().handleOutline, 1));
        p.drawRect(r);
    }
}

void CanvasView::drawBoundaryHandles(QPainter& p) {
    if (!m_showBoundaryHandles) return;
    const QColor outline = Theme::tokens().handleOutline;
    for (int i = 0; i < 8; ++i) {
        const QRect r = handleWidgetRect(i);
        const bool hover = m_boundaryResize && m_boundaryHandle == i;
        p.fillRect(r, Theme::tokens().handle);
        p.setPen(QPen(hover ? Theme::tokens().handleHover : outline, 1));
        p.drawRect(r);
    }
}

void CanvasView::drawBoundaryPreview(QPainter& p) {
    if (!m_boundaryResize) return;
    const QRectF w = fromImage(QRectF(m_boundaryLiveRect)).adjusted(0.5, 0.5, -0.5, -0.5);
    QPen pen(Theme::tokens().handleHover, 1, Qt::DashLine);
    pen.setCosmetic(true);
    pen.setDashOffset(m_dashOffset);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(w);
}
