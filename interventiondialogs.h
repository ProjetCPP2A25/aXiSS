#ifndef INTERVENTIONDIALOGS_H
#define INTERVENTIONDIALOGS_H

// Boîtes de dialogue : détails d'une intervention (Afficher), confirmation de suppression, messages.

#include <QDialog>

#include "intervention.h"

class DetailsDialog : public QDialog
{
    Q_OBJECT

public:
    // exec() == Accepted  -> l'utilisateur a cliqué sur "Modifier"
    explicit DetailsDialog(const Intervention &it, QWidget *parent = nullptr);
};

namespace Dialogs {

bool confirmDelete(QWidget *parent, const Intervention &it);
void showInfo(QWidget *parent, const QString &title, const QString &text);
void showWarning(QWidget *parent, const QString &title, const QString &text);
void showError(QWidget *parent, const QString &title, const QString &text);

} // namespace Dialogs

#endif // INTERVENTIONDIALOGS_H
