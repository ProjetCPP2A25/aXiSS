#pragma once
#include <QString>

// Gère la connexion à la base de données.
// Pour passer à Oracle plus tard, il suffira de modifier connexion.cpp.
class Connexion
{
public:
    static bool ouvrir(QString &erreur);
};
