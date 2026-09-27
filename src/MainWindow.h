#pragma once

#include "Tool.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QString>

#include <functional>

namespace Theme {
enum class Pref;
}

class QAction;
class QCloseEvent;
class QDockWidget;
class QEnterEvent;
class QContextMenuEvent;
class QEvent;
class QLabel;
class QMenu;
class QToolButton;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
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

// Full-size palette swatch. QToolButton can't hold a small fixed size here:
// the generic QToolButton stylesheet rule inflates every styled instance to
// ~28px, so swatches are plain widgets painted edge-to-edge.
class PaletteButton : public QWidget {
    Q_OBJECT
public:
    PaletteButton(const QColor& col, int size, QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void enterEvent(QEnterEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    QColor m_col;
    bool m_hover = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    CanvasView* canvas() const { return m_canvas; }

protected:
    void closeEvent(QCloseEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    // QMainWindow's default context menu is the stock widget/dock list, which
    // pops a bare "Layers" entry over the toolbar on any right-click nothing
    // else claimed. The canvas has its own selection menu, so this is only ever
    // noise -- swallow it here.
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    // construction
    void buildActions();
    void buildMenuBar();
    void buildToolbar();
    void buildCentral();
    void buildDocks();
    // Push every live setting into the widgets that read it. Called once at
    // startup and again whenever the table reports a change, so a preference
    // never has two sources of truth.
    void applySettings();
    // Re-read every binding from the table; called when the Shortcuts page
    // changes one, so a rebind takes effect without a restart.
    void applyShortcuts();
    void buildPaletteGrid();
    void buildLayerActions(const std::function<QAction*(QAction*, const QString&)>& bind);
    void setThemePreference(Theme::Pref pref);
    QSize documentSizeFromDefaults() const;
    void buildStatusBar();
    void placeSizePanel();
    bool eventFilter(QObject* watched, QEvent* ev) override;
    // `cols` standard buttons per row; a wpBig control always takes a column
    // of its own spanning both rows.
    QWidget* toolCluster(const QString& caption, const QList<QWidget*>& controls,
                         int cols = 2);
    QWidget* divider();
    QToolButton* toolButtonFor(ToolId id, int size = 0);

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
    void showSelectionContextMenu();
    void updateEditActions();

    // layers
    void addLayer();
    void removeLayer(int index);
    void toggleLayerVisibility(int index, bool visible);
    void renameLayer(int index, const QString& name);
    void editBackgroundColor(int index);
    void runLayerCommand(const QString& text, std::function<void()> mutate);

    // model + view
    LayerStack* m_stack;
    QUndoStack* m_undo;
    CanvasView* m_canvas;
    QScrollArea* m_scrollArea;
    QAction* m_boundaryHandlesAction = nullptr;
    QHash<QString, QAction*>* m_shortcutActions = nullptr;
    // The image transforms live here as well as in actions, because the toolbar
    // buttons and the keyboard must call the same code: two copies of a rotate
    // is how a shortcut ends up doing something subtly different from its button.
    QHash<QString, std::function<void()>> m_imageTransforms;
    QMenu* m_shapeStyleMenu = nullptr;
    QToolButton* m_layersToggle = nullptr;
    QWidget* m_paletteHost = nullptr;
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