#pragma once

#include "Tool.h"

#include <QBrush>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QTimer>
#include <QWidget>

class QScrollArea;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
class LayerStack;
class QUndoStack;
class ShapeTool;

// A shape kept in vector form so it can be moved/resized without raster
// rescaling. It stays transcendental (no pixels written) until the user
// "lets go"; then it is baked onto the layer as a single undo entry.
struct ShapeObject {
    int layer = -1;
    ToolId shape = ToolId::ShapeRect;
    ShapeStyle style = ShapeStyle::Outline;
    qreal penWidth = 1.0;
    QColor penColor;
    QColor fillColor;
    QRectF rect;            // bounding box in image coords
    QPointF a, b;           // line/curve endpoints
    QPointF c1, c2;         // curve bend controls
};

// Renders and edits the layered image. Coordinates: "widget coords" are
// logical pixels of this widget; "image coords" are pixels of the document.
// zoom() maps image -> widget.
class CanvasView : public QWidget {
    Q_OBJECT
public:
    CanvasView(LayerStack* stack, QUndoStack* undo, QWidget* parent = nullptr);

    // ---- model ----
    LayerStack* stack() const { return m_stack; }
    QUndoStack* undoStack() const { return m_undo; }
    QColor primary() const { return m_primary; }
    QColor secondary() const { return m_secondary; }
    void setColors(const QColor& p, const QColor& s);

    int brushSize() const { return m_brushSize; }
    void setBrushSize(int s);
    BrushStyle brushStyle() const { return m_brushStyle; }
    void setBrushStyle(BrushStyle s) { m_brushStyle = s; requestRepaint(); }

    // ---- coordinates ----
    QPointF toImage(const QPointF& widget) const;
    QRectF toImage(const QRectF& widget) const;
    QPointF fromImage(const QPointF& image) const;
    QRectF fromImage(const QRectF& image) const;
    QSize imageSize() const;
    QPointF canvasOrigin() const { return m_canvasOrigin; }
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);
    void zoomIn();
    void zoomOut();
    void zoomActual();
    void zoomFit();
    void reflow() { updateViewSize(); } // re-center after viewport resize
    void attachScrollArea(QScrollArea* sa) { m_scrollArea = sa; reflow(); }

    // ---- selection (image coords, normalized & clamped) ----
    bool hasSelection() const { return m_hasSelection; }
    QRectF selection() const { return m_selection; }
    void setSelection(const QRectF& r);
    void clearSelection();
    QRect selectionPixelRect() const;

    // ---- boundary resize handles ----
    bool boundaryHandlesEnabled() const { return m_showBoundaryHandles; }
    void setBoundaryHandlesEnabled(bool on);

    // ---- live vector shape object ----
    bool hasActiveObject() const { return m_hasObject; }
    void attachShapeObject(int layer, ToolId shape, ShapeStyle style, qreal penWidth,
                           const QColor& penColor, const QColor& fillColor,
                           const QRectF& rect, const QPointF& a, const QPointF& b,
                           const QPointF& c1, const QPointF& c2);
    void clearActiveObject();   // discard a transient object (Escape)
    void bakeActiveObject();    // paint the transient object onto its layer

    // ---- compositing ----
    QImage composite() const;
    QImage compositeRegion(const QRectF& imageRect) const;

    // ---- edit sessions (used by tools; undoable) ----
    void beginEdit(int layerIndex);
    void beginEditList(const QList<int>& layers);
    void markDirty(int layerIndex, const QRect& imageRect);
    void commitEdit(const QString& text);
    void revertActiveEdit();

    // ---- convenience for tools ----
    QImage& layerImage(int i);
    int activeLayerIndex() const;
    void setActiveLayer(int i);

    // ---- floating selection / clipboard machinery ----
    void liftSelection();                       // cut region to floating
    void pasteFloating(const QImage& img, const QPointF& topLeft);
    void weldFloating();                        // bake floating into active layer
    void cancelFloating();
    void commitFloatingRemoval(const QString& text); // discard floating pixels
    bool floatingActive() const { return m_floatingActive; }
    void clearSelectionRegion(const QString& text);   // erase region on active layer
    QImage copySelection() const;

    // ---- whole-image ops ----
    void cropTo(const QRectF& imageRect);
    void setCanvasSize(const QSize& size);
    void rotateCanvas(qreal degrees);
    void flipCanvas(Qt::Orientation orientation);
    void rotateSelection(qreal degrees);
    void flipSelection(Qt::Orientation orientation);
    void transformSelection(const QSize& targetSize, int rotateDegrees);

    // ---- tools ----
    Tool* tool() const { return m_tool; }
    void setTool(Tool* tool);
    ShapeTool* shapeTool() const { return m_shapeTool; }
    ShapeKit::Shape currentShape() const;
    void setShape(ShapeKit::Shape shape);
    ShapeStyle currentShapeStyle() const;
    void setShapeStyle(ShapeStyle style);

    int marqueeOffset() const { return m_dashOffset; }

    QString pendingStatus() const { return m_pendingStatus; }

    // selection drag machinery (used by tools)
    void beginSelectionDrag(const QPointF& imgAnchor);
    void updateSelectionDrag(const QPointF& imgCursor);
    void endSelectionDrag();
    QRectF selectionDragLive() const;
    void cancelFloatingLift();

    // floating drag machinery (used by tools)
    bool isFloatDragging() const;
    void beginFloatDrag(const QPointF& widgetPos);
    void moveFloatDrag(const QPointF& widgetPos);
    void endFloatDrag();

public slots:
    void requestRepaint() { update(); }

signals:
    void selectionChanged();
    void toolChanged(Tool* tool);
    void boundaryResizePreview(const QRect& imageRect);
    void zoomChanged(qreal zoom);
    void colorPicked(const QColor& color, bool primary);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;

private:
    void updateViewSize();
    void drawWorkspace(QPainter& p);
    void drawSelectionOverlay(QPainter& p);
    void drawSelectionHandles(QPainter& p);
    void drawObjectOverlay(QPainter& p);
    void drawObjectHandles(QPainter& p);
    void drawBoundaryHandles(QPainter& p);
    void drawBoundaryPreview(QPainter& p);
    QRect handleWidgetRect(int index) const;
    QRect handleWidgetRectFor(const QRectF& imageRect, int index) const;
    int handleAtWidget(const QPointF& widget) const;
    int handleAtImageRect(const QRectF& imageRect, const QPointF& imagePt) const;
    void updateBoundaryResize(const QPointF& widget);
    void finishBoundaryResize();
    void applyBoundaryResize(const QRect& imageRect);
    void eraseRegion(int layer, const QRect& rect);
    QPointF mapObjectPoint(const QRectF& fromR, const QRectF& toR,
                           const QPointF& p) const;
    void paintShapeObject(QPainter& p, const ShapeObject& o) const;
    void beginSelectionResize(int handle, const QPointF& widget);
    void updateSelectionResize(const QPointF& widget, bool mirror);
    void finishSelectionResize();
    void beginObjectResize(int handle, const QPointF& widget);
    void beginObjectMove(const QPointF& widget);
    void updateObjectDrag(const QPointF& widget, bool mirror);
    void finishObjectDrag();

    // Paint session state: snapshot + dirty boxes for one undo entry.
    QHash<int, QImage> m_sessionBefore;
    QHash<int, QRect> m_sessionDirty;
    bool m_sessionOpen = false;

    LayerStack* m_stack;
    QUndoStack* m_undo;
    QScrollArea* m_scrollArea = nullptr;
    Tool* m_tool = nullptr;
    ShapeTool* m_shapeTool = nullptr;

    qreal m_zoom = 1.0;
    bool m_autoFit = true;
    QPointF m_canvasOrigin;

    // selection / floating state (image coords)
    QRectF m_selection;
    bool m_hasSelection = false;
    bool m_floatingActive = false;
    QImage m_floating;
    QPointF m_floatingPos; // top-left, image coords
    bool m_floatMoving = false;
    QPointF m_floatDragAnchor;   // widget coords where drag started
    QPointF m_floatPosAtDrag;    // image pos at drag start

    // in-progress marquee
    bool m_makeSelection = false;
    QPointF m_selectAnchor; // image coords
    QRectF m_selectLive;

    // canvas boundary resize (live preview handled in paint)
    bool m_showBoundaryHandles = true;
    bool m_boundaryResize = false;
    int m_boundaryHandle = -1;
    QPointF m_boundaryStartWidget;
    QRect m_boundaryLiveRect;
    int m_boundaryMin = 16;

    // live vector shape object
    bool m_hasObject = false;
    ShapeObject m_object;
    ShapeObject m_objectOrig;
    bool m_objectDragging = false;
    int m_objectHandle = -1;
    QPointF m_objectStartWidget;
    bool m_mirrorResize = false;

    // selection free-resize
    bool m_selResizing = false;
    int m_selHandle = -1;
    QRectF m_selOrig;
    QPointF m_selStartWidget;
    QImage m_selFloatOrig;

    int m_brushSize = 4;
    BrushStyle m_brushStyle = BrushStyle::Round;
    QColor m_primary = QColor("#000000");
    QColor m_secondary = QColor("#FFFFFF");

    bool m_selectionDirty = false;
    int m_dashOffset = 0;
    QTimer m_antsTimer;

    QString m_pendingStatus;
};