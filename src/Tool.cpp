#include "Tool.h"

#include "CanvasView.h"
#include "DrawingUtils.h"
#include "LayerStack.h"

#include <QColorDialog>
#include <QFontComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>
#include <cmath>

// =====================================================================
// ShapeKit
// =====================================================================

namespace ShapeKit {

static QPolygonF regularPolygon(const QRectF& r, int sides, qreal rotationDeg) {
    QPolygonF pts;
    const QPointF c = r.center();
    const qreal rad = qMin(r.width(), r.height()) / 2.0;
    for (int i = 0; i < sides; ++i) {
        const qreal ang = qDegreesToRadians(rotationDeg + qreal(i) * 360.0 / sides - 90.0);
        pts << QPointF(c.x() + rad * std::cos(ang), c.y() + rad * std::sin(ang));
    }
    return pts;
}

QPainterPath path(Shape shape, const QRectF& r, const QPainterPath& curve) {
    if (shape == Shape::ShapeCurve)
        return curve;
    QPainterPath p;
    const QPointF tl = r.topLeft();
    const QPointF br = r.bottomRight();
    const QPointF c = r.center();
    switch (shape) {
    case Shape::ShapeLine:
        p.moveTo(tl);
        p.lineTo(br);
        break;
    case Shape::ShapeRect:
        p.addRect(r);
        break;
    case Shape::ShapeRounded:
        p.addRoundedRect(r, qMax(r.width(), 1.0) * 0.2, qMax(r.height(), 1.0) * 0.2);
        break;
    case Shape::ShapeEllipse:
        p.addEllipse(r);
        break;
    case Shape::ShapeTriangle: {
        QPolygonF pts;
        pts << QPointF(c.x(), tl.y()) << br << QPointF(tl.x(), br.y());
        p.addPolygon(pts);
        p.closeSubpath();
        break;
    }
    case Shape::ShapeRightTriangle: {
        QPolygonF pts;
        pts << tl << QPointF(br.x(), tl.y()) << QPointF(tl.x(), br.y());
        p.addPolygon(pts);
        p.closeSubpath();
        break;
    }
    case Shape::ShapeDiamond: {
        QPolygonF pts;
        pts << QPointF(c.x(), tl.y()) << QPointF(br.x(), c.y())
            << QPointF(c.x(), br.y()) << QPointF(tl.x(), c.y());
        p.addPolygon(pts);
        p.closeSubpath();
        break;
    }
    case Shape::ShapePentagon:
        p.addPolygon(regularPolygon(r, 5, 0));
        p.closeSubpath();
        break;
    case Shape::ShapeArrow: {
        const qreal w = r.width();
        const qreal h = r.height();
        const qreal tail = tl.x() + w * 0.62;
        const qreal th = qMax<qreal>(h * 0.28, 2.0);
        QPolygonF pts;
        pts << QPointF(tl.x(), c.y() - th / 2)
            << QPointF(tail, c.y() - th / 2)
            << QPointF(tail, tl.y())
            << QPointF(br.x(), c.y())
            << QPointF(tail, br.y())
            << QPointF(tail, c.y() + th / 2)
            << QPointF(tl.x(), c.y() + th / 2);
        p.addPolygon(pts);
        p.closeSubpath();
        break;
    }
    default:
        break;
    }
    return p;
}

void draw(QPainter& p, Shape shape, const QRectF& r, ShapeStyle style,
          const QPen& pen, const QBrush& brush, const QPainterPath& curve) {
    if (shape == Shape::ShapeLine || shape == Shape::ShapeCurve) {
        // Lines are always stroked (fill has no area).
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(shape == Shape::ShapeCurve ? curve
                                              : path(shape, r, curve));
        return;
    }
    const QPainterPath path_ = path(shape, r, curve);
    switch (style) {
    case ShapeStyle::Outline:
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        break;
    case ShapeStyle::Fill:
        p.setPen(Qt::NoPen);
        p.setBrush(brush);
        break;
    case ShapeStyle::OutlineFill:
        p.setPen(pen);
        p.setBrush(brush);
        break;
    }
    p.drawPath(path_);
}

QString shapeName(Shape shape) {
    switch (shape) {
    case Shape::ShapeLine: return QObject::tr("Line");
    case Shape::ShapeCurve: return QObject::tr("Curve");
    case Shape::ShapeRect: return QObject::tr("Rectangle");
    case Shape::ShapeRounded: return QObject::tr("Rounded rectangle");
    case Shape::ShapeEllipse: return QObject::tr("Ellipse");
    case Shape::ShapeTriangle: return QObject::tr("Triangle");
    case Shape::ShapeRightTriangle: return QObject::tr("Right triangle");
    case Shape::ShapeDiamond: return QObject::tr("Diamond");
    case Shape::ShapePentagon: return QObject::tr("Pentagon");
    case Shape::ShapeArrow: return QObject::tr("Arrow");
    default: return QObject::tr("Shape");
    }
}

} // namespace ShapeKit

// =====================================================================
// Stroke tool (pencil / brush / eraser)
// =====================================================================

class StrokeTool : public Tool {
public:
    enum Kind { Pencil, Brush, Eraser };
    StrokeTool(Kind k, ToolId id, const QString& name)
        : m_kind(k), m_id(id), m_name(name) {}

    ToolId id() const override { return m_id; }
    QString name() const override { return m_name; }
    bool supportsBrushSize() const override { return true; }
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }

    void mousePress(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton && ev->button() != Qt::RightButton) return;
        m_button = ev->button();
        m_last = c->toImage(ev->position());
        c->beginEdit(c->activeLayerIndex());
        paintAt(c, m_last, m_last);
    }

    void mouseMove(CanvasView* c, QMouseEvent* ev) override {
        const QPointF cur = c->toImage(ev->position());
        if (ev->buttons() & m_button) {
            paintAt(c, m_last, cur);
            m_last = cur;
        }
    }

    void mouseRelease(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() == m_button) {
            const QPointF cur = c->toImage(ev->position());
            paintAt(c, m_last, cur);
            m_last = cur;
            c->commitEdit(m_name);
        }
        m_button = Qt::NoButton;
    }

private:
    void paintAt(CanvasView* c, const QPointF& from, const QPointF& to) {
        QImage& img = c->layerImage(c->activeLayerIndex());
        const bool right = m_button == Qt::RightButton;
        const QColor color = right ? c->secondary() : c->primary();
        const int w = qMax(1, c->brushSize());
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, m_kind != Pencil);

        if (m_kind == Eraser) {
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            QPen pen(Qt::SolidPattern);
            pen.setWidth(w);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(from, to);
            if (w == 1)
                p.drawPoint(to);
        } else if (m_kind == Brush && c->brushStyle() == BrushStyle::Spray) {
            spray(p, to, w, color);
        } else if (m_kind == Brush && c->brushStyle() == BrushStyle::Calligraphy) {
            calligraphy(p, from, to, w, color);
        } else {
            QPen pen(color);
            pen.setWidth(w);
            pen.setJoinStyle(Qt::RoundJoin);
            if (m_kind == Pencil) {
                pen.setCapStyle(Qt::FlatCap);
            } else {
                pen.setCapStyle(c->brushStyle() == BrushStyle::Square ? Qt::FlatCap
                                                                      : Qt::RoundCap);
            }
            p.setPen(pen);
            p.drawLine(from, to);
            p.drawPoint(to);
        }

        const int pad = w / 2 + 2;
        const QRect dirty = QRectF(from, to).normalized()
                                .adjusted(-pad, -pad, pad, pad)
                                .toAlignedRect()
                                .intersected(QRect(QPoint(0, 0), img.size()));
        c->markDirty(c->activeLayerIndex(), dirty);
        c->requestRepaint();
    }

    void spray(QPainter& p, const QPointF& at, int w, const QColor& color) {
        QColor col = color;
        col.setAlpha(90);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        const int radius = qMax(2, (int)std::ceil(w * 1.6));
        const int n = w * 5 + 4;
        for (int i = 0; i < n; ++i) {
            const qreal ang = QRandomGenerator::global()->generateDouble() * 2 * M_PI;
            const qreal rr = QRandomGenerator::global()->generateDouble() * radius;
            const QPointF pt = at + QPointF(rr * std::cos(ang), rr * std::sin(ang));
            p.drawEllipse(pt, qMax(0.5, radius * 0.18), qMax(0.5, radius * 0.18));
        }
    }

    void calligraphy(QPainter& p, const QPointF& from, const QPointF& to,
                     int w, const QColor& color) {
        const QLineF seg(from, to);
        const qreal steps = qMax<qreal>(1, seg.length() / (qMax(w, 2) * 0.4));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const qreal hw = w / 2.0;
        const qreal hh = qMax(1.0, w * 0.35);
        for (int i = 0; i <= steps; ++i) {
            const QPointF pt = seg.pointAt(i / steps);
            p.save();
            p.translate(pt);
            p.rotate(45);
            p.drawEllipse(QPointF(0, 0), hw, hh);
            p.restore();
        }
    }

    Kind m_kind;
    ToolId m_id;
    QString m_name;
    QPointF m_last;
    Qt::MouseButton m_button = Qt::NoButton;
};

// =====================================================================
// Fill tool
// =====================================================================

class FillTool : public Tool {
public:
    ToolId id() const override { return ToolId::Fill; }
    QString name() const override { return QObject::tr("Fill"); }
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }

    void mousePress(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton && ev->button() != Qt::RightButton) return;
        const int layer = c->activeLayerIndex();
        const QPoint seed = c->toImage(ev->position()).toPoint();
        const QImage& imgRef = c->layerImage(layer);
        if (seed.x() < 0 || seed.y() < 0 || seed.x() >= imgRef.width() ||
            seed.y() >= imgRef.height())
            return;
        const bool right = ev->button() == Qt::RightButton;
        const QColor color = right ? c->secondary() : c->primary();
        c->beginEdit(layer);
        QImage img = imgRef; // detaches below inside floodFill
        if (Draw::floodFill(img, seed, color, kTolerance))
            c->layerImage(layer) = img;
        c->markDirty(layer, QRect(QPoint(0, 0), imgRef.size()));
        c->commitEdit(name());
    }

    static constexpr int kTolerance = 32; // ~12.5%; configurable assumption
};

// =====================================================================
// Eyedropper tool
// =====================================================================

class EyedropperTool : public Tool {
public:
    ToolId id() const override { return ToolId::Eyedropper; }
    QString name() const override { return QObject::tr("Eyedropper"); }
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }

    void mousePress(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton && ev->button() != Qt::RightButton) return;
        QImage comp = c->composite();
        if (comp.isNull()) return;
        const QPoint pt = c->toImage(ev->position()).toPoint();
        if (pt.x() < 0 || pt.y() < 0 || pt.x() >= comp.width() || pt.y() >= comp.height())
            return;
        QColor col = comp.pixelColor(pt);
        if (col.alpha() < 255) {
            // Sample as it appears against white (MS Paint has no real
            // transparency; composite visually over white).
            const qreal a = col.alphaF();
            col.setRedF(col.redF() + (1 - a));
            col.setGreenF(col.greenF() + (1 - a));
            col.setBlueF(col.blueF() + (1 - a));
            col.setAlpha(255);
        }
        const bool primary = ev->button() == Qt::LeftButton;
        emit c->colorPicked(col, primary);
    }
};

// =====================================================================
// Select tool
// =====================================================================

class SelectTool : public Tool {
public:
    ToolId id() const override { return ToolId::Select; }
    QString name() const override { return QObject::tr("Select"); }

    void mousePress(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton) return;
        const QPointF img = c->toImage(ev->position());
        const bool ctrl = (ev->modifiers() & Qt::ControlModifier);

        if (c->floatingActive() && !c->hasSelection())
            c->weldFloating();

        if (c->hasSelection() && c->selection().contains(img) && !ctrl) {
            if (!c->floatingActive())
                c->liftSelection();
            c->beginFloatDrag(ev->position());
        } else {
            if (c->floatingActive())
                c->weldFloating();
            c->beginSelectionDrag(img);
        }
    }

    void mouseMove(CanvasView* c, QMouseEvent* ev) override {
        if (ev->buttons() & Qt::LeftButton) {
            if (c->isFloatDragging())
                c->moveFloatDrag(ev->position());
            else
                c->updateSelectionDrag(c->toImage(ev->position()));
        }
    }

    void mouseRelease(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton) return;
        if (c->isFloatDragging()) {
            c->endFloatDrag();
        } else {
            c->endSelectionDrag();
        }
    }

    void keyPress(CanvasView* c, QKeyEvent* ev) override {
        if (ev->key() == Qt::Key_Escape) {
            if (c->floatingActive())
                c->cancelFloatingLift();
            else
                c->clearSelection();
        }
    }
};

// =====================================================================
// Crop tool
// =====================================================================

class CropTool : public Tool {
public:
    ToolId id() const override { return ToolId::Crop; }
    QString name() const override { return QObject::tr("Crop"); }

    void mousePress(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton) return;
        if (c->floatingActive())
            c->weldFloating();
        if (c->hasSelection()) {
            c->cropTo(c->selection());
            c->clearSelection();
            return;
        }
        c->beginSelectionDrag(c->toImage(ev->position()));
    }

    void mouseMove(CanvasView* c, QMouseEvent* ev) override {
        if (ev->buttons() & Qt::LeftButton)
            c->updateSelectionDrag(c->toImage(ev->position()));
    }

    void mouseRelease(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton) return;
        const QRectF sel = c->selectionDragLive();
        c->clearSelection();
        if (sel.width() >= 2 && sel.height() >= 2)
            c->cropTo(sel);
    }
};

// =====================================================================
// Shape tool
// =====================================================================

ShapeTool::ShapeTool() : m_shape(ToolId::ShapeRect) {}

QString ShapeTool::name() const { return ShapeKit::shapeName(m_shape); }

void ShapeTool::setShape(ToolId shape) {
    if (shape == m_shape) return;
    m_shape = shape;
    resetGesture();
}

void ShapeTool::mousePress(CanvasView* c, QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) return;
    const QPointF p = c->toImage(ev->position());
    if (m_shape == ToolId::ShapeCurve) {
        if (m_bendArm != 0) {
            m_bending = true;
            if (m_bendArm == 1)
                m_c1 = p;
            else
                m_c2 = p;
            c->requestRepaint();
            return;
        }
        if (m_active)
            commitGesture(c);
        m_active = true;
        m_bendArm = 0;
        m_c1 = m_c2 = QPointF();
        m_a = m_b = p;
    } else {
        if (m_active)
            commitGesture(c);
        m_active = true;
        m_a = m_b = p;
    }
    c->requestRepaint();
}

void ShapeTool::mouseMove(CanvasView* c, QMouseEvent* ev) {
    const QPointF p = c->toImage(ev->position());
    if (m_bending) {
        if (m_bendArm == 1)
            m_c1 = p;
        else if (m_bendArm == 2)
            m_c2 = p;
    } else if (m_active) {
        m_b = p;
    } else if (m_bendArm != 0) {
        if (m_bendArm == 1) m_c1 = p;
        else m_c2 = p;
    }
    c->requestRepaint();
}

void ShapeTool::mouseRelease(CanvasView* c, QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) return;
    if (m_bending) {
        m_bending = false;
        if (m_shape == ToolId::ShapeCurve) {
            if (m_bendArm == 1) {
                m_bendArm = 2;
            } else if (m_bendArm == 2) {
                commitGesture(c);
            }
        }
    } else if (m_shape == ToolId::ShapeCurve) {
        if (m_active && !m_bendArm) {
            m_bendArm = 1; // arm the first bend
        }
    } else if (m_active) {
        commitGesture(c);
    }
    c->requestRepaint();
}

void ShapeTool::keyPress(CanvasView* c, QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Escape) {
        if (m_bending) { m_bending = false; m_bendArm = 0; }
        else if (m_active) { resetGesture(); }
        else if (m_bendArm) { m_bendArm = 0; }
        m_c1 = m_c2 = QPointF();
        c->requestRepaint();
        ev->accept();
    }
}

void ShapeTool::onDeactivate(CanvasView* c) {
    if (m_active)
        commitGesture(c);
    else if (m_bendArm)
        resetGesture();
}

void ShapeTool::paintOverlay(QPainter& p, CanvasView* c) const {
    if (!m_active) return;
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPen pen = makePen(c);
    if (m_shape == ToolId::ShapeCurve) {
        const QPainterPath path = curvePath();
        if (!path.isEmpty())
            ShapeKit::draw(p, m_shape, QRectF(), m_style, pen, c->secondary(), path);
    } else {
        const QRectF r = QRectF(m_a, m_b).normalized();
        if (r.width() < 0.5 && r.height() < 0.5) return;
        ShapeKit::draw(p, m_shape, r, m_style, pen, c->secondary(), QPainterPath());
    }
}

QPen ShapeTool::makePen(CanvasView* c) const {
    QPen pen(c->primary());
    pen.setWidth(qMax(1, c->brushSize()));
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    return pen;
}

QPainterPath ShapeTool::curvePath() const {
    QPainterPath path;
    if (!m_active || m_shape != ToolId::ShapeCurve) return path;
    path.moveTo(m_a);
    const QPointF c1 = m_bendArm >= 1 ? m_c1 : m_a;
    const QPointF c2 = m_bendArm >= 2 ? m_c2 : m_b;
    path.cubicTo(c1, c2, m_b);
    return path;
}

void ShapeTool::resetGesture() {
    m_active = false;
    m_bending = false;
    m_bendArm = 0;
    m_c1 = m_c2 = QPointF();
}

void ShapeTool::commitGesture(CanvasView* c) {
    if (!m_active && m_bendArm == 0 && !m_bending) return;
    const int layer = c->activeLayerIndex();
    c->beginEdit(layer);
    QPainter p(&c->layerImage(layer));
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen = makePen(c);
    const QRectF r = QRectF(m_a, m_b).normalized();
    const QPainterPath curve = curvePath();
    ShapeKit::draw(p, m_shape, r, m_style, pen, c->secondary(), curve);
    p.end();

    QRect dirty;
    if (m_shape == ToolId::ShapeCurve)
        dirty = curve.boundingRect()
                    .adjusted(-pen.width(), -pen.width(), pen.width(), pen.width())
                    .toAlignedRect()
                    .intersected(QRect(QPoint(0, 0), c->imageSize()));
    else
        dirty = r.adjusted(-pen.width(), -pen.width(), pen.width(), pen.width())
                    .toAlignedRect()
                    .intersected(QRect(QPoint(0, 0), c->imageSize()));
    c->markDirty(layer, dirty);
    c->commitEdit(name());
    resetGesture();
    c->requestRepaint();
}

// =====================================================================
// Text tool
// =====================================================================

class TextFormatBar;
class TextTool : public Tool {
public:
    ToolId id() const override { return ToolId::Text; }
    QString name() const override { return QObject::tr("Text"); }

    ~TextTool() override = default;

    void onDeactivate(CanvasView* c) override { commitForced(c, false); }
    void onZoomChanged(CanvasView* c) override { commitForced(c, false); }

    void mousePress(CanvasView* c, QMouseEvent* ev) override {
        if (ev->button() != Qt::LeftButton) return;
        if (m_editing) {
            commitForced(c, true);
            return;
        }
        c->beginSelectionDrag(c->toImage(ev->position()));
    }

    void mouseMove(CanvasView* c, QMouseEvent* ev) override {
        if (!m_editing && (ev->buttons() & Qt::LeftButton))
            c->updateSelectionDrag(c->toImage(ev->position()));
    }

    void mouseRelease(CanvasView* c, QMouseEvent* ev) override {
        if (m_editing) return;
        if (ev->button() != Qt::LeftButton) return;
        const QRectF r = c->selectionDragLive();
        c->clearSelection();
        const QRect box = r.normalized().toAlignedRect();
        if (box.width() >= 8 && box.height() >= 8)
            openOverlay(c, box);
    }

    void keyPress(CanvasView* c, QKeyEvent* ev) override {
        if (ev->key() == Qt::Key_Escape && m_editing) {
            cancelForced(c);
            ev->accept();
        } else if ((ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) &&
                   (ev->modifiers() & Qt::ControlModifier) && m_editing) {
            commitForced(c, true);
            ev->accept();
        }
    }

    bool editing() const { return m_editing; }

private:
    friend class TextFormatBar;
    void commitForced(CanvasView*, bool paint);
    void cancelForced(CanvasView*);
    void openOverlay(CanvasView* c, const QRect& boxImageRect);
    void repositionBar(CanvasView* c);

    bool m_editing = false;
    QTextEdit* m_overlay = nullptr;
    TextFormatBar* m_bar = nullptr;
    QRect m_boxImage;
    QColor m_textColor;
};

class TextFormatBar : public QWidget {
public:
    TextFormatBar(TextTool* tool, CanvasView* canvas, QWidget* parent)
        : QWidget(parent), m_tool(tool), m_canvas(canvas) {
        setObjectName("TextFormatBar");
        setAttribute(Qt::WA_DeleteOnClose);
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        m_fonts = new QFontComboBox(this);
        m_fonts->setMaximumWidth(150);
        lay->addWidget(m_fonts);

        m_size = new QSpinBox(this);
        m_size->setRange(6, 400);
        m_size->setValue(24);
        m_size->setMaximumWidth(54);
        lay->addWidget(m_size);

        m_bold = makeToggle(QObject::tr("B"));
        m_italic = makeToggle(QObject::tr("I"));
        m_underline = makeToggle(QObject::tr("U"));
        m_bold->setFont(QFont(font().family(), font().pointSize(), QFont::Bold));
        m_italic->setFont(QFont(font().family(), font().pointSize(), -1, true));
        m_underline->setFont(QFont(font().family(), font().pointSize(), -1, false));
        m_underline->setFont(m_underline->font());
        m_underline->setText(QObject::tr("U"));
        lay->addWidget(m_bold);
        lay->addWidget(m_italic);
        lay->addWidget(m_underline);

        m_color = new QPushButton(this);
        m_color->setFixedSize(22, 22);
        m_color->setCursor(Qt::PointingHandCursor);
        lay->addWidget(m_color);

        auto* commit = new QPushButton(QObject::tr("OK"), this);
        commit->setFixedWidth(46);
        lay->addWidget(commit);
        auto* cancel = new QPushButton(QObject::tr("Cancel"), this);
        cancel->setFixedWidth(60);
        lay->addWidget(cancel);

        connect(m_fonts, &QFontComboBox::currentFontChanged, this,
                &TextFormatBar::applyFormat);
        connect(m_size, qOverload<int>(&QSpinBox::valueChanged), this,
                &TextFormatBar::applyFormat);
        connect(m_bold, &QToolButton::clicked, this, &TextFormatBar::applyFormat);
        connect(m_italic, &QToolButton::clicked, this, &TextFormatBar::applyFormat);
        connect(m_underline, &QToolButton::clicked, this, &TextFormatBar::applyFormat);
        connect(m_color, &QPushButton::clicked, this, [this]() {
            const QColor col = QColorDialog::getColor(currentColor(), this, tr("Text color"));
            if (col.isValid()) {
                m_tool->m_textColor = col;
                m_color->setStyleSheet(
                    QStringLiteral("background:%1; border:1px solid %2; border-radius:4px;")
                        .arg(m_tool->m_textColor.name(),
                             m_tool->m_textColor.lightness() > 128 ? "#666" : "#ccc"));
                applyFormat();
            }
        });
        connect(commit, &QPushButton::clicked, this, [this]() {
            m_tool->commitForced(m_canvas, true);
        });
        connect(cancel, &QPushButton::clicked, this, [this]() {
            m_tool->cancelForced(m_canvas);
        });

        applyFormat();
        setVisible(true);
    }

    QColor currentColor() const { return m_tool->m_textColor; }

private:
    QToolButton* makeToggle(const QString& text) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setCheckable(true);
        b->setFixedSize(24, 24);
        b->setProperty("wpFlat", 1);
        return b;
    }

    void applyFormat() {
        QTextEdit* ov = m_tool->m_overlay;
        if (!ov) return;
        QFont f = m_fonts->currentFont();
        f.setPointSize(m_size->value());
        f.setBold(m_bold->isChecked());
        f.setItalic(m_italic->isChecked());
        f.setUnderline(m_underline->isChecked());
        ov->selectAll();
        ov->setCurrentFont(f);
        if (m_tool->m_textColor.isValid()) {
            ov->setTextColor(m_tool->m_textColor);
            ov->selectAll();
            ov->setTextColor(m_tool->m_textColor);
        }
        ov->setFocus();
    }

    TextTool* m_tool;
    CanvasView* m_canvas;
    QFontComboBox* m_fonts;
    QSpinBox* m_size;
    QToolButton* m_bold, *m_italic, *m_underline;
    QPushButton* m_color;
};

void TextTool::openOverlay(CanvasView* c, const QRect& boxImageRect) {
    m_boxImage = boxImageRect;
    m_textColor = c->primary();
    m_editing = true;

    const QRectF boxW = c->fromImage(QRectF(m_boxImage));
    const QRect boxWidget = boxW.toAlignedRect();

    m_overlay = new QTextEdit(c);
    m_overlay->setFrameShape(QFrame::NoFrame);
    m_overlay->setObjectName("TextOverlay");
    m_overlay->setStyleSheet(
        QStringLiteral("QTextEdit#TextOverlay{background:transparent;"
                       "border:1.5px dashed #6E6E6E;border-radius:0px;}"));
    m_overlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_overlay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_overlay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_overlay->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_overlay->setPlaceholderText(QObject::tr("Text"));
    QFont f;
    f.setPointSize(24);
    m_overlay->setFont(f);
    m_overlay->setTextColor(m_textColor);
    m_overlay->setGeometry(boxWidget);
    m_overlay->show();
    m_overlay->setFocus();

    m_bar = new TextFormatBar(this, c, c);
    // position above the box inside the canvas widget's coordinate space
    const QRect barGeo(0, 0, 460, 34);
    const int barX = qBound(4, boxWidget.left(), c->width() - barGeo.width() - 4);
    const int barY = boxWidget.top() - barGeo.height() - 6;
    m_bar->setGeometry(barX, barY >= 4 ? barY : qMax(4, boxWidget.bottom() + 6),
                       barGeo.width(), barGeo.height());
    m_bar->show();
    m_bar->raise();

    c->update();
}

void TextTool::commitForced(CanvasView* c, bool paint) {
    if (!m_editing) return;
    QTextEdit* ov = m_overlay;
    TextFormatBar* bar = m_bar;
    m_overlay = nullptr;
    m_bar = nullptr;
    m_editing = false;

    const QRect box = m_boxImage & QRect(QPoint(0, 0), c->imageSize());
    const QString html = ov->toHtml();
    const QFont font = ov->font();
    ov->deleteLater();
    if (bar) bar->deleteLater();

    if (!paint || box.isEmpty() || html.trimmed().isEmpty()) {
        c->update();
        return;
    }
    // WYSIWYG: the text box lives in widget pixels, the layer in image pixels.
    // Render the document scaled by 1/zoom so it lands at the same size seen
    // on screen.
    QTextDocument doc;
    doc.setHtml(html);
    doc.setDefaultFont(font);
    doc.setPageSize(QSizeF(box.width() * c->zoom(), box.height() * c->zoom()));

    const int layer = c->activeLayerIndex();
    c->beginEdit(layer);
    QPainter p(&c->layerImage(layer));
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const qreal s = 1.0 / c->zoom();
    p.translate(box.left(), box.top());
    p.scale(s, s);
    p.setPen(m_textColor);
    doc.drawContents(&p, QRectF(0, 0, box.width() * c->zoom(), box.height() * c->zoom()));
    p.end();
    c->markDirty(layer, box.adjusted(-2, -2, 2, 2));
    c->commitEdit(name());
    c->clearSelection();
    c->update();
}

void TextTool::cancelForced(CanvasView* c) {
    if (!m_editing) return;
    if (m_overlay) m_overlay->deleteLater();
    if (m_bar) m_bar->deleteLater();
    m_overlay = nullptr;
    m_bar = nullptr;
    m_editing = false;
    c->update();
}

// =====================================================================
// Tool registry
// =====================================================================

namespace ToolRegistry {

static const QList<Spec> g_specs = {
    {ToolId::Select, QObject::tr("Select"), "select", false},
    {ToolId::Crop, QObject::tr("Crop"), "crop", false},
    {ToolId::Pencil, QObject::tr("Pencil"), "pencil", true},
    {ToolId::Fill, QObject::tr("Fill"), "fill", false},
    {ToolId::Eraser, QObject::tr("Eraser"), "eraser", true},
    {ToolId::Eyedropper, QObject::tr("Eyedropper"), "eyedropper", false},
    {ToolId::Text, QObject::tr("Text"), "text", false},
    {ToolId::Brush, QObject::tr("Brush"), "brush", true},
    {ToolId::ShapeLine, QObject::tr("Line"), "shape-line", true, true},
    {ToolId::ShapeCurve, QObject::tr("Curve"), "shape-curve", true, true},
    {ToolId::ShapeRect, QObject::tr("Rectangle"), "shape-rect", true, true},
    {ToolId::ShapeRounded, QObject::tr("Rounded rectangle"), "shape-rounded", true, true},
    {ToolId::ShapeEllipse, QObject::tr("Ellipse"), "shape-ellipse", true, true},
    {ToolId::ShapeTriangle, QObject::tr("Triangle"), "shape-triangle", true, true},
    {ToolId::ShapeRightTriangle, QObject::tr("Right triangle"), "shape-rtriangle", true, true},
    {ToolId::ShapeDiamond, QObject::tr("Diamond"), "shape-diamond", true, true},
    {ToolId::ShapePentagon, QObject::tr("Pentagon"), "shape-pentagon", true, true},
    {ToolId::ShapeArrow, QObject::tr("Arrow"), "shape-arrow", true, true},
};

const QList<Spec>& specs() { return g_specs; }

const Spec& spec(ToolId id) {
    for (const Spec& s : g_specs)
        if (s.id == id) return s;
    return g_specs.first();
}

QString clusterName(ToolId id) {
    if (id == ToolId::Select) return QObject::tr("Selection");
    if (id == ToolId::Crop) return QObject::tr("Image");
    if (id >= ToolId::Pencil && id <= ToolId::Text) return QObject::tr("Tools");
    if (id == ToolId::Brush) return QObject::tr("Brushes");
    if (id >= ToolId::ShapeLine) return QObject::tr("Shapes");
    return QObject::tr("Tools");
}

Tool* create(ToolId id) {
    static QHash<ToolId, Tool*> cache;
    static ShapeTool* shapeTool = nullptr;

    if (id >= ToolId::ShapeLine) {
        if (!shapeTool) shapeTool = new ShapeTool();
        if (shapeTool->shape() != id) shapeTool->setShape(id);
        return shapeTool;
    }
    if (cache.contains(id)) return cache.value(id);
    Tool* tool = nullptr;
    switch (id) {
    case ToolId::Select: tool = new SelectTool(); break;
    case ToolId::Pencil: tool = new StrokeTool(StrokeTool::Pencil, id, QObject::tr("Pencil")); break;
    case ToolId::Brush: tool = new StrokeTool(StrokeTool::Brush, id, QObject::tr("Brush")); break;
    case ToolId::Eraser: tool = new StrokeTool(StrokeTool::Eraser, id, QObject::tr("Eraser")); break;
    case ToolId::Fill: tool = new FillTool(); break;
    case ToolId::Eyedropper: tool = new EyedropperTool(); break;
    case ToolId::Text: tool = new TextTool(); break;
    case ToolId::Crop: tool = new CropTool(); break;
    default: return nullptr;
    }
    cache.insert(id, tool);
    return tool;
}

} // namespace ToolRegistry