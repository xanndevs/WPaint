#pragma once

#include "Layer.h"

#include <QList>
#include <QObject>
#include <QString>

// Ordered list of layers. The image (e.g. 816x461) defines the canvas size.
// Index 0 = topmost layer (drawn last), like Windows 11 Paint.
class LayerStack : public QObject {
    Q_OBJECT
public:
    struct SaveResult {
        bool ok = false;
        QString error;
    };

    explicit LayerStack(QObject* parent = nullptr);

    const QList<Layer>& layers() const { return m_layers; }
    const Layer& layerAt(int i) const { return m_layers.at(i); }
    Layer& layerAt(int i) { return m_layers[i]; }
    int count() const { return m_layers.size(); }
    QSize size() const;

    int activeIndex() const { return m_active; }
    Layer& activeLayer() { return m_layers[m_active]; }
    const Layer& activeLayer() const { return m_layers.at(m_active); }

    // The background layer always lives at the bottom of the stack, so it is
    // the last entry (index 0 is topmost). -1 when the document has none.
    int backgroundIndex() const;
    bool hasBackground() const { return backgroundIndex() >= 0; }
    // The colour to flatten over when exporting a format with no alpha.
    // Transparent-background documents fall back to white, since JPEG/BMP/GIF
    // cannot represent transparency.
    QColor exportBackdrop() const;

    // Flattened rendering of the visible layers onto a transparent image.
    QImage composite() const;

    // Bounds of non-transparent content across all layers (used by "fit to
    // content"-style ops). Returns null rect if empty.
    QRect contentBounds() const;

    // --- Mutators. Each emits changed(); undo commands wrap several calls.
    void setSize(const QSize& size); // resize every layer (crop or pad)
    void replaceAll(const QList<Layer>& layers, int activeIndex);
    void addLayer(int index, const Layer& layer);
    void removeLayer(int index);
    void moveLayer(int from, int to); // reorder around the active index
    void setLayerVisible(int i, bool visible);
    void setBackgroundColor(int i, const QColor& color);
    void renameLayer(int i, const QString& name);
    void setActiveIndex(int i);

    // ---- folders ----
    // A folder owns the `childCount` entries after it, so every question about
    // the tree is answered by walking the list rather than by nesting it.
    bool isFolder(int i) const;
    bool isChildOfFolder(int i) const;  // inside any folder, at any depth
    bool isInsideFoldedFolder(int i) const;
    int depthOf(int i) const;           // how many folders enclose it
    int childCountOf(int i) const;
    QList<int> childrenOf(int i) const; // the direct children, in stack order
    // Every index the entry takes with it, itself included: for a folder, its
    // whole run including nested groups.
    QList<int> spanOf(int i) const;
    // The outermost folder enclosing `i`, or -1.
    int owningFolder(int i) const;
    void setFolded(int i, bool folded);
    // Wraps one contiguous selection in a folder and returns its index, or -1
    // when the selection is not a single run. `folded` is the new folder's
    // initial state; the preference that decides it lives in Settings.
    int groupInto(const QList<int>& selection, bool folded);
    bool ungroup(int folderIndex);
    // Removes a selection -- folders, their children and nested groups -- as one
    // structural step, fixing up the counts of the folders that survive it.
    bool removeSpans(const QList<int>& selection);

    // A legal paint target: a real layer, not a folder, not the background, and
    // not hidden inside a folded one.
    bool isDrawable(int i) const;
    // The active layer, or -1 when it cannot be painted on.
    int drawableIndex() const;

    // Turns a selection into the index runs the structural operations work on:
    // folders expand to their children, a child whose folder is also selected is
    // dropped (the folder carries it), and the result is merged and sorted.
    QList<QList<int>> resolveSelection(const QList<int>& selection) const;

    // A layer may only go if a real, visible-to-the-user layer survives it.
    bool canRemove(int i) const;
    bool canRemoveAny(const QList<int>& selection) const;
    int layerCount() const; // excluding the background
    // Re-derives every folder's childCount from the entries that follow it.
    // Public because MainWindow removes runs of layers itself when the
    // selection spans a group boundary.
    void fixChildCounts();

    QString nextName(const QString& base) const;
    void clear();

    // Appends a default background layer to the bottom of the stack. Callers
    // that want one should go through here rather than addLayer(), which
    // deliberately refuses background entries.
    void addBackgroundLayer(const QColor& color);

    // Native layered project format (.wpa).
    SaveResult saveProject(const QString& path) const;
    SaveResult loadProject(const QString& path);

signals:
    void changed();
    void activeChanged(int index);

private:
    void clampActive();
    QList<Layer> m_layers;
    int m_active = 0;
};