#include "CanvasView.h"

#include "Commands.h"
#include "DrawingUtils.h"
#include "LayerStack.h"
#include "Theme.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QUndoStack>
#include <QWheelEvent>

namespace {
constexpr int kPad = 20; // room for boundary handles
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 8.0;
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
}

void CanvasView::setBrushSize(int s) {
    m_brushSize = qBound(1, s, 128);
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
        1.0, 1.25, 1.50, 1.75, 2.00, 2.50, 3.00, 4.00
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

void CanvasView::setZoom(qreal z) {
    z = snapZoom(z);
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
    const int needW = qCeil(img.width() * m_zoom) + 2 * kPad;
    const int needH = qCeil(img.height() * m_zoom) + 2 * kPad;
    const int w = qMax(needW, vp.width());
    const int h = qMax(needH, vp.height());
    setFixedSize(w, h);
    m_canvasOrigin = QPointF((w - img.width() * m_zoom) / 2.0,
                             (h - img.height() * m_zoom) / 2.0);

    // Center the view when we opened or auto-fit.
    if (m_scrollArea && m_autoFit) {
        const QRect centered = QRect(QPoint(0, 0), QSize(qMax(w - vp.width(), 0),
                                                         qMax(h - vp.height(), 0)));
        m_scrollArea->ensureVisible(centered.width() / 2.0, centered.height() / 2.0);
    }
}

void CanvasView::resizeEvent(QResizeEvent* ev) {
    updateViewSize();
    QWidget::resizeEvent(ev);
}

void CanvasView::wheelEvent(QWheelEvent* ev) {
    if (ev->modifiers() & Qt::ControlModifier) {
        const bool in = ev->angleDelta().y() > 0;
        if (in) zoomIn(); else zoomOut();
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

void CanvasView::weldFloating() {
    if (!m_floatingActive) return;
    const int layer = activeLayerIndex();
    const QPoint pos = m_floatingPos.toPoint();
    Draw::blit(m_stack->layerAt(layer).image, m_floating, pos);
    markDirty(layer, QRect(pos, m_floating.size()));
    m_floatingActive = false;
    m_floating = QImage();
    commitEdit(tr("Move selection"));
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
    if (!patches.isEmpty())
        m_undo->push(Commands::makePaint(m_stack, patches, text));
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
    const QRect r = imageRect.normalized().toAlignedRect()
                        .intersected(QRect(QPoint(0, 0), imageSize()));
    if (r.width() < 1 || r.height() < 1) return;
    if (m_floatingActive) weldFloating();
    clearSelection();
    if (r == QRect(QPoint(0, 0), imageSize())) return;
    const QList<Layer> before = m_stack->layers();
    QList<Layer> after;
    for (int i = 0; i < m_stack->count(); ++i) {
        Layer l = m_stack->layerAt(i);
        l.image = l.image.copy(r);
        after << l;
    }
    m_stack->replaceAll(after, m_stack->activeIndex());
    m_undo->push(Commands::makeLayerList(m_stack, before, tr("Crop")));
}

void CanvasView::setCanvasSize(const QSize& size) {
    if (size == imageSize() || size.isEmpty()) return;
    if (m_floatingActive) weldFloating();
    clearSelection();
    const QList<Layer> before = m_stack->layers();
    m_stack->setSize(size);
    m_undo->push(Commands::makeLayerList(m_stack, before, tr("Resize canvas")));
}

void CanvasView::rotateCanvas(qreal degrees) {
    if (m_floatingActive) weldFloating();
    clearSelection();
    const QList<Layer> before = m_stack->layers();
    QList<Layer> after;
    for (int i = 0; i < m_stack->count(); ++i) {
        Layer l = m_stack->layerAt(i);
        l.image = Draw::rotateImage(l.image, degrees);
        after << l;
    }
    m_stack->replaceAll(after, m_stack->activeIndex());
    m_undo->push(Commands::makeLayerList(m_stack, before, tr("Rotate canvas")));
}

void CanvasView::transformSelection(const QSize& targetSize, int rotateDegrees) {
    if (!m_hasSelection) return;
    if (m_floatingActive) weldFloating();
    const QRect src = selectionPixelRect().intersected(QRect(QPoint(0, 0), imageSize()));
    if (src.isEmpty()) return;

    const int layer = activeLayerIndex();
    beginEdit(layer);
    QImage region = m_stack->layerAt(layer).image.copy(src);
    const QSize size = targetSize.isEmpty() ? src.size() : targetSize;
    if (size.width() > 0 && size.height() > 0 && size != region.size())
        region = region.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (rotateDegrees != 0)
        region = Draw::rotateImage(region, rotateDegrees);

    eraseRegion(layer, src);
    const QPointF center = QRectF(src).center();
    const QPoint topLeft((center.x() - region.width() / 2.0),
                         (center.y() - region.height() / 2.0));
    const QRect pasteRect(topLeft, region.size());
    Draw::blit(m_stack->layerAt(layer).image, region, pasteRect.topLeft());

    markDirty(layer, src.united(pasteRect).intersected(QRect(QPoint(0, 0), imageSize())));
    commitEdit(tr("Transform selection"));
    requestRepaint();
}

// --------------------------------------------------------- tools ---------

void CanvasView::setTool(Tool* tool) {
    if (tool == m_tool) return;
    if (m_tool) m_tool->onDeactivate(this);
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
    if (m_boundaryResize) { finishBoundaryResize(); }
    const int handle = (m_tool && m_tool->id() == ToolId::Select) ? handleAtWidget(ev->position()) : -1;
    if (handle >= 0 && ev->button() == Qt::LeftButton) {
        m_boundaryResize = true;
        m_boundaryHandle = handle;
        m_boundaryStartWidget = ev->position();
        m_boundaryLiveRect = QRect(QPoint(0, 0), imageSize());
        update();
        return;
    }
    if (m_tool) {
        m_tool->mousePress(this, ev);
        return;
    }
    QWidget::mousePressEvent(ev);
}

void CanvasView::mouseMoveEvent(QMouseEvent* ev) {
    if (m_boundaryResize) {
        updateBoundaryResize(ev->position());
        ev->accept();
        return;
    }
    // live boundary cursor for the select tool
    if (m_tool && m_tool->id() == ToolId::Select) {
        const int handle = handleAtWidget(ev->position());
        Qt::CursorShape cur = Qt::ArrowCursor;
        if (handle >= 0) {
            switch (handle) {
            case 0: case 4: cur = Qt::SizeFDiagCursor; break;
            case 2: case 6: cur = Qt::SizeBDiagCursor; break;
            case 1: case 5: cur = Qt::SizeVerCursor; break;
            default: cur = Qt::SizeHorCursor;
            }
        }
        setCursor(cur);
    }
    if (m_tool) {
        m_tool->mouseMove(this, ev);
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* ev) {
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

void CanvasView::keyPressEvent(QKeyEvent* ev) {
    if (m_tool) {
        m_tool->keyPress(this, ev);
        if (ev->isAccepted()) return;
    }
    QWidget::keyPressEvent(ev);
}

// ------------------------------------------------- boundary resize ------

QRect CanvasView::handleWidgetRect(int index) const {
    const QSize img = imageSize();
    const QSizeF c(img.width() * m_zoom, img.height() * m_zoom);
    const qreal x = m_canvasOrigin.x();
    const qreal y = m_canvasOrigin.y();
    const qreal hs = 7;
    QPointF center;
    switch (index) {
    case 0: center = {x, y}; break;
    case 1: center = {x + c.width() / 2, y}; break;
    case 2: center = {x + c.width(), y}; break;
    case 3: center = {x + c.width(), y + c.height() / 2}; break;
    case 4: center = {x + c.width(), y + c.height()}; break;
    case 5: center = {x + c.width() / 2, y + c.height()}; break;
    case 6: center = {x, y + c.height()}; break;
    case 7: center = {x, y + c.height() / 2}; break;
    }
    return QRect(QPoint(center.x() - hs, center.y() - hs),
                 QPoint(center.x() + hs, center.y() + hs));
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
    m_undo->push(Commands::makeLayerList(m_stack, before, tr("Resize canvas")));
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

    // canvas content
    Draw::checkerboard(p, QRect(QPoint(0, 0), img), 8,
                       Theme::tokens().checkerLight, Theme::tokens().checkerDark);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int i = m_stack->count() - 1; i >= 0; --i) {
        const Layer& l = m_stack->layerAt(i);
        if (!l.visible) continue;
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

void CanvasView::drawBoundaryHandles(QPainter& p) {
    if (!m_tool || m_tool->id() != ToolId::Select) return;
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
