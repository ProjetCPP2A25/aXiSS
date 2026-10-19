#pragma once
#include <QMainWindow>
#include <QDateTime>
#include "employe.h"
#include "materielpage.h"
QT_BEGIN_NAMESPACE
namespace Ui { class AXIS; }
QT_END_NAMESPACE

class InterventionPage;
class VehiclesPage;
class SidebarFrame;
class QThread;
class QTimer;
class QLabel;
class EmployeDonutWidget;
class BarWidget;

// Fenêtre principale : gestion de la caserne (G = Gestion).
class AXIS : public QMainWindow
{
    Q_OBJECT
public:
    explicit AXIS(QWidget *parent = nullptr);
    ~AXIS() override;

private:
    // ===== MODULE EMPLOYÉS : déclarations (à copier dans la partie private de votre axis.h) =====
    // Connexion / navigation (stackedWidgetEmpl : 0 login, 1 menu, 2 mot de passe oublié, 3 CRUD)
    void connexionEmploye();
    void deconnexionEmploye();
    void appliquerDroits();
    void envoyerMdpOublie();
    void allerAuModule(int module);          // module : 0 Employés, 1 Interventions, 2 Véhicules, 3 Matériels
    void allerAuMenu();
    void chargerLogo(QLabel *label, int largeur);

    void rafraichirListe();
    void rafraichirStats();
    void rafraichirCombos();
    void afficherConges();
    void majAlerte();
    void viderFormulaire();
    void enregistrerEmploye();
    void modifierEmploye(const QString &id);
    void afficherEmploye(const QString &id);
    void supprimerEmploye(const QString &id);
    void exporterAttestation();
    void enregistrerConge();
    void deciderConge(int id, bool accepter);

    Ui::AXIS *ui;   // existe déjà dans votre axis.h : ne pas le copier une 2e fois
    Employe user_;
    QString editingId_;
    EmployeDonutWidget *donut_ = nullptr;
    BarWidget *bars_ = nullptr;
    bool peutDecider_ = false;
    QTimer *timerSurcharge_ = nullptr;
    MaterielPage *materielPage_ = nullptr;
    InterventionPage *interventionPage_ = nullptr;
    VehiclesPage *vehiclesPage_ = nullptr;
    SidebarFrame *sidebarEmpl_ = nullptr;
    // ===== FIN MODULE EMPLOYÉS =====
};
