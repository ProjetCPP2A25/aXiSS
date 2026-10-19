#include "connexion.h"
#include "employe.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>

bool Connexion::ouvrir(QString &erreur)
{
    // ---- SQLite (provisoire, en attendant Oracle) ----
    const QString dossier = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dossier);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dossier + "/caserne.db");

    // ---- Oracle (à activer plus tard via ODBC) ----
    // QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    // db.setDatabaseName("NOM_DU_DSN");
    // db.setUserName("utilisateur");
    // db.setPassword("motdepasse");

    if (!db.open()) {
        erreur = "Impossible d'ouvrir la base : " + db.lastError().text();
        return false;
    }

    QSqlQuery q;
    q.exec("CREATE TABLE IF NOT EXISTS employes("
           "id TEXT PRIMARY KEY, nom TEXT NOT NULL, prenom TEXT NOT NULL DEFAULT '', telephone TEXT NOT NULL, email TEXT NOT NULL UNIQUE, "
           "grade TEXT, poste TEXT, disponibilite TEXT, mot_de_passe TEXT NOT NULL, "
           "age INTEGER, heures INTEGER, conges INTEGER, alerte INTEGER DEFAULT 0)");
    q.exec("CREATE TABLE IF NOT EXISTS demandes_conge("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, id_employe TEXT NOT NULL, nom_employe TEXT, "
           "details TEXT, jours INTEGER, statut TEXT, "
           "FOREIGN KEY(id_employe) REFERENCES employes(id) ON DELETE CASCADE)");
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("CREATE INDEX IF NOT EXISTS idx_employes_nom ON employes(nom)");

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

    // Données de départ (mot de passe de tous les comptes : 1234)
    q.exec("SELECT COUNT(*) FROM employes");
    if (q.next() && q.value(0).toInt() == 0) {
        const QString pw = Employe::hash("1234");
        struct Dep { const char *id, *prenom, *nom, *tel, *email, *grade, *poste, *dispo; int age, heures, conges; };
        const QVector<Dep> depart = {
            {"E001", "Sami", "Gharbi", "20 123 456", "sami.gharbi@email.com", "Sergent", "Chef d'équipe", "En intervention", 34, 52, 4},
            {"E002", "Rim", "Trabelsi", "22 456 789", "rim.trabelsi@email.com", "Caporal", "Pompier", "Disponible", 28, 32, 18},
            {"E003", "Karim", "Majri", "98 321 654", "karim.majri@email.com", "Lieutenant", "Chef de caserne", "Disponible", 41, 49, 12},
            {"E004", "Leila", "Mansouri", "55 987 321", "leila.mansouri@email.com", "Caporal", "Opérateur de dispatch", "Indisponible", 30, 38, 0},
            {"E005", "Admin", "AXIS", "71 000 000", "admin@axis.com", "Capitaine", "Administrateur", "Disponible", 45, 40, 20},
        };
        for (const Dep &d : depart) {
            Employe e;
            e.id = d.id; e.prenom = d.prenom; e.nom = d.nom; e.telephone = d.tel; e.email = d.email;
            e.grade = d.grade; e.poste = d.poste; e.disponibilite = d.dispo; e.motDePasse = pw;
            e.age = d.age; e.heures = d.heures; e.conges = d.conges;
            e.ajouter();
        }
    }

    // Mise à jour pour les bases créées avec une ancienne version
    q.exec("UPDATE employes SET poste='Opérateur de dispatch' WHERE poste='Opérateur'");
    q.exec("SELECT COUNT(*) FROM employes WHERE poste='Magasinier'");
    if (q.next() && q.value(0).toInt() == 0) {
        Employe m;
        m.id = "E006"; m.prenom = "Youssef"; m.nom = "Ben Salah"; m.telephone = "24 111 222"; m.email = "youssef.bensalah@email.com";
        m.grade = "Caporal"; m.poste = "Magasinier"; m.disponibilite = "Disponible";
        m.motDePasse = Employe::hash("1234"); m.age = 36; m.heures = 40; m.conges = 15;
        m.ajouter();
    }
    return true;
}
