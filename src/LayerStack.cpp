#include "LayerStack.h"

#include "DrawingUtils.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QPainter>

namespace {
constexpr quint32 kMagic = 0x57504154; // "WPAT"
constexpr qint32 kVersion = 1;
} // namespace

LayerStack::LayerStack(QObject* parent) : QObject(parent) {}

QSize LayerStack::size() const {
    return m_layers.isEmpty() ? QSize() : m_layers.first().image.size();
}

QImage LayerStack::composite() const {
    if (m_layers.isEmpty()) return QImage();
    QImage out(size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    for (int i = m_layers.size() - 1; i >= 0; --i) {
        const Layer& l = m_layers.at(i);
        if (!l.visible || l.image.isNull()) continue;
        p.drawImage(0, 0, l.image);
    }
    return out;
}

QRect LayerStack::contentBounds() const {
    QRect r;
    for (const Layer& l : m_layers) {
        if (!l.visible) continue;
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
    for (Layer& l : m_layers)
        l.image = Draw::resizeCanvasImage(l.image, size);
    emit changed();
}

void LayerStack::replaceAll(const QList<Layer>& layers, int activeIndex) {
    m_layers = layers;
    m_active = layers.isEmpty() ? -1 : qBound(0, activeIndex, m_layers.size() - 1);
    emit changed();
    emit activeChanged(m_active);
}

void LayerStack::addLayer(int index, const Layer& layer) {
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

void LayerStack::removeLayer(int index) {
    if (index < 0 || index >= m_layers.size()) return;
    m_layers.removeAt(index);
    clampActive();
    emit changed();
}

void LayerStack::moveLayer(int from, int to) {
    if (from < 0 || from >= m_layers.size()) return;
    to = qBound(0, to, m_layers.size() - 1);
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
        QBuffer b(&png);
        b.open(QIODevice::WriteOnly);
        l.image.save(&b, "PNG");
        b.close();
        out << l.name << l.visible << png;
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
    if (in.status() != QDataStream::Ok || magic != kMagic || version != kVersion ||
        n < 1 || w <= 0 || h <= 0 || w > 100000 || h > 100000) {
        res.error = tr("Not a valid WPaint project file.");
        return res;
    }
    QList<Layer> layers;
    for (qint32 i = 0; i < n; ++i) {
        QString name;
        bool visible = true;
        QByteArray png;
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
        layers.append(Layer(name, img.convertToFormat(QImage::Format_ARGB32_Premultiplied), visible));
    }
    m_layers = layers;
    m_active = 0;
    for (int i = 0; i < m_layers.size(); ++i)
        if (m_layers.at(i).visible) { m_active = i; break; }
    emit changed();
    emit activeChanged(m_active);
    res.ok = true;
    return res;
}