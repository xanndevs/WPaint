#include "LayersPanel.h"
#include "DrawingUtils.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Settings.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <algorithm>
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
    Draw::checkerboard(p, QRect(QPoint(0, 0), out), 4, tk.checkerLight, tk.checkerDark);
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

// The chevron that folds a folder. Pointing right when closed, down when open,
// which is the same pair the toolbar's drop-down uses.
QIcon foldChevron(bool folded, const QColor& tint) {
    return Theme::icon(folded ? QStringLiteral("chevron-right")
                              : QStringLiteral("chevron-down"),
                       14, tint);
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
    // Extended rather than single: ctrl-click is how you pick several layers to
    // move, copy or group, and every one of those needs more than one.
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setDragDropOverwriteMode(false);
    m_list->setUniformItemSizes(true);
    m_list->setFocusPolicy(Qt::StrongFocus);

    const auto* t = &Theme::tokens();

    m_folder = new QToolButton(this);
    m_folder->setObjectName("LayerStripBtn");
    m_folder->setAutoRaise(true);
    m_folder->setToolTip(tr("Group the selected layers into a folder"));
    Theme::setIcon(m_folder, "layer-folder", t->toolbarBtnSmall);

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

    auto* header = new QHBoxLayout;
    header->setContentsMargins(6, 4, 4, 4);
    header->setSpacing(4);
    header->addWidget(m_count);
    header->addStretch(1);
    header->addWidget(m_folder);
    header->addWidget(m_add);
    header->addWidget(m_remove);

    m_header = new QWidget(this);
    m_header->setObjectName("LayersHeader");
    m_header->setLayout(header);

    // The background lives in its own pinned strip under the list: it is a
    // document property rather than a layer, so it should not look like one.
    m_backgroundBar = new QWidget(this);
    m_backgroundBar->setObjectName("LayerBackgroundBar");
    auto* bgLayout = new QHBoxLayout(m_backgroundBar);
    bgLayout->setContentsMargins(6, 4, 6, 6);
    bgLayout->setSpacing(6);
    auto* bgSwatch = new QToolButton(m_backgroundBar);
    bgSwatch->setObjectName("LayerBackgroundSwatch");
    bgSwatch->setFixedSize(66, 30);
    bgSwatch->setToolTip(tr("Change background color"));
    bgSwatch->setIcon(backgroundSwatch(QColor(Qt::white)));
    bgSwatch->setIconSize(QSize(66, 30));
    connect(bgSwatch, &QToolButton::clicked, this, [this] {
        emit backgroundEditRequested(m_stack->backgroundIndex());
    });
    auto* bgLabel = new QLabel(tr("Background"), m_backgroundBar);
    bgLabel->setObjectName("LayerName");
    bgLayout->addWidget(bgSwatch);
    bgLayout->addWidget(bgLabel, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_backgroundBar);

    connect(m_add, &QToolButton::clicked, this, &LayersPanel::addRequested);
    connect(m_remove, &QToolButton::clicked, this,
            [this] { emit removeSelectionRequested(selectedIndices()); });
    connect(m_folder, &QToolButton::clicked, this,
            [this] { emit folderRequested(selectedIndices()); });
    connect(m_list, &QListWidget::currentRowChanged, this,
            &LayersPanel::onCurrentRowChanged);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const QList<int> sel = selectedIndices();
        m_lastSelected = sel.isEmpty() ? -1 : sel.last();
        updateHeaderState();
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
                if (!item) return;
                startRename(item->data(Qt::UserRole).toInt());
            });
    connect(m_list, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& at) {
                // Right-clicking a row that is not already selected moves the
                // selection to it first, the way every list in every program
                // does -- otherwise the menu acts on rows the pointer is not
                // even over.
                QListWidgetItem* item = m_list->itemAt(at);
                if (item && !item->isSelected())
                    m_list->setCurrentItem(item);
                emit contextRequested(selectedIndices(), mapToGlobal(at));
            });
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex& parent, int start, int end,
                   const QModelIndex& dest, int row) {
                Q_UNUSED(parent);
                Q_UNUSED(dest);
                Q_UNUSED(end);
                const int from = start;
                const int to = row > from ? row - 1 : row;
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

bool LayersPanel::hasFocus() const { return m_list->hasFocus(); }

QList<int> LayersPanel::selectedIndices() const {
    QList<int> out;
    for (QListWidgetItem* item : m_list->selectedItems()) {
        bool ok = false;
        const int index = item->data(Qt::UserRole).toInt(&ok);
        if (ok)
            out << index;
    }
    std::sort(out.begin(), out.end());
    return out;
}

void LayersPanel::setSelection(const QList<int>& indices) {
    m_list->blockSignals(true);
    m_list->clearSelection();
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (!item) continue;
        if (indices.contains(item->data(Qt::UserRole).toInt()))
            item->setSelected(true);
    }
    if (indices.isEmpty()) {
        m_list->setCurrentRow(-1);
    } else {
        // The current row is the topmost of the selection, which is the one a
        // paste-above and a drag both anchor to.
        int first = m_list->count();
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem* item = m_list->item(row);
            if (item && item->isSelected())
                first = qMin(first, row);
        }
        if (first < m_list->count())
            m_list->setCurrentRow(first);
        m_lastSelected = indices.last();
    }
    m_list->blockSignals(false);
    applyActiveProperty(m_list->currentRow());
    updateHeaderState();
}

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
        if (auto* thumb = row->findChild<QLabel*>("LayerThumb")) {
            if (layer.isDrawable())
                thumb->setPixmap(QPixmap::fromImage(thumbFor(layer.image, QSize(64, 34))));
        }
    }
}

void LayersPanel::setActiveLayer(int index) {
    if (m_syncing) return;
    if (index >= 0 && index < m_list->count() && m_list->currentRow() != index) {
        m_list->setCurrentRow(index);
        m_lastSelected = index;
    }
    applyActiveProperty(index);
}

void LayersPanel::rebuildList() {
    m_syncing = true;
    commitRename();
    m_list->blockSignals(true);
    m_list->clear();

    // The background is the last entry in the stack and is drawn in its own
    // pinned strip, so the list holds everything above it.
    const int bg = m_stack->backgroundIndex();
    const int n = bg >= 0 ? bg : m_stack->count();
    m_count->setText(tr("Layers (%1)").arg(m_stack->layerCount()));

    // A folder's children are hidden while it is folded, so a rebuild has to
    // leave them out of the row order and the indices with it.
    int skip = 0;

    for (int i = 0; i < n; ++i) {
        if (skip > 0) {
            --skip;
            continue;
        }
        const Layer& layer = m_stack->layerAt(i);
        const bool isFolder = layer.isFolder;
        if (isFolder && layer.folded)
            skip = layer.childCount;

        QListWidgetItem* item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, i);

        QWidget* row = new QWidget(this);
        row->setObjectName("LayerRow");
        row->setProperty("layerIndex", i);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);
        // Depth is indentation: a child of a group reads as belonging to it
        // without a tree, a guide line or another widget.
        rowLayout->setContentsMargins(4 + m_stack->depthOf(i) * 14, 2, 4, 2);

        const bool isActive = i == m_stack->activeIndex();
        const auto& tk = Theme::tokens();
        const QColor glyphTint = isActive ? tk.iconOnAccent : tk.icon;

        auto* eye = new QToolButton(row);
        eye->setObjectName("LayerEyeBtn");
        eye->setAutoRaise(true);
        eye->setCheckable(true);
        eye->setChecked(layer.visible);
        eye->setToolTip(tr("Toggle layer visibility"));
        eye->setIcon(Theme::icon(layer.visible ? QStringLiteral("eye")
                                               : QStringLiteral("eye-off"),
                                  18, glyphTint));
        eye->setIconSize(QSize(18, 18));
        const int index = i;
        connect(eye, &QToolButton::clicked, this, [this, eye, index](bool on) {
            const bool active = index == m_stack->activeIndex();
            const QColor tint =
                active ? Theme::tokens().iconOnAccent : Theme::tokens().icon;
            eye->setIcon(Theme::icon(on ? QStringLiteral("eye")
                                         : QStringLiteral("eye-off"), 18, tint));
            emit visibilityRequested(index, on);
        });

        auto* thumbHolder = new QWidget(row);
        thumbHolder->setFixedSize(66, 36);
        auto* thumbLayout = new QHBoxLayout(thumbHolder);
        thumbLayout->setContentsMargins(0, 0, 0, 0);
        thumbLayout->setSpacing(0);

        if (isFolder) {
            // The chevron does the folding, and doubles as the row's thumbnail
            // area: a folder has no pixels to show.
            auto* fold = new QToolButton(thumbHolder);
            fold->setObjectName("LayerEyeBtn");
            fold->setAutoRaise(true);
            fold->setToolTip(layer.folded ? tr("Expand this group")
                                          : tr("Collapse this group"));
            fold->setIcon(foldChevron(layer.folded, glyphTint));
            fold->setIconSize(QSize(18, 18));
            fold->setFixedSize(20, 36);
            const int folderIndex = i;
            connect(fold, &QToolButton::clicked, this, [this, folderIndex] {
                emit foldedRequested(folderIndex, !m_stack->layerAt(folderIndex).folded);
            });
            auto* glyph = new QLabel(thumbHolder);
            glyph->setObjectName("LayerFolderGlyph");
            glyph->setFixedSize(24, 36);
            glyph->setPixmap(Theme::icon("layer-folder", 20, glyphTint)
                                 .pixmap(20, 20));
            thumbLayout->addWidget(fold);
            thumbLayout->addWidget(glyph, 1);
        } else {
            auto* thumb = new QLabel(thumbHolder);
            thumb->setObjectName("LayerThumb");
            thumb->setFixedSize(66, 36);
            thumb->setPixmap(QPixmap::fromImage(thumbFor(layer.image, QSize(64, 34))));
            thumbLayout->addWidget(thumb);
        }

        auto* name = new QLabel(layer.name, row);
        name->setObjectName("LayerName");
        name->setMinimumWidth(60);
        if (isFolder)
            name->setToolTip(tr("A group. It holds layers and cannot be drawn on."));

        rowLayout->addWidget(eye);
        rowLayout->addWidget(thumbHolder);
        rowLayout->addWidget(name, 1);

        item->setSizeHint(row->sizeHint());
        row->setProperty("wpActive", isActive);
        m_list->setItemWidget(item, row);
    }

    if (auto* swatch = m_backgroundBar->findChild<QToolButton*>("LayerBackgroundSwatch")) {
        const int b = m_stack->backgroundIndex();
        if (b >= 0)
            swatch->setIcon(backgroundSwatch(m_stack->layerAt(b).backgroundColor));
    }

    m_list->blockSignals(false);
    const int active = m_stack->activeIndex();
    if (active >= 0 && active < m_list->count()) {
        m_list->setCurrentRow(active);
        m_lastSelected = active;
    }
    m_syncing = false;
    updateHeaderState();
}

void LayersPanel::updateHeaderState() {
    const QList<int> sel = selectedIndices();
    const int bg = m_stack->backgroundIndex();
    // The list stops at the background, so a row index equal to it can only mean
    // a stale selection; either way the background is never a delete target.
    const bool removable =
        !sel.isEmpty() && !sel.contains(bg) && m_stack->canRemoveAny(sel);
    const bool groupable = sel.size() > 1 && m_stack->resolveSelection(sel).size() == 1;

    m_remove->setEnabled(removable);
    m_remove->setToolTip(removable ? tr("Delete the selected layers")
                                   : tr("A document keeps at least one layer"));
    // A different glyph, not just a dimmed one: "delete" and "nothing to delete"
    // are different states and the stock disabled tint does not say which.
    Theme::setIcon(m_remove, removable ? QStringLiteral("layer-delete")
                                       : QStringLiteral("layer-delete-disabled"),
                   Theme::tokens().toolbarBtnSmall);
    m_folder->setEnabled(groupable);
    m_folder->setToolTip(groupable ? tr("Group the selected layers into a folder")
                                   : tr("Select the layers to group, in one block"));
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
        // The active row is filled with the accent, so its glyphs have to
        // follow to the on-accent colour or they read as dark smudges.
        for (QToolButton* eye : rowWidget->findChildren<QToolButton*>("LayerEyeBtn")) {
            if (eye->parent() != rowWidget) continue; // the fold chevron is nested
            eye->setIcon(Theme::icon(eye->isChecked() ? QStringLiteral("eye")
                                                       : QStringLiteral("eye-off"),
                                      18, active ? t.iconOnAccent : t.icon));
        }
    }
}

void LayersPanel::onCurrentRowChanged(int row) {
    if (m_syncing || row < 0) return;
    m_lastSelected = row;
    applyActiveProperty(row);
    updateHeaderState();
    emit activeRequested(row);
}

void LayersPanel::beginRenameAt(int index) { startRename(index); }

void LayersPanel::startRename(int index) {
    if (index < 0 || index >= m_stack->count()) return;
    if (m_stack->layerAt(index).isBackground) return;
    if (m_renameEdit)
        return;
    QListWidgetItem* item = m_list->item(index);
    if (!item) return;
    QWidget* row = m_list->itemWidget(item);
    if (!row) return;

    // The editor replaces the label in place rather than opening a dialog: a
    // dialog cannot rename seven layers at once, and the point of the
    // multi-rename is that the name is typed once.
    auto* name = row->findChild<QLabel*>("LayerName");
    if (!name) return;
    m_renamingIndex = index;
    m_renameAlso = selectedIndices();
    m_renameAlso.removeAll(index);

    m_renameEdit = new QLineEdit(row);
    m_renameEdit->setObjectName("LayerNameEdit");
    m_renameEdit->setText(m_stack->layerAt(index).name);
    m_renameEdit->selectAll();
    name->hide();
    row->layout()->addWidget(m_renameEdit);
    m_renameEdit->setFocus();
    m_renameEdit->show();

    connect(m_renameEdit, &QLineEdit::returnPressed, this, &LayersPanel::commitRename);
    connect(m_renameEdit, &QLineEdit::editingFinished, this, [this] {
        // Escape closes the editor without renaming, so the finished signal has
        // to be able to tell "done" from "abandoned".
        if (m_renamingIndex < 0) return;
        if (m_renameEdit && !m_renameEdit->hasFocus()) commitRename();
    });
}

void LayersPanel::commitRename() {
    if (!m_renameEdit) {
        m_renamingIndex = -1;
        return;
    }
    const QString name = m_renameEdit->text().trimmed();
    const int index = m_renamingIndex;
    const QList<int> also = m_renameAlso;
    m_renameEdit->deleteLater();
    m_renameEdit = nullptr;
    m_renamingIndex = -1;
    m_renameAlso.clear();
    if (name.isEmpty() || index < 0)
        return;
    emit renameFinished(index, name, also);
}
