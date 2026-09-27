#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class LayerStack;

// Right-side layers dock. Row order matches the stack (index 0 = topmost).
// Emits requests that MainWindow turns into QUndoCommands.
//
// The background is deliberately *not* a row: it has its own pinned section
// under the list, it is not counted in "Layers (n)", and clicking it opens the
// colour picker rather than selecting a paint target. Everything else about the
// rail is the stack in order, with a folder drawn as a folder -- indented
// children, a chevron that folds it.
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(LayerStack* stack, QWidget* parent = nullptr);

    void refresh();
    // Repaint just the previews; cheaper than a full rebuild and keeps the
    // selection, eye buttons and hover state intact.
    void updateThumbnails();
    void setActiveLayer(int index);
    // Select a set of rows, without going through activeRequested.
    void setSelection(const QList<int>& indices);

    QList<int> selectedIndices() const;
    // The layer the user last touched, which is not necessarily the active one
    // after a ctrl-click extends the selection. A stack index, not a row: the
    // callers that want it are working in stack indices.
    int lastSelectedIndex() const { return m_lastSelected; }

    // The caption strip holding the title, the layer count and the
    // folder/add/remove buttons. MainWindow hands this to
    // QDockWidget::setTitleBarWidget so the dock doesn't paint a second,
    // competing title above it.
    QWidget* headerWidget() const { return m_header; }

    // Open the inline rename on a row, from the keyboard or a menu rather than a
    // double-click.
    void beginRenameAt(int index);

    // A drag of the current selection was dropped on this row -- ontoFolder says
    // the row is a group and the drop meant "inside it". False when the drop was
    // one that cannot be made, which the caller says out loud. Public because
    // Qt will not route a synthetic drop event, and the rule is worth testing.
    bool handleDrop(int row, bool ontoFolder);

    bool hasFocus() const;

signals:
    void activeRequested(int layerIndex);
    void visibilityRequested(int layerIndex, bool visible);
    // A dragged run of layers, from its first index to the index it should land
    // before -- or, with intoFolder, at the top of that folder instead.
    void moveRequested(int from, int to, bool intoFolder);
    // The drop was one the model will not make, e.g. a selection with a hole in
    // it. The shell says so in a toast rather than doing it anyway.
    void moveRefused();
    void addRequested();
    void removeRequested(int layerIndex);
    void removeSelectionRequested(QList<int> selection);
    void renameRequested(int layerIndex, const QString& name);
    void folderRequested(QList<int> selection);
    void foldedRequested(int layerIndex, bool folded);
    // The background swatch was clicked; MainWindow opens the colour picker
    // and turns the result into an undoable layer change.
    void backgroundEditRequested(int layerIndex);
    // A right-click, with the rows it covers.
    void contextRequested(QList<int> selection, QPoint at);
    // A rename finished on one row; MainWindow renames the rest of the
    // selection from the same base name.
    void renameFinished(int layerIndex, const QString& name,
                        QList<int> alsoSelected);

private:
    void rebuildList();
    void onCurrentRowChanged(int row);
    void updateRowLook(QWidget* row, QListWidgetItem* item, bool active);
    void applyActiveProperty(int activeRow);
    void updateHeaderState();
    // The rail's rows and the stack's indices are two different things as soon
    // as a group is folded, and every "which row/which layer" question has to
    // cross between them explicitly. Mixing them up is how the shift anchor
    // ended up pointing at the wrong row.
    int rowOfIndex(int index) const;
    int indexOfRow(int row) const;
    void startRename(int index);
    void commitRename();
    // Handles shift-click ourselves; see the implementation for why.
    bool eventFilter(QObject* watched, QEvent* ev) override;

    LayerStack* m_stack;
    QListWidget* m_list;
    QLabel* m_count;
    QWidget* m_header = nullptr;
    QToolButton* m_folder;
    QToolButton* m_add;
    QToolButton* m_remove;
    QWidget* m_backgroundBar = nullptr;
    bool m_syncing = false;
    // The stack index of the layer the user last touched, or -1. Always a stack
    // index, never a row: a folded group makes the two differ, and the callers
    // of this (the rename rule, the shift anchor) are both in stack indices.
    int m_lastSelected = -1;
    // The inline rename editor, when one is open.
    int m_renamingIndex = -1;
    QLineEdit* m_renameEdit = nullptr;
    QList<int> m_renameAlso;
};
