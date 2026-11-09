#ifndef GAXIS_H
#define GAXIS_H

// Interface unique de l'application (gaxis.ui + gaxis.h + gaxis.cpp).
// Elle ne contient aucune requête SQL : tout passe par la classe Vehicule.

#include <QMainWindow>
#include <QString>
#include <QVector>

class Vehicule;
class QNetworkAccessManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class GAxis;
}
QT_END_NAMESPACE

class GAxis : public QMainWindow
{
    Q_OBJECT

public:
    explicit GAxis(QWidget *parent = nullptr);
    ~GAxis() override;

private slots:
    void onSubmit();            // Ajouter ou Enregistrer (selon le mode)
    void viderFormulaire();
    void rafraichir();          // recharge tableau, alerte et statistiques
    void exporterExcel();
    void envoyerAlerteSms();

private:
    void remplirTableau(const QVector<Vehicule> &liste);
    void afficherAlerte();
    void afficherStatistiques();
    void afficherVehicule(const QString &id);
    void modifierVehicule(const QString &id);
    void supprimerVehicule(const QString &id);

    Ui::GAxis *ui;
    QString m_idEdition;        // vide = mode ajout
    QNetworkAccessManager *m_smsNetwork = nullptr;
};

#endif // GAXIS_H
