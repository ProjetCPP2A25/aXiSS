#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>

// ====== ORACLE : à modifier selon votre machine ======
static const QString ORACLE_NOM          = "oracle";              // nom interne de la connexion Qt
static const QString ORACLE_DSN = "AXIS_ODBC";               // source ODBC (Administrateur ODBC 64 bits)
static const QString ORACLE_UTILISATEUR  = "axis";                // utilisateur Oracle (connexion SQL Developer « axis »)
static const QString ORACLE_MOT_DE_PASSE =
    qEnvironmentVariable("AXIS_ORACLE_PASSWORD");  // ne jamais pousser le vrai mot de passe sur GitHub
// =====================================================

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
