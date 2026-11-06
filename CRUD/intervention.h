
#ifndef INTERVENTION_H
#define INTERVENTION_H

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVector>

namespace Grav {
inline const QString Elevee  = QStringLiteral("Élevée");
inline const QString Moyenne = QStringLiteral("Moyenne");
inline const QString Faible  = QStringLiteral("Faible");
}

namespace Etat {
inline const QString EnCours  = QStringLiteral("En cours");
inline const QString Terminee = QStringLiteral("Terminée");
}

namespace Role {
inline const QString Administrateur    = QStringLiteral("Administrateur");
inline const QString ChefCaserne       = QStringLiteral("Chef de caserne");
inline const QString OperateurDispatch = QStringLiteral("Opérateur de dispatch");
inline const QString Magasinier        = QStringLiteral("Magasinier");
}

class Intervention
{
public:
    // Attributs
    int id = 0;
    QString type;
    QString lieu;
    QString gravite;
    QString etat;

    double lat = 0.0;
    double lon = 0.0;

    QDateTime reception;
    QDateTime fin;

    // Valeurs calculées
    QString idText() const;
    QString gps() const;
    qint64 dureeMinutes() const;
    QString dureeTexte() const;
    int graviteRang() const;
    bool valider(QString *erreur) const;

    // CRUD
    bool ajouter(QString *erreur = nullptr);
    static bool afficher(int id, Intervention &resultat,
                         QString *erreur = nullptr);
    bool modifier(QString *erreur = nullptr) const;
    static bool supprimer(int id, QString *erreur = nullptr);
    static QVector<Intervention> lister(QString *erreur = nullptr);

    static bool roleAutorise(const QString &role);
};

#endif // INTERVENTION_H
