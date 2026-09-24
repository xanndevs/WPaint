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

} // namespace Draw