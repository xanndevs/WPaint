#include "SizeSliderPanel.h"

#include "FluentSlider.h"
#include "Theme.h"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QVBoxLayout>

SizeSliderPanel::SizeSliderPanel(QWidget* parent) : QWidget(parent) {
    const auto& t = Theme::tokens();

    setObjectName("SizeSliderPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(t.sizePanelW, t.sizePanelH);

    m_slider = new FluentSlider(Qt::Vertical);
    m_slider->setRange(1, 64);
    m_slider->setValue(4);
    m_slider->setSingleStep(1);
    m_slider->setPageStep(4);
    m_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_slider->setObjectName("SizeSlider");

    m_value = new QLabel("4", this);
    m_value->setObjectName("SizeSliderValue");
    m_value->setAlignment(Qt::AlignCenter);

    auto* caption = new QLabel("Size", this);
    caption->setObjectName("SizeSliderCaption");
    caption->setAlignment(Qt::AlignCenter);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 14, 4, 8);
    layout->setSpacing(2);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_value);
    layout->addWidget(caption);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(t.panelShadow);
    setGraphicsEffect(shadow);

    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_value->setText(QString::number(v));
        emit sizeChanged(v);
    });
}

int SizeSliderPanel::size() const { return m_slider->value(); }

void SizeSliderPanel::setSize(int s) {
    const QSignalBlocker b(m_slider);
    m_slider->setValue(s);
    m_value->setText(QString::number(s));
}

void SizeSliderPanel::setEnabledForTool(bool enabled) {
    m_slider->setEnabled(enabled);
    m_value->setEnabled(enabled);
}

void SizeSliderPanel::setImmediateVisible(bool visible) {
    setVisible(visible);
}