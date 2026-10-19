#pragma once
#include <QDate>
#include <QList>
#include <QString>

struct Materiel {
    int id = 0;
    QString reference;
    QString designation;
    QString categorie;
    int quantite = 0;
    int seuil = 0;
    QString etat;
    QString disponibilite;
    QDate dateAcquisition;
    QString emplacement;

    bool critique() const { return quantite < seuil; }
};

struct DatabaseEmploye {
    int id = 0;
    QString nom;
};
struct Mouvement {
    int id = 0;
    QDate date;
    QString type;            // "Entrée" ou "Sortie"
    int materielId = 0;
    QString materielNom;
    int quantite = 0;
    QString motif;
    int employeId = 0;
    QString employeNom;
};

// Accès SQLite (fichier axis.db créé automatiquement dans le dossier AppData de l'utilisateur).
class Database
{
public:
    static bool open(QString *error);

    static QList<Materiel> materiels(const QString &search, const QString &orderColumn);
    static Materiel materiel(int id);
    static bool referenceExists(const QString &reference, int ignoreId);
    static bool addMateriel(const Materiel &m, QString *error);
    static bool updateMateriel(const Materiel &m, QString *error);
    static bool deleteMateriel(int id, QString *error);

    static QList<DatabaseEmploye> employes();
    static QList<Mouvement> mouvements(int limit);
    // Enregistre le mouvement ET met à jour le stock (dans une transaction).
    static bool addMouvement(const Mouvement &m, QString *error);
};
