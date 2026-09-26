#include "Commands.h"
#include "LayerStack.h"

#include <QPainter>
#include <QUndoCommand>

namespace Commands {

namespace {
void stamp(QImage& dst, const QImage& src, const QPoint& pos) {
    if (src.isNull() || dst.isNull()) return;
    const QRect dstRect(0, 0, dst.width(), dst.height());
    const QRect srcRect = QRect(pos, src.size()).intersected(dstRect);
    if (srcRect.isEmpty()) return;
    QImage copy = src.copy(srcRect.translated(-pos));
    QPainter p(&dst);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(srcRect.topLeft(), copy);
}
} // namespace

class PaintCommand : public QUndoCommand {
public:
    PaintCommand(LayerStack* stack, const QVector<PaintPatch>& patches,
                 const QString& text)
        : QUndoCommand(text), m_stack(stack), m_patches(patches) {}

    void undo() override {
        for (const PaintPatch& p : m_patches)
            if (p.layer >= 0 && p.layer < m_stack->count())
                stamp(m_stack->layerAt(p.layer).image, p.before, p.pos);
    }

    void redo() override {
        for (const PaintPatch& p : m_patches)
            if (p.layer >= 0 && p.layer < m_stack->count())
                stamp(m_stack->layerAt(p.layer).image, p.after, p.pos);
    }

private:
    LayerStack* m_stack;
    QVector<PaintPatch> m_patches;
};

class LayerListCommand : public QUndoCommand {
public:
    LayerListCommand(LayerStack* stack, const QList<Layer>& before,
                     const QList<Layer>& after, int beforeActive, int afterActive,
                     const QString& text)
        : QUndoCommand(text), m_stack(stack), m_before(before), m_after(after),
          m_beforeActive(beforeActive), m_afterActive(afterActive) {}

    void undo() override {
        m_stack->replaceAll(m_before, m_beforeActive);
    }

    void redo() override {
        m_stack->replaceAll(m_after, m_afterActive);
    }

private:
    LayerStack* m_stack;
    QList<Layer> m_before;
    QList<Layer> m_after;
    int m_beforeActive;
    int m_afterActive;
};

QUndoCommand* makePaint(LayerStack* stack, const QVector<PaintPatch>& patches,
                        const QString& text) {
    return new PaintCommand(stack, patches, text);
}

QUndoCommand* makeLayerList(LayerStack* stack, const QList<Layer>& before,
                            int beforeActive, const QString& text) {
    return new LayerListCommand(stack, before, stack->layers(), beforeActive,
                                stack->activeIndex(), text);
}

} // namespace Commands