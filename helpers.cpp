#include "helpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QToolButton>

namespace Helpers {

QString formatDuration(qint64 minutes)
{
    if (minutes < 60)
        return QStringLiteral("%1 min").arg(minutes);
    const qint64 h = minutes / 60;
    const qint64 m = minutes % 60;
    return m == 0 ? QStringLiteral("%1 h").arg(h)
                  : QStringLiteral("%1 h %2").arg(h).arg(m, 2, 10, QChar('0'));
}

bool parseGps(const QString &text, double &lat, double &lon)
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*(-?\\d+(?:\\.\\d+)?)\\s*[,;]\\s*(-?\\d+(?:\\.\\d+)?)\\s*$"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return false;
    lat = m.captured(1).toDouble();
    lon = m.captured(2).toDouble();
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

QString normalizeGps(const QString &s)
{
    QString r = s;
    r.remove(QRegularExpression(QStringLiteral("\\s")));
    return r;
}

QString formatDate(const QDateTime &d)
{
    return d.isValid() ? d.toString(QLatin1String(kDateFormat)) : QStringLiteral("-");
}

QString formatDateTableau(const QDateTime &d)
{
    return d.isValid() ? d.toString(QStringLiteral("dd/MM/yy\nHH:mm")) : QStringLiteral("-");
}

PillColors graviteColors(const QString &gravite)
{
    if (gravite == Grav::Elevee)  return {"#F7D6D6", "#A61B1B"};
    if (gravite == Grav::Moyenne) return {"#FCE8CC", "#9A5A00"};
    return {"#DDF3E4", "#1B7A3E"};
}

PillColors etatColors(const QString &etat)
{
    return etat == Etat::EnCours ? PillColors{"#FCE8CC", "#9A5A00"} : PillColors{"#DDF3E4", "#1B7A3E"};
}

QString dotColor(const QString &gravite)
{
    if (gravite == Grav::Elevee)  return QStringLiteral("#D00000");
    if (gravite == Grav::Moyenne) return QStringLiteral("#C45A05");
    return QStringLiteral("#20994F");
}

QLabel *makePillLabel(const QString &text, const PillColors &c, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; border: none; border-radius: 9px;"
        "padding: 2px 8px; font-size: 11px;").arg(QLatin1String(c.bg), QLatin1String(c.fg)));
    return l;
}

QWidget *makePillCell(const QString &text, const PillColors &c)
{
    auto *box = new QWidget;
    box->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *lay = new QHBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(makePillLabel(text, c), 0, Qt::AlignLeft | Qt::AlignVCenter);
    return box;
}

QToolButton *makeActionButton(const QString &glyph, const QString &tip, const QString &hover)
{
    auto *b = new QToolButton;
    b->setText(glyph);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(26, 26);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: #181B1F; color: white; border: 1px solid #343A40;"
        "  border-radius: 5px; font-size: 12px; }"
        "QToolButton:hover { background-color: %1; border: 1px solid %1; }").arg(hover));
    return b;
}

} // namespace Helpers
