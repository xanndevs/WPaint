#pragma once

#include "Tool.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QString>

#include <functional>

class QAction;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QScrollArea;
class QSlider;
class QToolButton;
class CanvasView;
class CopilotPanel;
class LayerStack;
class LayersPanel;
class QUndoStack;
class SizeSliderPanel;

// Corner-cap color well: two overlapping swatches (primary in front).
class ColorWellButton : public QWidget {
    Q_OBJECT
public:
    explicit ColorWellButton(QWidget* parent = nullptr);

    QColor primary() const { return m_primary; }
    QColor secondary() const { return m_secondary; }
    void setColors(const QColor& p, const QColor& s);

signals:
    void primaryClicked();
    void secondaryClicked();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    QColor m_primary = QColor("#000000");
    QColor m_secondary = QColor("#FFFFFF");
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    CanvasView* canvas() const { return m_canvas; }

protected:
    void closeEvent(QCloseEvent* ev) override;

private:
    // construction
    void buildActions();
    void buildMenuBar();
    void buildToolbar();
    void buildCentral();
    void buildDocks();
    void buildStatusBar();
    QWidget* toolCluster(const QString& caption, const QList<QWidget*>& controls);
    QWidget* divider();
    QToolButton* toolButtonFor(ToolId id);

    // behavior
    void selectTool(ToolId id);
    void applyShape(ShapeKit::Shape shape);
    void applyShapeStyle(ShapeStyle style);
    void applyBrushStyle(BrushStyle style);
    void syncToolButtons();
    void syncColorWell();
    void onZoomChanged(qreal zoom);
    void syncStatusSize();
    void openResizeDialog();
    void editColor(bool primary);

    // file ops
    bool confirmDiscard();
    void newDocument();
    void openDocument();
    void saveDocument();
    void saveDocumentAs();
    bool saveTo(const QString& path);
    QImage exportedComposite() const;
    void updateWindowTitle();
    void installDocument(const LayerStack& loaded, const QString& path);

    // edit ops
    void doUndo();
    void doRedo();
    void doCut();
    void doCopy();
    void doPaste();
    void doSelectAll();
    void doDelete();
    void updateEditActions();

    // layers
    void addLayer();
    void removeLayer(int index);
    void toggleLayerVisibility(int index, bool visible);
    void renameLayer(int index, const QString& name);
    void runLayerCommand(const QString& text, std::function<void()> mutate);

    // model + view
    LayerStack* m_stack;
    QUndoStack* m_undo;
    CanvasView* m_canvas;
    QScrollArea* m_scrollArea;
    SizeSliderPanel* m_sizePanel;
    LayersPanel* m_layersPanel;
    CopilotPanel* m_copilotPanel;
    QDockWidget* m_layersDock;
    QDockWidget* m_copilotDock;

    QString m_currentPath;
    QImage m_copied;
    QPointF m_copiedTopLeft;

    // toolbar
    QHash<ToolId, QToolButton*> m_toolButtons;
    QToolButton* m_shapeButton;   // shape gallery (menu button popup)
    QToolButton* m_shapeStyleButton; // outline/fill pattern
    QToolButton* m_brushButton;   // brush gallery keep-current

    ColorWellButton* m_well;
    QList<QToolButton*> m_palette;

    // status bar
    QLabel* m_sizeStatus;
    QLabel* m_zoomLabel;
    QSlider* m_zoomSlider;
    QToolButton* m_zoomOutBtn;
    QToolButton* m_zoomInBtn;
    bool m_zooming = false;

    // actions
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_deleteAction;
    QAction* m_selectAllAction;
};