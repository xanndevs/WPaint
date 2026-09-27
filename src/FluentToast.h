#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;

// A transient message that flies out from the bottom edge of the window and
// takes itself away again.
//
// It exists for the one thing a modal is wrong for: telling the user that an
// action did nothing, and why, while they are still holding the pointer down on
// the thing that did nothing. A dialog would be a modal for a refusal.
//
// Painted rather than styled, for the same reason the switch and the combo's
// chevron are: a drop shadow and an accent edge are two things QSS cannot do
// between them, and a toast that does not look like the rest of the app is worse
// than no toast.
class FluentToast : public QWidget {
    Q_OBJECT
public:
    explicit FluentToast(QWidget* parent = nullptr);

    // Queues a message. Several in a row stack rather than queue behind each
    // other, so "you did this wrong" twice in a second is both visible.
    void show(const QString& message, int msec = 3200);
    bool isIdle() const { return m_pending.isEmpty(); }

    // Re-stacks and re-anchors; call when the window resizes.
    void relayout();

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    struct Entry {
        QString text;
        QLabel* label = nullptr;
    };

    QLabel* addRow(const QString& text);
    void dropOldest();
    void place();

    QWidget* m_host = nullptr;
    QVBoxLayout* m_rows = nullptr;
    QList<Entry> m_pending;
};
