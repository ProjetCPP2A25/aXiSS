#pragma once
#include <QString>

// Singleton : UNE SEULE connexion Oracle (via ODBC) pour toute l'application.
class Connexion
{
public:
    static Connexion &instance();
    static bool ouvrir(QString &erreur);      // compatible avec l'ancien appel Connexion::ouvrir(erreur)
    Connexion(const Connexion &) = delete;
    Connexion &operator=(const Connexion &) = delete;
private:
    Connexion() = default;
};
