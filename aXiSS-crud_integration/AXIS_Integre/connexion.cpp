#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>

// >>> À MODIFIER avec les paramètres de ta base <<<
static const char *ORACLE_NOM = "axis_oracle";
static const char *ORACLE_DSN = "AXIS_ORACLE";
static const char *ORACLE_UTILISATEUR = "AXIS_COMMUN";
static const char *ORACLE_MOT_DE_PASSE = "1234";


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
