#include "Theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QMap>
#include <QPainter>
#include <QPalette>
#include <QStyleHints>
#include <QSvgRenderer>

#include <utility>

namespace Theme {

static Mode g_mode = Mode::Light;
static Pref g_pref = Pref::System;
static ModeChangedCallback g_modeChanged;
static QHash<QString, QIcon> g_iconCache;

static Tokens makeLight() {
    Tokens t;
    t.window = QColor("#F3F3F3");
    t.workspace = QColor("#EAEAEA");
    t.surface = QColor("#FAFAFA");
    t.surfaceAlt = QColor("#F0F0F0");
    t.surfaceHigh = QColor("#FFFFFF");
    t.control = QColor("#FFFFFF");
    t.controlHover = QColor("#F2F2F2");
    t.controlPressed = QColor("#E5E5E5");
    t.controlStroke = QColor("#E0E0E0");
    t.controlStrokeSecondary = QColor("#D0D0D0");
    t.divider = QColor("#E3E3E3");

    t.accent = QColor("#005FB8");
    t.accentHover = QColor("#0067C0");
    t.accentPressed = QColor("#004E97");
    t.danger = QColor("#C42B1C");

    t.textPrimary = QColor("#1B1B1B");
    t.textSecondary = QColor("#5E5E5E");
    t.textTertiary = QColor("#8A8A8A");
    t.textOnAccent = QColor("#FFFFFF");
    t.icon = QColor("#3B3B3B");
    t.iconOnAccent = QColor("#FFFFFF");
    t.focusRing = QColor("#005FB8");

    t.checkerLight = QColor("#FFFFFF");
    t.checkerDark = QColor("#D4D4D4");
    t.canvasBorder = QColor("#C8C8C8");
    t.handle = QColor("#FFFFFF");
    t.handleHover = QColor("#4CC2FF");
    t.handleOutline = QColor("#767676");
    t.selectionA = QColor("#000000");
    t.selectionB = QColor("#FFFFFF");
    t.panelShadow = QColor(0, 0, 0, 60);

    t.fontFamily = "Inter";
    return t;
}

static Tokens makeDark() {
    Tokens t;
    t.window = QColor("#202020");
    t.workspace = QColor("#1B1B1B");
    t.surface = QColor("#272727");
    t.surfaceAlt = QColor("#222222");
    t.surfaceHigh = QColor("#2D2D2D");
    t.control = QColor("#2D2D2D");
    t.controlHover = QColor("#333333");
    t.controlPressed = QColor("#3D3D3D");
    t.controlStroke = QColor("#3A3A3A");
    t.controlStrokeSecondary = QColor("#4A4A4A");
    t.divider = QColor("#383838");

    t.accent = QColor("#4CC2FF");
    t.accentHover = QColor("#5CC8FF");
    t.accentPressed = QColor("#3AB4E8");
    t.danger = QColor("#FF99A4");

    t.textPrimary = QColor("#FFFFFF");
    t.textSecondary = QColor("#C9C9C9");
    t.textTertiary = QColor("#8E8E8E");
    t.textOnAccent = QColor("#000000");
    t.icon = QColor("#D0D0D0");
    t.iconOnAccent = QColor("#000000");
    t.focusRing = QColor("#4CC2FF");

    t.checkerLight = QColor("#3C3C3C");
    t.checkerDark = QColor("#2A2A2A");
    t.canvasBorder = QColor("#555555");
    t.handle = QColor("#3A3A3A");
    t.handleHover = QColor("#4CC2FF");
    t.handleOutline = QColor("#9A9A9A");
    t.selectionA = QColor("#FFFFFF");
    t.selectionB = QColor("#000000");
    t.panelShadow = QColor(0, 0, 0, 110);

    t.fontFamily = "Inter";
    return t;
}

static Mode resolveMode() {
    switch (g_pref) {
    case Pref::Dark:  return Mode::Dark;
    case Pref::Light: return Mode::Light;
    case Pref::System: break;
    }
    return systemMode();
}

const Tokens& tokens() {
    static Tokens light = makeLight();
    static Tokens dark = makeDark();
    return resolveMode() == Mode::Light ? light : dark;
}

Mode systemMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const Qt::ColorScheme cs = QGuiApplication::styleHints()->colorScheme();
    if (cs == Qt::ColorScheme::Dark) return Mode::Dark;
    if (cs == Qt::ColorScheme::Light) return Mode::Light;
#endif
    const QColor win = QApplication::palette().color(QPalette::Window);
    return win.lightness() < 128 ? Mode::Dark : Mode::Light;
}

Mode mode() { return resolveMode(); }

Pref preference() { return g_pref; }

static void applyStylesheet() {
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance()))
        app->setStyleSheet(stylesheet(tokens()));
}

// Re-apply on OS theme flips only while we follow the system.
static void applyMode();

class ThemeFilter : public QObject {
public:
    bool eventFilter(QObject* watched, QEvent* ev) override {
        if (ev->type() == QEvent::ApplicationPaletteChange &&
            g_pref == Pref::System)
            applyMode();
        return QObject::eventFilter(watched, ev);
    }
};

static void ensureSystemWatch() {
    static bool s_watching = false;
    if (s_watching) return;
    s_watching = true;
    if (QGuiApplication::instance()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QObject::connect(QGuiApplication::styleHints(),
                         &QStyleHints::colorSchemeChanged,
                         [](Qt::ColorScheme) {
                             if (g_pref == Pref::System) applyMode();
                         });
#endif
        static ThemeFilter* filter = new ThemeFilter;
        QApplication::instance()->installEventFilter(filter);
    }
}

static void applyMode() {
    ensureSystemWatch();
    const Mode after = resolveMode();
    if (after == g_mode) return;
    g_mode = after;
    clearIconCache();
    applyStylesheet();
    reapplyIcons(QApplication::activeWindow());
    if (QApplication::instance())
        for (QWidget* w : QApplication::topLevelWidgets())
            w->update();
    if (g_modeChanged)
        g_modeChanged();
}

void setMode(Mode m) {
    setPreference(m == Mode::Dark ? Pref::Dark : Pref::Light);
}

void setPreference(Pref p) {
    if (p == g_pref) return;
    g_pref = p;
    applyMode();
}

void toggleMode() {
    setPreference(resolveMode() == Mode::Dark ? Pref::Light : Pref::Dark);
}

void setModeChangedCallback(ModeChangedCallback cb) { g_modeChanged = std::move(cb); }

void init() {
    ensureSystemWatch();
    g_mode = resolveMode();
    clearIconCache();
    applyStylesheet();
}

// ---------------------------------------------------------------- QSS -----

static QString c(const QColor& col) { return col.name(); }

QString stylesheet(const Tokens& t) {
    const QString radiusSm = QString::number(t.radiusSm);
    const QString radiusMd = QString::number(t.radiusMd);
    const QString radiusLg = QString::number(t.radiusLg);
    const QString font = t.fontFamily;

    return QStringLiteral(R"QSS(
* { font-family: "%1"; font-size: %2px; color: %3; outline: none; }

/* ---- Menu bar ---- */
QMenuBar {
    background: %4; border-bottom: 1px solid %5; padding: 2px 4px;
}
QMenuBar::item { background: transparent; padding: 5px 9px; border-radius: %6; }
QMenuBar::item:selected { background: %7; }
QMenuBar::item:pressed { background: %8; }
QMenu {
    background: %9; border: 1px solid %5; border-radius: %6;
    padding: 4px; margin: 0px;
}
QMenu::item {
    padding: 5px 26px 5px 28px; border-radius: %6; margin: 1px 3px;
    background: transparent;
}
QMenu::item:selected { background: %7; }
QMenu::item:disabled { color: %10; }
QMenu::separator { height: 1px; background: %5; margin: 4px 8px; }
QMenu::indicator { width: 14px; height: 14px; }

/* ---- Toolbar ---- */
#AppToolbar {
    background: %4; border-bottom: 1px solid %5;
}
#ClusterCaption {
    color: %11; font-size: %12px; padding-top: 1px;
}
#ToolbarDivider {
    background: %5; max-width: 1px; min-width: 1px;
    margin: 6px 3px;
}

/* ---- Tool buttons ---- */
QToolButton {
    background: transparent; border: 1px solid transparent; border-radius: %6;
    padding: 2px; margin: 0;
    min-width: %13px; min-height: %13px;
}
QToolButton:hover { background: %7; border-color: transparent; }
QToolButton:pressed { background: %8; }
QToolButton:checked { background: %14; border-color: %14; }
QToolButton:checked:hover { background: %15; }
QToolButton:disabled { background: %16; }
QToolButton[wpFlat="1"] { min-width: 0px; min-height: 0px; padding: 3px 4px; }
QToolButton[wpBig="1"] { min-width: %26px; min-height: %26px; }
QToolButton#MenuButtonPopup { padding-bottom: %30px; }
/* A split button highlights only the piece under the pointer, so the generic
   whole-button hover has to get out of its way. A split button that is also
   active stays fully accent -- hover then only tints the hovered piece. */
QToolButton[wpSplit="1"]:hover { background: transparent; border-color: transparent; }
QToolButton[wpSplit="1"]:checked { background: %14; border-color: %14; }
QToolButton[wpSplit="1"]:checked:hover { background: %14; border-color: %14; }
/* The gallery caret is painted by PopupButton with the marker-tinted
   chevron-down glyph; a QSS sub-control has no cross-axis room and
   squashed it into a 14x5 bar. */
QToolButton#MenuButtonPopup::menu-indicator { image: none; width: 0; height: 0; }
QToolButton::menu-indicator { image: none; width: 0; height: 0; }

/* Drop-down arrow for menu-button popups drawn by Qt itself */
QToolButton::menu-arrow { image: none; }

/* ---- Push buttons ---- */
QPushButton {
    background: %9; border: 1px solid %17; border-radius: %6;
    padding: 6px 14px; min-height: 18px;
}
QPushButton:hover { background: %7; }
QPushButton:pressed { background: %8; }
QPushButton:default { background: %14; border-color: %14; color: %18; }
QPushButton:default:hover { background: %15; border-color: %15; }
QPushButton:disabled { background: %16; color: %10; border-color: %19; }

/* ---- Inputs ---- */
QLineEdit, QSpinBox, QComboBox, QTextEdit {
    background: %9; border: 1px solid %17; border-radius: %6; padding: 4px 6px;
    selection-background-color: %14; selection-color: %18;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus {
    border-color: %14;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox::down-arrow {
    image: none; border-left: 3.5px solid transparent;
    border-right: 3.5px solid transparent; border-top: 4px solid %10;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: %9; border: 1px solid %5; border-radius: %6;
    selection-background-color: %7; selection-color: %3;
    outline: none; padding: 2px;
}
QSpinBox::up-button, QSpinBox::down-button { width: 16px; border: none; background: transparent; }

/* ---- Checkbox / radio ---- */
QCheckBox::indicator, QRadioButton::indicator {
    width: 16px; height: 16px; border-radius: %6;
    border: 1px solid %17; background: %9;
}
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: %14; border-color: %14;
}
QRadioButton::indicator { border-radius: 9px; }
QRadioButton::indicator:checked {
    background: %9; border: 5px solid %14;
}
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: %20; }

/* ---- Slider ---- */
QSlider::groove:vertical {
    width: 6px; background: %17; border-radius: 3px;
}
QSlider::handle:vertical {
    width: 16px; height: 16px; margin: -5px 0;
    background: %9; border: 1px solid %17; border-radius: 8px;
}
QSlider::handle:vertical:hover { background: %7; border-color: %20; }
QSlider::handle:vertical:pressed { background: %14; border-color: %14; }
QSlider::groove:horizontal { height: 6px; background: %17; border-radius: 3px; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: 0 -5px;
    background: %9; border: 1px solid %17; border-radius: 8px;
}
QSlider::handle:horizontal:hover { background: %7; border-color: %20; }
QSlider::handle:horizontal:pressed { background: %14; border-color: %14; }
QSlider::sub-page:horizontal { background: %14; border-radius: 3px; }
QSlider::add-page:horizontal { background: %17; border-radius: 3px; }
QSlider::sub-page:vertical { background: %14; border-radius: 3px; }
QSlider::add-page:vertical { background: %17; border-radius: 3px; }

/* ---- Dock / panels ---- */
QDockWidget { background: %4; color: %3; }
QDockWidget::title {
    background: %4; padding: 6px 8px; border-bottom: 1px solid %5;
    text-align: left; font-weight: 600;
}
QTabWidget::pane { border: none; background: %4; }
QTabBar::tab {
    background: %16; padding: 6px 14px; border: 1px solid %5;
    border-bottom: none; border-top-left-radius: %6; border-top-right-radius: %6;
}
QTabBar::tab:selected { background: %4; }

/* ---- Status bar ---- */
QStatusBar { background: %4; border-top: 1px solid %5; }
QStatusBar::item { border: none; }

/* ---- Scroll areas / scroll bars ---- */
QScrollArea { background: %21; border: none; }
QScrollArea > QWidget > QWidget { background: %21; }
QScrollBar:vertical {
    background: %4; width: 12px; margin: 0; border-left: 1px solid %5;
}
QScrollBar::handle:vertical {
    background: %22; min-height: 30px; border-radius: 5px; margin: 2px;
}
QScrollBar::handle:vertical:hover { background: %23; }
QScrollBar:horizontal { background: %4; height: 12px; margin: 0; }
QScrollBar::handle:horizontal {
    background: %22; min-width: 30px; border-radius: 5px; margin: 2px;
}
QScrollBar::handle:horizontal:hover { background: %23; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ---- Splitter ---- */
QSplitter::handle { background: %5; }

/* ---- List (layers) ---- */
QListWidget {
    background: %4; border: 1px solid %5; border-radius: %6;
    outline: none; padding: 2px;
}
QListWidget::item { border-radius: %6; padding: 3px; margin: 1px; }
QListWidget::item:selected { background: %14; color: %18; }
QListWidget::item:hover { background: %7; color: %3; }

/* ---- Tooltips ---- */
QToolTip {
    background: %9; color: %3; border: 1px solid %5;
    border-radius: %6; padding: 4px 7px; font-size: %12px;
}

/* ---- Dialogs ---- */
QDialog { background: %4; }
#DialogHeader { font-size: %24px; font-weight: 600; color: %3; }
#HintLabel { color: %11; }
#StatusDim { color: %11; }

/* ---- Panels ---- */
#PanelRoot { background: %4; }
#PanelHeader {
    background: %4; border-bottom: 1px solid %5;
    font-weight: 600; font-size: %25px;
}
#PanelRow { background: %4; }
#PanelRow:hover { background: %7; }
#PanelRow[wpActive="true"] { background: %16; }
#ColorWell { border-radius: %6; }
#StatusZoomLabel { min-width: 46px; color: %11; }
#SizeValue { color: %11; font-size: %12px; }
#CaptionMuted { color: %11; }

/* ---- Layers rail ---- */
#LayersPanel { background: %4; }
#LayersHeader {
    background: %4; border-bottom: 1px solid %5;
}
/* The layers dock carries its caption in a custom title-bar widget, so the
   stock QDockWidget title styling would double up. */
#LayersDock::title { background: %4; border: none; padding: 0; }
#LayersCount {
    color: %3; font-weight: 600; font-size: %12px; background: transparent;
}
#LayersList {
    background: %4; border: none; border-radius: 0; outline: none; padding: 0;
}
#LayersList::item {
    background: transparent; border: none; border-radius: 0;
    padding: 0; margin: 0;
}
#LayersList::item:hover { background: %7; }
#LayersList::item:selected { background: transparent; color: %3; }
#LayersList::item:selected:hover { background: %7; }
#LayerRow { background: transparent; border-radius: %6; }
#LayerRow:hover { background: %7; }
#LayerName {
    color: %3; background: transparent; padding: 0 4px 0 0;
}
/* The active layer is a selection, so the row takes the accent fill and the
   label the on-accent text -- pairing them is what Fluent does and stops the
   hard black/white on a pale grey that read as a colour bug. Weight, not
   colour, is what distinguishes the label. */
#LayerRow[wpActive="true"] { background: %14; }
#LayerRow[wpActive="true"] #LayerName { color: %18; font-weight: 600; }
#LayerRow[wpActive="true"]:hover { background: %15; }
/* On the accent row the eye must not pick up a pale hover patch. */
#LayerRow[wpActive="true"] #LayerEyeBtn:hover { background: transparent; }
#LayerEyeBtn, #LayerStripBtn {
    background: transparent; border: none; border-radius: %6;
    padding: 0; min-width: 0; min-height: 0;
}
#LayerEyeBtn:hover, #LayerStripBtn:hover { background: %7; }
#LayerEyeBtn:disabled, #LayerStripBtn:disabled { background: transparent; }
#LayerBackgroundSwatch {
    border: 1px solid %5; border-radius: %6; padding: 0;
    min-width: 0; min-height: 0;
}
#LayerBackgroundSwatch:hover { border-color: %14; }

/* ---- Floating brush-size panel ---- */
#SizeSliderPanel {
    background: %9; border: 1px solid %17; border-radius: %28;
}
#SizeSliderCaption {
    color: %11; font-size: %12px;
}
#SizeSliderValue {
    color: %11; font-size: %12px;
    background: transparent; border: 1px solid transparent;
    padding: 1px 2px; min-height: 0;
}
#SizeSliderValue[wpEditing="1"] {
    color: %3; background: %9; border: 1px solid %14;
}
)QSS")
        .arg(t.fontFamily)      // %1
        .arg(t.pxBase)          // %2
        .arg(c(t.textPrimary))  // %3
        .arg(c(t.window))       // %4
        .arg(c(t.divider))      // %5
        .arg(radiusSm)          // %6
        .arg(c(t.controlHover)) // %7
        .arg(c(t.controlPressed)) // %8
        .arg(c(t.surfaceHigh))  // %9
        .arg(c(t.textTertiary)) // %10
        .arg(c(t.textSecondary))// %11
        .arg(t.pxCaption)       // %12
        .arg(t.toolbarBtn - 6)  // %13 min size
        .arg(c(t.accent))       // %14
        .arg(c(t.accentHover))  // %15
        .arg(c(t.surfaceAlt))   // %16
        .arg(c(t.controlStroke))// %17
        .arg(c(t.textOnAccent)) // %18
        .arg(c(t.controlStrokeSecondary)) // %19
        .arg(c(t.accent))       // %20 focus/hover border reuse
        .arg(c(t.workspace))    // %21
        .arg(c(t.controlStrokeSecondary)) // %22 scrollbar handle
        .arg(c(t.controlHover)) // %23 scrollbar hover
        .arg(t.pxTitle + 4)     // %24
        .arg(t.pxBase + 1)      // %25
        .arg(2 * t.toolbarBtn - 6)  // %26 big tool button min size
        .arg(4)                     // %27 popup buttons bottom padding
        .arg(t.radiusXl)            // %28 floating panel radius
        .arg(t.caretBand)            // %29 gallery chevron strip height
        .arg(t.caretPad)             // %30 bottom padding below a gallery glyph
        .arg(t.caretGlyph);          // %31 gallery caret glyph size
}

// ------------------------------------------------------------- icons -----

static const char* kTokenMarker = "#3b3b3b";

QIcon icon(const QString& name, int px, const QColor& tint) {
    const QColor colour = tint.isValid() ? tint : tokens().icon;
    const QString key = name + QLatin1Char('|') + QString::number(px) +
                        QLatin1Char('|') + colour.name();
    if (g_iconCache.contains(key))
        return g_iconCache.value(key);

    QFile f(QStringLiteral(":/assets/icons/%1.svg").arg(name));
    QIcon ic;
    if (f.open(QIODevice::ReadOnly)) {
        QString data = QString::fromUtf8(f.readAll());
        data.replace(QLatin1String(kTokenMarker), colour.name());
        QSvgRenderer renderer(data.toUtf8());
        // Render at 2x for high-DPI crispness.
        QPixmap pm(QSize(px * 2, px * 2));
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(2.0);
        QPainter p(&pm);
        renderer.render(&p, QRectF(0, 0, px, px));
        p.end();
        ic = QIcon(pm);
        ic.setIsMask(true);
    }
    g_iconCache.insert(key, ic);
    return ic;
}

QIcon tintedIcon(const QString& name, int px, const QColor& color) {
    QFile f(QStringLiteral(":/assets/icons/%1.svg").arg(name));
    QIcon ic;
    if (f.open(QIODevice::ReadOnly)) {
        QString data = QString::fromUtf8(f.readAll());
        data.replace(QLatin1String(kTokenMarker), color.name());
        QSvgRenderer renderer(data.toUtf8());
        QPixmap pm(QSize(px * 2, px * 2));
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(2.0);
        QPainter p(&pm);
        renderer.render(&p, QRectF(0, 0, px, px));
        p.end();
        ic = QIcon(pm);
    }
    return ic;
}

void clearIconCache() { g_iconCache.clear(); }

void reapplyIcons(QWidget* root) {
    if (!root) return;
    const auto widgets = root->findChildren<QWidget*>();
    for (QWidget* w : widgets) {
        const QString iconName = w->property("wpIconName").toString();
        if (iconName.isEmpty()) continue;
        const int px = w->property("wpIconPx").toInt();
        const int size = px > 0 ? px : 20;
        if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            if (b->isChecked() && b->isCheckable())
                b->setIcon(tintedIcon(iconName, size, tokens().iconOnAccent));
            else
                b->setIcon(icon(iconName, size));
        }
    }
    // Menu items are QActions, so they never show up in the widget walk above.
    const auto actions = root->findChildren<QAction*>();
    for (QAction* a : actions) {
        const QString iconName = a->property("wpIconName").toString();
        if (iconName.isEmpty()) continue;
        const int px = a->property("wpIconPx").toInt();
        const int size = px > 0 ? px : 18;
        if (a->isChecked())
            a->setIcon(tintedIcon(iconName, size, tokens().iconOnAccent));
        else
            a->setIcon(icon(iconName, size));
    }
}

void refreshIcon(QAbstractButton* b) {
    if (!b) return;
    const QString name = b->property("wpIconName").toString();
    if (name.isEmpty()) return;
    const int px = b->property("wpIconPx").toInt();
    const int size = px > 0 ? px : 20;
    if (b->isChecked() && b->isCheckable())
        b->setIcon(tintedIcon(name, size, tokens().iconOnAccent));
    else
        b->setIcon(icon(name, size));
}

void setActionIcon(QAction* action, const QString& iconName, int px) {
    action->setProperty("wpIconName", iconName);
    action->setProperty("wpIconPx", px);
    if (action->isChecked())
        action->setIcon(tintedIcon(iconName, px, tokens().iconOnAccent));
    else
        action->setIcon(icon(iconName, px));
}

void setIcon(QWidget* w, const QString& iconName, int px) {
    w->setProperty("wpIconName", iconName);
    w->setProperty("wpIconPx", px);
    if (auto* b = qobject_cast<QAbstractButton*>(w))
        b->setIcon(icon(iconName, px));
}

} // namespace Theme