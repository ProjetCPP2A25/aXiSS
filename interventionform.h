#ifndef INTERVENTIONFORM_H
#define INTERVENTIONFORM_H

// Formulaire "Ajouter" : saisie d'une intervention, validation, et mode modification.

#include <QObject>

#include "intervention.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class InterventionPage;
}
QT_END_NAMESPACE

class InterventionForm : public QObject
{
    Q_OBJECT

public:
    explicit InterventionForm(Ui::InterventionPage *ui, QObject *parent = nullptr);

    bool read(Intervention &out, QString &error) const;   // lit et valide les champs
    void clear();                                          // remet le formulaire à zéro
    void startEdit(const Intervention &it);                // passe en mode modification
    void endEdit();                                        // revient au mode ajout
    void setEnabled(bool enabled);                         // verrouille le formulaire (accès refusé)

    bool isEditing() const { return m_editId >= 0; }
    int editId() const { return m_editId; }

signals:
    void saveRequested();       // clic sur Ajouter/Modifier ou touche Entrée
    void editCancelled();       // clic sur Annuler pendant une modification

private slots:
    void onEtatChanged();
    void updateDuree();         // durée calculée automatiquement
    void onClearOrCancel();

private:
    Ui::InterventionPage *ui;
    int m_editId = -1;
};

#endif // INTERVENTIONFORM_H
