#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRect>

namespace Draw {

void checkerboard(QPainter& p, const QRect& rect, int cell, const QColor& light,
                  const QColor& dark);

void blit(QImage& dst, const QImage& src, const QPoint& pos,
          QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver);

// Flood fill (4-connected). tolerance: 0 = exact match, 255 = everything.
bool floodFill(QImage& img, const QPoint& seed, const QColor& color, int tolerance);

QPen selectionPen(qreal dashOffset, const QColor& a, const QColor& b);

QImage resizeCanvasImage(const QImage& img, const QSize& size);
QImage rotateImage(const QImage& img, qreal degrees);
QImage flipImage(const QImage& img, Qt::Orientation orientation);

// Where an image dropped on the canvas goes, and how big the canvas has to be for
// it to fit there. Pure geometry, so the rule can be read and tested on its own.
struct DropPlacement {
    QPointF topLeft; // image coordinates
    QSize canvas;    // what the canvas has to be afterwards
};

// `mouse` is the drop point in image coordinates, and `mouseInside` says whether
// that point is over the document at all -- a file dropped on the rail or the
// toolbar is not "over the canvas", and the middle of the view is not necessarily
// over the document either, because the canvas can be panned off-centre.
//
// The rule, which is three cases and not many more:
//  * dropped over the document, with room to the right and below the pointer:
//    the top-left corner goes on the pointer, and the canvas does not move;
//  * dropped anywhere else, including over the document with no room: the image
//    goes to the top-left corner, because the canvas only ever grows towards the
//    bottom right and there is no point growing it for a placement the user did
//    not ask for -- and the canvas is only made bigger if the image itself does
//    not fit;
//  * dropped outside the document: centred, growing only if the image is bigger
//    than the document.
DropPlacement placeDroppedImage(const QSize& canvas, const QSize& image, const QPointF& mouse,
                                bool mouseInside);

} // namespace Draw