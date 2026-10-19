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
    void menuRequested();       // bouton « Menu principal » (barre latérale)
    void logoutRequested();     // bouton « Déconnexion » (barre latérale)

public slots:
    void refresh();

private slots:
    void submitForm();
    void clearForm();
    void exportCsv();
    void handleAction(const QString &link);

private:
    void fillForm(const Materiel &m);
    void refreshMateriels();
    void refreshStats();
    void sendAlertEmail(const QList<Materiel> &critical);

    Ui::MaterielPage *ui;   // widgets définis dans materielpage.ui
    int editingId = 0;
    QSet<int> lastCritical;
    bool alertsPrimed = false;   // 1er rafraîchissement (démarrage) : on mémorise seulement l'état, sans email
    QString mailStatus;          // résultat du dernier envoi, affiché dans le bandeau
};
