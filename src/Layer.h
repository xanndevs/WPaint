#pragma once

#include <QImage>
#include <QString>

// One paint layer. QImage is implicitly shared, so value-copies used by the
// undo stack are cheap until a layer is actually painted on.
struct Layer {
    QString name;
    QImage image;   // ARGB32_Premultiplied
    bool visible = true;

    Layer() = default;
    Layer(const QString& n, const QImage& img, bool vis = true)
        : name(n), image(img), visible(vis) {}
};