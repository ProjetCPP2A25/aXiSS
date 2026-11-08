#pragma once
#include <QString>

// Connexion à la base Oracle via ODBC (Source de données configurée dans l'administrateur ODBC)
class Connection
{
public:
    static bool ouvrir(QString *error = nullptr);
    static void fermer();
};
