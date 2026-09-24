#include "DrawingUtils.h"

#include <QTransform>
#include <cmath>
#include <vector>

namespace Draw {

void checkerboard(QPainter& p, const QRect& rect, int cell, const QColor& light,
                  const QColor& dark) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect, light);
    const int x0 = rect.left() - ((rect.left() % cell + cell) % cell);
    const int y0 = rect.top() - ((rect.top() % cell + cell) % cell);
    p.setBrush(dark);
    p.setPen(Qt::NoPen);
    for (int y = y0; y < rect.bottom() + cell; y += cell) {
        for (int x = x0; x < rect.right() + cell; x += cell) {
            const bool odd = ((x / cell) + (y / cell)) & 1;
            if (odd)
                p.drawRect(QRect(x, y, cell, cell) & rect);
        }
    }
    p.restore();
}

void blit(QImage& dst, const QImage& src, const QPoint& pos,
          QPainter::CompositionMode mode) {
    if (src.isNull()) return;
    QPainter p(&dst);
    p.setCompositionMode(mode);
    p.drawImage(pos, src);
}

namespace {
inline int colorDist(const QRgb a, const QRgb b) {
    const int dr = qRed(a) - qRed(b);
    const int dg = qGreen(a) - qGreen(b);
    const int db = qBlue(a) - qBlue(b);
    const int da = qAlpha(a) - qAlpha(b);
    return std::max({std::abs(dr), std::abs(dg), std::abs(db), std::abs(da)});
}
} // namespace

bool floodFill(QImage& img, const QPoint& seed, const QColor& color, int tolerance) {
    if (img.isNull()) return false;
    if (seed.x() < 0 || seed.y() < 0 || seed.x() >= img.width() || seed.y() >= img.height())
        return false;

    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width();
    const int h = img.height();
    QRgb* bits = reinterpret_cast<QRgb*>(img.bits());
    const int bpl = img.bytesPerLine() / 4;

    const QRgb target = bits[seed.y() * bpl + seed.x()];
    const QRgb fill = color.rgba();
    if (target == fill && tolerance == 0) return false;
    if (colorDist(target, fill) <= tolerance && tolerance > 0) return false;

    std::vector<QPoint> stack;
    stack.push_back(seed);
    std::vector<quint8> visited(size_t(w) * h, 0);

    while (!stack.empty()) {
        const QPoint pt = stack.back();
        stack.pop_back();
        const int x = pt.x();
        const int y = pt.y();
        if (x < 0 || y < 0 || x >= w || y >= h) continue;
        const size_t idx = size_t(y) * w + x;
        if (visited[idx]) continue;
        if (colorDist(bits[y * bpl + x], target) > tolerance) continue;
        visited[idx] = 1;
        bits[y * bpl + x] = fill;
        stack.push_back({x + 1, y});
        stack.push_back({x - 1, y});
        stack.push_back({x, y + 1});
        stack.push_back({x, y - 1});
    }
    return true;
}

QPen selectionPen(qreal dashOffset, const QColor& a, const QColor& b) {
    QPen pen;
    pen.setWidth(1);
    pen.setCosmetic(true);
    QVector<qreal> pattern;
    pattern << 5 << 4 << 5 << 4;
    pen.setDashPattern(pattern);
    pen.setDashOffset(dashOffset);
    // Two-tone ants: approximate with a single dark pen plus a light underlay
    // handled by the caller.
    pen.setColor(a);
    Q_UNUSED(b);
    return pen;
}

QImage resizeCanvasImage(const QImage& img, const QSize& size) {
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    if (img.isNull()) return out;
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(0, 0, img);
    return out;
}

QImage rotateImage(const QImage& img, qreal degrees) {
    if (img.isNull()) return img;
    const bool quarter = std::fmod(std::abs(degrees), 90.0) < 0.01;
    QTransform t;
    t.rotate(degrees);
    const QRectF mapped =
        t.mapRect(QRectF(QPointF(0, 0), QSizeF(img.size())));
    const QSize sz = QSize(std::ceil(mapped.width()), std::ceil(mapped.height()));
    QImage out(sz, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, !quarter);
    const QRectF target = t.mapRect(QRectF(QPointF(0, 0), QSizeF(img.size())));
    p.translate(-target.left(), -target.top());
    p.rotate(degrees);
    p.drawImage(0, 0, img);
    return out;
}

QImage flipImage(const QImage& img, Qt::Orientation orientation) {
    if (img.isNull()) return img;
    return img.flipped(orientation);
}

} // namespace Draw