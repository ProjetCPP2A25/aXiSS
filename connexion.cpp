#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QStringList>

static const QString DSN          = "axis";                 // source ODBC (Administrateur ODBC 64 bits)
static const QString UTILISATEUR  = "axis";                 // utilisateur Oracle
static const QString MOT_DE_PASSE = "axis";   // mot de passe Oracle

Connexion &Connexion::instance()
{
    static Connexion c;
    return c;
}

bool Connexion::ouvrir(QString &erreur)
{
    instance();   // garantit l'existence du singleton
    QSqlDatabase db = QSqlDatabase::contains() ? QSqlDatabase::database()
                                               : QSqlDatabase::addDatabase("QODBC");
    if (db.isOpen())
        return true;

    db.setDatabaseName(DSN);
    db.setUserName(UTILISATEUR);
    db.setPassword(MOT_DE_PASSE);

    if (!db.open()) {
        erreur = "Connexion Oracle impossible :\n" + db.lastError().text();
        if (!QSqlDatabase::drivers().contains("QODBC"))
            erreur += "\n\nLe pilote QODBC n'est pas disponible dans ce kit Qt.";
        return false;
    }
    return true;
}
