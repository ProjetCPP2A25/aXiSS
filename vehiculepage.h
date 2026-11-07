#pragma once
#include <QVector>
#include <QWidget>

#include "vehicule.h"

namespace Ui { class VehiculePage; }
class SidebarFrame;

// Module « Gestion des véhicules » (même structure que MaterielPage : barre latérale + page défilante).
class VehiculePage : public QWidget
{
    Q_OBJECT
public:
    explicit VehiculePage(QWidget *parent = nullptr);
    ~VehiculePage() override;

    void setInfo(const QString &text);      // utilisateur connecté, affiché dans la barre latérale

signals:
    void menuRequested();       // bouton « Menu principal »
    void logoutRequested();     // bouton « Déconnexion »

public slots:
    void reload();              // relit la base puis réaffiche

private slots:
    void onSubmit();
    void clearForm();
    void exportPdf();
    void exportExcel();

private:
    void refresh();             // tri + tableau + alerte + statistiques (à partir de m_vehicles)
    void viewVehicle(const QString &id);
    void editVehicle(const QString &id);
    void deleteVehicle(const QString &id);
    QString nextId() const;
    QString kmText(int km) const;

    static QDate parseDate(const QString &s);
    static QString fmt(const QDate &d);

    Ui::VehiculePage *ui;       // widgets définis dans vehiculepage.ui
    SidebarFrame *m_sidebar = nullptr;
    QVector<Vehicule> m_vehicles;
    QString m_editId;
};
