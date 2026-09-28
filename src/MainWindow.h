#pragma once

#include "Layer.h"
#include "Tool.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QMimeData>
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
class FluentToast;
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
    // An image file dragged in from a file manager, or an image dragged straight
    // out of a browser or an image viewer. CanvasView does not accept drops, so
    // these are on the window and they see everything.
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dragMoveEvent(QDragMoveEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;

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

public slots:
    // Everything a drop of `mime` at `windowPos` (window coordinates) comes to.
    // A slot because Qt delivers real drops through the drag manager and will not
    // route a synthetic one to a widget, so this is the only way the routing --
    // which is all of the deciding here -- can be reached from outside.
    bool handleDroppedImage(const QMimeData* mime, const QPointF& windowPos);
    // Whether a payload is an image the window could take, decided without
    // reading a pixel of it. Public because this is the question the drag
    // manager asks on every pointer move of a drag -- dozens of times a second
    // -- and the answer it wants is a yes or a no. Decoding the file to give it
    // one meant re-reading a full-resolution image each time, which froze the
    // app for as long as an image was anywhere near the window. All that is
    // needed to know whether a file is an image is its first few bytes.
    static bool canAcceptDrop(const QMimeData* mime);
    // "Place to a new layer" from the drop prompt, which is the one placement
    // the dialog offers and the one nothing else reaches.
    void placeImageOnLayerForTest(const QImage& img) { placeImageOnLayer(img); }
    // A second window with its own layers and its own history. A slot because
    // the drop prompt asks for it and because "open this in its own window" is a
    // thing worth being able to ask for directly.
    void openBlankWindow();
    // The same, for a file: a second window with the file already in it. Also
    // how the command line opens a second and third file, which is what makes
    // "Open with WPaint" work when a file manager hands over more than one.
    void openInNewWindow(const QString& file);
    // Opening is one thing with several callers -- the File menu, a file dropped
    // on the window, a second window, the command line -- so it is one function.
    // False when the file could not be opened, or when the user declined to lose
    // what is there. hideBackground is for a flat image that is being *opened by
    // a drop*: the image is the document, and a white backdrop under a
    // transparent PNG makes its transparency look like white.
    //
    // Public because the command line opens the file the desktop entry was
    // invoked with: without an entry point here, "Open with WPaint" starts the
    // app on a blank canvas and ignores the file it was handed, which is exactly
    // the bug the desktop entry is supposed to be the fix for.
    bool openFile(const QString& file, bool hideBackground = false);

    // A document that has never been drawn on, resized or otherwise touched, and
    // has never been saved. A file dropped on one of those needs no question
    // asked; on anything else it does. Not the same question as "are there
    // unsaved changes": a file opened from disk is not pristine either.
    bool isDocumentPristine() const {
        return m_documentPristine && m_currentPath.isEmpty();
    }

    // The rail's own verbs. Public because the panel's actions, the rail's
    // scoped shortcuts and the context menu all three reach them, and because
    // "copy these layers" is a thing worth being able to ask for from outside
    // the window rather than by synthesising a click.
    void copyLayers(const QList<int>& selection, bool cut);
    void pasteLayers(bool above);
    void removeLayers(const QList<int>& selection);
    void groupLayers(const QList<int>& selection);
    void mergeLayerDown(int index);
    void mergeLayers(const QList<int>& selection);
    // Where a new layer or group goes, from the preference and the selection.
    int newLayerInsertIndex() const;
    void renameLayers(int index, const QString& name, const QList<int>& alsoSelected);
    void showToast(const QString& message);

private:
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

    // The two ways a dropped image can land on the canvas: as a floating object
    // where the pointer was, growing the canvas if it has to; or on a new layer,
    // placed by the same rule as Add layer.
    void placeImageOnCanvas(const QImage& img, const QPointF& atCanvas, bool inside);
    void placeImageOnLayer(const QImage& img);
    void openDocument();
    // False when the file dialog was cancelled, so a caller that is about to
    // replace the document -- confirmDiscard() -- can decline to.
    bool saveDocument();
    bool saveDocumentAs();
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
    void renameLayer(int index, const QString& name);
    void showLayerContextMenu(const QList<int>& selection, const QPoint& at);
    void buildLayerClipboardActions();
    void toggleLayerVisibility(int index, bool visible);
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
    FluentToast* m_toast = nullptr;
    QAction* m_pasteAboveAction = nullptr;
    QAction* m_pasteBelowAction = nullptr;
    // Layers copied or cut in the rail, in rail order. Separate from the image
    // clipboard on purpose: pasting a picture and pasting five layers are
    // different acts and must not share a buffer.
    QList<Layer> m_layerClipboard;
    SizeSliderPanel* m_sizePanel;
    LayersPanel* m_layersPanel;
    CopilotPanel* m_copilotPanel;
    QDockWidget* m_layersDock;
    QDockWidget* m_copilotDock;

    QString m_currentPath;
    // Has anything happened to this document since it was created? Not the same
    // question as "are there unsaved changes": a document opened from a file has
    // never been drawn on and is not pristine either, and a new one that has
    // been drawn on is not -- which is what makes the two questions different
    // for anything that wants to know whether a dropped file can be taken
    // without asking (see the file drop). Driven by the undo index, so undoing
    // back to the start makes a document pristine again.
    bool m_documentPristine = true;
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