#ifndef VEHICULE_H
#define VEHICULE_H

// Classe Vehicule : attributs + contrôle de saisie + CRUD (Ajouter / Afficher / Modifier / Supprimer)
// + fonctionnalités métier (recherche, tri, statistiques, alerte maintenance).
// Toutes les requêtes SQL sont ici : l'interface (gaxis) n'écrit jamais de SQL.

#include <QDate>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class Vehicule
{
public:
    // --- Attributs -----------------------------------------------------------------------------
    QString id;                 // "V001"
    QString plaque;             // immatriculation, ex. "123 TUN 4567"
    QString type;
    int km = 0;
    QString etat;               // Bon / À surveiller / Hors service
    QString dispo;              // Disponible / En intervention / En maintenance
    QDate derniereMaint, prochaineMaint, assurance, visiteTech;

    // --- Valeurs autorisées (utilisées aussi par l'interface pour remplir les listes) ----------
    static const QStringList &types();
    static const QStringList &etats();
    static const QStringList &disponibilites();

    // --- Contrôle de saisie --------------------------------------------------------------------
    static QString normaliserPlaque(const QString &texte);   // "123tun4567" -> "123 TUN 4567"
    bool valider(QString *erreur = nullptr) const;

    // --- CRUD ----------------------------------------------------------------------------------
    bool ajouter(QString *erreur = nullptr);                  // attribue l'identifiant (V001, V002...)
    static bool afficher(const QString &id, Vehicule &resultat, QString *erreur = nullptr);
    bool modifier(QString *erreur = nullptr) const;
    static bool supprimer(const QString &id, QString *erreur = nullptr);

    // --- Fonctionnalités métier ----------------------------------------------------------------
    // tri : 0 = kilométrage, 1 = ID, 2 = immatriculation, 3 = prochaine maintenance
    // recherche : sur l'ID, l'immatriculation ou le type (vide = tout)
    // En cas d'erreur SQL, la liste est vide et *erreur est rempli.
    static QVector<Vehicule> lister(const QString &recherche, int tri, QString *erreur = nullptr);
    static QVector<Vehicule> maintenanceRequise(QString *erreur = nullptr);   // prochaine maintenance dépassée
    static QMap<QString, int> statistiques(bool parEtat, QString *erreur = nullptr);  // COUNT + GROUP BY

private:
    static bool plaqueExiste(const QString &plaque, const QString &sauf);
    static QString prochainId();
};

#endif // VEHICULE_H
