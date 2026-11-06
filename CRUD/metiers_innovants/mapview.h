#ifndef MAPVIEW_H
#define MAPVIEW_H

// Carte interactive : chaque intervention (les plus récentes, au plus kPointsMax) est un point placé selon ses coordonnées GPS,
// coloré selon sa gravité. Un clic sur un point sélectionne l'intervention : la carte affiche alors la caserne, la distance
// et l'itinéraire routier le plus rapide (service OSRM) entre la caserne et le lieu. Hors ligne, la distance à vol d'oiseau
// (formule de haversine) est affichée à la place. Le bouton « Détails » ouvre la fiche de l'intervention.

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVector>

#include "intervention.h"

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class RouteOverlay;

QT_BEGIN_NAMESPACE
namespace Ui {
class GAxis;
}
QT_END_NAMESPACE

class MapView : public QObject
{
    Q_OBJECT

public:
    explicit MapView(Ui::GAxis *ui, QObject *parent = nullptr);

    static constexpr int kPointsMax = 300;

    // Position de la caserne de départ des itinéraires (à adapter à la caserne réelle)
    static constexpr double kCaserneLat = 36.7990;
    static constexpr double kCaserneLon = 10.1760;

    QString refresh();      // relit les interventions en base ; renvoie l'erreur éventuelle

signals:
    void interventionClicked(int id);

private:
    enum class RouteState { None, Loading, Ready, Failed };

    QPointF project(double lat, double lon) const;
    void select(const Intervention &it);
    void requestRoute();
    void onRouteReply(QNetworkReply *reply, int token);
    void updateRouteView();

   Ui::GAxis *ui;
    QVector<QWidget *> m_widgets;
    QNetworkAccessManager *m_net;
    QPointer<QNetworkReply> m_reply;
    RouteOverlay *m_overlay = nullptr;
    QLabel *m_info = nullptr;
    QPushButton *m_detailsBtn = nullptr;

    QVector<Intervention> m_data;
    double m_minLat = 0, m_maxLat = 0, m_minLon = 0, m_maxLon = 0;
    int m_padX = 70, m_padTop = 50, m_padBottom = 40;

    Intervention m_selected;            // id == 0 : aucune sélection
    RouteState m_state = RouteState::None;
    int m_token = 0;                    // ignore les réponses d'une requête périmée
    QVector<QPointF> m_routePoints;     // x = longitude, y = latitude
    double m_routeKm = 0.0;
    double m_routeMinutes = 0.0;
};

#endif // MAPVIEW_H
