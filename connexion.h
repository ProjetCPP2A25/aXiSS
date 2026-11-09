#ifndef CONNEXION_H
#define CONNEXION_H

#include <QSqlDatabase>
#include <QString>

// Gère la connexion à la base Oracle commune du groupe (via ODBC, DSN "axis").
// La connexion est ouverte comme connexion PAR DÉFAUT de Qt :
// un simple "QSqlQuery q;" travaille donc directement sur Oracle.
class Connexion
{
public:
    static bool ouvrir(QString &erreur);

    // Renvoie la connexion Oracle (même chose que la connexion par défaut).
    // Permet d'écrire QSqlQuery q(Connexion::oracle()); comme dans le reste du projet du groupe.
    static QSqlDatabase oracle();
};

#endif // CONNEXION_H
