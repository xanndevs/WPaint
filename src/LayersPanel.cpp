#include "LayersPanel.h"
#include "DrawingUtils.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
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
    Draw::checkerboard(p, QRect(QPoint(0, 0), out), 4, QColor("#ffffff"),
                       QColor("#c8c8c8"));
    const QImage thumb =
        img.scaled(scaled, Qt::KeepAspectRatio, Qt::FastTransformation);
    const QPoint pos((out.width() - thumb.width()) / 2,
                     (out.height() - thumb.height()) / 2);
    p.drawImage(pos, thumb);
    p.end();
    return canvas;
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
    header->addWidget(m_count);
    header->addStretch(1);
    header->addWidget(m_add);
    header->addWidget(m_remove);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
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

    auto* hint = new QLabel(tr("Select a layer to draw on"), this);
    hint->setObjectName("LayersHint");
    Q_UNUSED(hint);

    connect(m_stack, &LayerStack::changed, this, &LayersPanel::rebuildList);
    connect(m_stack, &LayerStack::activeChanged, this,
            &LayersPanel::setActiveLayer);

    rebuildList();
}

void LayersPanel::refresh() { rebuildList(); }

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
    m_remove->setEnabled(n > 1);

    for (int i = 0; i < n; ++i) {
        const Layer& layer = m_stack->layerAt(i);

        QListWidgetItem* item = new QListWidgetItem(m_list);

        QWidget* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);

        auto* eye = new QToolButton(row);
        eye->setObjectName("LayerEyeBtn");
        eye->setAutoRaise(true);
        eye->setCheckable(true);
        eye->setChecked(layer.visible);
        eye->setToolTip(tr("Toggle layer visibility"));
        const QIcon eyeIcon = Theme::icon("eye", 18);
        const QIcon eyeOff = Theme::icon("eye-off", 18);
        const int ei = m_stack->activeIndex() == i && !layer.visible ? 18 : 18;
        Q_UNUSED(ei);
        eye->setIcon(layer.visible ? eyeIcon : eyeOff);
        eye->setIconSize(QSize(18, 18));
        connect(eye, &QToolButton::clicked, this, [this, eye, i](bool on) {
            eye->setIcon(on ? Theme::icon("eye", 18) : Theme::icon("eye-off", 18));
            emit visibilityRequested(i, on);
        });

        auto* thumb = new QLabel(row);
        thumb->setPixmap(QPixmap::fromImage(
            thumbFor(layer.image, QSize(64, 34))));
        thumb->setFixedSize(66, 36);

        auto* name = new QLabel(layer.name, row);
        name->setObjectName("LayerName");
        name->setMinimumWidth(60);

        rowLayout->addWidget(eye);
        rowLayout->addWidget(thumb);
        rowLayout->addWidget(name, 1);

        item->setSizeHint(row->sizeHint());
        row->setProperty("layerIndex", i);
        m_list->setItemWidget(item, row);
    }

    m_list->blockSignals(false);
    const int active = m_stack->activeIndex();
    if (active >= 0 && active < m_list->count())
        m_list->setCurrentRow(active);
    m_syncing = false;
}

void LayersPanel::onCurrentRowChanged(int row) {
    if (m_syncing || row < 0) return;
    for (int i = 0; i < m_list->count(); ++i) {
        QWidget* rowWidget = m_list->itemWidget(m_list->item(i));
        if (rowWidget)
            rowWidget->setProperty("selected", i == row);
    }
    emit activeRequested(row);
}