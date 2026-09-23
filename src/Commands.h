#pragma once

#include "Layer.h"

#include <QImage>
#include <QList>
#include <QPoint>
#include <QString>
#include <QVector>

class QUndoCommand;
class LayerStack;

// Factories for QUndoCommand subclasses. The caller pushes the returned
// command onto a QUndoStack (which takes ownership).
namespace Commands {

struct PaintPatch {
    int layer = -1;   // layer index inside the stack
    QPoint pos;       // top-left in image coordinates
    QImage before;    // snapshot of the patched region before the edit
    QImage after;     // snapshot of the patched region after the edit
};

// Paints/generates pixel edits (strokes, fills, text, selection welding, ...).
QUndoCommand* makePaint(LayerStack* stack, const QVector<PaintPatch>& patches,
                        const QString& text);

// Structural change: add/remove/reorder/rename/visibility of layers, canvas
// resize or crop, etc. Captures the full layer list (cheap thanks to QImage
// implicit sharing) before and after the caller's mutation.
QUndoCommand* makeLayerList(LayerStack* stack, const QList<Layer>& before,
                            const QString& text);

} // namespace Commands