#pragma once

#include "Tool.h"

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <functional>

// User settings that outlive the session. The whole surface is one struct plus
// one table (see Settings.cpp), so a new setting is a field, a table row and a
// row in the preferences dialog — nothing else in the app learns its name.
//
// Rules: read through the accessors (they are cheap), write through apply(),
// and never touch QSettings directly from a widget. apply() persists only the
// keys whose value actually changed and fires the changed-callback once, which
// is what live-tracks the canvas repaint.
//
// Two kinds of value live here and they are not the same kind of thing.
// *Behaviour* is how the editor responds -- antialiasing, gesture bindings,
// history limits. *Defaults* is what a new document starts as, and the colour
// palette is the one piece of "what you are looking at" that is a preference
// rather than document state: a document carries the layers it was drawn on,
// not the twenty colours the toolbar offers. The current zoom, brush size,
// brush style and colour pair are still session state and deliberately not
// here -- they live on CanvasView and belong to the document you are editing.
struct SettingsValues {
    // ---- canvas ----
    // False = nearest-neighbour blit of the document, so magnified pixels stay
    // hard-edged instead of being bilinearly smeared. Shapes and text keep
    // their own antialiasing when they are baked, so the file quality does not
    // depend on a display preference.
    bool antialiasCanvas = true;
    // Above 100% zoom, drop back to nearest-neighbour even when antialiasCanvas
    // is on: magnifying a pixel-art document far enough and the smoothing
    // stops being helpful.
    bool crispPixelsWhenMagnified = false;
    bool showBoundaryHandles = true;

    // ---- tools ----
    bool smoothShapes = true;
    bool smoothText = true;
    bool spaceWheelBrushSize = true;
    // 0 = unlimited, matching QUndoStack's own default.
    int undoLimit = 0;

    // ---- files ----
    bool confirmDiscard = true;

    // ---- layers ----
    // 0 = Fast (nearest, correct for pixel art), 1 = Smooth.
    int thumbnailQuality = 0;

    // ---- defaults for a new document ----
    // The ToolId of the shape a new document starts with, not its position in
    // the gallery: the order is a presentation choice and reordering it must
    // not silently repoint somebody's default.
    int defaultShape = static_cast<int>(ToolId::ShapeRect);
    // ShapeStyle::Outline / Fill / OutlineFill.
    int defaultShapeStyle = 0;
    // BrushStyle::Round / Square / Spray / Calligraphy.
    int defaultBrushStyle = 0;
    int defaultBrushSize = 4;
    QColor defaultPrimary = QColor("#000000");
    QColor defaultSecondary = QColor("#FFFFFF");
    QColor defaultBackground = QColor("#FFFFFF");
    // "400x400", or a "W x H" pair for a custom size.
    QString defaultCanvasSize = QStringLiteral("400x400");

    // ---- colour palette ----
    // Index into PalettePresets::all(); the last entry means "the user's own".
    int palettePreset = 0;
    // Always 20 entries, in the same 10x2 order the toolbar grid uses.
    QVector<QColor> paletteCustom;

    // ---- shortcuts ----
    // Action id -> key sequence text. Only the *overrides* live here: a missing
    // id falls back to the default in Shortcuts.h, so shipping a new binding
    // does not strand anyone who has never opened the Shortcuts page.
    QMap<QString, QString> shortcuts;

    // Theme::Pref as an int, so the table does not have to know about Theme.
    int themePreference = 0;
};

namespace Settings {

using ChangedCallback = std::function<void()>;

const SettingsValues& values();
bool antialiasCanvas();
bool crispPixelsWhenMagnified();
bool showBoundaryHandles();
bool smoothShapes();
bool smoothText();
bool spaceWheelBrushSize();
int undoLimit();
bool confirmDiscard();
int thumbnailQuality();
int defaultShape();
int defaultShapeStyle();
int defaultBrushStyle();
int defaultBrushSize();
QColor defaultPrimary();
QColor defaultSecondary();
QColor defaultBackground();
QString defaultCanvasSize();
int palettePreset();
QVector<QColor> paletteCustom();
QMap<QString, QString> shortcuts();
int themePreference();

// Persists the subset that differs from the live values and notifies once.
void apply(const SettingsValues& v);
void resetDefaults();

// Reads every key once, right after the QApplication identity is known.
void load();

void setChangedCallback(ChangedCallback cb);

} // namespace Settings
