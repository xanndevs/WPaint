#pragma once

#include <functional>

// User settings that outlive the session. The whole surface is one struct plus
// one table (see Settings.cpp), so a new setting is a field, a table row and a
// widget in the preferences dialog — nothing else in the app learns its name.
//
// Rules: read through the accessors (they are cheap), write through apply(),
// and never touch QSettings directly from a widget. apply() persists only the
// keys whose value actually changed and fires the changed-callback once, which
// is what live-tracks the canvas repaint. A setting that is *presentation* of
// the document (zoom, brush size, colors) is deliberately NOT here: those live
// on CanvasView and belong to the document, not to the user.
struct SettingsValues {
    // False = nearest-neighbour blit of the document, so magnified pixels stay
    // hard-edged instead of being bilinearly smeared. Shapes and text keep
    // their own antialiasing when they are baked, so the file quality does not
    // depend on a display preference.
    bool antialiasCanvas = true;
};

namespace Settings {

using ChangedCallback = std::function<void()>;

const SettingsValues& values();
bool antialiasCanvas();

// Persists the subset that differs from the live values and notifies once.
void apply(const SettingsValues& v);
void resetDefaults();

// Reads every key once, right after the QApplication identity is known.
void load();

void setChangedCallback(ChangedCallback cb);

} // namespace Settings
