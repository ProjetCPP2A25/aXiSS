#ifndef INTERVENTION_H
#define INTERVENTION_H

// Classe Intervention : ses attributs et toutes ses opérations (stockage en mémoire)
// (CRUD, recherche par GPS, tri par gravité, pagination, statistiques, zones à risque).

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVector>

namespace Grav {
inline const QString Elevee  = QStringLiteral("Élevée");
inline const QString Moyenne = QStringLiteral("Moyenne");
inline const QString Faible  = QStringLiteral("Faible");
}

namespace Etat {
inline const QString EnCours  = QStringLiteral("En cours");
inline const QString Terminee = QStringLiteral("Terminée");
}

// Rôles de l'application (cahier des charges, tableau des exigences fonctionnelles)
namespace Role {
inline const QString Administrateur    = QStringLiteral("Administrateur");
inline const QString ChefCaserne       = QStringLiteral("Chef de caserne");
inline const QString OperateurDispatch = QStringLiteral("Opérateur de dispatch");
inline const QString Magasinier        = QStringLiteral("Magasinier");
}

class Intervention
{
public:
    enum class Tri { Aucun, GraviteDecroissante, GraviteCroissante, PlusRecentes };

    // Critères de recherche (coordonnées GPS) et de tri appliqués par la base
    struct Filtre {
        QString gps;
        Tri tri = Tri::Aucun;
    };

    // Résultat agrégé par la base pour la détection des zones à risque
    struct ZoneAgregee {
        QString cle;
        QString nom;
        int nombre = 0;     // interventions sur la période
        int poids = 0;      // gravité cumulée (élevée = 3, moyenne = 2, faible = 1)
        double lat = 0.0;
        double lon = 0.0;
    };

    // --- Attributs -----------------------------------------------------------------------------
    int id = 0;
    QString type;
    QString lieu;
    QString gravite;
    QString etat;
    double lat = 0.0;
    double lon = 0.0;
    QDateTime reception;    // date/heure de réception de l'alerte
    QDateTime fin;          // invalide tant que l'intervention est en cours

    // --- Valeurs calculées ---------------------------------------------------------------------
    QString idText() const;                 // "I001"
    QString gps() const;                    // "36.8065, 10.1815"
    qint64 dureeMinutes() const;            // fin - réception (ou durée écoulée si en cours)
    QString dureeTexte() const;             // "45 min", "1 h 15"
    int graviteRang() const;                // 0 = élevée (la plus urgente), 1 = moyenne, 2 = faible
    bool valider(QString *erreur) const;    // contrôle des champs obligatoires et des valeurs

    // --- CRUD ----------------------------------------------------------------------------------
    bool ajouter(QString *erreur = nullptr);                 // attribue l'identifiant (prochain identifiant)
    static bool afficher(int id, Intervention &resultat, QString *erreur = nullptr);
    bool modifier(QString *erreur = nullptr) const;
    static bool supprimer(int id, QString *erreur = nullptr);

    // --- Fonctionnalités avancées --------------------------------------------------------------
    // nombre <= 0 : toutes les lignes ; sinon "nombre" lignes à partir de la ligne "debut" (0 = première)
    static QVector<Intervention> lister(const Filtre &filtre, int debut, int nombre, QString *erreur = nullptr);
    static int compter(const Filtre &filtre, QString *erreur = nullptr);
    static QMap<QString, int> statistiquesParGravite(QString *erreur = nullptr);
    static QVector<ZoneAgregee> zonesARisque(const QDateTime &depuis, QString *erreur = nullptr);

    static bool roleAutorise(const QString &role);           // opérateur de dispatch ou chef de caserne
};

#endif // INTERVENTION_H
