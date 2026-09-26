#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

#include <functional>

class QWidget;
class QAction;

namespace Theme {

enum class Mode { Light, Dark };

// How the active theme is chosen: System follows the OS color scheme,
// Light/Dark pin a fixed mode regardless of the OS.
enum class Pref { System, Light, Dark };

// Design tokens (Fluent-2 inspired). All colors/metrics come from here so a
// theme change is a token swap + stylesheet regen, never a per-widget rework.
struct Tokens {
    // Surfaces
    QColor window;          // app chrome background
    QColor workspace;       // area surrounding the canvas
    QColor surface;         // toolbar / panels / status bar
    QColor surfaceAlt;      // secondary fill (caption strips, wells, rows)
    QColor surfaceHigh;     // raised content (dialogs, menus)
    QColor control;         // control rest fill (may be transparent)
    QColor controlHover;
    QColor controlPressed;
    QColor controlStroke;
    QColor controlStrokeSecondary;
    QColor divider;

    // Accent / status
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor danger;

    // Content
    QColor textPrimary;
    QColor textSecondary;
    QColor textTertiary;
    QColor textOnAccent;
    QColor icon;
    QColor iconOnAccent;
    QColor focusRing;

    // Canvas / document
    QColor checkerLight;
    QColor checkerDark;
    QColor canvasBorder;
    QColor handle;
    QColor handleHover;
    QColor handleOutline;
    QColor selectionA;
    QColor selectionB;
    QColor panelShadow;

    // Metrics
    int radiusSm = 4;
    int radiusMd = 6;
    int radiusLg = 8;
    int radiusXl = 12;
    int gap = 4;
    int pad = 6;
    int toolbarBtn = 28;
    int toolbarBtnSmall = 24;
    int statusH = 28;
    int captionH = 13;
    int panelW = 260;
    int sizePanelW = 50;
    int sizePanelH = 300;
    int sizePanelGap = 14;
    int sliderGroove = 6;
    int sliderHandle = 16;

    // Type
    QString fontFamily;
    int pxCaption = 11;
    int pxBase = 12;
    int pxTitle = 13;
    int pxStatus = 12;
};

const Tokens& tokens();
Mode mode();
Mode systemMode();
Pref preference();
void setPreference(Pref p);
void setMode(Mode m);
void toggleMode();

// Applies the initial preference (uses the OS scheme by default when the app
// has no forced choice) and starts watching for OS color-scheme changes.
// Call once after the QApplication exists.
void init();

// Invoked whenever the *effective* mode changes, including when the OS scheme
// flips while Pref::System is active. Call with {} to release a stale target.
using ModeChangedCallback = std::function<void()>;
void setModeChangedCallback(ModeChangedCallback cb);

// Full application stylesheet for the given tokens.
QString stylesheet(const Tokens& t);

// Icon provider: loads :/assets/icons/<name>.svg, tints the token marker
// color to the active icon color, caches results.
QIcon icon(const QString& name, int px = 20);
void clearIconCache();

// Call after a theme change: re-tints every button tagged with wpIconName.
void reapplyIcons(QWidget* root);

// Helpers for building Fluent-ish controls.
void setIcon(QWidget* w, const QString& iconName, int px = 20);
// Same, for a QAction -- menu items are not widgets, so they are tagged and
// re-tinted separately. A checked action uses the on-accent colour.
void setActionIcon(QAction* action, const QString& iconName, int px = 18);

} // namespace Theme