#ifndef INTERVENTION_H
#define INTERVENTION_H

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QPolygonF>
#include <QSet>
#include <QWidget>
#include <QDateTime>
#include <QString>
#include <QVector>

#include <algorithm>

namespace Grav {
inline const QString Elevee  = QStringLiteral("Élevée");
inline const QString Moyenne = QStringLiteral("Moyenne");
inline const QString Faible  = QStringLiteral("Faible");
}

namespace Etat {
inline const QString EnCours  = QStringLiteral("En cours");
inline const QString Terminee = QStringLiteral("Terminée");
}

// Position de la caserne (point de départ des itinéraires). À adapter à la vraie caserne : latitude, longitude.
namespace Caserne {
inline constexpr double Lat = 36.8065;
inline constexpr double Lon = 10.1815;
inline const QString Nom = QStringLiteral("Caserne RedFlame");
}

// Classe métier du module Interventions (table Oracle INTERVENTION).
// Toutes les requêtes SQL du module sont écrites ici (requêtes préparées) ; l'interface (AXIS) ne contient aucun SQL.
// La connexion Oracle est la connexion commune du projet : Connexion::oracle().
class Intervention
{
public:
    // Attributs (colonnes de la table INTERVENTION)
    int id = 0;               // ID_INTERVENTION
    QString type;             // TYPE (obligatoire)
    QString lieu;             // LIEU (obligatoire, 100 caractères max)
    QString gravite;          // GRAVITE
    QString etat;             // ETAT

    double lat = 0.0;         // latitude  \_ stockées ensemble dans COORDONNEES_GPS ("lat, lon", texte)
    double lon = 0.0;         // longitude /

    QDateTime reception;      // DATE_RECEPTION_APPEL + HEURE_RECEPTION_APPEL
    QDateTime fin;            // DATE_FIN_INTERVENTION + HEURE_FIN_INTERVENTION (invalide = pas de fin)

    // ID_GARDE (obligatoire, clé étrangère vers GARDE). 0 = non renseignée : à l'ajout, la première garde
    // existante est utilisée (LIMITATION PROVISOIRE : l'attribution automatique des gardes viendra plus tard).
    int idGarde = 0;

    // Valeurs calculées
    QString idText() const;
    QString gps() const;
    qint64 dureeMinutes() const;
    QString dureeTexte() const;
    int graviteRang() const;
    bool valider(QString *erreur) const;

    // Lecture d'une saisie « latitude, longitude » ; renvoie false (et un message) si le format ou les bornes sont invalides.
    static bool lireGps(const QString &texte, double &lat, double &lon, QString *erreur = nullptr);

    // Opérations CRUD
    bool ajouter(QString *erreur = nullptr);
    static bool afficher(int id, Intervention &resultat, QString *erreur = nullptr);
    bool modifier(QString *erreur = nullptr) const;
    static bool supprimer(int id, QString *erreur = nullptr);
    static QVector<Intervention> lister(QString *erreur = nullptr);

    // Vérifie qu'une garde existe dans la table GARDE.
    static bool gardeExiste(int idGarde, QString *erreur = nullptr);

    // ======================================================================================
    // Fonctionnalités avancées (aucun SQL : elles travaillent sur la liste déjà lue par lister())
    // ======================================================================================

    // 1 - Tri : « Aucun tri » (ordre des ID), gravité élevée → faible, gravité faible → élevée (à gravité égale : plus récentes d'abord).
    static void trierParGravite(QVector<Intervention> &liste);
    static void trierParGraviteCroissante(QVector<Intervention> &liste);   // Faible, Moyenne, Élevée
    static void trierParDate(QVector<Intervention> &liste);     // réception, plus récentes d'abord
    static void trierParId(QVector<Intervention> &liste);

    // 2 - Recherche par coordonnées GPS.
    //   « 36.80 »            : toute intervention dont les coordonnées contiennent ce texte
    //   « 36.8065, 10.1815 » : interventions situées à moins de « toleranceM » mètres de ce point
    static QVector<Intervention> rechercherGps(const QVector<Intervention> &liste, const QString &saisie,
                                               double toleranceM = 150.0);
    // Historique d'une intervention : les autres interventions survenues au même endroit (rayon en mètres),
    // les plus récentes d'abord.
    static QVector<Intervention> historiqueProche(const QVector<Intervention> &liste, const Intervention &ref,
                                                  double rayonM = 300.0);

    // 3 - Export Excel : vrai fichier .xlsx (une feuille « Interventions », en-têtes, filtre, dates Excel).
    static bool exporterExcel(const QVector<Intervention> &liste, const QString &chemin, QString *erreur = nullptr);

    // 4 - Statistiques par niveau de gravité
    struct StatsGravite {
        int elevee = 0, moyenne = 0, faible = 0;
        int total() const { return elevee + moyenne + faible; }
        int maximum() const { return std::max({elevee, moyenne, faible}); }
    };
    static StatsGravite statistiques(const QVector<Intervention> &liste);

    // 5 - Détection automatique des zones à risque (historique des interventions + conditions météo)
    struct Meteo {
        bool disponible = false;
        double temperatureC = 0.0;
        double ventKmh = 0.0;
        double pluieMm = 0.0;
        QString resume;                                  // ex. « 34 °C, vent 28 km/h, sans pluie »
    };
    struct ZoneRisque {
        double lat = 0.0, lon = 0.0;                     // centre de la zone
        QString nom;                                     // lieu le plus fréquent de la zone
        int nbInterventions = 0;
        int nbGraves = 0;                                // gravité « Élevée »
        double score = 0.0;
        QString niveau;                                  // Grav::Elevee / Moyenne / Faible = niveau de risque
        QString facteurs;                                // ex. « 4 interventions, 2 graves · chaleur + vent »
    };
    static Meteo lireMeteoJson(const QByteArray &json);  // réponse de l'API Open-Meteo (current=...)
    static QVector<ZoneRisque> detecterZonesRisque(const QVector<Intervention> &liste, const Meteo &meteo,
                                                   int maxZones = 5, double rayonM = 1500.0);

    // 6 - Distance (à vol d'oiseau, formule de haversine) en kilomètres
    static double distanceKm(double lat1, double lon1, double lat2, double lon2);
    double distanceDepuisCaserneKm() const { return distanceKm(Caserne::Lat, Caserne::Lon, lat, lon); }
};

// =============================================================================================
// Carte interactive du module Interventions (widget de l'interface, rangé ici : pas de classe/fichier en plus)
// =============================================================================================
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

// Vraie carte interactive (tuiles OpenStreetMap : noms des rues, des quartiers et des lieux) :
//  - glisser pour déplacer, molette / boutons +/- pour zoomer, double-clic pour zoomer ;
//  - marqueurs colorés selon la gravité des interventions + marqueur de la caserne ;
//  - zones à risque dessinées en cercles translucides ;
//  - clic sur une intervention : distance et itinéraire routier le plus rapide depuis la caserne (service OSRM),
//    avec repli sur la distance à vol d'oiseau si le service n'est pas joignable.
// Nécessite seulement le module Qt « network » (déjà présent dans AXIS.pro). Pas de QtWebEngine.
class CarteWidget : public QWidget
{
    Q_OBJECT
public:
    struct Marqueur {
        int id = 0;
        double lat = 0.0, lon = 0.0;
        QString etiquette;                 // texte de l'infobulle
        QString titre;                     // ex. « I012 · Incendie »
        QString lieu;                      // ex. « Avenue Habib Bourguiba »
        QColor couleur;
    };
    struct Zone {
        double lat = 0.0, lon = 0.0;
        double rayonM = 0.0;
        QColor couleur;
        QString nom;
    };

    explicit CarteWidget(QWidget *parent = nullptr);

    void setCaserne(double lat, double lon, const QString &nom);
    void setMarqueurs(const QVector<Marqueur> &marqueurs);
    void setZones(const QVector<Zone> &zones);
    void selectionner(int id);             // 0 : aucune sélection ; sinon centre la carte et calcule l'itinéraire
    int selection() const { return m_selection; }
    void ajusterVue();                     // cadre la caserne et toutes les interventions

signals:
    void marqueurSelectionne(int id);      // clic sur un marqueur de la carte

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    // --- projection Web Mercator (même système que les tuiles OpenStreetMap) ---
    static QPointF projeter(double lat, double lon, int zoom);          // -> pixel « monde » au zoom donné
    static void deprojeter(const QPointF &pixel, int zoom, double &lat, double &lon);
    QPointF versEcran(double lat, double lon) const;
    void ecranVersGeo(const QPointF &ecran, double &lat, double &lon) const;

    // --- vue ---
    void definirZoom(int zoom, const QPointF &ancrageEcran);
    void cadrer(const QVector<QPointF> &pointsLatLon, int zoomMax);

    // --- tuiles ---
    const QPixmap *tuile(int z, int x, int y);
    void demanderTuile(int z, int x, int y);

    // --- marqueurs / itinéraire ---
    int marqueurSous(const QPointF &ecran) const;                       // index dans m_marqueurs ou -1
    void calculerItineraire(const Marqueur &cible);
    void dessinerMarqueur(QPainter &p, const QPointF &pointe, const QColor &couleur, bool selectionne) const;

    // état de la vue
    double m_lat = 36.8065, m_lon = 10.1815;          // centre
    int m_zoom = 12;

    QVector<Marqueur> m_marqueurs;
    QVector<Zone> m_zones;
    double m_casLat = 36.8065, m_casLon = 10.1815;
    QString m_casNom;
    int m_selection = 0;

    // itinéraire de la sélection
    QVector<QPointF> m_route;                         // (lat, lon) ; vide = pas d'itinéraire routier
    QString m_infoTrajet;                             // texte de la ligne « distance / durée »
    QPointer<QNetworkReply> m_reponseRoute;

    // réseau et cache des tuiles
    QNetworkAccessManager *m_reseau = nullptr;
    QHash<quint64, QPixmap> m_tuiles;
    QSet<quint64> m_enCours;
    QHash<quint64, qint64> m_echecs;                  // tuile -> instant du dernier échec (nouvel essai après 15 s)

    // interaction
    bool m_glisse = false;
    QPoint m_pressPos, m_dernierePos;
    bool m_aBouge = false;

    QPushButton *m_btnPlus = nullptr;
    QPushButton *m_btnMoins = nullptr;
    QPushButton *m_btnCadrer = nullptr;
};

#endif // INTERVENTION_H
