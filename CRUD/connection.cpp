#include "connection.h"

#include <QSqlError>

namespace {
const QString kNomConnexion = QStringLiteral("AXIS_CONNECTION");
} // namespace

Connection &Connection::instance()
{
    static Connection unique;
    return unique;
}

QSqlDatabase Connection::database() const
{
    return QSqlDatabase::contains(kNomConnexion) ? QSqlDatabase::database(kNomConnexion, false)
                                                 : QSqlDatabase();
}

bool Connection::estOuverte() const
{
    return database().isOpen();
}

bool Connection::ouvrir()
{
    if (estOuverte())
        return true;

    // Paramètres d'origine du projet (Oracle via ODBC) ; les variables d'environnement
    // AXIS_DB_DRIVER, AXIS_DB_DSN, AXIS_DB_USER et AXIS_DB_PASSWORD permettent de les remplacer.
    QSqlDatabase db = QSqlDatabase::contains(kNomConnexion)
        ? QSqlDatabase::database(kNomConnexion, false)
        : QSqlDatabase::addDatabase(qEnvironmentVariable("AXIS_DB_DRIVER", QStringLiteral("QODBC")),
                                    kNomConnexion);

    db.setDatabaseName(qEnvironmentVariable("AXIS_DB_DSN", QStringLiteral("AXIS_ODBC")));
    db.setUserName(qEnvironmentVariable("AXIS_DB_USER", QStringLiteral("AXIS")));

    // Temporary local test: enter your group's password here.
    db.setPassword(qEnvironmentVariable("AXIS_DB_PASSWORD", QStringLiteral("1234")));

    if (!db.open()) {
        m_erreur = db.lastError().text();
        return false;
    }
    m_erreur.clear();
    return true;
}

void Connection::fermer()
{
    if (QSqlDatabase::contains(kNomConnexion)) {
        {
            QSqlDatabase db = QSqlDatabase::database(kNomConnexion, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(kNomConnexion);
    }
}

QString Connection::derniereErreur() const
{
    return m_erreur;
}
