#include "SizeSliderPanel.h"
#include "FluentSlider.h"
#include "Theme.h"
#include "Tool.h"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

// A number that behaves like a spin box only once you click it. Before that it
// is inert, so the panel reads as a plain caption and a stray click cannot drop
// a caret into it; clicking turns it into a real field, and it goes inert again
// when focus leaves. The click has to be caught on the line edit, not on the
// spin box: the line edit is the child that actually sits under the cursor, so a
// press on it never reaches QSpinBox::mousePressEvent.
class ValueBox : public QSpinBox {
public:
    explicit ValueBox(QWidget* parent = nullptr) : QSpinBox(parent) {
        lineEdit()->installEventFilter(this);
    }

protected:
    void mousePressEvent(QMouseEvent* ev) override {
        if (isReadOnly()) {
            beginEdit();
            ev->accept();
            return;
        }
        QSpinBox::mousePressEvent(ev);
    }

    bool eventFilter(QObject* watched, QEvent* ev) override {
        if (watched == lineEdit() && ev->type() == QEvent::MouseButtonPress &&
            isReadOnly()) {
            beginEdit();
            static_cast<QMouseEvent*>(ev)->accept();
            return true;
        }
        return QSpinBox::eventFilter(watched, ev);
    }

    void focusOutEvent(QFocusEvent* ev) override {
        setReadOnly(true);
        setProperty("wpEditing", 0);
        style()->unpolish(this);
        style()->polish(this);
        QSpinBox::focusOutEvent(ev);
    }

private:
    void beginEdit() {
        setReadOnly(false);
        setProperty("wpEditing", 1);
        style()->unpolish(this);
        style()->polish(this);
        selectAll();
        setFocus(Qt::MouseFocusReason);
    }
};

} // namespace

SizeSliderPanel::SizeSliderPanel(QWidget* parent) : QWidget(parent) {
    const auto& t = Theme::tokens();

    setObjectName("SizeSliderPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(t.sizePanelW, t.sizePanelH);

    m_slider = new FluentSlider(Qt::Vertical);
    m_slider->setRange(1, kSizeSliderMax);
    m_slider->setValue(m_size);
    m_slider->setSingleStep(1);
    m_slider->setPageStep(4);
    m_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_slider->setObjectName("SizeSlider");
    m_slider->installEventFilter(this);

    m_value = new ValueBox(this);
    m_value->setObjectName("SizeSliderValue");
    m_value->setAlignment(Qt::AlignCenter);
    m_value->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_value->setReadOnly(true);
    // A value typed here is allowed to exceed the slider's travel; the slider
    // simply parks at its end until the value comes back down into range.
    m_value->setRange(1, kMaxBrushSize);
    m_value->setValue(m_size);
    // Commit on Return/focus-out rather than per keystroke, so a half-typed
    // number never takes effect on the way through.
    m_value->setKeyboardTracking(false);

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

    // Moving the handle always lands inside the range, which is what ends an
    // overflow: the value snaps back to something the slider can show.
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        apply(v, true);
    });
    connect(m_value, &QSpinBox::valueChanged, this, [this](int v) {
        apply(v, true);
    });
}

void SizeSliderPanel::apply(int value, bool notify) {
    const int nv = qBound(1, value, kMaxBrushSize);
    const bool changed = nv != m_size;
    m_size = nv;
    {
        const QSignalBlocker b(m_slider);
        m_slider->setValue(qBound(m_slider->minimum(), m_size, m_slider->maximum()));
    }
    {
        const QSignalBlocker b(m_value);
        m_value->setValue(m_size);
    }
    if (notify && changed)
        emit sizeChanged(m_size);
}

void SizeSliderPanel::setSize(int s) {
    // External sync (the canvas asking for the current size): do not re-emit,
    // or this would bounce sizeChanged back at whoever just set it.
    apply(s, false);
}

void SizeSliderPanel::wheelEvent(QWheelEvent* event) {
    event->accept();
}

bool SizeSliderPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_slider && event->type() == QEvent::Wheel) {
        auto* ev = static_cast<QWheelEvent*>(event);
        const int dy = ev->angleDelta().y();
        if (dy != 0) {
            // Step the real value, not the slider's: once the value is past the
            // slider's maximum the slider is already parked at its end and
            // stepping it would do nothing, so the wheel would go dead exactly
            // when the user is trying to keep nudging.
            apply(m_size + (dy > 0 ? 1 : -1), true);
            ev->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SizeSliderPanel::setEnabledForTool(bool enabled) {
    m_slider->setEnabled(enabled);
    m_value->setEnabled(enabled);
}

void SizeSliderPanel::setImmediateVisible(bool visible) {
    setVisible(visible);
}
