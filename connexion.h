#pragma once
#include <QString>
#include <QSqlDatabase>

// Connexion Oracle commune à toute l'application (connexion nommée "oracle", via ODBC, ouverte une seule fois par main()).
// Utilisée par les classes métier Employe et Intervention : toujours passer par Connexion::oracle().
class Connexion
{
public:
    static bool ouvrir(QString &erreur);     // ouvre Oracle
    static QSqlDatabase oracle();            // connexion Oracle, ouverte une seule fois pour toute l'application
};
