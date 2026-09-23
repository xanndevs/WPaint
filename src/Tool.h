#pragma once

#include <QBrush>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QString>

class QMouseEvent;
class QKeyEvent;
class QWheelEvent;
class CanvasView;

enum class ToolId {
    Select,
    Pencil,
    Brush,
    Eraser,
    Fill,
    Eyedropper,
    Text,
    Crop,
    ShapeLine,
    ShapeCurve,
    ShapeRect,
    ShapeRounded,
    ShapeEllipse,
    ShapeTriangle,
    ShapeRightTriangle,
    ShapeDiamond,
    ShapePentagon,
    ShapeArrow,
};

enum class BrushStyle { Round, Square, Spray, Calligraphy };

// Shape "fill rules" mirroring MS Paint's pattern choices.
enum class ShapeStyle { Outline, Fill, OutlineFill };

// Shape geometry library, shared by the live preview and the commit draw.
namespace ShapeKit {

using Shape = ToolId; // ShapeLine..ShapeArrow (leaf of ToolId)

QPainterPath path(Shape shape, const QRectF& rect, const QPainterPath& curve);

void draw(QPainter& p, Shape shape, const QRectF& rect, ShapeStyle style,
          const QPen& pen, const QBrush& brush, const QPainterPath& curve);

QString shapeName(Shape shape);

} // namespace ShapeKit

// A tool is a stateless-per-gesture object held by the CanvasView. Direct
// superclass for all editing gestures.
class Tool {
public:
    virtual ~Tool() = default;

    virtual ToolId id() const = 0;
    virtual QString name() const = 0;
    virtual bool supportsBrushSize() const { return false; }
    virtual Qt::CursorShape cursor() const { return Qt::ArrowCursor; }

    virtual void onActivate(CanvasView* canvas) { Q_UNUSED(canvas); }
    virtual void onDeactivate(CanvasView* canvas) { Q_UNUSED(canvas); }
    virtual void onZoomChanged(CanvasView* canvas) { Q_UNUSED(canvas); }

    // Transient overlay (previews). Painter is transformed to image coords.
    virtual void paintOverlay(QPainter& p, CanvasView* canvas) const {
        Q_UNUSED(p);
        Q_UNUSED(canvas);
    }

    virtual void mousePress(CanvasView* canvas, QMouseEvent* ev) {
        Q_UNUSED(canvas);
        Q_UNUSED(ev);
    }
    virtual void mouseMove(CanvasView* canvas, QMouseEvent* ev) {
        Q_UNUSED(canvas);
        Q_UNUSED(ev);
    }
    virtual void mouseRelease(CanvasView* canvas, QMouseEvent* ev) {
        Q_UNUSED(canvas);
        Q_UNUSED(ev);
    }
    virtual void mouseDoubleClick(CanvasView* canvas, QMouseEvent* ev) {
        Q_UNUSED(canvas);
        Q_UNUSED(ev);
    }
    virtual void keyPress(CanvasView* canvas, QKeyEvent* ev) {
        Q_UNUSED(canvas);
        Q_UNUSED(ev);
    }
};

// Shape gallery tool: one instance handles every shape kind; the active
// shape/style is set from the toolbar. Maintains live preview + the
// MS-Paint-style two-bend conversation for the curve tool.
class ShapeTool : public Tool {
public:
    ShapeTool();

    ToolId id() const override { return m_shape; }
    QString name() const override;
    bool supportsBrushSize() const override { return true; }
    bool isShapeTool() const { return true; }

    void setShape(ToolId shape);
    ToolId shape() const { return m_shape; }
    void setStyle(ShapeStyle s) { m_style = s; }
    ShapeStyle style() const { return m_style; }

    void mousePress(CanvasView* c, QMouseEvent* ev) override;
    void mouseMove(CanvasView* c, QMouseEvent* ev) override;
    void mouseRelease(CanvasView* c, QMouseEvent* ev) override;
    void keyPress(CanvasView* c, QKeyEvent* ev) override;
    void onDeactivate(CanvasView* c) override;
    void paintOverlay(QPainter& p, CanvasView* c) const override;

private:
    QPen makePen(CanvasView* c) const;
    QPainterPath curvePath() const;
    void resetGesture();
    void commitGesture(CanvasView* c);

    ToolId m_shape;
    ShapeStyle m_style = ShapeStyle::Outline;
    // gesture state
    bool m_active = false;
    bool m_bending = false;
    int m_bendArm = 0; // curve bend phase 0=none, 1=arm1, 2=arm2
    QPointF m_a, m_b;  // shape corners / curve endpoints
    QPointF m_c1, m_c2;
};

namespace ToolRegistry {

// All tools with their cluster metadata. Icons live under /assets/icons/.
struct Spec {
    ToolId id;
    QString name;   // menu/tooltip label
    QString icon;   // svg name in resources
    bool supportsSize = false;
    bool inShapes = false; // grouped under the Shapes gallery
};

const QList<Spec>& specs();
const Spec& spec(ToolId id);
QString clusterName(ToolId id);
Tool* create(ToolId id);

} // namespace ToolRegistry