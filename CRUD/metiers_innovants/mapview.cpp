#include "mapview.h"
#include "./ui_gaxis.h"
#include "helpers.h"

#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {

// Distance à vol d'oiseau en km (formule de haversine)
double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kEarthKm = 6371.0;
    const double rad = 3.14159265358979323846 / 180.0;
    const double dLat = (lat2 - lat1) * rad, dLon = (lon2 - lon1) * rad;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                     + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * kEarthKm * std::asin(std::min(1.0, std::sqrt(a)));
}

QString formatKm(double km)
{
    return QString::number(km, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(','));
}

} // namespace

// Calque transparent posé sur la carte : trace de l'itinéraire et repère de la caserne (insensible à la souris).
class RouteOverlay : public QWidget
{
public:
    explicit RouteOverlay(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    }

    QPolygonF path;             // vide : pas d'itinéraire
    bool dashed = false;        // trait pointillé = vol d'oiseau (itinéraire indisponible)
    QPointF caserne;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (path.size() >= 2) {
            QPen pen(QColor("#3B82F6"), 4, dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(path);
        }

        // Caserne : carré bleu avec son nom
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(QColor("#3B82F6"));
        p.drawRoundedRect(QRectF(caserne.x() - 9, caserne.y() - 9, 18, 18), 3, 3);
        p.setPen(Qt::white);
        QFont f = font();
        f.setPixelSize(13);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(caserne.x() + 14, caserne.y() - 11, 90, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Caserne"));
    }
};

MapView::MapView(Ui::GAxis *ui_, QObject *parent)
    : QObject(parent)
    , ui(ui_)
    , m_net(new QNetworkAccessManager(this))
{
    QFrame *map = ui->frameMap;

    m_overlay = new RouteOverlay(map);
    m_overlay->hide();

    m_info = new QLabel(map);
    m_info->setStyleSheet(QStringLiteral("color: #E5E7EB; font-size: 13px; background: transparent; border: none;"));
    m_info->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_info->show();

    m_detailsBtn = new QPushButton(QStringLiteral("Détails"), map);
    m_detailsBtn->setCursor(Qt::PointingHandCursor);
    m_detailsBtn->setFocusPolicy(Qt::NoFocus);
    m_detailsBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #202428; color: white; border: 1px solid #343A40;"
        "  border-radius: 5px; font-size: 12px; }"
        "QPushButton:hover { background-color: #30353A; }"));
    m_detailsBtn->hide();
    connect(m_detailsBtn, &QPushButton::clicked, this, [this] {
        if (m_selected.id != 0)
            emit interventionClicked(m_selected.id);
    });

    updateRouteView();
}

// Projette les coordonnées GPS dans le cadre de la carte.
QPointF MapView::project(double lat, double lon) const
{
    const QFrame *map = ui->frameMap;
    const int W = map->width(), H = map->height();
    const double spanLat = m_maxLat - m_minLat, spanLon = m_maxLon - m_minLon;
    return QPointF(m_padX + (lon - m_minLon) / spanLon * (W - 2 * m_padX),
                   m_padTop + (m_maxLat - lat) / spanLat * (H - m_padTop - m_padBottom));
}

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

    // L'intervention sélectionnée a pu être supprimée ou déplacée depuis le dernier affichage
    if (m_selected.id != 0) {
        const auto it = std::find_if(m_data.cbegin(), m_data.cend(),
                                     [this](const Intervention &i) { return i.id == m_selected.id; });
        if (it == m_data.cend()) {
            m_selected = Intervention();
            m_state = RouteState::None;
            m_routePoints.clear();
            ++m_token;
        } else if (it->lat != m_selected.lat || it->lon != m_selected.lon) {
            m_selected = *it;
            requestRoute();
        } else {
            m_selected = *it;
        }
    }

    if (m_data.isEmpty()) {
        updateRouteView();
        return QString();
    }

    // La caserne fait partie de la zone affichée
    m_minLat = m_maxLat = kCaserneLat;
    m_minLon = m_maxLon = kCaserneLon;
    for (const Intervention &i : std::as_const(m_data)) {
        m_minLat = std::min(m_minLat, i.lat); m_maxLat = std::max(m_maxLat, i.lat);
        m_minLon = std::min(m_minLon, i.lon); m_maxLon = std::max(m_maxLon, i.lon);
    }
    const double minSpan = 0.05;
    const double spanLat = std::max(m_maxLat - m_minLat, minSpan);
    const double spanLon = std::max(m_maxLon - m_minLon, minSpan);
    const double midLat = (m_minLat + m_maxLat) / 2, midLon = (m_minLon + m_maxLon) / 2;
    m_minLat = midLat - spanLat / 2; m_maxLat = midLat + spanLat / 2;
    m_minLon = midLon - spanLon / 2; m_maxLon = midLon + spanLon / 2;

    QFrame *map = ui->frameMap;
    const int W = map->width();

    for (const Intervention &i : std::as_const(m_data)) {
        const int size = i.gravite == Grav::Elevee ? 32 : (i.gravite == Grav::Moyenne ? 28 : 24);
        const QPointF c = project(i.lat, i.lon);
        const int cx = int(c.x()), cy = int(c.y());
        const bool selected = i.id == m_selected.id;

        auto *dot = new QPushButton(map);
        dot->setGeometry(cx - size / 2, cy - size / 2, size, size);
        dot->setCursor(Qt::PointingHandCursor);
        dot->setFocusPolicy(Qt::NoFocus);
        dot->setToolTip(QStringLiteral("%1 – %2 (%3)\n%4\nCliquer pour calculer l'itinéraire depuis la caserne")
                            .arg(i.idText(), i.lieu, i.gravite, i.gps()));
        dot->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border: %3; border-radius: %2px; }"
            "QPushButton:hover { background-color: %1; border: 2px solid white; }")
                               .arg(Helpers::dotColor(i.gravite)).arg(size / 2)
                               .arg(selected ? QStringLiteral("3px solid #3B82F6") : QStringLiteral("none")));
        connect(dot, &QPushButton::clicked, this, [this, i] { select(i); });
        dot->show();
        m_widgets.push_back(dot);

        const bool labelLeft = cx + size / 2 + 140 > W;
        auto *label = new QLabel(i.lieu, map);
        label->setStyleSheet(QStringLiteral("color: white; font-size: 14px; background: transparent; border: none;"));
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setAlignment((labelLeft ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        label->setGeometry(labelLeft ? cx - size / 2 - 6 - 130 : cx + size / 2 + 6, cy - 15, 130, 30);
        label->show();
        m_widgets.push_back(label);
    }

    updateRouteView();
    return QString();
}

void MapView::select(const Intervention &it)
{
    m_selected = it;
    requestRoute();
    refresh();      // remet en évidence le point sélectionné
}

// Demande le trajet routier le plus rapide caserne -> lieu de l'intervention (OSRM, sans clé d'API).
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

    const QUrl url(QStringLiteral("https://router.project-osrm.org/route/v1/driving/%1,%2;%3,%4"
                                  "?overview=full&geometries=geojson&alternatives=true")
                       .arg(kCaserneLon, 0, 'f', 6).arg(kCaserneLat, 0, 'f', 6)
                       .arg(m_selected.lon, 0, 'f', 6).arg(m_selected.lat, 0, 'f', 6));
    QNetworkRequest req(url);
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
        return;     // une autre intervention a été choisie entre-temps

    m_state = RouteState::Failed;
    m_routePoints.clear();
    if (reply->error() == QNetworkReply::NoError) {
        const QJsonArray routes = QJsonDocument::fromJson(reply->readAll()).object()["routes"].toArray();
        // Itinéraire optimal = celui dont la durée est la plus courte parmi les alternatives proposées
        QJsonObject best;
        for (const QJsonValue &v : routes) {
            const QJsonObject r = v.toObject();
            if (best.isEmpty() || r["duration"].toDouble() < best["duration"].toDouble())
                best = r;
        }
        const QJsonArray coords = best["geometry"].toObject()["coordinates"].toArray();
        if (coords.size() >= 2) {
            for (const QJsonValue &c : coords) {
                const QJsonArray p = c.toArray();
                m_routePoints.push_back(QPointF(p.at(0).toDouble(), p.at(1).toDouble()));
            }
            m_routeKm = best["distance"].toDouble() / 1000.0;
            m_routeMinutes = best["duration"].toDouble() / 60.0;
            m_state = RouteState::Ready;
        }
    }
    updateRouteView();
}

// Met à jour le calque (tracé, caserne) et la ligne d'information en haut de la carte.
void MapView::updateRouteView()
{
    QFrame *map = ui->frameMap;
    m_info->setGeometry(12, 8, map->width() - 140, 30);
    m_detailsBtn->setGeometry(map->width() - 100, 8, 88, 28);
    m_detailsBtn->setVisible(m_selected.id != 0);

    const bool hasMap = !m_data.isEmpty();
    m_overlay->setGeometry(map->rect());
    m_overlay->setVisible(hasMap);
    if (hasMap) {
        m_overlay->caserne = project(kCaserneLat, kCaserneLon);
        m_overlay->path.clear();
        m_overlay->dashed = false;
        if (m_selected.id != 0) {
            if (m_state == RouteState::Ready) {
                for (const QPointF &p : std::as_const(m_routePoints))
                    m_overlay->path << project(p.y(), p.x());
            } else if (m_state == RouteState::Failed) {
                m_overlay->path << m_overlay->caserne << project(m_selected.lat, m_selected.lon);
                m_overlay->dashed = true;
            }
        }
        m_overlay->lower();     // sous les points et leurs libellés
        m_overlay->update();
    }

    const QString target = QStringLiteral("%1 – %2").arg(m_selected.idText(), m_selected.lieu).toHtmlEscaped();
    switch (m_state) {
    case RouteState::None:
        m_info->setText(QStringLiteral("Cliquer sur une intervention pour calculer la distance et l'itinéraire depuis la caserne"));
        break;
    case RouteState::Loading:
        m_info->setText(QStringLiteral("Calcul de l'itinéraire vers %1…").arg(target));
        break;
    case RouteState::Ready:
        m_info->setText(QStringLiteral("Caserne → %1 : <b>%2 km</b> · ≈ <b>%3</b> (itinéraire le plus rapide)")
                            .arg(target, formatKm(m_routeKm), Helpers::formatDuration(qint64(std::lround(m_routeMinutes)))));
        break;
    case RouteState::Failed:
        m_info->setText(QStringLiteral("Itinéraire routier indisponible (hors ligne ?) – %1 : <b>%2 km</b> à vol d'oiseau")
                            .arg(target, formatKm(haversineKm(kCaserneLat, kCaserneLon, m_selected.lat, m_selected.lon))));
        break;
    }
}
