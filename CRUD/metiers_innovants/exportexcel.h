#ifndef EXPORTEXCEL_H
#define EXPORTEXCEL_H

// Export de la liste vers un classeur Excel (.xlsx).
// L'écriture du fichier se fait dans un thread séparé pour ne pas bloquer l'interface.

#include <QVector>

#include "intervention.h"

class QMainWindow;
class QWidget;

namespace ExportExcel {

// Demande l'emplacement du fichier puis écrit le .xlsx en arrière-plan.
// "bouton" est désactivé pendant l'export (peut être nul). Le résultat s'affiche dans la barre d'état.
void run(QMainWindow *window, const QVector<Intervention> &rows, QWidget *bouton = nullptr);

} // namespace ExportExcel

#endif // EXPORTEXCEL_H
