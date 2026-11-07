#ifndef RISKZONES_H
#define RISKZONES_H

// Détection automatique des zones à risque :
//   score = gravité cumulée des interventions des 6 derniers mois (agrégée par la base)
//           + bonus météo (vent / pluie sur 7 jours, prévisions Open-Meteo).
// Les 3 zones les plus exposées sont affichées. Le calcul est relancé régulièrement par la fenêtre principale.

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include "intervention.h"

class QNetworkAccessManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class InterventionPage;
}
QT_END_NAMESPACE

class RiskZones : public QObject
{
    Q_OBJECT

public:
    explicit RiskZones(Ui::InterventionPage *ui, QObject *parent = nullptr);

    QString refresh();      // relit l'historique en base ; renvoie l'erreur éventuelle

private:
    struct Weather {
        bool ok = false;
        double gust = 0.0;     // rafales max sur 7 jours (km/h)
        double rain = 0.0;     // cumul de pluie sur 7 jours (mm)
        QDateTime fetched;     // les prévisions sont redemandées après 30 minutes
    };

    void fetchWeather(const Intervention::ZoneAgregee &z);
    void render();

   Ui::InterventionPage *ui;
    QNetworkAccessManager *m_net;
    QVector<Intervention::ZoneAgregee> m_zones;
    QHash<QString, Weather> m_weather;
    QSet<QString> m_pending;
    QVector<QWidget *> m_widgets;
};

#endif // RISKZONES_H
