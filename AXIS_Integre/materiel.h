#pragma once
#include <QDate>
#include <QList>
#include <QString>

// Entité MATERIEL (table Oracle MATERIEL) + les 4 CRUD : ajouter / modifier / supprimer / afficher
class Materiel
{
public:
    int id = 0;                 // ID_MATERIEL
    QString reference;          // REFERENCE
    QString designation;        // DESIGNATION
    QString categorie;          // CATEGORIE
    int quantite = 0;           // QUANTITE_STOCK
    int seuil = 0;              // SEUIL_MINIMAL_STOCK
    QString etat;               // ETAT
    QString disponibilite;      // DISPONIBILITE
    QDate dateAcquisition;      // DATE_ACQUISITION
    QString emplacement;        // EMPLACEMENT_STOCKAGE

    Materiel() = default;

    bool critique() const { return quantite < seuil; }

    // --- CRUD ---
    bool ajouter(QString *error = nullptr);                 // INSERT (l'id est généré)
    bool modifier(QString *error = nullptr) const;          // UPDATE
    static bool supprimer(int id, QString *error = nullptr);// DELETE
    static QList<Materiel> afficher(const QString &search, const QString &orderColumn); // SELECT

    // --- Utilitaires ---
    static Materiel trouver(int id);
    static bool referenceExiste(const QString &reference, int ignoreId);
};
