#pragma once

#include "PaletteEditor.h"
#include "PalettePresetCombo.h"

#include <QDialog>
#include <QList>
#include <QListWidget>
#include <QStackedWidget>

class FluentSwitch;
class QComboBox;
class SettingsPage;
class QSpinBox;
class QLabel;
class QVBoxLayout;

// "Preferences" modal: a left rail of sections, a stacked page per section, and
// the usual Ok / Cancel / Restore Defaults footer.
//
// The sections are a rail rather than a tab strip because a tab bar has to fit
// four labels on one line, and these labels are sentences. The dialog stages
// its edits and applies them on accept, so Cancel is a true no-op.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    // Re-reads every control from the settings table.
    void reload();

private:
    void buildBehaviorPage();
    void buildShortcutsPage();
    void buildDefaultsPage();
    void buildAboutPage();
    // A row of two titled labels with one control on the right.
    FluentSwitch* addSwitch(const QString& title, const QString& description,
                            bool checked, QWidget* page, QVBoxLayout* body);
    QComboBox* addCombo(const QString& title, const QString& description,
                        const QStringList& items, int index, QWidget* page,
                        QVBoxLayout* body);

    QListWidget* m_nav = nullptr;
    QStackedWidget* m_pages = nullptr;
    QWidget* m_behaviorPage = nullptr;
    QWidget* m_defaultsPage = nullptr;
    QVBoxLayout* m_behaviorBody = nullptr;
    QVBoxLayout* m_defaultsBody = nullptr;

    SettingsPage* m_shortcuts = nullptr;
    FluentSwitch* m_antialias = nullptr;
    FluentSwitch* m_crispMagnified = nullptr;
    FluentSwitch* m_boundaryHandles = nullptr;
    FluentSwitch* m_spaceWheel = nullptr;
    FluentSwitch* m_smoothShapes = nullptr;
    FluentSwitch* m_smoothText = nullptr;
    FluentSwitch* m_confirmDiscard = nullptr;
    QComboBox* m_thumbnailQuality = nullptr;
    QSpinBox* m_undoLimit = nullptr;

    QComboBox* m_defaultShape = nullptr;
    QList<int> m_defaultShapeIds;
    QComboBox* m_defaultShapeStyle = nullptr;
    QComboBox* m_defaultBrushStyle = nullptr;
    QSpinBox* m_defaultBrushSize = nullptr;
    QComboBox* m_defaultCanvasSize = nullptr;
    QWidget* m_primarySwatch = nullptr;
    QWidget* m_secondarySwatch = nullptr;
    QWidget* m_backgroundSwatch = nullptr;
    PalettePresetCombo* m_palettePreset = nullptr;
    PaletteEditor* m_paletteEditor = nullptr;
};
