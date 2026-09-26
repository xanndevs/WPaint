#include "LayerStack.h"

#include "DrawingUtils.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QPainter>

namespace {
constexpr quint32 kMagic = 0x57504154; // "WPAT"
constexpr qint32 kVersion = 2;         // v2 added the background layer fields
} // namespace

LayerStack::LayerStack(QObject* parent) : QObject(parent) {}

int LayerStack::backgroundIndex() const {
    for (int i = m_layers.size() - 1; i >= 0; --i)
        if (m_layers.at(i).isBackground) return i;
    return -1;
}

QColor LayerStack::exportBackdrop() const {
    const int bg = backgroundIndex();
    if (bg < 0) return QColor(Qt::white);
    const Layer& l = m_layers.at(bg);
    if (!l.visible) return QColor(Qt::white);
    return l.backgroundColor;
}

QSize LayerStack::size() const {
    // The canvas size comes from the first pixel-bearing layer; the background
    // layer holds no image.
    for (int i = 0; i < m_layers.size(); ++i) {
        const QImage& img = m_layers.at(i).image;
        if (!m_layers.at(i).isBackground && !img.isNull()) return img.size();
    }
    return QSize();
}

QImage LayerStack::composite() const {
    const QSize sz = size();
    if (sz.isEmpty()) return QImage();
    QImage out(sz, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    {
        QPainter p(&out);
        const int bg = backgroundIndex();
        // A hidden background leaves the canvas fully transparent.
        if (bg >= 0 && m_layers.at(bg).visible) {
            p.fillRect(out.rect(), m_layers.at(bg).backgroundColor);
            p.end();
        }
        p.begin(&out);
        for (int i = m_layers.size() - 1; i >= 0; --i) {
            const Layer& l = m_layers.at(i);
            if (l.isBackground || !l.visible || l.image.isNull()) continue;
            p.drawImage(0, 0, l.image);
        }
    }
    return out;
}

QRect LayerStack::contentBounds() const {
    QRect r;
    for (const Layer& l : m_layers) {
        if (!l.visible || l.isBackground) continue;
        const QImage img = l.image;
        for (int y = 0; y < img.height(); ++y) {
            const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                if (qAlpha(line[x]) != 0) {
                    r |= QRect(x, y, 1, 1);
                }
            }
        }
    }
    return r;
}

void LayerStack::setSize(const QSize& size) {
    if (size.isEmpty()) return;
    // The background layer carries no pixels, so resizing leaves it untouched
    // and its colour keeps covering whatever the new canvas size is.
    for (Layer& l : m_layers) {
        if (l.isBackground) continue;
        l.image = Draw::resizeCanvasImage(l.image, size);
    }
    emit changed();
}

void LayerStack::replaceAll(const QList<Layer>& layers, int activeIndex) {
    m_layers = layers;
    m_active = layers.isEmpty() ? -1 : qBound(0, activeIndex, m_layers.size() - 1);
    emit changed();
    emit activeChanged(m_active);
}

void LayerStack::addLayer(int index, const Layer& layer) {
    // A background layer is a document property, not a paintable layer; it is
    // only ever created by addBackgroundLayer().
    if (layer.isBackground) return;
    // New layers go above the background (the background is the bottom entry).
    if (backgroundIndex() >= 0 && (index < 0 || index > backgroundIndex()))
        index = backgroundIndex();
    if (!m_layers.isEmpty() && layer.image.size() != size()) {
        Layer copy = layer;
        copy.image = Draw::resizeCanvasImage(copy.image, size());
        m_layers.insert(index < 0 ? 0 : qMin(index, m_layers.size()), copy);
    } else {
        m_layers.insert(index < 0 ? 0 : qMin(index, m_layers.size()), layer);
    }
    clampActive();
    emit changed();
}

void LayerStack::addBackgroundLayer(const QColor& color) {
    const int existing = backgroundIndex();
    if (existing >= 0) {
        m_layers[existing].backgroundColor = color;
        m_layers[existing].visible = true;
        emit changed();
        return;
    }
    Layer bg;
    bg.name = tr("Background");
    bg.isBackground = true;
    bg.backgroundColor = color;
    bg.visible = true;
    m_layers.append(bg);
    emit changed();
}

void LayerStack::removeLayer(int index) {
    if (index < 0 || index >= m_layers.size()) return;
    if (m_layers.at(index).isBackground) return; // never remove the backdrop
    m_layers.removeAt(index);
    clampActive();
    emit changed();
}

void LayerStack::moveLayer(int from, int to) {
    if (from < 0 || from >= m_layers.size()) return;
    if (m_layers.at(from).isBackground) return; // always stays at the bottom
    to = qBound(0, to, m_layers.size() - 1);
    if (m_layers.at(to).isBackground) return;
    const int active = m_active;
    m_layers.move(from, to);
    // Keep the same layer active after the move.
    m_active = qBound(0, active + (to - from), m_layers.size() - 1);
    emit changed();
    emit activeChanged(m_active);
}

void LayerStack::setLayerVisible(int i, bool visible) {
    if (i < 0 || i >= m_layers.size()) return;
    m_layers[i].visible = visible;
    emit changed();
}

void LayerStack::setBackgroundColor(int i, const QColor& color) {
    if (i < 0 || i >= m_layers.size()) return;
    if (!m_layers.at(i).isBackground) return;
    m_layers[i].backgroundColor = color;
    emit changed();
}

void LayerStack::renameLayer(int i, const QString& name) {
    if (i < 0 || i >= m_layers.size()) return;
    m_layers[i].name = name;
    emit changed();
}

void LayerStack::setActiveIndex(int i) {
    if (i < 0 || i >= m_layers.size() || i == m_active) return;
    m_active = i;
    emit activeChanged(m_active);
    emit changed();
}

void LayerStack::clampActive() {
    if (m_layers.isEmpty()) {
        m_active = -1;
        return;
    }
    m_active = qBound(0, m_active, m_layers.size() - 1);
}

QString LayerStack::nextName(const QString& base) const {
    const QString full = base.isEmpty() ? tr("Layer") : base;
    QString candidate = full;
    int serial = 2;
    for (;;) {
        bool clash = false;
        for (const Layer& l : m_layers)
            if (l.name == candidate) { clash = true; break; }
        if (!clash) return candidate;
        candidate = QStringLiteral("%1 %2").arg(full).arg(serial++);
    }
}

void LayerStack::clear() {
    m_layers.clear();
    m_active = 0;
    emit changed();
}

LayerStack::SaveResult LayerStack::saveProject(const QString& path) const {
    SaveResult res{false, {}};
    if (m_layers.isEmpty() || size().isEmpty()) {
        res.error = tr("Nothing to save.");
        return res;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        res.error = f.errorString();
        return res;
    }
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_6_0);
    out << kMagic << kVersion << static_cast<qint32>(size().width())
        << static_cast<qint32>(size().height())
        << static_cast<qint32>(m_layers.size());
    for (const Layer& l : m_layers) {
        QByteArray png;
        if (!l.isBackground) {
            QBuffer b(&png);
            b.open(QIODevice::WriteOnly);
            l.image.save(&b, "PNG");
            b.close();
        }
        out << l.name << l.visible << l.isBackground << l.backgroundColor << png;
    }
    res.ok = true;
    return res;
}

LayerStack::SaveResult LayerStack::loadProject(const QString& path) {
    SaveResult res{false, {}};
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        res.error = f.errorString();
        return res;
    }
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    qint32 version = 0, w = 0, h = 0, n = 0;
    in >> magic >> version >> w >> h >> n;
    if (in.status() != QDataStream::Ok || magic != kMagic ||
        (version != 1 && version != 2) ||
        n < 1 || w <= 0 || h <= 0 || w > 100000 || h > 100000) {
        res.error = tr("Not a valid WPaint project file.");
        return res;
    }
    QList<Layer> layers;
    for (qint32 i = 0; i < n; ++i) {
        QString name;
        bool visible = true;
        QByteArray png;
        if (version >= 2) {
            bool isBackground = false;
            QColor backgroundColor = QColor(Qt::white);
            in >> name >> visible >> isBackground >> backgroundColor >> png;
            if (in.status() != QDataStream::Ok) {
                res.error = tr("Project file is corrupted.");
                return res;
            }
            Layer l;
            l.name = name;
            l.visible = visible;
            l.isBackground = isBackground;
            l.backgroundColor = backgroundColor;
            if (!isBackground) {
                QImage img;
                if (!img.loadFromData(png, "PNG")) {
                    res.error = tr("Project file is corrupted (bad layer image).");
                    return res;
                }
                l.image = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
            layers.append(l);
        } else {
            // v1 predates the background layer: every layer is a paint layer.
            in >> name >> visible >> png;
            if (in.status() != QDataStream::Ok) {
                res.error = tr("Project file is corrupted.");
                return res;
            }
            QImage img;
            if (!img.loadFromData(png, "PNG")) {
                res.error = tr("Project file is corrupted (bad layer image).");
                return res;
            }
            layers.append(Layer(name,
                                img.convertToFormat(QImage::Format_ARGB32_Premultiplied),
                                visible));
        }
    }
    m_layers = layers;
    // Give v1 documents a backdrop so they open in the same shape as new ones.
    if (backgroundIndex() < 0) addBackgroundLayer(QColor(Qt::white));
    m_active = 0;
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers.at(i).visible) { m_active = i; break; }
    emit changed();
    emit activeChanged(m_active);
    res.ok = true;
    return res;
}