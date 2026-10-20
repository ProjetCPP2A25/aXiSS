#pragma once
#include <QDate>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class SidebarFrame;
class VehicleDonutWidget;

struct Vehicle {
    QString id, plate, type;
    int km = 0;
    QString etat, dispo;
    QDate lastMaint, nextMaint, insurance, tech;
};

// Module « Gestion des véhicules » intégré à A.X.I.S :
//  - barre latérale commune (SidebarFrame) avec « Menu principal » et « Déconnexion »
//  - données stockées dans la table SQLite « vehicules » (même base que les employés et interventions)
class VehiclesPage : public QWidget
{
    Q_OBJECT
public:
    explicit VehiclesPage(QWidget *parent = nullptr);

    void setUserInfo(const QString &text);   // « Connecté : ... » sous les boutons de la barre latérale
    void reload();                           // relit la base de données et rafraîchit l'affichage

signals:
    void menuRequested();       // bouton « Menu principal »
    void logoutRequested();     // bouton « Déconnexion »

private slots:
    void onSubmit();
    void clearForm();
    void exportPdf();
    void exportExcel();
    void sendAlert();

private:
    QWidget *buildForm();
    QWidget *buildList();
    QWidget *buildStatCard(const QString &title, const QString &sub, VehicleDonutWidget *&donut,
                           QLabel *&legend);

    void refresh();
    void load();
    bool loadFromDatabase();
    bool importLegacyJson();
    void seedSamples();
    bool dbInsert(const Vehicle &v);
    bool dbUpdate(const Vehicle &v);
    bool dbDelete(const QString &id);
    void viewVehicle(const QString &id);
    void editVehicle(const QString &id);
    void deleteVehicle(const QString &id);
    QStringList maintenanceAlerts() const;
    QString nextId() const;
    QString legacyJsonPath() const;
    QString kmText(int km) const;

    static QDate parseDate(const QString &s);
    static QString fmt(const QDate &d);

    QVector<Vehicle> m_vehicles;
    QString m_editId;

    SidebarFrame *m_sidebar = nullptr;

    // formulaire
    QLineEdit *m_plate = nullptr, *m_km = nullptr;
    QLineEdit *m_last = nullptr, *m_next = nullptr, *m_ins = nullptr, *m_tech = nullptr;
    QComboBox *m_type = nullptr, *m_etat = nullptr, *m_dispo = nullptr;
    QPushButton *m_submit = nullptr;

    // liste
    QComboBox *m_sort = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_alert = nullptr;
    QPushButton *m_alertButton = nullptr;

    // stats
    VehicleDonutWidget *m_donutEtat = nullptr, *m_donutDispo = nullptr;
    QLabel *m_legendEtat = nullptr, *m_legendDispo = nullptr;
};
