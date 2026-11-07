#ifndef STATSPANEL_H
#define STATSPANEL_H

// Statistiques : diagramme en barres verticales du nombre d'interventions par niveau de gravité
// (comptage fait par la base avec COUNT / GROUP BY).

#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class InterventionPage;
}
QT_END_NAMESPACE

class StatsPanel
{
public:
    explicit StatsPanel(Ui::InterventionPage *ui) : ui(ui) {}

    QString refresh();      // renvoie l'erreur éventuelle de la base

private:
   Ui::InterventionPage *ui;
};

#endif // STATSPANEL_H
