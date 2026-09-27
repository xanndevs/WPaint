#pragma once

#include <QWidget>

class QHBoxLayout;
class QVBoxLayout;

// One preference, as a sentence and a control.
//
// The sentence is the control's label: "Antialias the canvas" says what the
// setting *does*, and the description underneath says what it costs or affects.
// That is the whole reason this widget exists -- a row of bare checkboxes
// labelled "Antialias" and "Boundary handles" makes the reader do the work
// that the UI should have done, and a pair of radios for a two-state choice
// doubles the row count to say the same thing.
class SettingsRow : public QWidget {
    Q_OBJECT
public:
    SettingsRow(const QString& title, const QString& description,
                QWidget* parent = nullptr);

    // Right-hand control, at its natural size. Ownership moves here.
    void setControl(QWidget* control);

    // Full-width block under the description, for a control that needs the
    // whole row -- a palette grid, a shortcut table. Ownership moves here.
    void addContent(QWidget* content);

    QHBoxLayout* controlLayout() { return m_controlRow; }
    QVBoxLayout* textLayout() { return m_text; }

private:
    QVBoxLayout* m_text = nullptr;
    QHBoxLayout* m_controlRow = nullptr;
    QVBoxLayout* m_root = nullptr;
};
