                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            #include "connexion.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// ====== ORACLE : à modifier selon votre machine ======
static const QString ORACLE_DSN          = "axis";   // source ODBC (Administrateur ODBC 64 bits)
static const QString ORACLE_UTILISATEUR  = "axis";   // utilisateur Oracle (connexion SQL Developer « axis »)
static const QString ORACLE_MOT_DE_PASSE = "axis";   // ne jamais pousser un vrai mot de passe sur GitHub
// =====================================================

QSqlDatabase Connexion::oracle()
{
    return QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
}

bool Connexion::ouvrir(QString &erreur)
{
    // Déjà ouverte : rien à faire
    if (oracle().isValid() && oracle().isOpen())
        return true;

    if (!QSqlDatabase::drivers().contains("QODBC")) {
        erreur = "Le pilote QODBC n'est pas disponible dans ce kit Qt.";
        return false;
    }

    // ---- Connexion Oracle (ODBC) ----
    QSqlDatabase db = QSqlDatabase::contains(QSqlDatabase::defaultConnection)
                          ? QSqlDatabase::database(QSqlDatabase::defaultConnection, false)
                          : QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(ORACLE_DSN);
    db.setUserName(ORACLE_UTILISATEUR);
    db.setPassword(ORACLE_MOT_DE_PASSE);

    if (!db.open()) {
        erreur = "Connexion Oracle impossible :\n" + db.lastError().text()
        + "\n\nVérifiez que le DSN ODBC « " + ORACLE_DSN + " » existe (ODBC 64 bits).";
        return false;
    }

    // ---- La table VEHICULE doit exister (créée par le script SQL du groupe) ----
    QSqlQuery q(db);
    if (!q.exec("SELECT COUNT(*) FROM VEHICULE")) {
        erreur = "La table VEHICULE est introuvable dans Oracle.\n"
                 "Exécutez d'abord le script de création des tables (connexion « axis » dans SQL Developer).\n\n"
                 + q.lastError().text();
        return false;
    }

    // ---- Table vide : quelques véhicules d'exemple (une seule fois pour tout le groupe) ----
    if (q.next() && q.value(0).toInt() == 0) {
        struct Ex {
            int id;
            const char *immat, *type;
            int km;
            const char *etat, *dispo, *derniereMaint, *assurance, *visiteTech;
        };
        const Ex exemples[] = {
                                {1, "123 TUN 4567", "Camion-citerne",   42300,  "Bon",          "Disponible",
                                 "2026-09-15", "2026-12-31", "2026-11-15"},
                                {2, "145 TUN 8821", "Ambulance",        68950,  "À surveiller", "En maintenance",
                                 "2026-08-02", "2026-11-30", "2026-12-10"},
                                {3, "98 TUN 3310",  "Échelle aérienne", 101400, "Bon",          "En intervention",
                                 "2026-07-20", "2026-10-12", "2027-01-05"},
                                };

        for (const Ex &e : exemples) {
            QSqlQuery ins(db);
            ins.prepare("INSERT INTO VEHICULE (ID_VEHICULE, IMMATRICULATION, TYPE, KILOMETRAGE, ETAT, "
                        "DISPONIBILITE, DATE_DERNIERE_MAINTENANCE, DATE_EXPIRATION_ASSURANCE, "
                        "DATE_VISITE_TECHNIQUE) "
                        "VALUES (?, ?, ?, ?, ?, ?, "
                        "TO_DATE(?, 'YYYY-MM-DD'), TO_DATE(?, 'YYYY-MM-DD'), TO_DATE(?, 'YYYY-MM-DD'))");
            ins.addBindValue(e.id);
            ins.addBindValue(QString::fromUtf8(e.immat));
            ins.addBindValue(QString::fromUtf8(e.type));
            ins.addBindValue(e.km);
            ins.addBindValue(QString::fromUtf8(e.etat));
            ins.addBindValue(QString::fromUtf8(e.dispo));
            ins.addBindValue(QString::fromUtf8(e.derniereMaint));
            ins.addBindValue(QString::fromUtf8(e.assurance));
            ins.addBindValue(QString::fromUtf8(e.visiteTech));
            ins.exec();
        }
    }

    return true;
}

