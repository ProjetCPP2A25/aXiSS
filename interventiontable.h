#ifndef INTERVENTIONTABLE_H
#define INTERVENTIONTABLE_H

// Tableau des interventions : affichage par pages de 50 lignes, recherche par coordonnées GPS
// et tri par gravité (la recherche, le tri et la pagination sont exécutés par la base).

#include <QObject>

#include "intervention.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class InterventionPage;
}
QT_END_NAMESPACE

class InterventionTable : public QObject
{
    Q_OBJECT

public:
    static constexpr int kLignesParPage = 50;

    explicit InterventionTable(Ui::InterventionPage *ui, QObject *parent = nullptr);

    QString refresh();                      // recharge la page courante ; renvoie l'erreur éventuelle
    Intervention::Filtre filtre() const;    // recherche et tri choisis (utilisé aussi par l'export)

signals:
    void viewRequested(int id);     // bouton Afficher ou double-clic
    void editRequested(int id);     // bouton Modifier
    void deleteRequested(int id);   // bouton Supprimer
    void noMatch();                 // la recherche ne trouve aucune intervention

private:
    void setupTable();
    void setupFilters();
    void goToFirstPage();

   Ui::InterventionPage *ui;
    int m_page = 0;                 // numéro de page, à partir de 0
};

#endif // INTERVENTIONTABLE_H
