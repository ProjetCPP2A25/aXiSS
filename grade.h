#ifndef GRADE_H
#define GRADE_H

#include <QMainWindow>
#include <QList>
#include <QStringList>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui {
class Grade;
}
QT_END_NAMESPACE

class QButtonGroup;
class QTableWidget;
class QLineEdit;
class InterventionPage;
class VehiclesPage;
class MaterielPage;

struct Garde {
    QString id;                 // "GRD-2026-001"
    QString date;               // "18/10/2026"
    QString type;               // "Jour" ou "Nuit"
    QString heureDebut;         // "07:00" ou "19:00"
    QString heureFin;           // "19:00" ou "07:00"
    int effectifRequis = 4;     // Ex: 4 pompiers
    QString chefGarde;          // Ex: "Lt. Karim Majri"
    QString statut;             // "Planifiée", "En cours", "Terminée", "Annulée"
    QString statutStyle;        // "pendingStatus", "activeStatus", "completedStatus", "expiredStatus"
    QString vehicules;          // "FPT-01, VSAV-02"
    QString secteur;            // "Secteur Centre (Caserne Principale)"
    QStringList pompiersAffectes; // Liste des noms des pompiers affectés

    int effectifAffecte() const { return pompiersAffectes.size(); }
    bool effectifSuffisant() const { return effectifAffecte() >= effectifRequis; }
};

class Grade : public QMainWindow
{
    Q_OBJECT

public:
    explicit Grade(QWidget *parent = nullptr);
    ~Grade() override;

private slots:
    // Selection, Recherche & Tri
    void onGardeSelectionChanged();
    void onFilterChanged();
    void onSortChanged(int index);

    // CRUD Gardes
    void onAddNewGardeClicked();
    void onViewGardeDetailsClicked();
    void onEditGardeClicked();
    void onDeleteGardeClicked();
    void onAssignPompiersClicked();

    // Export PDF & Statistiques
    void onExportPdfClicked();
    void onShowStatisticsClicked();

    // Navigation & Déconnexion
    void onLogoutClicked();
    void navigateToMenu(int stackIndex);

private:
    void setupNavigation();
    void initDatabase();
    void loadGardesFromDatabase();
    void renderTable();
    void updateMetrics();
    void applyTheme();

    void setupIntegratedModules();
    void setupEmployesPage();
    void refreshEmployesTable(const QString &search = QString());

    // Statistiques et effectifs
    QMap<QString, int> compterGardesParPompier() const;
    QStringList obtenirNomsPompiers() const;

    Ui::Grade *ui;
    QButtonGroup *m_navGroup;
    QList<Garde> m_gardes;

    // Modules intégrés
    InterventionPage *m_interventionPage;
    VehiclesPage *m_vehiculePage;
    MaterielPage *m_materielPage;

    // Employés
    QTableWidget *m_tblEmployes;
    QLineEdit *m_searchEmployes;
};

#endif // GRADE_H
