#pragma once

#include <QColor>
#include <QImage>
#include <QString>

// One entry in the layer stack.
//
// QImage is implicitly shared, so value-copies used by the undo stack are cheap
// until a layer is actually painted on.
struct Layer {
    QString name;
    QImage image;   // ARGB32_Premultiplied
    bool visible = true;

    // The background layer sits at the very bottom of the stack and holds no
    // pixels: it is a flat colour applied across the whole canvas at composite
    // time. Storing it as a property rather than baked pixels is what lets it
    // survive crop, rotate and canvas resize unchanged.
    bool isBackground = false;
    QColor backgroundColor = QColor(Qt::white);

    // A folder holds the entries that follow it and has no pixels of its own.
    //
    // The stack stays a flat list: a folder at index i owns the childCount
    // entries after it, and nesting is expressed by a folder owning a folder.
    // That is what keeps undo (whole-list snapshots), the .wpa format and
    // composite() working unchanged -- none of them has to learn about a tree.
    // The cost is that a folder's children are a contiguous run, which is also
    // what a user expects: grouping four layers makes those four a group.
    bool isFolder = false;
    int childCount = 0;
    // Collapsed in the rail. Purely a view state, but it lives on the layer
    // because a saved project should reopen looking the way it was left, and
    // because a child of a folded folder is not visible, so it is not a legal
    // thing to draw on.
    bool folded = false;

    Layer() = default;
    Layer(const QString& n, const QImage& img, bool vis = true)
        : name(n), image(img), visible(vis) {}

    // A folder is not a thing you can paint on; neither is anything inside one
    // that is folded, because it is not on screen.
    bool isDrawable() const { return !isBackground && !isFolder; }
};
