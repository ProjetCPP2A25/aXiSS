#include "vehiculedonut.h"
#include <QPainter>
#include <QPen>
#include <QtMath>

VehiculeDonutWidget::VehiculeDonutWidget(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(100, 100);
}

void VehiculeDonutWidget::setSlices(const QVector<VehiculeSlice> &slices)
{
    m_slices = slices;
    update();
}

void VehiculeDonutWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int total = 0;
    for (const VehiculeSlice &s : m_slices) total += s.value;

    const int side = qMin(width(), height());
    const qreal pw = side * 0.17;
    const QRectF r((width() - side) / 2.0 + pw / 2, (height() - side) / 2.0 + pw / 2,
                   side - pw, side - pw);

    QPen pen;
    pen.setWidthF(pw);
    pen.setCapStyle(Qt::FlatCap);

    if (total == 0) {
        pen.setColor(QColor("#3a3d41"));
        p.setPen(pen);
        p.drawEllipse(r);
    } else {
        int start = 90 * 16;
        for (const VehiculeSlice &s : m_slices) {
            if (s.value <= 0) continue;
            const int span = -qRound(5760.0 * s.value / total);
            pen.setColor(s.color);
            p.setPen(pen);
            p.drawArc(r, start, span);
            start += span;
        }
    }

    p.setPen(QColor("#e6e6e6"));
    QFont f = font();
    f.setPointSizeF(15);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(0, 0, width(), height() * 0.58), Qt::AlignHCenter | Qt::AlignBottom,
               QString::number(total));
    f.setPointSizeF(8);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#b0b3b8"));
    p.drawText(QRectF(0, height() * 0.58, width(), height() * 0.3), Qt::AlignHCenter | Qt::AlignTop,
               "total");
}
