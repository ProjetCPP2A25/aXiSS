#pragma once
#include <QString>
#include <QSqlDatabase>

// Gère les connexions de l'application :
//  - Oracle (connexion nommée "oracle", via ODBC) : utilisée par le module Employés (classe Employe) ;
//  - SQLite (connexion par défaut) : utilisée par les autres modules (interventions, véhicules, matériels),
//    en attendant qu'ils soient eux aussi reliés à Oracle.
class Connexion
{
public:
    static bool ouvrir(QString &erreur);     // ouvre Oracle puis SQLite
    static QSqlDatabase oracle();            // connexion Oracle, ouverte une seule fois pour toute l'application
};
