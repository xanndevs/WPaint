#include "LayersPanel.h"
#include "DrawingUtils.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Settings.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QImage thumbFor(const QImage& img, const QSize& out) {
    QImage canvas(out, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    if (img.isNull()) return canvas;
    const QSize scaled = img.size().scaled(out - QSize(2, 2), Qt::KeepAspectRatio);
    if (scaled.width() < 1 || scaled.height() < 1) return canvas;
    QPainter p(&canvas);
    const auto& tk = Theme::tokens();
    Draw::checkerboard(p, QRect(QPoint(0, 0), out), 4, tk.checkerLight,
                       tk.checkerDark);
    const QImage thumb =
        img.scaled(scaled, Qt::KeepAspectRatio,
                   Settings::thumbnailQuality() == 1 ? Qt::SmoothTransformation
                                                     : Qt::FastTransformation);
    const QPoint pos((out.width() - thumb.width()) / 2,
                     (out.height() - thumb.height()) / 2);
    p.drawImage(pos, thumb);
    p.end();
    return canvas;
}

QIcon backgroundSwatch(const QColor& color) {
    QPixmap pm(66, 36);
    pm.fill(color);
    QPainter p(&pm);
    p.setPen(QPen(Theme::tokens().canvasBorder, 1));
    p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
    p.end();
    return QIcon(pm);
}

} // namespace

LayersPanel::LayersPanel(LayerStack* stack, QWidget* parent)
    : QWidget(parent), m_stack(stack) {
    setObjectName("LayersPanel");
    setMinimumWidth(Theme::tokens().panelW);
    setMaximumWidth(Theme::tokens().panelW + 40);

    m_count = new QLabel("Layer 1", this);
    m_count->setObjectName("LayersCount");
    m_list = new QListWidget(this);
    m_list->setObjectName("LayersList");
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setDragDropOverwriteMode(false);
    m_list->setUniformItemSizes(true);

    const auto* t = &Theme::tokens();
    const int thumbH = 36;

    m_add = new QToolButton(this);
    m_add->setObjectName("LayerStripBtn");
    m_add->setAutoRaise(true);
    m_add->setToolTip(tr("Add new layer"));
    Theme::setIcon(m_add, "layer-add", t->toolbarBtnSmall);

    m_remove = new QToolButton(this);
    m_remove->setObjectName("LayerStripBtn");
    m_remove->setAutoRaise(true);
    m_remove->setToolTip(tr("Delete selected layer"));
    Theme::setIcon(m_remove, "layer-delete", t->toolbarBtnSmall);
    Q_UNUSED(thumbH);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(6, 4, 4, 4);
    header->setSpacing(4);
    header->addWidget(m_count);
    header->addStretch(1);
    header->addWidget(m_add);
    header->addWidget(m_remove);

    m_header = new QWidget(this);
    m_header->setObjectName("LayersHeader");
    m_header->setLayout(header);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_list, 1);

    connect(m_add, &QToolButton::clicked, this, &LayersPanel::addRequested);
    connect(m_remove, &QToolButton::clicked, this, [this] {
        if (m_list->currentRow() >= 0)
            emit removeRequested(m_list->currentRow());
    });
    connect(m_list, &QListWidget::currentRowChanged, this,
            &LayersPanel::onCurrentRowChanged);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex& parent, int start, int end,
                   const QModelIndex& dest, int row) {
                Q_UNUSED(parent);
                Q_UNUSED(dest);
                const int from = start;
                const int to = row > from ? row - 1 : row;
                Q_UNUSED(end);
                rebuildList();
                if (from != to)
                    emit moveRequested(from, to);
            });

    connect(m_stack, &LayerStack::changed, this, &LayersPanel::rebuildList);
    connect(m_stack, &LayerStack::activeChanged, this,
            &LayersPanel::setActiveLayer);

    rebuildList();
}

void LayersPanel::refresh() { rebuildList(); }

// Repaints the previews in place. Drawing only changes pixels, so there is no
// reason to tear the rows down and rebuild them on every stroke -- that
// discards the selection, the eye buttons and the hover state each time.
void LayersPanel::updateThumbnails() {
    for (int i = 0; i < m_list->count(); ++i) {
        QWidget* row = m_list->itemWidget(m_list->item(i));
        if (!row) continue;
        const int index = row->property("layerIndex").toInt();
        if (index < 0 || index >= m_stack->count()) continue;
        const Layer& layer = m_stack->layerAt(index);
        if (auto* swatch = row->findChild<QToolButton*>("LayerBackgroundSwatch")) {
            if (layer.isBackground)
                swatch->setIcon(backgroundSwatch(layer.backgroundColor));
            continue;
        }
        if (auto* thumb = row->findChild<QLabel*>("LayerThumb")) {
            if (!layer.isBackground)
                thumb->setPixmap(QPixmap::fromImage(
                    thumbFor(layer.image, QSize(64, 34))));
        }
    }
}

void LayersPanel::setActiveLayer(int index) {
    if (m_syncing) return;
    if (index >= 0 && index < m_list->count() && m_list->currentRow() != index)
        m_list->setCurrentRow(index);
}

void LayersPanel::rebuildList() {
    m_syncing = true;
    m_list->blockSignals(true);
    m_list->clear();

    const int n = m_stack->count();
    m_count->setText(tr("Layers (%1)").arg(n));
    // The background can neither be removed nor be the only thing left.
    const int bg = m_stack->backgroundIndex();
    m_remove->setEnabled(n > 1 && m_list->currentRow() != bg);

    for (int i = 0; i < n; ++i) {
        const Layer& layer = m_stack->layerAt(i);

        QListWidgetItem* item = new QListWidgetItem(m_list);
        // The background is pinned to the bottom of the stack, so it must not
        // be selectable as a paint target or dragged out of position.
        if (layer.isBackground)
            item->setFlags(Qt::ItemIsEnabled);

        QWidget* row = new QWidget(this);
        row->setObjectName("LayerRow");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);

        auto* eye = new QToolButton(row);
        eye->setObjectName("LayerEyeBtn");
        eye->setAutoRaise(true);
        eye->setCheckable(true);
        eye->setChecked(layer.visible);
        eye->setToolTip(tr("Toggle layer visibility"));
        const bool isActive = i == m_stack->activeIndex();
        const auto& tk = Theme::tokens();
        const QColor eyeTint = isActive ? tk.iconOnAccent : tk.icon;
        const QIcon eyeIcon = Theme::icon("eye", 18, eyeTint);
        const QIcon eyeOff = Theme::icon("eye-off", 18, eyeTint);
        eye->setIcon(layer.visible ? eyeIcon : eyeOff);
        eye->setIconSize(QSize(18, 18));
        connect(eye, &QToolButton::clicked, this, [this, eye, i](bool on) {
            const bool isActive = i == m_stack->activeIndex();
            const QColor tint = isActive ? Theme::tokens().iconOnAccent
                                         : Theme::tokens().icon;
            eye->setIcon(Theme::icon(on ? QStringLiteral("eye")
                                        : QStringLiteral("eye-off"), 18, tint));
            emit visibilityRequested(i, on);
        });

        auto* thumbLayout = new QHBoxLayout;
        if (layer.isBackground) {
            // A flat swatch of the backdrop colour, clickable to recolour it.
            auto* swatch = new QToolButton(row);
            swatch->setObjectName("LayerBackgroundSwatch");
            swatch->setProperty("layerIndex", i);
            swatch->setFixedSize(66, 36);
            swatch->setToolTip(tr("Change background color"));
            swatch->setIcon(backgroundSwatch(layer.backgroundColor));
            swatch->setIconSize(QSize(66, 36));
            const int index = i;
            connect(swatch, &QToolButton::clicked, this,
                    [this, index] { emit backgroundEditRequested(index); });
            thumbLayout->addWidget(swatch);
        } else {
            auto* thumb = new QLabel(row);
            thumb->setObjectName("LayerThumb");
            thumb->setProperty("layerIndex", i);
            thumb->setPixmap(QPixmap::fromImage(
                thumbFor(layer.image, QSize(64, 34))));
            thumb->setFixedSize(66, 36);
            thumbLayout->addWidget(thumb);
        }

        auto* name = new QLabel(layer.name, row);
        name->setObjectName("LayerName");
        name->setMinimumWidth(60);

        rowLayout->addWidget(eye);
        rowLayout->addLayout(thumbLayout);
        rowLayout->addWidget(name, 1);

        item->setSizeHint(row->sizeHint());
        row->setProperty("layerIndex", i);
        row->setProperty("wpActive", i == m_stack->activeIndex());
        m_list->setItemWidget(item, row);
    }

    m_list->blockSignals(false);
    const int active = m_stack->activeIndex();
    if (active >= 0 && active < m_list->count())
        m_list->setCurrentRow(active);
    m_syncing = false;
}

void LayersPanel::applyActiveProperty(int activeRow) {
    const auto& t = Theme::tokens();
    for (int i = 0; i < m_list->count(); ++i) {
        QWidget* rowWidget = m_list->itemWidget(m_list->item(i));
        if (!rowWidget) continue;
        const bool active = i == activeRow;
        rowWidget->setProperty("wpActive", active);
        rowWidget->style()->unpolish(rowWidget);
        rowWidget->style()->polish(rowWidget);
        // The active row is filled with the accent, so its eye glyph has to
        // follow to the on-accent colour or it reads as a dark smudge.
        for (QToolButton* eye : rowWidget->findChildren<QToolButton*>("LayerEyeBtn")) {
            eye->setIcon(Theme::icon(eye->isChecked() ? QStringLiteral("eye")
                                                       : QStringLiteral("eye-off"),
                                      18, active ? t.iconOnAccent : t.icon));
        }
    }
}

void LayersPanel::onCurrentRowChanged(int row) {
    if (m_syncing || row < 0) return;
    applyActiveProperty(row);
    emit activeRequested(row);
}