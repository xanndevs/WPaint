#pragma once

#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLayout;
class QVBoxLayout;

// One preference, as a sentence and a control.
//
// The sentence is the control's label: "Antialias the canvas" says what the
// setting *does*, and the description says what it costs or affects -- on hover,
// not underneath. A page of twenty rows with two lines each is a wall of grey
// prose nobody reads, and the second line only matters to the one person
// wondering what a setting does, which is what hovering is for.
//
// The control is the row's tooltip too, unless it has something more specific to
// say, so hovering a switch explains its setting the same as hovering its name.
class SettingsRow : public QWidget {
    Q_OBJECT
public:
    enum class Description { Tooltip, Inline };

    SettingsRow(const QString& title, const QString& description,
                QWidget* parent = nullptr,
                Description desc = Description::Tooltip);

    // Right-hand control, at its natural size. Ownership moves here.
    void setControl(QWidget* control);
    // A second right-hand control, for a row that needs more than one.
    void addControl(QWidget* control);

    // Full-width block under the description, for a control that needs the
    // whole row -- a palette grid, a shortcut table. Ownership moves here.
    void addContent(QWidget* content);
    // The same, for a row of several controls that is a layout rather than a
    // single widget.
    void addContentLayout(QLayout* layout);

    QHBoxLayout* controlLayout() { return m_controlRow; }
    QVBoxLayout* textLayout() { return m_text; }

private:
    void adoptTooltip(QWidget* control);

    QString m_description;
    QVBoxLayout* m_text = nullptr;
    QHBoxLayout* m_controlRow = nullptr;
    QVBoxLayout* m_root = nullptr;
};
