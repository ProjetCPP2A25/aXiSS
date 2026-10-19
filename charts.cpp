#include "charts.h"

#include <QPainter>
#include <QtMath>

BarChartWidget::BarChartWidget(QWidget *parent) : QWidget(parent) {}

void BarChartWidget::setData(const QVector<QPair<QString, int>> &data)
{
    m_data = data;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_data.isEmpty()) return;

    const int topPad = 36, bottomPad = 40;
    const QRectF area(10, topPad, width() - 20, height() - topPad - bottomPad);

    int maxV = 1;
    for (const auto &d : m_data) maxV = qMax(maxV, d.second);

    const double slot = area.width() / m_data.size();
    const double barW = qMin(80.0, slot * 0.4);

    QFont valueFont = font();
    valueFont.setPointSize(15);
    valueFont.setBold(true);
    QFont labelFont = font();
    labelFont.setPointSize(11);

    for (int i = 0; i < m_data.size(); ++i) {
        const double h = area.height() * m_data[i].second / maxV;
        const QRectF bar(area.left() + slot * i + (slot - barW) / 2, area.bottom() - h, barW, h);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ff0000"));
        p.drawRoundedRect(bar, 3, 3);

        p.setPen(Qt::white);
        p.setFont(valueFont);
        p.drawText(QRectF(bar.left() - 20, bar.top() - 30, barW + 40, 28),
                   Qt::AlignHCenter | Qt::AlignBottom, QString::number(m_data[i].second));

        p.setPen(QColor("#9aa0a6"));
        p.setFont(labelFont);
        p.drawText(QRectF(area.left() + slot * i, area.bottom() + 8, slot, 28),
                   Qt::AlignHCenter | Qt::AlignTop, m_data[i].first);
    }
    p.setPen(QPen(QColor("#3a3f46"), 1));
    p.drawLine(area.bottomLeft(), area.bottomRight());
}

DonutWidget::DonutWidget(QWidget *parent) : QWidget(parent) {}

void DonutWidget::setData(int critical, int sufficient)
{
    m_critical = critical;
    m_sufficient = sufficient;
    update();
}

void DonutWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int thick = 14;
    const int side = qMin(width(), height()) - thick - 6;
    const QRectF r((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    const int total = m_critical + m_sufficient;

    QPen pen(total > 0 ? QColor("#1e9e4a") : QColor("#3a3f46"), thick);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    p.drawArc(r, 0, 360 * 16);

    if (total > 0 && m_critical > 0) {
        pen.setColor(QColor("#d40000"));
        p.setPen(pen);
        p.drawArc(r, 90 * 16, -int(360.0 * 16 * m_critical / total));
    }

    QFont big = font();
    big.setPointSize(16);
    big.setBold(true);
    p.setFont(big);
    p.setPen(Qt::white);
    p.drawText(QRectF(r.left(), r.center().y() - 22, r.width(), 28), Qt::AlignCenter, QString::number(total));

    QFont small = font();
    small.setPointSize(8);
    p.setFont(small);
    p.setPen(QColor("#9aa0a6"));
    p.drawText(QRectF(r.left(), r.center().y() + 4, r.width(), 16), Qt::AlignCenter, "total");
}
