#include "ColorDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QVBoxLayout>

#include <functional>

constexpr int kSide = 200;
constexpr int kHueH = 22;

// Interactive SV (saturation/value) square + hue bar. Keeps its own HSV and
// reports changes through a plain callback (no Qt meta-object needed).
class RgbHsvPicker : public QWidget {
public:
    explicit RgbHsvPicker(QWidget* parent = nullptr) : QWidget(parent) {
        setMouseTracking(true);
        setFixedSize(kSide + 8, kSide + kHueH + 14);
        m_h = 0;
        m_s = 0;
        m_v = 255;
    }

    std::function<void(const QColor&)> onChanged;

    QColor color() const {
        return QColor::fromHsv(m_h, m_s, m_v);
    }

    void setColor(const QColor& c) {
        m_h = qBound(0, c.hsvHue() < 0 ? 0 : c.hsvHue(), 359);
        m_s = qBound(0, c.hsvSaturation(), 255);
        m_v = qBound(0, c.value(), 255);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF sv(4, 4, kSide, kSide);

        QLinearGradient horiz(sv.topLeft(), sv.topRight());
        horiz.setColorAt(0.0, Qt::white);
        horiz.setColorAt(1.0, QColor::fromHsv(m_h, 255, 255));
        p.fillRect(sv, horiz);

        QLinearGradient vert(sv.topLeft(), sv.bottomLeft());
        vert.setColorAt(0.0, QColor(0, 0, 0, 0));
        vert.setColorAt(1.0, QColor(0, 0, 0, 255));
        p.fillRect(sv, vert);

        p.setPen(QColor(0, 0, 0, 90));
        p.drawRect(sv);

        const QPointF sx(sv.left() + kSide * (m_s / 255.0),
                         sv.top() + kSide * (1.0 - m_v / 255.0));
        p.setPen(Qt::white);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(sx, 5, 5);
        p.setPen(QColor(0, 0, 0, 160));
        p.drawEllipse(sx, 5.5, 5.5);

        const QRectF hueBar(4, kSide + 12, kSide, kHueH);
        QLinearGradient hueGrad(hueBar.topLeft(), hueBar.topRight());
        for (int i = 0; i <= 6; ++i) {
            const int deg = i * 60;
            hueGrad.setColorAt(i / 6.0, QColor::fromHsv(deg, 255, 255));
        }
        p.fillRect(hueBar, hueGrad);
        p.setPen(QColor(0, 0, 0, 90));
        p.drawRect(hueBar);

        const qreal hx = hueBar.left() + kSide * (m_h / 359.0);
        p.setPen(Qt::white);
        p.drawLine(QPointF(hx, hueBar.top()), QPointF(hx, hueBar.bottom()));
        p.setPen(QColor(0, 0, 0, 160));
        p.drawLine(QPointF(hx + 1, hueBar.top() + 1),
                   QPointF(hx + 1, hueBar.bottom() - 1));
    }

    void mousePressEvent(QMouseEvent* ev) override {
        setCursor(Qt::CrossCursor);
        pick(ev->pos());
        ev->accept();
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (ev->buttons() & Qt::LeftButton)
            pick(ev->pos());
        ev->accept();
    }

    void mouseReleaseEvent(QMouseEvent* ev) override {
        setCursor(Qt::ArrowCursor);
        ev->accept();
    }

private:
    void pick(const QPoint& pos) {
        // hue bar
        if (pos.y() >= kSide + 12 && pos.y() <= kSide + 12 + kHueH) {
            const int h = qBound(0, int((pos.x() - 4) * 360.0 / kSide), 359);
            m_h = h;
            update();
            if (onChanged)
                onChanged(color());
            return;
        }
        // sv square
        if (pos.x() >= 4 && pos.x() < 4 + kSide && pos.y() >= 4 && pos.y() < 4 + kSide) {
            const double x = qBound(0.0, (pos.x() - 4) / double(kSide), 1.0);
            const double y = qBound(0.0, (pos.y() - 4) / double(kSide), 1.0);
            m_s = int(x * 255);
            m_v = int((1.0 - y) * 255);
            update();
            if (onChanged)
                onChanged(color());
        }
    }

    int m_h;
    int m_s;
    int m_v;
};

ColorDialog::ColorDialog(const QColor& initial, QWidget* parent)
    : QDialog(parent), m_color(initial.isValid() ? initial : QColor("#000000")) {
    setWindowTitle(tr("Edit Colors"));
    setModal(true);
    setMinimumWidth(470);

    m_picker = new RgbHsvPicker(this);
    m_picker->setColor(m_color);

    m_swatch = new QLabel(this);
    m_swatch->setObjectName("ColorSwatch");
    m_swatch->setFixedSize(64, 46);
    m_swatch->setStyleSheet(QStringLiteral("background:%1;border:1px solid #808080;")
                                .arg(m_color.name()));

    auto* hexLabel = new QLabel(tr("Hex:"), this);
    m_hex = new QLineEdit(m_color.name(), this);
    m_hex->setFixedWidth(90);

    m_r = new QSpinBox(this);
    m_g = new QSpinBox(this);
    m_b = new QSpinBox(this);
    m_h = new QSpinBox(this);
    m_s = new QSpinBox(this);
    m_v = new QSpinBox(this);
    for (QSpinBox* sb : {m_r, m_g, m_b, m_s, m_v}) {
        sb->setRange(0, 255);
        sb->setFixedWidth(58);
    }
    m_h->setRange(0, 359);
    m_h->setFixedWidth(58);

    const QColor c0 = m_color;
    m_r->setValue(c0.red());
    m_g->setValue(c0.green());
    m_b->setValue(c0.blue());
    m_h->setValue(c0.hsvHue() < 0 ? 0 : c0.hsvHue());
    m_s->setValue(c0.hsvSaturation());
    m_v->setValue(c0.value());

    auto* rgbBox = new QGroupBox(tr("RGB"), this);
    auto* rgbLayout = new QGridLayout(rgbBox);
    rgbLayout->addWidget(new QLabel("Red", rgbBox), 0, 0);
    rgbLayout->addWidget(m_r, 0, 1);
    rgbLayout->addWidget(new QLabel("Green", rgbBox), 1, 0);
    rgbLayout->addWidget(m_g, 1, 1);
    rgbLayout->addWidget(new QLabel("Blue", rgbBox), 2, 0);
    rgbLayout->addWidget(m_b, 2, 1);

    auto* hsvBox = new QGroupBox(tr("HSV"), this);
    auto* hsvLayout = new QGridLayout(hsvBox);
    hsvLayout->addWidget(new QLabel(tr("Hue"), hsvBox), 0, 0);
    hsvLayout->addWidget(m_h, 0, 1);
    hsvLayout->addWidget(new QLabel(tr("Sat."), hsvBox), 1, 0);
    hsvLayout->addWidget(m_s, 1, 1);
    hsvLayout->addWidget(new QLabel(tr("Value"), hsvBox), 2, 0);
    hsvLayout->addWidget(m_v, 2, 1);

    auto* previewRow = new QHBoxLayout;
    previewRow->addWidget(m_swatch);
    previewRow->addWidget(hexLabel);
    previewRow->addWidget(m_hex, 1);
    previewRow->addStretch(1);

    auto* leftCol = new QVBoxLayout;
    leftCol->addLayout(previewRow);
    auto* boxRow = new QHBoxLayout;
    boxRow->addWidget(rgbBox);
    boxRow->addWidget(hsvBox);
    boxRow->addStretch(1);
    leftCol->addLayout(boxRow);
    leftCol->addStretch(1);

    auto* body = new QHBoxLayout;
    body->addWidget(m_picker);
    body->addSpacing(12);
    body->addLayout(leftCol, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, [this] {
        m_color = m_picker->color();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &ColorDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(body);
    root->addWidget(buttons);

    m_picker->onChanged = [this](const QColor&) { syncFromPicker(); };

    auto spinToPicker = [this] {
        if (m_updating) return;
        QColor c;
        if (sender() == m_h || sender() == m_s || sender() == m_v)
            c = QColor::fromHsv(m_h->value(), m_s->value(), m_v->value());
        else
            c = QColor(m_r->value(), m_g->value(), m_b->value());
        m_updating = true;
        m_picker->setColor(c);
        m_updating = false;
        syncFromPicker();
    };
    for (QSpinBox* sb : {m_r, m_g, m_b, m_h, m_s, m_v})
        connect(sb, qOverload<int>(&QSpinBox::valueChanged), this, spinToPicker);

    connect(m_hex, &QLineEdit::editingFinished, this, [this] {
        const QColor c(m_hex->text());
        if (c.isValid()) {
            m_updating = true;
            m_picker->setColor(c);
            m_updating = false;
            syncFromPicker();
        }
    });

    syncFromPicker();
}

void ColorDialog::syncFromPicker() {
    if (m_updating) return;
    m_updating = true;
    const QColor c = m_picker->color();
    m_r->setValue(c.red());
    m_g->setValue(c.green());
    m_b->setValue(c.blue());
    m_h->setValue(c.hsvHue() < 0 ? 0 : c.hsvHue());
    m_s->setValue(c.hsvSaturation());
    m_v->setValue(c.value());
    m_hex->setText(c.name().toUpper());
    m_swatch->setStyleSheet(QStringLiteral("background:%1;border:1px solid #808080;")
                                .arg(c.name()));
    m_updating = false;
}

void ColorDialog::syncToPicker() {
    m_updating = true;
    m_picker->setColor(m_color);
    m_updating = false;
    syncFromPicker();
}