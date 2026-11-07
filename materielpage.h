#pragma once
#include <QSet>
#include <QWidget>

#include "database.h"

namespace Ui { class MaterielPage; }

class MaterielPage : public QWidget
{
    Q_OBJECT
public:
    explicit MaterielPage(QWidget *parent = nullptr);
    ~MaterielPage() override;

signals:
    void menuRequested();       // bouton « Menu principal »
    void logoutRequested();     // bouton « Déconnexion »

public slots:
    void refresh();

private slots:
    void submitForm();
    void clearForm();
    void exportCsv();
    void handleAction(const QString &link);
    void addMouvement();

private:
    void fillForm(const Materiel &m);
    void refreshMateriels();
    void refreshStats();
    void refreshMouvements();
    void sendAlertEmail(const QList<Materiel> &critical);

    Ui::MaterielPage *ui;   // widgets définis dans materielpage.ui
    int editingId = 0;
    QSet<int> lastCritical;
};
