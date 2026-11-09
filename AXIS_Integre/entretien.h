#ifndef ENTRETIEN_H
#define ENTRETIEN_H

#include <QDate>
#include <QPair>
#include <QString>
#include <QVector>

struct Entretien
{
    QString id;
    QString vehiculeId;
    QString plaque;
    QString type;
    QString description;
    QString garage;
    QString statut;
    QDate datePrevue;
    QDate dateRealisee;
    QDate prochaineDate;
    int kmPrevu = 0;
    int kmRealise = 0;
    int prochainKm = 0;
    int kmVehicule = 0;
    double cout = 0.0;
};

// Persistance Oracle et règles métier du suivi préventif des entretiens.
class GestionEntretiens
{
public:
    static bool initialiser(QString *erreur = nullptr);
    static QVector<Entretien> lister(const QString &recherche = QString(), QString *erreur = nullptr);
    static bool vehicules(QVector<QPair<QString, QString>> &resultat, QString *erreur = nullptr);
    static bool ajouter(const Entretien &entretien, QString *erreur = nullptr);
    static bool terminer(const Entretien &entretien, QString *erreur = nullptr);
    static bool supprimer(const QString &id, QString *erreur = nullptr);
};

#endif // ENTRETIEN_H
