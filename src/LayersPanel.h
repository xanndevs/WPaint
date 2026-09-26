#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QToolButton;
class LayerStack;

// Right-side layers dock. Row order matches the stack (index 0 = topmost).
// Emits requests that MainWindow turns into QUndoCommands.
class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(LayerStack* stack, QWidget* parent = nullptr);

    void refresh();
    void setActiveLayer(int index);

    // The caption strip holding the title, the layer count and the add/remove
    // buttons. MainWindow hands this to QDockWidget::setTitleBarWidget so the
    // dock doesn't paint a second, competing title above it.
    QWidget* headerWidget() const { return m_header; }

signals:
    void activeRequested(int layerIndex);
    void visibilityRequested(int layerIndex, bool visible);
    void moveRequested(int from, int to);
    void addRequested();
    void removeRequested(int layerIndex);
    void renameRequested(int layerIndex, const QString& name);

private:
    void rebuildList();
    void onCurrentRowChanged(int row);
    void applyActiveProperty(int activeRow);

    LayerStack* m_stack;
    QListWidget* m_list;
    QLabel* m_count;
    QWidget* m_header = nullptr;
    QToolButton* m_add;
    QToolButton* m_remove;
    bool m_syncing = false;
};