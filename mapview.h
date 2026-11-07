#ifndef MAPVIEW_H
#define MAPVIEW_H

// Carte interactive : chaque intervention (les plus récentes, au plus kPointsMax) est un point placé selon ses coordonnées GPS,
// coloré selon sa gravité ; la caserne y figure aussi.
// Un clic sur un point le sélectionne : l'itinéraire routier le plus rapide depuis la caserne est demandé (service OSRM),
// tracé sur la carte, et la distance ainsi que la durée sont affichées dans le panneau d'information.
// Le bouton « Détails » du panneau demande l'affichage de l'intervention.

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVector>

#include "intervention.h"

class QFrame;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QWidget;
class RouteOverlay;
class MapGrid;

QT_BEGIN_NAMESPACE
namespace Ui {
class InterventionPage;
}
QT_END_NAMESPACE

class MapView : public QObject
{
    Q_OBJECT

public:
    explicit MapView(Ui::InterventionPage *ui, QObject *parent = nullptr);

    static constexpr int kPointsMax = 300;

    // Position de la caserne (point de départ de tous les itinéraires)
    static constexpr double kCaserneLat = 36.8190;
    static constexpr double kCaserneLon = 10.1658;

    QString refresh();      // relit les interventions en base ; renvoie l'erreur éventuelle

signals:
    void interventionClicked(int id);

private:
    enum class RouteState { Idle, Loading, Ready, Failed };

    void select(const Intervention &it);
    void requestRoute();
    void onRouteReply(QNetworkReply *reply, int token);
    void clearSelection();
    void updateGrid();                      // quadrillage de latitude / longitude
    void showRoute();                     // met à jour le texte du panneau et le tracé selon m_state
    void styleDot(QPushButton *dot, const Intervention &it, bool selected) const;
    QString target() const;                 // "I003 – Lieu"
    QPointF project(double lat, double lon) const;

    Ui::InterventionPage *ui;
    QVector<QWidget *> m_widgets;
    QVector<Intervention> m_data;
    QHash<int, QPushButton *> m_dots;       // identifiant d'intervention -> point sur la carte

    QNetworkAccessManager *m_net;
    QPointer<QNetworkReply> m_reply;
    int m_token = 0;                        // identifie la dernière requête ; les réponses périmées sont ignorées

    MapGrid *m_grid;
    RouteOverlay *m_overlay;
    QFrame *m_panel;
    QLabel *m_info;
    QPushButton *m_detailsBtn;

    Intervention m_selected;                // id == 0 : aucune sélection
    RouteState m_state = RouteState::Idle;
    QVector<QPointF> m_routePoints;         // (lat, lon)
    double m_routeKm = 0.0;
    double m_routeMinutes = 0.0;

    // Cadre géographique courant (pour projeter latitude/longitude vers les pixels)
    double m_minLat = 0, m_maxLat = 0, m_minLon = 0, m_maxLon = 0;
    int m_width = 0, m_height = 0;
    static constexpr int kPadX = 70, kPadTop = 40, kPadBottom = 80;
};

#endif // MAPVIEW_H
