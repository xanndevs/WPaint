#include "LayerStack.h"

#include "DrawingUtils.h"

#include <QBuffer>
#include <QDataStream>
#include <QHash>
#include <algorithm>
#include <QFile>
#include <QPainter>

namespace {
constexpr quint32 kMagic = 0x57504154; // "WPAT"
constexpr qint32 kVersion = 3;         // v3 added the folder fields
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

QImage LayerStack::composite() const { return flatten(-1, -1, true, true); }

// onlyVisible is the difference between what the canvas shows and what a merge
// is asked to do: a hidden layer is still a layer with pixels in it, and a merge
// of the layers the user picked has to carry all of them.
QImage LayerStack::flatten(int first, int last, bool withBackground,
                           bool onlyVisible) const {
    const QSize sz = size();
    if (sz.isEmpty()) return QImage();
    if (first < 0) {
        first = 0;
        last = m_layers.size() - 1;
    }
    QImage out(sz, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    {
        QPainter p(&out);
        const int bg = backgroundIndex();
        // A hidden background leaves the canvas fully transparent.
        if (withBackground && bg >= 0 && bg >= first && bg <= last &&
            m_layers.at(bg).visible) {
            p.fillRect(out.rect(), m_layers.at(bg).backgroundColor);
            p.end();
        }
        p.begin(&out);
        // Index 0 is the topmost, so the drawing order is the list backwards.
        for (int i = qMin(last, m_layers.size() - 1); i >= first; --i) {
            const Layer& l = m_layers.at(i);
            if (l.isBackground || l.image.isNull()) continue;
            if (onlyVisible && !l.visible) continue;
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
    // With nothing but a background there is no size to conform to -- the
    // background carries no image -- and resizing to an empty size throws the
    // layer's pixels away rather than keeping them.
    if (!m_layers.isEmpty() && !size().isEmpty() && layer.image.size() != size()) {
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
    // A folder takes its children with it, and a document always keeps at least
    // one real layer: with nothing left there is no layer to draw on and no
    // sensible thing for the panel's add button to add to.
    if (!canRemove(index)) return;
    // Highest first, so the indices below it stay valid.
    const QList<int> span = spanOf(index);
    for (int k = span.size() - 1; k >= 0; --k)
        m_layers.removeAt(span.at(k));
    // A folder that just lost a child needs its count fixed, and so does its
    // parent when the folder itself was nested.
    fixChildCounts();
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
    // activeChanged and not changed: nothing about the layers themselves moved.
    // Emitting both used to make the rail tear itself down and rebuild on every
    // click -- and it rebuilds *from inside* the click, so Qt finished its own
    // selection update afterwards and threw away everything the user had
    // ctrl-clicked. Selecting a second layer looked like doing nothing at all.
    emit activeChanged(m_active);
}


// ---- folders ----------------------------------------------------------
// Everything here reads the flat list. A folder owns a contiguous run after it,
// so "is this inside a folder" is a scan rather than a parent pointer, and the
// list stays a list for the undo stack, the .wpa file and composite().

bool LayerStack::isFolder(int i) const {
    return i >= 0 && i < m_layers.size() && m_layers.at(i).isFolder;
}

int LayerStack::childCountOf(int i) const {
    return isFolder(i) ? qMax(0, m_layers.at(i).childCount) : 0;
}

QList<int> LayerStack::childrenOf(int i) const {
    QList<int> out;
    const int n = childCountOf(i);
    for (int k = 1; k <= n; ++k)
        if (i + k < m_layers.size())
            out << i + k;
    return out;
}

// Everything a folder carries: itself and its whole run, nested groups
// included. This is what an operation that takes a layer away has to take with
// it -- a folder whose grandchildren were left behind would be a group that
// claims to hold layers that are no longer inside it.
QList<int> LayerStack::spanOf(int i) const {
    QList<int> out;
    if (i < 0 || i >= m_layers.size() || !isFolder(i))
        return out << i;
    const int last = qMin(m_layers.size() - 1, i + childCountOf(i));
    for (int k = i; k <= last; ++k)
        out << k;
    return out;
}

int LayerStack::depthOf(int i) const {
    int depth = 0;
    for (int folder = owningFolder(i); folder >= 0; folder = owningFolder(folder))
        ++depth;
    return depth;
}

// The nearest folder that encloses `i`: a folder at f owns f+1 .. f+childCount.
// Scanning upwards and taking the first folder whose run reaches i gives the
// *nearest* ancestor, and asking again with that index gives the next one up --
// which is how nesting is derived without a parent pointer. Counting the runs
// while walking down does not work: the folders above i are, by definition, at
// indices already passed, so a child of one of them is a sibling, not a child.
int LayerStack::owningFolder(int i) const {
    for (int f = i - 1; f >= 0; --f) {
        if (m_layers.at(f).isFolder && f + m_layers.at(f).childCount >= i)
            return f;
    }
    return -1;
}

bool LayerStack::isChildOfFolder(int i) const { return owningFolder(i) >= 0; }

bool LayerStack::isInsideFoldedFolder(int i) const {
    int folder = owningFolder(i);
    while (folder >= 0) {
        if (m_layers.at(folder).folded)
            return true;
        folder = owningFolder(folder);
    }
    return false;
}

void LayerStack::setFolded(int i, bool folded) {
    if (!isFolder(i) || m_layers.at(i).folded == folded)
        return;
    m_layers[i].folded = folded;
    emit changed();
}

bool LayerStack::isDrawable(int i) const {
    if (i < 0 || i >= m_layers.size())
        return false;
    const Layer& l = m_layers.at(i);
    return l.isDrawable() && !isInsideFoldedFolder(i);
}

int LayerStack::drawableIndex() const {
    const int i = m_active;
    if (isDrawable(i))
        return i;
    // A folder is a perfectly good selection -- it is just not a thing you can
    // paint on -- so fall back to the nearest drawable layer above it, which is
    // the one the user was drawing on a moment ago.
    for (int k = i; k >= 0; --k)
        if (isDrawable(k))
            return k;
    for (int k = qMax(0, i + 1); k < m_layers.size(); ++k)
        if (isDrawable(k))
            return k;
    return -1;
}

QList<QList<int>> LayerStack::resolveSelection(const QList<int>& selection) const {
    QList<int> wanted;
    for (int i : selection) {
        if (i < 0 || i >= m_layers.size() || m_layers.at(i).isBackground)
            continue;
        wanted += spanOf(i);
    }
    // No dedupe against the parent is needed: spanOf() already returns a
    // folder's *whole* run, so a child whose folder was picked is in the set
    // once, by the same route. Sorting and uniquing is the whole of it.
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());

    QList<QList<int>> runs;
    for (int i : wanted) {
        if (!runs.isEmpty() && runs.last().last() + 1 == i)
            runs.last() << i;
        else
            runs.append(QList<int>{i});
    }
    return runs;
}

int LayerStack::layerCount() const {
    int n = 0;
    for (const Layer& l : m_layers)
        if (!l.isBackground)
            ++n;
    return n;
}

// What a document keeps alive. A group counts only while it still holds
// something, because an empty group is a container rather than content -- and
// counting it would let the last real layer be deleted, leaving a document with
// nothing to draw on and a group that cannot itself be deleted.
static bool holdsContent(const Layer& l) {
    return l.isDrawable() || (l.isFolder && l.childCount > 0);
}

bool LayerStack::canRemove(int i) const {
    if (i < 0 || i >= m_layers.size() || m_layers.at(i).isBackground)
        return false;
    // A folder takes its children with it.
    const QList<int> span = spanOf(i);
    int survivors = 0;
    for (int k = 0; k < m_layers.size(); ++k) {
        if (span.contains(k) || m_layers.at(k).isBackground)
            continue;
        if (holdsContent(m_layers.at(k)))
            ++survivors;
    }
    // An empty group holds nothing, so removing one cannot leave the document
    // without a layer -- it must always be removable, or a group created by the
    // button is a thing the user cannot get rid of again.
    if (survivors == 0 && span.size() == 1 && isFolder(span.first()))
        return true;
    return survivors > 0;
}

bool LayerStack::canRemoveAny(const QList<int>& selection) const {
    const QList<QList<int>> runs = resolveSelection(selection);
    if (runs.isEmpty())
        return false;
    QList<int> doomed;
    for (const QList<int>& run : runs)
        doomed += run;
    int survivors = 0;
    for (int k = 0; k < m_layers.size(); ++k) {
        if (doomed.contains(k) || m_layers.at(k).isBackground)
            continue;
        if (holdsContent(m_layers.at(k)))
            ++survivors;
    }
    // The same exception as canRemove(): an empty group in the selection is
    // content-free, so it does not count as what is being kept.
    if (survivors == 0)
        for (int k : doomed)
            if (isFolder(k) && spanOf(k).size() == 1)
                return true;
    return survivors > 0;
}

// ---- merging ------------------------------------------------------------
//
// A merge is always downward and always whole: the result sits where the top of
// the run was and carries the name of the bottom of it, because that is the
// entry the user was looking at when they asked. What happens to a group is not
// a special case -- flattening a run draws its children in order, so a folder
// merges by way of its own contents.

int LayerStack::mergeTargetOf(int i) const {
    if (i < 0 || i >= m_layers.size() || m_layers.at(i).isBackground)
        return -1;
    // A folder's own children are part of it, not below it: they run to
    // i + childCount inclusive, so the first entry after the run is one past
    // that.
    int below = isFolder(i) ? i + childCountOf(i) + 1 : i + 1;
    if (below >= m_layers.size() || m_layers.at(below).isBackground)
        return -1; // the background is the canvas, not somewhere to merge into
    return below;
}

bool LayerStack::canMergeDown(int i) const {
    if (i < 0 || i >= m_layers.size() || m_layers.at(i).isBackground)
        return false;
    if (mergeTargetOf(i) >= 0)
        return true;
    // Nothing below, but a group of several can still become the one layer it
    // is standing in for.
    return isFolder(i) && spanOf(i).size() > 1;
}

bool LayerStack::canMergeSelection(const QList<int>& selection) const {
    const QList<QList<int>> runs = resolveSelection(selection);
    // A hole in the selection is refused rather than closed: merging across it
    // would move pixels past layers the user never picked.
    return runs.size() == 1 && runs.first().size() > 1;
}

int LayerStack::mergeDown(int i) {
    const int target = mergeTargetOf(i);
    if (target < 0) {
        if (!canMergeDown(i))
            return -1;
        return mergeRun(spanOf(i)); // flatten the group where it stands
    }
    QList<int> run = spanOf(i);
    run << target;
    return mergeRun(run);
}

int LayerStack::mergeSelected(const QList<int>& selection) {
    if (!canMergeSelection(selection))
        return -1;
    return mergeRun(resolveSelection(selection).first());
}

int LayerStack::mergeRun(const QList<int>& run) {
    if (run.size() < 2)
        return -1;
    const int first = run.first();
    const int last = run.last();
    if (first < 0 || last >= m_layers.size())
        return -1;
    if (m_layers.at(first).isBackground || m_layers.at(last).isBackground)
        return -1;

    Layer merged;
    merged.name = m_layers.at(last).name;
    // Hidden means "do not show this", not "this does not exist" -- so a hidden
    // layer's pixels are part of the merge, and a merge of anything visible
    // comes out visible. The alternative loses the work on the floor whenever
    // the layer somebody happened to be merging into was the hidden one.
    merged.visible = false;
    for (int k = first; k <= last; ++k)
        if (m_layers.at(k).visible)
            merged.visible = true;
    merged.image = flatten(first, last, false, false);

    m_layers[first] = merged;
    for (int k = last; k > first; --k)
        m_layers.removeAt(k);
    // Whatever folder the run started inside just lost a child.
    fixChildCounts();

    if (m_active >= first && m_active <= last)
        m_active = first;
    else if (m_active > last)
        m_active -= last - first;
    clampActive();
    emit changed();
    emit activeChanged(m_active);
    return first;
}

// Keeps every folder's childCount agreeing with the entries that actually
// follow it, and truncates a count that runs off the end of the list. Called
// after anything that can take entries out from under a folder.
//
// A folder with no children left stays a folder. It used to be demoted to a
// plain layer here, which quietly threw away a group the user had emptied on
// purpose -- and it made the button that creates one unusable, since the count
// is zero the moment it is born.
void LayerStack::fixChildCounts() {
    for (int i = 0; i < m_layers.size(); ++i) {
        if (!m_layers.at(i).isFolder)
            continue;
        const int available = m_layers.size() - i - 1;
        if (m_layers.at(i).childCount > available)
            m_layers[i].childCount = qMax(0, available);
    }
}

int LayerStack::groupInto(const QList<int>& selection, bool folded) {
    const QList<QList<int>> runs = resolveSelection(selection);
    // Grouping is contiguous by construction: a folder owns a run. A selection
    // with a hole in it is refused rather than quietly widened, because
    // quietly widening would swallow layers the user did not pick.
    if (runs.size() != 1)
        return -1;
    const QList<int> run = runs.first();
    if (run.isEmpty())
        return -1;

    // The new folder goes where the top of the run was, so nothing above the
    // selection moves.
    const int at = run.first();
    // Grouping inside a group changes the parent's child count, and this is the
    // only place that can know by how much: the new folder lands where the top
    // of the run was, so afterwards nothing can tell which of those entries used
    // to be children and which was the parent.
    //
    // The parent's count goes down by however many of its own direct children
    // were selected and up by one for the folder that replaces them -- so
    // grouping a whole group is a wash, and grouping two of its four children
    // makes it one lighter.
    int parent = -1;
    for (int index : run) {
        const int owner = owningFolder(index);
        if (owner >= 0 && !run.contains(owner)) {
            parent = owner;
            break;
        }
    }
    int absorbed = 0;
    if (parent >= 0) {
        for (int index : run)
            if (owningFolder(index) == parent)
                ++absorbed;
    }
    Layer folder;
    folder.name = nextName(tr("Group"));
    folder.isFolder = true;
    folder.childCount = run.size();
    folder.folded = folded;
    folder.visible = true;
    m_layers.insert(at, folder);
    if (parent >= 0)
        m_layers[parent].childCount += 1 - absorbed;
    m_active = at;
    fixChildCounts();
    emit changed();
    emit activeChanged(m_active);
    return at;
}

int LayerStack::addFolder(bool folded) {
    // A group holds what is put in it, and what goes in it goes on top, so an
    // empty one belongs at the top of the stack rather than at the selection --
    // which is also where "add layer" puts the layers that will fill it.
    Layer folder;
    folder.name = nextName(tr("Group"));
    folder.isFolder = true;
    folder.childCount = 0;
    folder.folded = folded;
    folder.visible = true;
    const int at = qMin(0, m_layers.size());
    m_layers.insert(at, folder);
    m_active = at;
    emit changed();
    emit activeChanged(m_active);
    return at;
}

bool LayerStack::ungroup(int folderIndex) {    if (!isFolder(folderIndex))
        return false;
    const int n = childCountOf(folderIndex);
    m_layers.removeAt(folderIndex);
    for (int k = 0; k < n; ++k)
        m_layers.removeAt(folderIndex);  // children sat right below it
    // A nested folder inside the group kept its own children, so the counts are
    // still consistent -- but anything above is unaffected, so re-derive anyway.
    fixChildCounts();
    m_active = qMin(m_active, m_layers.size() - 1);
    clampActive();
    emit changed();
    emit activeChanged(m_active);
    return true;
}

// Removes a selection, folders and all, as one structural step.
//
// Looping removeLayer() is not enough: a nested group's parent has to be told it
// just lost a child, and by the time the loop reaches the child the parent that
// knew is already gone. So the counts are worked out first, from the whole
// doomed set, and applied afterwards.
bool LayerStack::removeSpans(const QList<int>& selection) {
    const QList<QList<int>> runs = resolveSelection(selection);
    if (runs.isEmpty() || !canRemoveAny(selection))
        return false;

    QList<int> doomed;
    for (const QList<int>& run : runs)
        doomed += run;
    std::sort(doomed.begin(), doomed.end());

    // Every folder that loses children, and how many.
    QHash<int, int> lost;
    for (int folder = 0; folder < m_layers.size(); ++folder) {
        if (!m_layers.at(folder).isFolder)
            continue;
        int count = 0;
        for (int k = 0; k < m_layers.size(); ++k)
            if (m_layers.at(folder).isFolder && k > folder &&
                k <= folder + m_layers.at(folder).childCount && doomed.contains(k))
                ++count;
        // A folder's own children go away when it does, which is one "lost"
        // child for the parent rather than one per grandchild.
        for (int k = 0; k < m_layers.size(); ++k) {
            if (k <= folder || k > folder + m_layers.at(folder).childCount)
                continue;
            if (!m_layers.at(k).isFolder)
                continue;
            for (int g = k + 1; g <= k + m_layers.at(k).childCount; ++g)
                if (doomed.contains(g))
                    ++count; // its children are being removed on their own
        }
        if (count > 0)
            lost.insert(folder, count);
    }

    // Bottom up, so the indices still mean what they said.
    for (int k = doomed.size() - 1; k >= 0; --k) {
        if (doomed.at(k) < 0 || doomed.at(k) >= m_layers.size())
            continue;
        m_layers.removeAt(doomed.at(k));
    }
    for (auto it = lost.constBegin(); it != lost.constEnd(); ++it) {
        if (it.key() < 0 || it.key() >= m_layers.size() || !m_layers.at(it.key()).isFolder)
            continue;
        m_layers[it.key()].childCount -= it.value();
    }
    fixChildCounts();
    clampActive();
    emit changed();
    return true;
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
        // A folder has no pixels, so it writes an empty payload rather than
        // being special-cased: the reader already skips the image for the
        // background, and one branch for "holds no pixels" is easier to keep
        // right than two.
        out << l.name << l.visible << l.isBackground << l.backgroundColor << png
            << l.isFolder << l.childCount << l.folded;
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
        (version < 1 || version > kVersion) ||
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
            bool isFolder = false, folded = false;
            qint32 childCount = 0;
            in >> name >> visible >> isBackground >> backgroundColor >> png;
            if (version >= 3) {
                in >> isFolder >> childCount >> folded;
            }
            if (in.status() != QDataStream::Ok) {
                res.error = tr("Project file is corrupted.");
                return res;
            }
            Layer l;
            l.name = name;
            l.visible = visible;
            l.isBackground = isBackground;
            l.backgroundColor = backgroundColor;
            l.isFolder = isFolder;
            l.childCount = int(childCount);
            l.folded = folded;
            if (!isBackground && !isFolder) {
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