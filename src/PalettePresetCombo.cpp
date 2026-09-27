#include "PalettePresetCombo.h"
#include "PalettePresets.h"
#include "Theme.h"

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

// The strip is drawn by the popup delegate and by the closed combo, so it lives
// here rather than in either of them.
static void paintStrip(QPainter* p, const QVector<QColor>& colors, const QRect& area);

namespace {

// Paints "name" on the left and the preset's twenty swatches on the right, two
// rows of ten, exactly as the toolbar lays them out. A delegate rather than a
// plain item list, because the swatches have to be drawn.
class SwatchDelegate : public QStyledItemDelegate {
public:
    explicit SwatchDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const auto& t = Theme::tokens();
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        if (option.state & QStyle::State_Selected) {
            p->fillRect(option.rect, t.accent);
        } else if (option.state & QStyle::State_MouseOver) {
            p->fillRect(option.rect, t.controlHover);
        }

        const QColor text =
            (option.state & QStyle::State_Selected) ? t.textOnAccent : t.textPrimary;
        const int pad = t.pad * 2;
        QFont font = option.font;
        p->setFont(font);
        const QFontMetrics fm(font);
        const int nameW = fm.horizontalAdvance(index.data().toString());
        p->setPen(text);
        p->drawText(option.rect.adjusted(pad, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    index.data().toString());

        paintStrip(p, index.data(Qt::UserRole + 1).value<QVector<QColor>>(),
                   QRect(option.rect).adjusted(pad, 0, -pad, 0));
        Q_UNUSED(nameW);
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto& t = Theme::tokens();
        const int cell = t.gap * 2 + 2;
        const int stripW = PalettePresets::kColumns * cell + (PalettePresets::kColumns - 1);
        const int rows = index.data(Qt::UserRole + 1).value<QVector<QColor>>().size() /
                             qMax(1, PalettePresets::kColumns)
                         + 1;
        return QSize(option.fontMetrics.horizontalAdvance(index.data().toString()) + stripW +
                         t.pad * 6,
                     rows * (t.pad + 2));
    }
};

} // namespace

static void paintStrip(QPainter* p, const QVector<QColor>& colors, const QRect& area) {
    const auto& t = Theme::tokens();
    if (colors.isEmpty())
        return;
    // A touch larger than the toolbar's swatches: at 18px the strip is 180px
    // wide and the preset's name has nowhere to go.
    const int cell = t.gap * 2 + 2;
    const int columns = qMin(PalettePresets::kColumns, colors.size());
    const int rows = (colors.size() + columns - 1) / columns;
    const int stripW = columns * cell + (columns - 1);
    const int stripH = rows * cell + (rows - 1);
    if (area.width() < stripW || area.height() < stripH)
        return;
    const int x0 = area.right() - stripW + 1;
    const int y0 = area.center().y() - stripH / 2;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < colors.size(); ++i) {
        const int r = i / columns;
        const int c = i % columns;
        const QRect cellRect(x0 + c * (cell + 1), y0 + r * (cell + 1), cell, cell);
        p->setPen(QPen(t.controlStroke, 1));
        p->setBrush(colors.at(i));
        p->drawRect(cellRect.adjusted(0, 0, -1, -1));
    }
    p->restore();
}

PalettePresetCombo::PalettePresetCombo(QWidget* parent) : QComboBox(parent) {
    setObjectName("PalettePreset");
    setItemDelegate(new SwatchDelegate(this));
    setStyleSheet(QString());
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setMaxVisibleItems(8);
    // Wide enough for the name plus the whole strip, since both are on the row.
    setMinimumWidth(320);
}

void PalettePresetCombo::paintEvent(QPaintEvent* ev) {
    QComboBox::paintEvent(ev);
    if (isEditable())
        return;
    const QVector<QColor> colors =
        currentIndex() >= 0 ? itemData(currentIndex(), Qt::UserRole + 1).value<QVector<QColor>>()
                            : QVector<QColor>();
    // Clear the drop-down area first: the base paint leaves the text and the
    // chevron there, and the strip has to go between them.
    const int dropW = 22;
    QPainter p(this);
    const auto& t = Theme::tokens();
    p.fillRect(width() - dropW - t.pad * 2, 0, dropW + t.pad * 2 - 2, height(),
               palette().color(QPalette::Base));
    paintStrip(&p, colors, QRect(t.pad * 2, 0, width() - dropW - t.pad * 3, height()));
}

void PalettePresetCombo::setPresets(const QStringList& names,
                                    const QVector<QVector<QColor>>& swatches) {
    m_swatches = swatches;
    const int keep = currentIndex();
    clear();
    for (int i = 0; i < names.size(); ++i) {
        addItem(names.at(i));
        setItemData(i, QVariant::fromValue(swatches.value(i)), Qt::UserRole + 1);
    }
    setCurrentIndex(qBound(0, keep, qMax(0, names.size() - 1)));
}

void PalettePresetCombo::setSelected(int index) { setCurrentIndex(index); }

int PalettePresetCombo::selected() const { return currentIndex(); }
