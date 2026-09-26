#pragma once

#include <QColor>
#include <QImage>
#include <QString>

// One paint layer. QImage is implicitly shared, so value-copies used by the
// undo stack are cheap until a layer is actually painted on.
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

    Layer() = default;
    Layer(const QString& n, const QImage& img, bool vis = true)
        : name(n), image(img), visible(vis) {}
};
