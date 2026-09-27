#include "Settings.h"

#include <QSettings>

namespace {

SettingsValues g_values;
Settings::ChangedCallback g_changed;

// A row per persisted setting. The read/write pair is what makes the table work
// for more than bools: QSettings stores a QVariant, so a row only has to know
// how to lift its own field out of the struct and put it back. Without that the
// whole mechanism is bool-only, and the second non-bool setting would be the
// thing that forces the rewrite -- so it happens before the first one lands.
struct Entry {
    const char* key;
    QVariant (*read)(const SettingsValues&);
    void (*write)(SettingsValues&, const QVariant&);
};

// A template argument with a comma inside it would split the macro's arguments,
// so the two container fields get a name each.
using ColorList = QVector<QColor>;
using ShortcutMap = QMap<QString, QString>;

// The key is spelled out rather than derived from the field name: the flat name
// would put twenty-one settings in one namespace at the top level of the config
// file, and renaming it later would orphan the value anybody had already saved.
#define WP_SETTING(key, field, type)                                       \
    {                                                                       \
        key,                                                                \
        [](const SettingsValues& v) { return QVariant::fromValue(v.field); }, \
            [](SettingsValues& v, const QVariant& q) { v.field = q.value<type>(); } \
    }

// For the one field that is a container. A QSettings QVariantMap is written as a
// quoted string holding the variant's bytes, which does survive a round trip --
// and only because Qt happens to decode a variant smuggled inside a string when
// reading it back. That is unreadable in the config file and fragile across a Qt
// version, so this one is stored as plain text: one "id=sequence" per line.
#define WP_TEXT_SETTING(key, field, encode, decode)                         \
    {                                                                        \
        key,                                                                 \
            [](const SettingsValues& v) -> QVariant { return encode(v.field); }, \
                [](SettingsValues& v, const QVariant& q) { v.field = decode(q.toString()); } \
    }

static QString encodeShortcuts(const ShortcutMap& m) {
    QStringList lines;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it)
        lines << it.key() + QLatin1Char('=') + it.value();
    return lines.join(QLatin1Char('\n'));
}

static ShortcutMap decodeShortcuts(const QString& text) {
    ShortcutMap m;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const int split = line.indexOf(QLatin1Char('='));
        if (split > 0)
            m.insert(line.left(split), line.mid(split + 1));
    }
    return m;
}

// Every persisted setting in the app. Adding one is a row here, a field in
// SettingsValues and a row in the preferences dialog. Multi-valued settings
// stay one row each -- the shortcut map and the custom palette are a QMap and a
// QVector inside one field, not one field per binding or per swatch.
constexpr Entry kEntries[] = {
    WP_SETTING("canvas/antialias", antialiasCanvas, bool),
    WP_SETTING("canvas/crispAbove100", crispPixelsWhenMagnified, bool),
    WP_SETTING("canvas/showBoundaryHandles", showBoundaryHandles, bool),
    WP_SETTING("tools/smoothShapes", smoothShapes, bool),
    WP_SETTING("text/antialias", smoothText, bool),
    WP_SETTING("tools/spaceWheelBrushSize", spaceWheelBrushSize, bool),
    WP_SETTING("undo/maxSteps", undoLimit, int),
    WP_SETTING("files/confirmDiscard", confirmDiscard, bool),
    WP_SETTING("layers/thumbnailQuality", thumbnailQuality, int),
    WP_SETTING("defaults/shape", defaultShape, int),
    WP_SETTING("defaults/shapeStyle", defaultShapeStyle, int),
    WP_SETTING("defaults/brushStyle", defaultBrushStyle, int),
    WP_SETTING("defaults/brushSize", defaultBrushSize, int),
    WP_SETTING("defaults/primaryColor", defaultPrimary, QColor),
    WP_SETTING("defaults/secondaryColor", defaultSecondary, QColor),
    WP_SETTING("defaults/backgroundColor", defaultBackground, QColor),
    WP_SETTING("defaults/canvasSize", defaultCanvasSize, QString),
    WP_SETTING("colors/palettePreset", palettePreset, int),
    WP_SETTING("colors/paletteCustom", paletteCustom, ColorList),
    WP_TEXT_SETTING("input/shortcuts", shortcuts, encodeShortcuts, decodeShortcuts),
    WP_SETTING("theme/preference", themePreference, int),
};


// The palette always has 20 entries, in the same 10x2 order the toolbar grid
// uses, whatever a stale or hand-edited config says -- a short list would leave
// the last row of swatches missing rather than showing a default.
ColorList defaultCustomPalette() {
    return ColorList(20, QColor("#FFFFFF"));
}

} // namespace

namespace Settings {

const SettingsValues& values() { return g_values; }

bool antialiasCanvas() { return g_values.antialiasCanvas; }
bool crispPixelsWhenMagnified() { return g_values.crispPixelsWhenMagnified; }
bool showBoundaryHandles() { return g_values.showBoundaryHandles; }
bool smoothShapes() { return g_values.smoothShapes; }
bool smoothText() { return g_values.smoothText; }
bool spaceWheelBrushSize() { return g_values.spaceWheelBrushSize; }
int undoLimit() { return g_values.undoLimit; }
bool confirmDiscard() { return g_values.confirmDiscard; }
int thumbnailQuality() { return g_values.thumbnailQuality; }
int defaultShape() { return g_values.defaultShape; }
int defaultShapeStyle() { return g_values.defaultShapeStyle; }
int defaultBrushStyle() { return g_values.defaultBrushStyle; }
int defaultBrushSize() { return g_values.defaultBrushSize; }
QColor defaultPrimary() { return g_values.defaultPrimary; }
QColor defaultSecondary() { return g_values.defaultSecondary; }
QColor defaultBackground() { return g_values.defaultBackground; }
QString defaultCanvasSize() { return g_values.defaultCanvasSize; }
int palettePreset() { return g_values.palettePreset; }
QVector<QColor> paletteCustom() { return g_values.paletteCustom; }
QMap<QString, QString> shortcuts() { return g_values.shortcuts; }
int themePreference() { return g_values.themePreference; }

void setChangedCallback(ChangedCallback cb) { g_changed = std::move(cb); }

void apply(const SettingsValues& v) {
    QSettings s;
    bool dirty = false;
    for (const Entry& e : kEntries) {
        const QVariant nv = e.read(v);
        if (nv == e.read(g_values))
            continue;
        e.write(g_values, nv);
        s.setValue(QLatin1String(e.key), nv);
        dirty = true;
    }
    if (dirty && g_changed)
        g_changed();
}

void resetDefaults() { apply(SettingsValues{}); }

void load() {
    QSettings s;
    for (const Entry& e : kEntries)
        e.write(g_values, s.value(QLatin1String(e.key), e.read(g_values)));
    if (g_values.paletteCustom.size() != 20)
        g_values.paletteCustom = defaultCustomPalette();
}

} // namespace Settings
