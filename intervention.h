#ifndef INTERVENTION_H
#define INTERVENTION_H

#include <QDateTime>
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
};

#endif // INTERVENTION_H
