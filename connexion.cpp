#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QVariant>

// ====== ORACLE : à modifier selon votre machine ======
static const QString ORACLE_NOM          = "oracle";              // nom interne de la connexion Qt
static const QString ORACLE_DSN          = "axis";                // source ODBC (Administrateur ODBC 64 bits)
static const QString ORACLE_UTILISATEUR  = "axis";                // utilisateur Oracle (connexion SQL Developer « axis »)
static const QString ORACLE_MOT_DE_PASSE = "axis";  // ne jamais pousser le vrai mot de passe sur GitHub
// =====================================================

QSqlDatabase Connexion::oracle()
{
    return QSqlDatabase::database(ORACLE_NOM);   // connexion nommée, créée par ouvrir()
}

bool Connexion::ouvrir(QString &erreur)
{
    // ---- Oracle (ODBC) : module Employés ----
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

    // ---- SQLite : connexion par défaut, utilisée par les autres modules (inchangée) ----
    const QString dossier = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dossier);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dossier + "/caserne.db");

    if (!db.open()) {
        erreur = "Impossible d'ouvrir la base : " + db.lastError().text();
        return false;
    }

    QSqlQuery q;
    q.exec("PRAGMA foreign_keys = ON");   // doit être activé avant tout, sinon ON DELETE CASCADE est ignoré

    // ---- Modèle physique ----
    // Les contraintes CHECK valident les données des employés.
    q.exec("CREATE TABLE IF NOT EXISTS employes("
           "id TEXT PRIMARY KEY, "
           "nom TEXT NOT NULL CHECK(length(trim(nom)) > 0), "
           "prenom TEXT NOT NULL DEFAULT '', "
           "telephone TEXT NOT NULL CHECK(length(trim(telephone)) >= 8), "
           "email TEXT NOT NULL UNIQUE COLLATE NOCASE CHECK(email LIKE '%_@_%._%'), "
           "grade TEXT CHECK(grade IN ('Caporal','Sergent','Lieutenant','Capitaine')), "
           "poste TEXT CHECK(poste IN ('Pompier','Chef d''équipe','Chef de caserne',"
           "'Opérateur de dispatch','Administrateur','Magasinier')), "
           "disponibilite TEXT CHECK(disponibilite IN ('Disponible','En intervention','Indisponible')), "
           "age INTEGER CHECK(age BETWEEN 18 AND 70), "
           "heures INTEGER CHECK(heures BETWEEN 0 AND 100), "
           "conges INTEGER CHECK(conges >= 0))");

    q.exec("CREATE INDEX IF NOT EXISTS idx_employes_nom ON employes(nom)");

    // Bases créées avec une ancienne version : la connexion par mot de passe n'existe plus, on retire la colonne.
    q.exec("PRAGMA table_info(employes)");
    bool aMdp = false, aAlerte = false;
    while (q.next()) {
        if (q.value(1).toString() == "mot_de_passe") aMdp = true;
        if (q.value(1).toString() == "alerte") aAlerte = true;
    }
    if (aMdp)    q.exec("ALTER TABLE employes DROP COLUMN mot_de_passe");
    if (aAlerte) q.exec("ALTER TABLE employes DROP COLUMN alerte");

    // Bases créées avant la séparation Nom / Prénom : ajout de la colonne puis découpage de l'ancien nom complet
    bool aPrenom = false;
    q.exec("PRAGMA table_info(employes)");
    while (q.next()) if (q.value(1).toString() == "prenom") aPrenom = true;
    if (!aPrenom) q.exec("ALTER TABLE employes ADD COLUMN prenom TEXT NOT NULL DEFAULT ''");
    {
        QSqlQuery lect, maj;
        lect.exec("SELECT id, nom FROM employes WHERE prenom = '' AND nom LIKE '% %'");
        while (lect.next()) {
            const QString complet = lect.value(1).toString().trimmed();
            const int sp = complet.indexOf(' ');
            if (sp <= 0) continue;
            maj.prepare("UPDATE employes SET prenom=?, nom=? WHERE id=?");
            maj.addBindValue(complet.left(sp));
            maj.addBindValue(complet.mid(sp + 1).trimmed());
            maj.addBindValue(lect.value(0).toString());
            maj.exec();
        }
    }

    // Données de départ
    q.exec("SELECT COUNT(*) FROM employes");
    if (q.next() && q.value(0).toInt() == 0) {
        struct Dep { const char *id, *prenom, *nom, *tel, *email, *grade, *poste, *dispo; int age, heures, conges; };
        const QVector<Dep> depart = {
            {"E001", "Sami", "Gharbi", "20 123 456", "sami.gharbi@email.com", "Sergent", "Chef d'équipe", "En intervention", 34, 52, 4},
            {"E002", "Rim", "Trabelsi", "22 456 789", "rim.trabelsi@email.com", "Caporal", "Pompier", "Disponible", 28, 32, 18},
            {"E003", "Karim", "Majri", "98 321 654", "karim.majri@email.com", "Lieutenant", "Chef de caserne", "Disponible", 41, 49, 12},
            {"E004", "Leila", "Mansouri", "55 987 321", "leila.mansouri@email.com", "Caporal", "Opérateur de dispatch", "Indisponible", 30, 38, 0},
            {"E005", "Admin", "AXIS", "71 000 000", "admin@axis.com", "Capitaine", "Administrateur", "Disponible", 45, 40, 20},
            {"E006", "Youssef", "Ben Salah", "24 111 222", "youssef.bensalah@email.com", "Caporal", "Magasinier", "Disponible", 36, 40, 15},
        };
        // Table SQLite « employes » : encore lue par le module Matériels (liste des employés responsables).
        // Les vrais employés du module Employés sont dans Oracle (table EMPLOYE).
        for (const Dep &d : depart) {
            QSqlQuery ins;
            ins.prepare("INSERT OR IGNORE INTO employes(id, nom, prenom, telephone, email, grade, poste, disponibilite, "
                        "age, heures, conges) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
            ins.addBindValue(QString(d.id)); ins.addBindValue(QString(d.nom)); ins.addBindValue(QString(d.prenom));
            ins.addBindValue(QString(d.tel)); ins.addBindValue(QString(d.email)); ins.addBindValue(QString(d.grade));
            ins.addBindValue(QString(d.poste)); ins.addBindValue(QString(d.dispo));
            ins.addBindValue(d.age); ins.addBindValue(d.heures); ins.addBindValue(d.conges);
            ins.exec();
        }
    }

    // Mise à jour pour les bases créées avec une ancienne version
    q.exec("UPDATE employes SET poste='Opérateur de dispatch' WHERE poste='Opérateur'");
    return true;
}
