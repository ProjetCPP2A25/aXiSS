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

// Employé tel qu'utilisé par le suivi de stock (lu dans la table "employes" existante : id TEXT, nom TEXT).
// Nommé EmployeLite pour ne pas entrer en conflit avec la classe Employe du projet AXIS.
struct EmployeLite {
    QString id;
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
    QString employeId;      // id texte de la table employes (ex. "E003"), vide si inconnu
    QString employeNom;
};

// Accès SQLite (fichier axis.db créé automatiquement dans le dossier AppData de l'utilisateur).
class Database
{
public:
    // Mode autonome : ouvre sa propre base SQLite (axis.db) puis appelle init().
    static bool open(QString *error);
    // Mode intégré dans AXIS : à appeler APRÈS Connexion::ouvrir(). Réutilise la connexion déjà ouverte
    // et crée seulement les tables "materiels" et "mouvements" (la table "employes" doit déjà exister).
    static bool init(QString *error);

    static QList<Materiel> materiels(const QString &search, const QString &orderColumn);
    static Materiel materiel(int id);
    static bool referenceExists(const QString &reference, int ignoreId);
    static bool addMateriel(const Materiel &m, QString *error);
    static bool updateMateriel(const Materiel &m, QString *error);
    static bool deleteMateriel(int id, QString *error);

    static QList<EmployeLite> employes();
    static QList<Mouvement> mouvements(int limit);
    // Enregistre le mouvement ET met à jour le stock (dans une transaction).
    static bool addMouvement(const Mouvement &m, QString *error);
};
