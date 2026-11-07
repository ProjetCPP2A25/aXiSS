#pragma once
#include <QMainWindow>
#include <QDateTime>
#include "employe.h"
#include "materielpage.h"
QT_BEGIN_NAMESPACE
namespace Ui { class AXIS; }
QT_END_NAMESPACE

class InterventionPage;
class VehiculePage;
class SidebarFrame;
class QThread;
class QLabel;

// Fenêtre principale : gestion de la caserne (G = Gestion).
class AXIS : public QMainWindow
{
    Q_OBJECT
public:
    explicit AXIS(QWidget *parent = nullptr);
    ~AXIS() override;

private:
    // ===== MODULE EMPLOYÉS : déclarations (à copier dans la partie private de votre axis.h) =====
    // Navigation (stackedWidgetEmpl : 0 menu principal, 1 page CRUD des employés)
    void allerAuModule(int module);          // module : 0 Employés, 1 Interventions, 2 Véhicules, 3 Matériels
    void allerAuMenu();
    void chargerLogo(QLabel *label, int largeur);

    void rafraichirListe();
    void viderFormulaire();
    void enregistrerEmploye();
    void modifierEmploye(const QString &id);
    void afficherEmploye(const QString &id);
    void supprimerEmploye(const QString &id);

    Ui::AXIS *ui;   // existe déjà dans votre axis.h : ne pas le copier une 2e fois
    QString editingId_;
    MaterielPage *materielPage_ = nullptr;
    InterventionPage *interventionPage_;
    VehiculePage *vehiculePage_ = nullptr;
    SidebarFrame *sidebarEmpl_ = nullptr;
    // ===== FIN MODULE EMPLOYÉS =====
};
