#include "ResizeDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

ResizeDialog::ResizeDialog(const QSize& documentSize,
                           const QSize& selectionSize, QWidget* parent)
    : QDialog(parent), m_doc(documentSize), m_selectionSize(selectionSize) {
    setWindowTitle(tr("Resize and Rotate"));
    setModal(true);
    setMinimumWidth(340);

    auto* targetBox = new QGroupBox("Resize", this);

    auto* canvasRadio = new QRadioButton(tr("Canvas"), targetBox);
    canvasRadio->setChecked(true);
    auto* selectionRadio = new QRadioButton(tr("Selection"), targetBox);
    selectionRadio->setEnabled(selectionSize.isValid() && !selectionSize.isEmpty());
    selectionRadio->setToolTip(selectionRadio->isEnabled()
                                   ? tr("Resize or rotate the selected area")
                                   : tr("Select an area of the image first"));
    m_targetGroup = new QButtonGroup(this);
    m_targetGroup->addButton(canvasRadio, 0);
    m_targetGroup->addButton(selectionRadio, 1);

    auto* pctRadio = new QRadioButton(tr("Percentage"), targetBox);
    pctRadio->setChecked(true);
    auto* pxRadio = new QRadioButton(tr("Pixels"), targetBox);
    m_unitGroup = new QButtonGroup(this);
    m_unitGroup->addButton(pctRadio, 0);
    m_unitGroup->addButton(pxRadio, 1);

    m_hwPct = new QSpinBox(targetBox);
    m_hwPct->setRange(1, 1000);
    m_hwPct->setValue(100);
    m_vwPct = new QSpinBox(targetBox);
    m_vwPct->setRange(1, 1000);
    m_vwPct->setValue(100);
    m_hwPx = new QSpinBox(targetBox);
    m_hwPx->setRange(1, 20000);
    m_hwPx->setValue(documentSize.width());
    m_vwPx = new QSpinBox(targetBox);
    m_vwPx->setRange(1, 20000);
    m_vwPx->setValue(documentSize.height());

    auto* targetVal = new QLabel(targetBox);
    Q_UNUSED(targetVal);

    auto* sizeGrid = new QGridLayout(targetBox);
    sizeGrid->addWidget(canvasRadio, 0, 0, 1, 2);
    sizeGrid->addWidget(selectionRadio, 1, 0, 1, 2);
    sizeGrid->addWidget(pctRadio, 2, 0);
    sizeGrid->addWidget(m_hwPct, 2, 1);
    sizeGrid->addWidget(new QLabel("x", targetBox), 2, 2);
    sizeGrid->addWidget(m_vwPct, 2, 3);
    sizeGrid->addWidget(pxRadio, 3, 0);
    sizeGrid->addWidget(m_hwPx, 3, 1);
    sizeGrid->addWidget(new QLabel("x", targetBox), 3, 2);
    sizeGrid->addWidget(m_vwPx, 3, 3);
    sizeGrid->setColumnStretch(4, 1);

    m_keep = new QCheckBox(tr("Maintain aspect ratio"), targetBox);
    m_keep->setChecked(true);
    sizeGrid->addWidget(m_keep, 4, 0, 1, 4);

    auto* rotateBox = new QGroupBox(tr("Rotate"), this);
    auto* none = new QRadioButton(tr("No rotation"), rotateBox);
    none->setChecked(true);
    auto* r180 = new QRadioButton(tr("Rotate 180°"), rotateBox);
    auto* r90 = new QRadioButton(tr("Rotate 90° right"), rotateBox);
    auto* r270 = new QRadioButton(tr("Rotate 90° left"), rotateBox);
    m_rotateGroup = new QButtonGroup(this);
    m_rotateGroup->addButton(none, 0);
    m_rotateGroup->addButton(r180, 1);
    m_rotateGroup->addButton(r90, 2);
    m_rotateGroup->addButton(r270, 3);

    auto* rotateLayout = new QGridLayout(rotateBox);
    rotateLayout->addWidget(none, 0, 0);
    rotateLayout->addWidget(r90, 0, 1);
    rotateLayout->addWidget(r180, 1, 0);
    rotateLayout->addWidget(r270, 1, 1);

    m_hw = m_hwPct;
    m_vw = m_vwPct;
    Q_UNUSED(m_hw);
    Q_UNUSED(m_vw);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ResizeDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ResizeDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addWidget(targetBox);
    auto* rotateRow = new QHBoxLayout;
    rotateRow->addWidget(rotateBox);
    rotateRow->addStretch(1);
    root->addLayout(rotateRow);
    root->addWidget(buttons);

    auto syncUnits = [this, pctRadio, pxRadio] {
        const bool pct = pctRadio->isChecked();
        m_hwPct->setEnabled(pct);
        m_vwPct->setEnabled(pct);
        m_hwPx->setEnabled(!pct);
        m_vwPx->setEnabled(!pct);
    };
    connect(pctRadio, &QRadioButton::toggled, this, syncUnits);
    syncUnits();

    auto onTargetChanged = [this] { Q_UNUSED(this); };
    connect(m_targetGroup, &QButtonGroup::idClicked, this, onTargetChanged);

    connect(m_hwPct, qOverload<int>(&QSpinBox::valueChanged), this,
            &ResizeDialog::onHorizontalChanged);
    connect(m_hwPx, qOverload<int>(&QSpinBox::valueChanged), this,
            &ResizeDialog::onHorizontalChanged);
    connect(m_vwPct, qOverload<int>(&QSpinBox::valueChanged), this,
            &ResizeDialog::onVerticalChanged);
    connect(m_vwPx, qOverload<int>(&QSpinBox::valueChanged), this,
            &ResizeDialog::onVerticalChanged);

    auto* hint = new QLabel(tr("Current size: %1 × %2 px")
                                .arg(documentSize.width())
                                .arg(documentSize.height()),
                            this);
    hint->setObjectName("ResizeHint");
    root->insertWidget(2, hint);
}

ResizeParams ResizeDialog::params() const {
    ResizeParams p;
    p.selection = m_targetGroup->checkedId() == 1;
    p.usePixels = m_unitGroup->checkedId() == 1;
    p.keepAspect = m_keep->isChecked();
    p.rotateDegrees = m_rotateGroup->checkedId() == 2
                          ? 90
                          : (m_rotateGroup->checkedId() == 1
                                 ? 180
                                 : (m_rotateGroup->checkedId() == 3 ? 270 : 0));

    const QSize base = p.selection && !m_selectionSize.isEmpty()
                           ? m_selectionSize
                           : m_doc;
    if (p.usePixels) {
        p.width = m_hwPx->value();
        p.height = m_vwPx->value();
    } else {
        p.width = qMax(1, qRound(base.width() * m_hwPct->value() / 100.0));
        p.height = qMax(1, qRound(base.height() * m_vwPct->value() / 100.0));
    }
    if (p.rotateDegrees == 90 || p.rotateDegrees == 270)
        qSwap(p.width, p.height);
    return p;
}

void ResizeDialog::onHorizontalChanged() {
    if (!m_keep->isChecked()) return;
    const bool pct = m_unitGroup->checkedId() == 0;
    QSpinBox* h = pct ? m_hwPct : m_hwPx;
    QSpinBox* v = pct ? m_vwPct : m_vwPx;
    const QSize base = m_doc;
    if (base.isEmpty()) return;
    const double ratio = double(base.height()) / base.width();
    const QSignalBlocker b(v);
    v->setValue(pct ? h->value() : qRound(h->value() * ratio));
}

void ResizeDialog::onVerticalChanged() {
    if (!m_keep->isChecked()) return;
    const bool pct = m_unitGroup->checkedId() == 0;
    QSpinBox* h = pct ? m_hwPct : m_hwPx;
    QSpinBox* v = pct ? m_vwPct : m_vwPx;
    const QSize base = m_doc;
    if (base.isEmpty()) return;
    const double ratio = double(base.width()) / base.height();
    const QSignalBlocker b(h);
    h->setValue(pct ? v->value() : qRound(v->value() * ratio));
}