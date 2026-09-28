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

    // Where a newly created layer or group goes, and which folder counts it as a
    // child. The two are one answer and cannot be derived from each other: "the
    // bottom of the group" and "one past the last child" are the same index and
    // mean different things, so the index and the intent travel together.
    struct NewEntrySpot {
        int index = 0;
        int owner = -1; // the folder that gains a child, or -1
        // "Work the owner out from the index", for a caller that has only an
        // index. Deliberately not 0 or -1: both are places a real owner can be.
        static constexpr int kFromIndex = -2;
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
    // Returns where the layer landed, which is not `index` if the background
    // pushed it up: the shell makes the new layer active, and "the new layer" is
    // only meaningful if it knows which one that is.
    int addLayer(int index, const Layer& layer);
    // The same, for a caller that was handed a NewEntrySpot and means it.
    int addLayer(const NewEntrySpot& spot, const Layer& layer);
    void removeLayer(int index);
    void moveLayer(int from, int to); // reorder around the active index
    // Moves the run of `count` entries that starts at `from`, so that it lands
    // immediately before whatever entry was at `to` -- the index the caller had
    // *before* the move. `intoFolder` puts it at the top of that folder instead
    // of between rows. Returns where it landed, or -1 when the move is not one
    // that can be made.
    //
    // The count is the caller's, not something guessed from `from`, and that is
    // the whole point of the parameter: a run that starts with a plain layer and
    // goes on into a folder -- a layer and the group under it, two groups side
    // by side -- is longer than "a layer, or a folder and its contents", and
    // guessing moved the first entry and left the rest behind. The rail knows
    // the run: it is what `resolveSelection` returned.
    //
    // The count sits *between* the two indices on purpose. A trailing bool
    // after two ints is a trap: `moveSpan(4, 1, true)` under the old signature
    // -- the layer, and the group's index -- keeps compiling and means "move
    // entry 4 to index 1", a different move entirely. Nothing catches it.
    int moveSpan(int from, int count, int to, bool intoFolder = false);
    bool canMoveSpan(int from, int count, int to, bool intoFolder = false) const;
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
    // An empty group, for when there is nothing to group yet. Placed by the same
    // rule as a new layer, so a group and a layer added one after the other do
    // not end up in two different parts of the document. Returns its index, or
    // -1 on a document with no background to sit above.
    int addFolder(bool folded, NewLayerPlacement placement = NewLayerPlacement::AboveAll,
                  int anchor = -1, bool stayInGroup = false);

    NewEntrySpot newEntrySpot(NewLayerPlacement placement, int anchor,
                              bool stayInGroup) const;
    // Just the index, for callers that do not care which folder claims it.
    int newEntryIndex(NewLayerPlacement placement, int anchor, bool stayInGroup) const;
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

    // ---- merging ----
    // The entry below `i` that would take its pixels, or -1 when there is none.
    // A folder skips its own run, so merging one down flattens it and hands the
    // result to whatever sits below the group.
    int mergeTargetOf(int i) const;
    bool canMergeDown(int i) const;
    bool canMergeSelection(const QList<int>& selection) const;
    // Both return the index of the merged layer -- the topmost entry of the run,
    // which is the only place in the list the result could keep -- or -1.
    int mergeDown(int i);
    int mergeSelected(const QList<int>& selection);

    // Clamps every folder's childCount to the entries that actually follow it --
    // it does NOT re-derive them. A count is only ever grown where a run is
    // deliberately put inside that folder, which is why moveSpan() credits the
    // folder the caller named rather than looking for one: an empty group claims
    // nothing, so a lookup cannot find it. Public because MainWindow removes runs
    // of layers itself when the selection spans a group boundary.
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
    // Where a dragged run comes to rest, as an index in the list with the run
    // lifted out, or -1 when the move cannot be made. Shared by canMoveSpan()
    // and moveSpan() so the two can never disagree about what is legal.
    int spanDestination(int from, int count, int to, bool intoFolder) const;
    // The insert that fixes up the folder that gained a child, and returns where
    // the entry landed. addLayer() and addFolder() both go through here.
    // `owner` is NewEntrySpot::kFromIndex for a caller that only has an index.
    int insertEntry(int index, const Layer& layer,
                    int owner = NewEntrySpot::kFromIndex);
    // Draws entries first..last (inclusive) bottom-up into one image, topmost
    // last. A negative `first` means the whole list; withBackground paints the
    // backdrop colour under them, and onlyVisible is what the canvas shows
    // rather than what a merge is asked to carry.
    QImage flatten(int first, int last, bool withBackground, bool onlyVisible) const;
    // The one step both merges are made of: a contiguous run becomes a single
    // plain layer at the run's top index.
    int mergeRun(const QList<int>& run);
    QList<Layer> m_layers;
    int m_active = 0;
};