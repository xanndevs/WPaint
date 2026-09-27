#include "FluentToast.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kRowGap = 8;
}

FluentToast::FluentToast(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setObjectName("FluentToast");
    m_rows = new QVBoxLayout(this);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(kRowGap);
    m_rows->addStretch(1);
}

QLabel* FluentToast::addRow(const QString& text) {
    auto* card = new QWidget(this);
    card->setObjectName("FluentToastCard");
    auto* line = new QHBoxLayout(card);
    line->setContentsMargins(Theme::tokens().pad * 2, Theme::tokens().pad,
                             Theme::tokens().pad * 2, Theme::tokens().pad);
    auto* label = new QLabel(text, card);
    label->setObjectName("FluentToastText");
    label->setWordWrap(false);
    line->addWidget(label);
    m_rows->insertWidget(m_rows->count() - 1, card);
    return label;
}

void FluentToast::show(const QString& message, int msec) {
    auto* label = addRow(message);
    Entry entry{message, label};
    m_pending.append(entry);
    // The card is what fades, so the timer has to find it again by walking up
    // from the label: keeping a second pointer to the same widget is how one of
    // them ends up dangling.
    QWidget* card = label->parentWidget();
    QTimer::singleShot(msec, this, [this, card] {
        for (int i = m_pending.size() - 1; i >= 0; --i) {
            if (m_pending.at(i).label->parentWidget() != card) continue;
            m_pending.removeAt(i);
            card->deleteLater();
            break;
        }
        relayout();
    });
    relayout();
}

void FluentToast::dropOldest() {
    if (m_pending.isEmpty()) return;
    m_pending.takeFirst().label->parentWidget()->deleteLater();
    relayout();
}

void FluentToast::relayout() {
    // Hidden entirely when there is nothing to say, so it cannot eat clicks or
    // paint a stray edge over the canvas.
    const bool any = !m_pending.isEmpty();
    setVisible(any);
    if (!any) return;

    const auto& t = Theme::tokens();
    QWidget* window = parentWidget();
    adjustSize();
    // Two passes: the first size with the previous width, the second with the
    // width that size implies. A toast that jumps on the way in is the one
    // thing that would make it feel like a dialog.
    move(window ? (window->width() - width()) / 2 : 0,
         window ? qMax(0, window->height() - height() - t.pad * 3) : 0);
    update();
}

void FluentToast::paintEvent(QPaintEvent* ev) {
    Q_UNUSED(ev);
    QPainter p(this);
    const auto& t = Theme::tokens();
    for (QWidget* card : findChildren<QWidget*>("FluentToastCard")) {
        const QRectF box = QRectF(card->mapTo(this, QPoint(0, 0)), card->size());
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(t.controlStroke, 1));
        p.setBrush(t.surfaceHigh);
        p.drawRoundedRect(box, t.radiusLg, t.radiusLg);
        // An accent edge on the left, which is what makes a row of toasts read
        // as notifications rather than as stray labels.
        p.setPen(Qt::NoPen);
        p.setBrush(t.accent);
        p.drawRoundedRect(QRectF(box.left() + 1, box.top() + 1, 3, box.height() - 2), 1.5,
                          1.5);
    }
}
