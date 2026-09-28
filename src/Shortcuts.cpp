#include "Shortcuts.h"
#include "Settings.h"

#include <QCoreApplication>

namespace {

using namespace Shortcuts;

// The defaults. Grouped the way the toolbar is, so the page reads as the
// toolbar's keyboard shadow rather than as a flat list of key codes.
const QList<Entry> kEntries = {
    // ---- File ----
    {"file.new", Group::File, QT_TRANSLATE_NOOP("Shortcuts", "New"), "Ctrl+N", nullptr},
    {"file.newWindow", Group::File, QT_TRANSLATE_NOOP("Shortcuts", "New Window"), "Ctrl+Shift+N", nullptr},
    {"file.open", Group::File, QT_TRANSLATE_NOOP("Shortcuts", "Open..."), "Ctrl+O", nullptr},
    {"file.save", Group::File, QT_TRANSLATE_NOOP("Shortcuts", "Save"), "Ctrl+S", nullptr},
    {"file.saveAs", Group::File, QT_TRANSLATE_NOOP("Shortcuts", "Save as..."), "Ctrl+Shift+S", nullptr},
    {"file.exit", Group::File, QT_TRANSLATE_NOOP("Shortcuts", "Exit"), "Ctrl+Q", nullptr},

    // ---- Edit ----
    {"edit.undo", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Undo"), "Ctrl+Z", nullptr},
    {"edit.redo", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Redo"), "Ctrl+Y", nullptr},
    {"edit.cut", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Cut"), "Ctrl+X", nullptr},
    {"edit.copy", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Copy"), "Ctrl+C", nullptr},
    {"edit.paste", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Paste"), "Ctrl+V", nullptr},
    {"edit.selectAll", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Select all"), "Ctrl+A", nullptr},
    {"edit.deselect", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Deselect"), "Ctrl+Shift+A", nullptr},
    {"edit.delete", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Delete the selection"), "Del", nullptr},
    {"edit.preferences", Group::Edit, QT_TRANSLATE_NOOP("Shortcuts", "Preferences..."), "Ctrl+,", nullptr},

    // ---- View ----
    {"view.zoomIn", Group::View, QT_TRANSLATE_NOOP("Shortcuts", "Zoom in"), "Ctrl++", nullptr},
    {"view.zoomOut", Group::View, QT_TRANSLATE_NOOP("Shortcuts", "Zoom out"), "Ctrl+-", nullptr},
    {"view.actualSize", Group::View, QT_TRANSLATE_NOOP("Shortcuts", "Actual size"), "Ctrl+0", nullptr},
    {"view.fit", Group::View, QT_TRANSLATE_NOOP("Shortcuts", "Fit to window"), "Ctrl+9", nullptr},
    {"view.theme", Group::View, QT_TRANSLATE_NOOP("Shortcuts", "Theme preference"), "Ctrl+Shift+T", nullptr},
    {"view.boundaryHandles", Group::View, QT_TRANSLATE_NOOP("Shortcuts", "Show canvas resize handles"), "Ctrl+Shift+H", nullptr},

    // ---- Tools ----
    {"tool.select", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Select"), "M", nullptr},
    {"tool.pencil", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Pencil"), "P", nullptr},
    {"tool.fill", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Paint bucket"), "G", nullptr},
    {"tool.text", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Text"), "T", nullptr},
    {"tool.eraser", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Eraser"), "E", nullptr},
    {"tool.eyedropper", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Eyedropper"), "I", nullptr},
    {"color.swap", Group::Tools, QT_TRANSLATE_NOOP("Shortcuts", "Swap color 1 and color 2"), "X", nullptr},

    // ---- Image ----
    {"image.crop", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Crop"), "C", nullptr},
    {"image.magnify", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Magnify"), "Z", nullptr},
    {"image.flipH", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Flip horizontal"), "Ctrl+F", nullptr},
    {"image.flipV", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Flip vertical"), "Ctrl+Shift+F", nullptr},
    {"image.rotateLeft", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Rotate left 90 degrees"), "Ctrl+R", nullptr},
    {"image.rotateRight", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Rotate right 90 degrees"), "Ctrl+Shift+R", nullptr},
    {"image.resize", Group::Image, QT_TRANSLATE_NOOP("Shortcuts", "Resize and rotate..."), "Ctrl+E", nullptr},

    // ---- Shapes ----
    {"shape.gallery", Group::Shapes, QT_TRANSLATE_NOOP("Shortcuts", "Shapes"), "U", nullptr},
    {"shape.fillModeNext", Group::Shapes, QT_TRANSLATE_NOOP("Shortcuts", "Shape fill mode: next"), "Ctrl+U", nullptr},
    {"shape.fillModePrev", Group::Shapes, QT_TRANSLATE_NOOP("Shortcuts", "Shape fill mode: previous"), "Ctrl+Shift+U", nullptr},
    {"shape.next", Group::Shapes, QT_TRANSLATE_NOOP("Shortcuts", "Next shape"), "Ctrl+Tab", nullptr},
    {"shape.previous", Group::Shapes, QT_TRANSLATE_NOOP("Shortcuts", "Previous shape"), "Ctrl+Shift+Tab", nullptr},

    // ---- Brushes ----
    {"brush.gallery", Group::Brushes, QT_TRANSLATE_NOOP("Shortcuts", "Brush"), "B", nullptr},
    {"brush.smaller", Group::Brushes, QT_TRANSLATE_NOOP("Shortcuts", "Brush size: smaller"), "[", nullptr},
    {"brush.larger", Group::Brushes, QT_TRANSLATE_NOOP("Shortcuts", "Brush size: larger"), "]", nullptr},

    // ---- Layers ----
    {"layers.panel", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Show or hide the layers panel"), "F7", nullptr},
    {"layer.above", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Focus the layer above"), "Alt+Up", nullptr},
    {"layer.below", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Focus the layer below"), "Alt+Down", nullptr},
    {"layer.add", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Add a layer"), "Ctrl+Shift+N", nullptr},
    {"layer.duplicate", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Duplicate the layer"), "Ctrl+Shift+D", nullptr},
    {"layer.moveUp", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Move the layer up"), "Ctrl+Shift+Up", nullptr},
    {"layer.moveDown", Group::Layers, QT_TRANSLATE_NOOP("Shortcuts", "Move the layer down"), "Ctrl+Shift+Down", nullptr},

    // ---- Gestures: documented on the page, not rebindable ----
    {"gesture.brushWheel", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Brush size"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Space + wheel"), true},
    {"gesture.zoomWheel", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Zoom at the pointer"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Ctrl + wheel"), true},
    {"gesture.pan", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Pan the canvas"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Middle-drag"), true},
    {"gesture.nudge", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Nudge by one pixel"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Arrow keys"), true},
    {"gesture.nudge10", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Nudge by ten pixels"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Shift + arrow keys"), true},
    {"gesture.sketch", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Draw with the keyboard"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Space + arrow keys"), true},
    {"gesture.square", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Square a shape drag"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Shift + drag"), true},
    {"gesture.fromCentre", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Grow a shape from its centre"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Alt + drag"), true},
    {"gesture.cancel", Group::Navigation, QT_TRANSLATE_NOOP("Shortcuts", "Cancel the current gesture"), "",
     QT_TRANSLATE_NOOP("Shortcuts", "Esc"), true},
};

} // namespace

namespace Shortcuts {

const QList<Entry>& entries() { return kEntries; }

const Entry* find(const QString& id) {
    for (const Entry& e : kEntries)
        if (id == QLatin1String(e.id))
            return &e;
    return nullptr;
}

QString groupName(Group g) {
    switch (g) {
    case Group::File: return QCoreApplication::translate("Shortcuts", "File");
    case Group::Edit: return QCoreApplication::translate("Shortcuts", "Edit");
    case Group::View: return QCoreApplication::translate("Shortcuts", "View");
    case Group::Tools: return QCoreApplication::translate("Shortcuts", "Tools");
    case Group::Image: return QCoreApplication::translate("Shortcuts", "Image");
    case Group::Shapes: return QCoreApplication::translate("Shortcuts", "Shapes");
    case Group::Brushes: return QCoreApplication::translate("Shortcuts", "Brushes");
    case Group::Layers: return QCoreApplication::translate("Shortcuts", "Layers");
    case Group::Navigation: return QCoreApplication::translate("Shortcuts", "Navigation");
    }
    return {};
}

QList<Group> groups() {
    // In table order, so the page's groups follow the order they are declared in
    // rather than the order the enum happens to have.
    QList<Group> out;
    for (const Entry& e : kEntries)
        if (!out.contains(e.group))
            out << e.group;
    return out;
}

QString overrideFor(const QString& id) {
    return Settings::shortcuts().value(id);
}

void setOverride(const QString& id, const QString& sequence) {
    QMap<QString, QString> map = Settings::shortcuts();
    // An override that matches the default is not an override: storing it would
    // make "reset this row" a no-op for the row you most expect it to work on.
    const Entry* e = find(id);
    if (e && sequence == QLatin1String(e->sequence)) {
        if (map.remove(id) == 0)
            return;
    } else {
        if (sequence.isEmpty()) {
            if (map.remove(id) == 0)
                return;
        } else if (map.value(id) == sequence) {
            return;
        } else {
            map.insert(id, sequence);
        }
    }
    SettingsValues v = Settings::values();
    v.shortcuts = map;
    Settings::apply(v);
}

QString toText(const QKeySequence& seq) { return seq.toString(QKeySequence::PortableText); }

bool fromText(const QString& text, QKeySequence& out) {
    out = QKeySequence(text, QKeySequence::PortableText);
    return !out.isEmpty();
}

QKeySequence effective(const Entry& e, QString* outGesture) {
    if (e.isGesture) {
        if (outGesture)
            *outGesture = QCoreApplication::translate("Shortcuts", e.gesture);
        return {};
    }
    const QString over = overrideFor(QLatin1String(e.id));
    if (over.isEmpty()) {
        if (outGesture)
            outGesture->clear();
        return QKeySequence(QLatin1String(e.sequence), QKeySequence::PortableText);
    }
    if (outGesture)
        outGesture->clear();
    return QKeySequence(over, QKeySequence::PortableText);
}

QString describe(const QKeySequence& seq) {
    if (seq.isEmpty())
        return QStringLiteral("—");
    return seq.toString(QKeySequence::NativeText);
}

QList<QString> conflictsWith(const QKeySequence& seq, const QString& exceptId) {
    QList<QString> out;
    if (seq.isEmpty())
        return out;
    for (const Entry& e : kEntries) {
        if (e.isGesture)
            continue;
        if (QLatin1String(e.id) == exceptId)
            continue;
        if (effective(e) == seq)
            out << QLatin1String(e.id);
    }
    return out;
}

} // namespace Shortcuts
