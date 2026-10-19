#pragma once
#include <QDate>
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

class VehiclesPage : public QWidget
{
    Q_OBJECT
public:
    explicit VehiclesPage(QWidget *parent = nullptr);

    // Texte affiché sous les boutons de la barre latérale (utilisateur connecté)
    void setUserInfo(const QString &text);

signals:
    void menuRequested();       // bouton « Menu principal »
    void logoutRequested();     // bouton « Déconnexion »

private slots:
    void onSubmit();
    void clearForm();
    void exportPdf();
    void exportExcel();

private:
    QWidget *buildForm();
    QWidget *buildList();
    QWidget *buildStatCard(const QString &title, const QString &sub, VehicleDonutWidget *&donut,
                           QLabel *&legend);

    void refresh();
    void load();
    void save() const;
    void viewVehicle(const QString &id);
    void editVehicle(const QString &id);
    void deleteVehicle(const QString &id);
    QString nextId() const;
    QString dataPath() const;
    QString kmText(int km) const;

    static QDate parseDate(const QString &s);
    static QString fmt(const QDate &d);

    QVector<Vehicle> m_vehicles;
    QString m_editId;

    // formulaire
    QLineEdit *m_plate = nullptr, *m_km = nullptr;
    QLineEdit *m_last = nullptr, *m_next = nullptr, *m_ins = nullptr, *m_tech = nullptr;
    QComboBox *m_type = nullptr, *m_etat = nullptr, *m_dispo = nullptr;
    QPushButton *m_submit = nullptr;

    // liste
    QComboBox *m_sort = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_alert = nullptr;
    QPushButton *m_smsButton = nullptr;

    // barre latérale commune
    SidebarFrame *m_sidebar = nullptr;

    // stats
    VehicleDonutWidget *m_donutEtat = nullptr, *m_donutDispo = nullptr;
    QLabel *m_legendEtat = nullptr, *m_legendDispo = nullptr;
};
