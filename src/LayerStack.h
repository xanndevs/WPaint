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
    void renameLayer(int i, const QString& name);
    void setActiveIndex(int i);

    QString nextName(const QString& base) const;
    void clear();

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