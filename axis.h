#pragma once
#include <QMainWindow>
#include <QDateTime>
#include "employe.h"
QT_BEGIN_NAMESPACE
namespace Ui { class AXIS; }
QT_END_NAMESPACE

class SidebarFrame;
class QTimer;
class QLabel;
class EmployeDonutWidget;
class BarWidget;
class QPushButton;

// Fenêtre principale : gestion de la caserne (G = Gestion). Un seul fichier .ui (axis.ui), pages via QStackedWidget.
class AXIS : public QMainWindow
{
    Q_OBJECT
public:
    explicit AXIS(QWidget *parent = nullptr);
    ~AXIS() override;

private:
    // Connexion / navigation (stackedWidgetEmpl : 0 login, 1 menu, 3 CRUD)
    void connexionEmploye();
    void deconnexionEmploye();
    void appliquerDroits();
    void allerAuModule(int module);          // module 0 : gestion des employés
    void allerAuMenu();
    void chargerLogo(QLabel *label, int largeur);

    void rafraichirListe();
    void rafraichirStats();
    void majAlerte();
    void viderFormulaire();
    void enregistrerEmploye();
    void modifierEmploye(const QString &id);
    void afficherEmploye(const QString &id);
    void supprimerEmploye(const QString &id);
    void exporterAttestation();

    Ui::AXIS *ui;
    Employe user_;
    QString editingId_;
    EmployeDonutWidget *donut_ = nullptr;
    BarWidget *bars_ = nullptr;
    QTimer *timerSurcharge_ = nullptr;
    SidebarFrame *sidebarEmpl_ = nullptr;
};
