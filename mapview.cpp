#include "mapview.h"
#include "ui_interventionpage.h"
#include "helpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {

const QString kHint = QStringLiteral(
    "Cliquez sur une intervention pour calculer la distance et l'itinéraire depuis la caserne.");

// Distance à vol d'oiseau (formule de haversine), en kilomètres
double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double R = 6371.0;
    constexpr double rad = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * rad, dLon = (lon2 - lon1) * rad;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                     + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * R * std::asin(std::min(1.0, std::sqrt(a)));
}

QString formatKm(double km)
{
    return QString::number(km, 'f', 1).replace('.', ',');
}

QString formatMinutes(double minutes)
{
    return Helpers::formatDuration(qint64(std::llround(std::max(1.0, minutes))));
}

} // namespace

// Quadrillage de la carte (lignes verticales = longitudes, horizontales = latitudes), tout en bas de la pile.
class MapGrid : public QWidget
{
public:
    explicit MapGrid(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    struct Line {
        qreal pos;          // x (vertical) ou y (horizontale), en pixels
        QString label;
    };

    void setLines(const QVector<Line> &verticals, const QVector<Line> &horizontals)
    {
        m_v = verticals;
        m_h = horizontals;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QFont f = font();
        f.setPixelSize(10);
        p.setFont(f);

        const QPen linePen(QColor(255, 255, 255, 34), 1);
        const QColor textColor(255, 255, 255, 110);
        for (const Line &l : std::as_const(m_v)) {
            p.setPen(linePen);
            p.drawLine(QPointF(l.pos, 0), QPointF(l.pos, height()));
            p.setPen(textColor);
            p.drawText(QPointF(l.pos + 4, 12), l.label);
        }
        for (const Line &l : std::as_const(m_h)) {
            p.setPen(linePen);
            p.drawLine(QPointF(0, l.pos), QPointF(width(), l.pos));
            p.setPen(textColor);
            p.drawText(QPointF(4, l.pos - 3), l.label);
        }
    }

private:
    QVector<Line> m_v, m_h;
};

// Calque transparent qui trace l'itinéraire par-dessus la carte (sous les points).
class RouteOverlay : public QWidget
{
public:
    explicit RouteOverlay(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void setLine(const QVector<QPointF> &pts, bool dashed)
    {
        m_pts = pts;
        m_dashed = dashed;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_pts.size() < 2)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0, 0, 0, 160), 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(m_pts.constData(), int(m_pts.size()));
        p.setPen(QPen(QColor("#38BDF8"), 4, m_dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(m_pts.constData(), int(m_pts.size()));
    }

private:
    QVector<QPointF> m_pts;
    bool m_dashed = false;
};

MapView::MapView(Ui::InterventionPage *ui_, QObject *parent)
    : QObject(parent)
    , ui(ui_)
    , m_net(new QNetworkAccessManager(this))
{
    QFrame *map = ui->frameMap;

    m_grid = new MapGrid(map);      // créé en premier : il reste sous tout le reste
    m_grid->show();

    m_overlay = new RouteOverlay(map);
    m_overlay->show();

    m_panel = new QFrame(map);
    m_panel->setStyleSheet(QStringLiteral(
        "QFrame { background-color: rgba(24,27,31,230); border: 1px solid #343A40; border-radius: 8px; }"
        "QLabel { color: #E5E7EB; font-size: 13px; background: transparent; border: none; }"
        "QPushButton { background-color: #FF1717; color: white; border: none; border-radius: 5px;"
        "  padding: 6px 14px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #E31313; }"));
    auto *lay = new QHBoxLayout(m_panel);
    lay->setContentsMargins(12, 6, 12, 6);
    m_info = new QLabel(m_panel);
    m_info->setTextFormat(Qt::RichText);
    m_info->setWordWrap(true);
    m_detailsBtn = new QPushButton(QStringLiteral("Détails"), m_panel);
    m_detailsBtn->setCursor(Qt::PointingHandCursor);
    lay->addWidget(m_info, 1);
    lay->addWidget(m_detailsBtn);
    m_panel->show();
    connect(m_detailsBtn, &QPushButton::clicked, this, [this] {
        if (m_selected.id > 0)
            emit interventionClicked(m_selected.id);
    });
    showRoute();
}

QPointF MapView::project(double lat, double lon) const
{
    const double spanLat = m_maxLat - m_minLat, spanLon = m_maxLon - m_minLon;
    return QPointF(kPadX + (lon - m_minLon) / spanLon * (m_width - 2 * kPadX),
                   kPadTop + (m_maxLat - lat) / spanLat * (m_height - kPadTop - kPadBottom));
}

QString MapView::target() const
{
    return QStringLiteral("%1 – %2").arg(m_selected.idText(), m_selected.lieu);
}

void MapView::styleDot(QPushButton *dot, const Intervention &it, bool selected) const
{
    const int size = dot->width();
    dot->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; border: %3; border-radius: %2px; }"
        "QPushButton:hover { background-color: %1; border: 2px solid white; }")
                           .arg(Helpers::dotColor(it.gravite)).arg(size / 2)
                           .arg(selected ? QStringLiteral("3px solid white") : QStringLiteral("none")));
}

// Projette les coordonnées GPS dans le cadre de la carte.
QString MapView::refresh()
{
    QString error;
    Intervention::Filtre recentes;
    recentes.tri = Intervention::Tri::PlusRecentes;
    const QVector<Intervention> data = Intervention::lister(recentes, 0, kPointsMax, &error);
    if (!error.isEmpty())
        return error;
    m_data = data;

    for (QWidget *w : std::as_const(m_widgets))
        w->deleteLater();
    m_widgets.clear();
    m_dots.clear();

    QFrame *map = ui->frameMap;
    m_width = map->width();
    m_height = map->height();
    m_grid->setGeometry(0, 0, m_width, m_height);
    m_overlay->setGeometry(0, 0, m_width, m_height);
    m_panel->setGeometry(10, m_height - 58, m_width - 20, 48);

    // La sélection disparaît si l'intervention n'existe plus ; sinon on garde sa version à jour
    if (m_selected.id > 0) {
        const auto cur = std::find_if(data.cbegin(), data.cend(),
                                      [this](const Intervention &i) { return i.id == m_selected.id; });
        if (cur == data.cend())
            clearSelection();
        else
            m_selected = *cur;
    }

    // Le cadre inclut toujours la caserne
    double minLat = kCaserneLat, maxLat = kCaserneLat, minLon = kCaserneLon, maxLon = kCaserneLon;
    for (const Intervention &i : data) {
        minLat = std::min(minLat, i.lat); maxLat = std::max(maxLat, i.lat);
        minLon = std::min(minLon, i.lon); maxLon = std::max(maxLon, i.lon);
    }
    const double minSpan = 0.05;
    const double spanLat = std::max(maxLat - minLat, minSpan);
    const double spanLon = std::max(maxLon - minLon, minSpan);
    const double midLat = (minLat + maxLat) / 2, midLon = (minLon + maxLon) / 2;
    m_minLat = midLat - spanLat / 2; m_maxLat = midLat + spanLat / 2;
    m_minLon = midLon - spanLon / 2; m_maxLon = midLon + spanLon / 2;

    updateGrid();

    // Caserne
    {
        const QPointF c = project(kCaserneLat, kCaserneLon);
        auto *base = new QLabel(QStringLiteral("⌂"), map);   // symbole maison
        base->setAlignment(Qt::AlignCenter);
        base->setGeometry(int(c.x()) - 16, int(c.y()) - 16, 32, 32);
        base->setToolTip(QStringLiteral("Caserne\n%1, %2").arg(kCaserneLat, 0, 'f', 4).arg(kCaserneLon, 0, 'f', 4));
        base->setStyleSheet(QStringLiteral(
            "background-color: #2563EB; color: white; font-size: 20px; font-weight: bold;"
            " border: 2px solid white; border-radius: 6px;"));
        base->show();
        m_widgets.push_back(base);

        auto *label = new QLabel(QStringLiteral("Caserne"), map);
        label->setStyleSheet(QStringLiteral("color: #93C5FD; font-size: 14px; font-weight: bold; background: transparent; border: none;"));
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        const bool labelLeft = c.x() + 16 + 100 > m_width;
        label->setAlignment((labelLeft ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        label->setGeometry(labelLeft ? int(c.x()) - 22 - 90 : int(c.x()) + 22, int(c.y()) - 15, 90, 30);
        label->show();
        m_widgets.push_back(label);
    }

    for (const Intervention &i : data) {
        const int size = i.gravite == Grav::Elevee ? 32 : (i.gravite == Grav::Moyenne ? 28 : 24);
        const QPointF p = project(i.lat, i.lon);
        const int cx = int(p.x()), cy = int(p.y());

        auto *dot = new QPushButton(map);
        dot->setGeometry(cx - size / 2, cy - size / 2, size, size);
        dot->setCursor(Qt::PointingHandCursor);
        dot->setFocusPolicy(Qt::NoFocus);
        dot->setToolTip(QStringLiteral("%1 – %2 (%3)\n%4\nCliquez pour l'itinéraire depuis la caserne")
                            .arg(i.idText(), i.lieu, i.gravite, i.gps()));
        styleDot(dot, i, i.id == m_selected.id);
        connect(dot, &QPushButton::clicked, this, [this, i] { select(i); });
        dot->show();
        m_widgets.push_back(dot);
        m_dots.insert(i.id, dot);

        const bool labelLeft = cx + size / 2 + 140 > m_width;
        auto *label = new QLabel(i.lieu, map);
        label->setStyleSheet(QStringLiteral("color: white; font-size: 14px; background: transparent; border: none;"));
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setAlignment((labelLeft ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        label->setGeometry(labelLeft ? cx - size / 2 - 6 - 130 : cx + size / 2 + 6, cy - 15, 130, 30);
        label->show();
        m_widgets.push_back(label);
    }

    m_panel->raise();
    showRoute();
    return QString();
}

// Lignes de latitude / longitude à intervalle "rond", environ 6 lignes dans la plus grande direction.
void MapView::updateGrid()
{
    const double span = std::max(m_maxLat - m_minLat, m_maxLon - m_minLon);
    double step = 0.001;
    for (const double s : {0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0}) {
        if (span / step <= 6)
            break;
        step = s;
    }
    const int decimals = step < 0.01 ? 3 : (step < 0.1 ? 2 : 1);

    QVector<MapGrid::Line> vertical, horizontal;
    // On couvre tout le cadre, marges comprises
    const double lonMargin = (m_maxLon - m_minLon) * kPadX / std::max(1, m_width - 2 * kPadX);
    const double latMargin = (m_maxLat - m_minLat) * std::max(kPadTop, kPadBottom)
                             / std::max(1, m_height - kPadTop - kPadBottom);
    for (double lon = std::ceil((m_minLon - lonMargin) / step) * step; lon <= m_maxLon + lonMargin; lon += step) {
        const double x = project(m_minLat, lon).x();
        if (x >= 0 && x <= m_width)
            vertical.push_back({x, QString::number(lon, 'f', decimals) + QStringLiteral("°E")});
    }
    for (double lat = std::ceil((m_minLat - latMargin) / step) * step; lat <= m_maxLat + latMargin; lat += step) {
        const double y = project(lat, m_minLon).y();
        if (y >= 0 && y <= m_height)
            horizontal.push_back({y, QString::number(lat, 'f', decimals) + QStringLiteral("°N")});
    }
    m_grid->setLines(vertical, horizontal);
}

void MapView::clearSelection()
{
    ++m_token;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_selected = Intervention();
    m_state = RouteState::Idle;
    m_routePoints.clear();
}

// Sélection d'une intervention : mise en évidence du point, puis demande de l'itinéraire.
void MapView::select(const Intervention &it)
{
    if (const auto old = m_dots.value(m_selected.id, nullptr))
        styleDot(old, m_selected, false);
    m_selected = it;
    if (const auto cur = m_dots.value(it.id, nullptr))
        styleDot(cur, it, true);
    requestRoute();
}

// Itinéraire le plus rapide par la route (OSRM). Pas de clé requise ; nécessite un accès Internet.
void MapView::requestRoute()
{
    ++m_token;

    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }

    m_routePoints.clear();
    m_state = RouteState::Loading;
    showRoute();

    const QUrl url(QStringLiteral(
        "https://router.project-osrm.org/route/v1/driving/%1,%2;%3,%4"
        "?overview=full&geometries=geojson&alternatives=true")
        .arg(kCaserneLon, 0, 'f', 6)
        .arg(kCaserneLat, 0, 'f', 6)
        .arg(m_selected.lon, 0, 'f', 6)
        .arg(m_selected.lat, 0, 'f', 6));

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AXIS-RedFlame/1.0"));
    req.setTransferTimeout(8000);

    QNetworkReply *reply = m_net->get(req);
    m_reply = reply;

    const int token = m_token;
    connect(reply, &QNetworkReply::finished, this, [this, reply, token] { onRouteReply(reply, token); });
}

void MapView::onRouteReply(QNetworkReply *reply, int token)
{
    reply->deleteLater();
    if (token != m_token)
        return;     // la sélection a changé entre-temps

    m_state = RouteState::Failed;
    if (reply->error() == QNetworkReply::NoError) {
        const QJsonArray routes = QJsonDocument::fromJson(reply->readAll()).object()
                                      .value(QStringLiteral("routes")).toArray();

        // Itinéraire optimal : le plus court en durée parmi ceux proposés
        QJsonObject best;
        for (const QJsonValue &v : routes) {
            const QJsonObject r = v.toObject();
            if (best.isEmpty() || r["duration"].toDouble() < best["duration"].toDouble())
                best = r;
        }

        const QJsonArray coords = best["geometry"].toObject()["coordinates"].toArray();
        if (!best.isEmpty() && coords.size() >= 2) {
            m_routePoints.clear();
            m_routePoints.reserve(int(coords.size()));
            for (const QJsonValue &c : coords) {
                const QJsonArray ll = c.toArray();
                m_routePoints.push_back(QPointF(ll.at(1).toDouble(), ll.at(0).toDouble()));   // (lat, lon)
            }
            m_routeKm = best["distance"].toDouble() / 1000.0;
            m_routeMinutes = best["duration"].toDouble() / 60.0;
            m_state = RouteState::Ready;
        }
    }

    if (m_state == RouteState::Failed) {
        // Repli : ligne droite et estimation (coefficient route / vol d'oiseau, vitesse urbaine moyenne)
        m_routePoints = {QPointF(kCaserneLat, kCaserneLon), QPointF(m_selected.lat, m_selected.lon)};
        m_routeKm = haversineKm(kCaserneLat, kCaserneLon, m_selected.lat, m_selected.lon) * 1.3;
        m_routeMinutes = m_routeKm / 40.0 * 60.0;
    }
    showRoute();
}

// Met à jour le panneau d'information et le tracé selon l'état courant.
void MapView::showRoute()
{
    m_detailsBtn->setVisible(m_selected.id > 0);

    switch (m_state) {
    case RouteState::Idle:
        m_info->setText(kHint);
        m_overlay->setLine({}, false);
        return;
    case RouteState::Loading:
        m_info->setText(QStringLiteral("Caserne → %1 : calcul de l'itinéraire...").arg(target()));
        m_overlay->setLine({}, false);
        return;
    case RouteState::Ready:
        m_info->setText(QStringLiteral(
            "Caserne → %1 : <b>%2 km</b> · ≈ <b>%3</b> (itinéraire le plus rapide)")
                            .arg(target(), formatKm(m_routeKm), formatMinutes(m_routeMinutes)));
        break;
    case RouteState::Failed:
        m_info->setText(QStringLiteral(
            "Caserne → %1 : ≈ <b>%2 km</b> · ≈ <b>%3</b> (estimation à vol d'oiseau, itinéraire routier indisponible)")
                            .arg(target(), formatKm(m_routeKm), formatMinutes(m_routeMinutes)));
        break;
    }

    QVector<QPointF> px;
    px.reserve(m_routePoints.size());
    for (const QPointF &ll : std::as_const(m_routePoints))
        px.push_back(project(ll.x(), ll.y()));
    m_overlay->setLine(px, m_state == RouteState::Failed);
}
