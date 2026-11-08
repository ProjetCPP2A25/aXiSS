#include "connection.h"

#include <QSqlDatabase>
#include <QSqlError>

// >>> À MODIFIER avec les paramètres de ta base <<<
static const char *DSN      = "AXIS_ORACLE";        // nom de la source de données ODBC
static const char *USER     = "AXIS_COMMUN";       // utilisateur Oracle
static const char *PASSWORD = "VOTRE_MOT_DE_PASSE";

bool Connection::ouvrir(QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(DSN);
    db.setUserName(USER);
    db.setPassword(PASSWORD);
    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

void Connection::fermer()
{
    QSqlDatabase::database().close();
}
