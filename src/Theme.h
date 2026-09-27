#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

#include <functional>

class QWidget;
class QAction;
class QAbstractButton;

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
    // Gallery buttons that open a menu from a chevron strip: the strip's
    // height, which is also how much taller the tool-select zone is than the
    // strip, and the caret size itself.
    int caretBand = 14;
    int caretGlyph = 11;
    // Optical lift of a gallery button's glyph: geometric centring reads as
    // sitting low next to the plain big buttons, and the chevron strip takes
    // the space the lift frees up.
    int glyphLift = 5;
    // Toggle switch: the track is switchW x switchH and the thumb is inset into
    // it, so the thumb follows the stroke instead of staying a fixed dot.
    int switchW = 40;
    int switchH = 20;
    int switchThumb = 14;
    // Preferences shell: the width of the left navigation rail.
    int settingsNavW = 200;

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
// color to the active icon color, caches results. Pass an explicit tint when
// the glyph has to sit on something other than the default surface -- e.g. on
// an accent-filled row, where the normal icon color would disappear.
QIcon icon(const QString& name, int px = 20, const QColor& tint = QColor());
void clearIconCache();

// Call after a theme change: re-tints every button tagged with wpIconName.
void reapplyIcons(QWidget* root);
// Re-tint one button right now. Needed when a button's checked state changes
// at runtime rather than at theme-change time: a checked button sits on the
// accent fill and its glyph has to switch to the on-accent colour, or the icon
// disappears into the background.
void refreshIcon(QAbstractButton* button);

// Helpers for building Fluent-ish controls.
void setIcon(QWidget* w, const QString& iconName, int px = 20);
// Same, for a QAction -- menu items are not widgets, so they are tagged and
// re-tinted separately. A checked action uses the on-accent colour.
void setActionIcon(QAction* action, const QString& iconName, int px = 18);

} // namespace Theme