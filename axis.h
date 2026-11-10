#pragma once
#include <QMainWindow>
#include <QDateTime>
#include <QSet>
#include "employe.h"
#include "intervention.h"
#include "materiel.h"
#include "garde.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AXIS; }
QT_END_NAMESPACE

class SidebarFrame;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;

// Fenêtre principale unique : gestion de la caserne (G = Gestion). Une seule interface (axis.ui) pour les cinq modules.
class AXIS : public QMainWindow
{
    Q_OBJECT
public:
    explicit AXIS(QWidget *parent = nullptr);
    ~AXIS() override;

private:
    // ==========================================
    // MODULE EMPLOYES
    // ==========================================
    // Code existant de ma coéquipière : préserver.
    void rafraichirListe();
    void viderFormulaire();
    void enregistrerEmploye();
    void modifierEmploye(const QString &id);
    void afficherEmploye(const QString &id);
    void supprimerEmploye(const QString &id);

    QString editingId_;
    SidebarFrame *sidebarEmpl_ = nullptr;

    // ==========================================
    // MODULE INTERVENTIONS
    // ==========================================
    // Gestion de l'interface et des CRUD Intervention (la classe métier est dans intervention.h / intervention.cpp).
    void initialiserModuleInterventions();          // présentation du tableau/formulaire + connexions des widgets

    // Opérations CRUD
    void enregistrerIntervention();                 // bouton Ajouter / Modifier du formulaire : ajouter() ou modifier()
    void afficherIntervention(int id);              // détails d'une intervention
    void modifierIntervention(int id);              // charge l'intervention dans le formulaire (mode modification)
    void supprimerIntervention(int id);             // avec confirmation

    // Gestion du formulaire et du tableau
    void viderFormulaireIntervention();             // bouton Vider / Annuler
    void rafraichirListeInterventions();
    void majEtatFormulaireIntervention();           // date de fin saisissable seulement si « Terminée »
    void majDureeIntervention();                    // durée calculée (jamais saisie)
    bool lireFormulaireIntervention(Intervention &it, QString &erreurs) const;

    int interventionEnEdition_ = 0;                 // 0 : mode ajout ; sinon ID_INTERVENTION en cours de modification

    // ==========================================
    // MODULE VEHICULES
    // ==========================================
    // Page PVehicule de axis.ui : CRUD, recherche, tri, statistiques, exports PDF/Excel, alerte SMS, entretiens (classe Vehicule).
    void initialiserModuleVehicules();              // barre latérale, présentation du tableau, navigation

    SidebarFrame *sidebarVeh_ = nullptr;
    QLineEdit *leRechercheVeh_ = nullptr;           // champ de recherche ajouté par code dans la barre d'outils
    QNetworkAccessManager *smsNetwork_ = nullptr;   // alerte SMS (Twilio)
    QString vehiculeEnEdition_;                     // vide : mode ajout ; sinon ID (« V001 ») en cours de modification
    void rafraichirListeVehicules();                // tableau + bandeau « maintenance requise » + statistiques
    void rafraichirStatsVehicules();
    void enregistrerVehicule();                     // bouton Ajouter / Enregistrer
    void viderFormulaireVehicule();
    void afficherVehicule(const QString &id);
    void modifierVehicule(const QString &id);
    void supprimerVehicule(const QString &id);
    void exporterVehiculesExcel();
    void exporterVehiculesPdf();
    void envoyerAlerteSmsVehicules();

    // ==========================================
    // MODULE MATERIELS
    // ==========================================
    // Page PMateriel de axis.ui (affichage seul : les boutons ne sont reliés à aucune action, sauf la navigation).
    void initialiserModuleMateriels();              // barre latérale, présentation des tableaux, navigation

    SidebarFrame *sidebarMat_ = nullptr;
    int materielEnEdition_ = 0;
    void rafraichirListeMateriels();
    void rafraichirStatsMateriels();
    void enregistrerMateriel();
    void viderFormulaireMateriel();
    void chargerMateriel(int id);
    void afficherMateriel(int id);
    void supprimerMateriel(int id);
    void exporterMateriels();
    void envoyerAlerteStock(const QList<Materiel> &critiques);   // email au magasinier (stock critique)
    QSet<int> materielsCritiques_;                               // pour n'alerter que lorsque la liste change

    // ==========================================
    // MODULE GARDES
    // ==========================================
    // Page PGarde de axis.ui : CRUD, recherche, tri, statistiques, export PDF (classe Garde).
    void initialiserModuleGardes();
    SidebarFrame *sidebarGrd_ = nullptr;
    QString gardeEnEdition_;                                     // vide : mode ajout ; sinon ID en cours de modification
    void rafraichirListeGardes();
    void rafraichirStatsGardes();
    void enregistrerGarde();
    void viderFormulaireGarde();
    void chargerGarde(const QString &id);
    void afficherGarde(const QString &id);
    void supprimerGarde(const QString &id);
    void exporterGardesPdf();

    // ==========================================
    // SLOTS GENERAUX
    // ==========================================
    // Navigation entre les modules (stackedWidgetEmpl : 0 menu principal, 1 page CRUD des employés ;
    // SWAxis : 0 Employés/menu, 1 Interventions, 2 Véhicules, 3 Matériels, 4 Gardes).
    void allerAuModule(int module);          // module : 0 Employés, 1 Interventions, 2 Véhicules, 3 Matériels, 4 Gardes
    void allerAuMenu();
    void chargerLogo(QLabel *label, int largeur);

    Ui::AXIS *ui;
};
