#ifndef GAXIS_H
#define GAXIS_H

#include <QMainWindow>
#include <QString>

#include "intervention.h"

class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class GAxis;
}
QT_END_NAMESPACE

// Classe d'interface (vue) d'A.X.I.S : barre latérale + une page (QStackedWidget) par module.
// Elle gère le module "Gestion des interventions" : formulaire, tableau et boîtes de dialogue.
// Aucune requête SQL ici : tout passe par la classe Intervention (modèle).
class GAxis : public QMainWindow
{
    Q_OBJECT

public:
    // "role" : rôle de l'utilisateur connecté (vide = pas de contrôle d'accès, mode développement)
    explicit GAxis(const QString &role = QString(), QWidget *parent = nullptr);
    ~GAxis() override;

private slots:
    void onSave();                      // Ajouter ou enregistrer la modification
    void showDetails(int id);           // Afficher
    void startEdit(int id);             // Modifier
    void deleteIntervention(int id);    // Supprimer
    void showModule(int page);

    void onEtatChanged();               // formulaire : la date de fin n'est saisissable que si l'intervention est terminée
    void updateDuree();                 // formulaire : durée calculée automatiquement
    void onClearOrCancel();             // formulaire : bouton Vider / Annuler

private:
    // Formulaire "Ajouter" / "Modifier"
    void setupForm();
    bool readForm(Intervention &out, QString &error) const;     // lit et valide les champs
    void clearForm();
    void startFormEdit(const Intervention &it);                 // passe en mode modification
    void endFormEdit();                                         // revient au mode ajout
    bool isEditing() const { return m_editId >= 0; }

    // Tableau des interventions
    void setupTable();
    QString refreshTable();                                     // recharge la liste ; renvoie l'erreur éventuelle

    void refreshAll(bool userAction = false);
    void reportError(const QString &error, bool userAction);

    Ui::GAxis *ui;
    QTimer *m_timer;                    // actualisation automatique (durées des interventions en cours)
    int m_editId = -1;                  // identifiant de l'intervention en cours de modification (-1 = ajout)
    bool m_lastRefreshFailed = false;
};
#endif // GAXIS_H
