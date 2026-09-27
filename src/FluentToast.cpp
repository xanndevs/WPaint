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
    // Shown explicitly. A card created while the toast is already on screen --
    // which is every card after the first -- comes up hidden, and the toast
    // paints the card's background itself, so the result is a stack of empty
    // cards: the shape of a notification with nothing in it.
    card->show();
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
    // The cards are placed by hand rather than by a layout. A QVBoxLayout does
    // not count a widget that was inserted a moment ago until it has been
    // through a layout pass, and a card is clipped by the toast it lives in --
    // so the second notification was drawn on top of the first, both sharing
    // the height of one. It is not a cosmetic problem: it reads as a single
    // toast with the wrong text in it, which is what the user reported.
    const QList<QWidget*> cards = findChildren<QWidget*>(QStringLiteral("FluentToastCard"));
    int wanted = 0;
    int y = 0;
    for (QWidget* card : cards) {
        wanted = qMax(wanted, card->sizeHint().width());
        y += card->sizeHint().height();
        if (card != cards.first())
            y += kRowGap;
    }
    if (wanted > 0 && y > 0)
        resize(wanted, y);
    y = 0;
    for (QWidget* card : cards) {
        card->setGeometry(0, y, qMax(0, wanted - 1), card->sizeHint().height());
        y += card->height() + kRowGap;
    }
    move(window ? (window->width() - wanted) / 2 : 0,
         window ? qMax(0, window->height() - y + kRowGap - t.pad * 3) : 0);
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
