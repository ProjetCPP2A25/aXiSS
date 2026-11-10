#pragma once

#include <QString>
#include <QDate>
#include <QVector>
#include <QSqlQueryModel>

// ============================================================================
// Classe Garde : Données et CRUD métier pour la table Oracle GARDE.
// Structurée à l'identique de la classe Employe (employe.h / employe.cpp).
// ============================================================================
class Garde
{
public:
    QString id;                 // ID_GARDE sous forme QString pour conformité avec Employe::id
    QDate date;                 // DATE_GARDE
    QString type;               // TYPE ("Jour", "Nuit")
    QString heureDebut;         // HEURE_DEBUT ("07:00")
    QString heureFin;           // HEURE_FIN ("19:00")
    int effectif = 4;           // EFFECTIF
    QString statut;             // STATUT ("Planifiée", "En cours", "Terminée", "Annulée")

    Garde();
    Garde(const QString &id, const QDate &date, const QString &type,
          const QString &heureDebut, const QString &heureFin,
          int effectif, const QString &statut);

    // Opérations CRUD
    bool ajouter();
    bool ajouter_garde(QString *err);
    bool modifier() const;
    bool modifier_garde(QString *err);
    static bool supprimer(const QString &id);
    static bool supprimer_garde(const QString &id, QString *err);

    // Consultation & Recherche
    static Garde trouver(const QString &id);
    static QVector<Garde> lister();
    static QSqlQueryModel* afficher_garde(QString *err);

    // Validations & Utilitaires (sur le modèle de Employe)
    static bool typeValide(const QString &type);
    static bool heureValide(const QString &heure);
    static bool effectifValide(int effectif);
    static bool existe(const QString &id);

    static QString prochainId();
    static QString derniereErreur();
};
