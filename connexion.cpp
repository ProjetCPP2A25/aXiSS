#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>

// ====== ORACLE ======
static const QString ORACLE_NOM          = "oracle";              // nom interne de la connexion Qt
static const QString ORACLE_DSN          = "AXIS_COMMUN";         // source ODBC 64 bits
static const QString ORACLE_UTILISATEUR  = "AXIS_COMMUN";         // utilisateur Oracle
static const QString ORACLE_MOT_DE_PASSE = "1234";                // mot de passe partagé
// ====================

QSqlDatabase Connexion::oracle()
{
    return QSqlDatabase::database(ORACLE_NOM);   // connexion nommée, créée par ouvrir()
}

bool Connexion::ouvrir(QString &erreur)
{
    // ---- Oracle (ODBC) : connexion commune (modules Employés et Interventions) ----
    if (!QSqlDatabase::contains(ORACLE_NOM)) {
        QSqlDatabase ora = QSqlDatabase::addDatabase("QODBC", ORACLE_NOM);
        ora.setDatabaseName(ORACLE_DSN);
        ora.setUserName(ORACLE_UTILISATEUR);
        ora.setPassword(ORACLE_MOT_DE_PASSE);
        if (!ora.open()) {
            erreur = "Connexion Oracle impossible :\n" + ora.lastError().text();
            if (!QSqlDatabase::drivers().contains("QODBC"))
                erreur += "\n\nLe pilote QODBC n'est pas disponible dans ce kit Qt.";
            return false;
        }
    }

    return true;
}
