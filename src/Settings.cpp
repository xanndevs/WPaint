#include "Settings.h"

#include <QSettings>

namespace {

SettingsValues g_values;
Settings::ChangedCallback g_changed;

struct Entry {
    const char* key;
    bool SettingsValues::*field;
};

// Every persisted setting in the app. Adding one is a row here, a field in
// SettingsValues and a widget in the preferences dialog.
constexpr Entry kEntries[] = {
    {"canvas/antialias", &SettingsValues::antialiasCanvas},
};

} // namespace

namespace Settings {

const SettingsValues& values() { return g_values; }

bool antialiasCanvas() { return g_values.antialiasCanvas; }

void setChangedCallback(ChangedCallback cb) { g_changed = std::move(cb); }

void apply(const SettingsValues& v) {
    QSettings s;
    bool dirty = false;
    for (const Entry& e : kEntries) {
        const bool nv = v.*(e.field);
        if (nv == g_values.*(e.field))
            continue;
        g_values.*(e.field) = nv;
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
        g_values.*(e.field) = s.value(QLatin1String(e.key), g_values.*(e.field)).toBool();
}

} // namespace Settings
