#pragma once
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

// Une ligne de l'historique des mouvements de stock (table MOUVEMENT_STOCK)
struct Mouvement
{
    QDateTime date;
    QString type;        // "ENTREE" ou "SORTIE"
    int quantite = 0;
    QString motif;
    QString employe;     // "Prenom Nom"
    QString reference;   // référence du matériel (historique global)
    QString designation; // désignation du matériel (historique global)
};

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
    bool rupture() const { return quantite == 0; }

    // --- Règles métier simples (sans colonne supplémentaire) ---
    int dureeVieAns() const;        // durée de vie selon la catégorie (0 = non définie)
    int ageAns() const;             // âge en années depuis la date d'acquisition
    QString statutVie() const;      // "ok", "bientot" (< 1 an avant la limite) ou "fin"

    // --- CRUD ---
    bool ajouter(QString *error = nullptr);                 // INSERT (l'id est généré)
    bool modifier(QString *error = nullptr) const;          // UPDATE
    static bool supprimer(int id, QString *error = nullptr);// DELETE
    static QList<Materiel> afficher(const QString &search, const QString &orderColumn); // SELECT

    // --- Utilitaires ---
    static Materiel trouver(int id);
    static bool referenceExiste(const QString &reference, int ignoreId);

    // --- Mouvements de stock (table MOUVEMENT_STOCK) ---
    static bool enregistrerMouvement(int idMateriel, int idEmploye, const QString &type,
                                     int quantite, const QString &motif, QString *error = nullptr);
    static QList<Mouvement> historique(int idMateriel);
    // Historique de TOUS les matériels : type = "TOUS" | "ENTREE" | "SORTIE", recherche libre
    static QList<Mouvement> historiqueGlobal(const QString &type, const QString &search);
    // Consommation moyenne par jour (sorties des N derniers jours), clé = ID_MATERIEL
    static QMap<int, double> consommationParJour(int jours = 30);
    static QList<QPair<int, QString>> employes(QString *error = nullptr);   // liste déroulante
    // Emails des employés de grade « Magasinier » (destinataires des alertes de stock)
    static QStringList emailsMagasiniers();
};

// Boîtes de dialogue (construites en code, aucun fichier .ui ajouté)
class QWidget;
bool dialogueMouvement(QWidget *parent, const Materiel &m);   // true si un mouvement a été enregistré
void dialogueHistorique(QWidget *parent, const Materiel &m);  // historique d'un matériel
void dialogueHistoriqueGlobal(QWidget *parent);               // historique de tous les mouvements

