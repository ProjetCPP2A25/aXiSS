#include "vehicule.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QString iso(const QDate &d) { return d.isValid() ? d.toString(Qt::ISODate) : QString(); }
QDate fromIso(const QString &s) { return QDate::fromString(s, Qt::ISODate); }

bool echec(QString *erreur, const QSqlQuery &q)
{
    if (erreur) *erreur = q.lastError().text();
    return false;
}
} // namespace

bool Vehicule::initialiser(QString *erreur)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        if (erreur) *erreur = "La base de données du projet n'est pas ouverte.";
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS vehicules ("
                "id TEXT PRIMARY KEY, "
                "immatriculation TEXT NOT NULL, "
                "type TEXT, "
                "km INTEGER NOT NULL DEFAULT 0, "
                "etat TEXT, "
                "dispo TEXT, "
                "derniere_maint TEXT, "
                "prochaine_maint TEXT, "
                "assurance TEXT, "
                "visite_technique TEXT)"))
        return echec(erreur, q);

    // données d'exemple (identiques à la maquette)
    q.exec("SELECT COUNT(*) FROM vehicules");
    if (q.next() && q.value(0).toInt() == 0) {
        auto make = [](const char *id, const char *plate, const char *type, int km, const char *etat,
                       const char *dispo, QDate last, QDate next, QDate ins, QDate tech) {
            Vehicule v;
            v.id = id; v.plate = plate; v.type = type; v.km = km; v.etat = etat; v.dispo = dispo;
            v.lastMaint = last; v.nextMaint = next; v.insurance = ins; v.tech = tech;
            return v;
        };
        const QVector<Vehicule> exemples = {
            make("V001", "123 TUN 4567", "Camion-citerne", 42300, "Bon", "Disponible",
                 QDate(2026, 9, 15), QDate(2026, 12, 15), QDate(2026, 12, 31), QDate(2026, 11, 15)),
            make("V002", "145 TUN 8821", "Ambulance", 68950, "À surveiller", "En maintenance",
                 QDate(2026, 8, 2), QDate(2026, 10, 2), QDate(2026, 11, 30), QDate(2026, 12, 10)),
            make("V003", "98 TUN 3310", "Échelle aérienne", 101400, "Bon", "En intervention",
                 QDate(2026, 7, 20), QDate(2026, 10, 20), QDate(2026, 10, 12), QDate(2027, 1, 5)),
        };
        for (const Vehicule &v : exemples) ajouter(v);
    }
    return true;
}

QVector<Vehicule> Vehicule::tous()
{
    QVector<Vehicule> liste;
    QSqlQuery q;
    if (!q.exec("SELECT id, immatriculation, type, km, etat, dispo, derniere_maint, prochaine_maint, "
                "assurance, visite_technique FROM vehicules ORDER BY id"))
        return liste;
    while (q.next()) {
        Vehicule v;
        v.id = q.value(0).toString();
        v.plate = q.value(1).toString();
        v.type = q.value(2).toString();
        v.km = q.value(3).toInt();
        v.etat = q.value(4).toString();
        v.dispo = q.value(5).toString();
        v.lastMaint = fromIso(q.value(6).toString());
        v.nextMaint = fromIso(q.value(7).toString());
        v.insurance = fromIso(q.value(8).toString());
        v.tech = fromIso(q.value(9).toString());
        liste.push_back(v);
    }
    return liste;
}

bool Vehicule::ajouter(const Vehicule &v, QString *erreur)
{
    QSqlQuery q;
    q.prepare("INSERT INTO vehicules (id, immatriculation, type, km, etat, dispo, derniere_maint, "
              "prochaine_maint, assurance, visite_technique) VALUES (?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(v.id);
    q.addBindValue(v.plate);
    q.addBindValue(v.type);
    q.addBindValue(v.km);
    q.addBindValue(v.etat);
    q.addBindValue(v.dispo);
    q.addBindValue(iso(v.lastMaint));
    q.addBindValue(iso(v.nextMaint));
    q.addBindValue(iso(v.insurance));
    q.addBindValue(iso(v.tech));
    return q.exec() ? true : echec(erreur, q);
}

bool Vehicule::modifier(const Vehicule &v, QString *erreur)
{
    QSqlQuery q;
    q.prepare("UPDATE vehicules SET immatriculation=?, type=?, km=?, etat=?, dispo=?, derniere_maint=?, "
              "prochaine_maint=?, assurance=?, visite_technique=? WHERE id=?");
    q.addBindValue(v.plate);
    q.addBindValue(v.type);
    q.addBindValue(v.km);
    q.addBindValue(v.etat);
    q.addBindValue(v.dispo);
    q.addBindValue(iso(v.lastMaint));
    q.addBindValue(iso(v.nextMaint));
    q.addBindValue(iso(v.insurance));
    q.addBindValue(iso(v.tech));
    q.addBindValue(v.id);
    return q.exec() ? true : echec(erreur, q);
}

bool Vehicule::supprimer(const QString &id, QString *erreur)
{
    QSqlQuery q;
    q.prepare("DELETE FROM vehicules WHERE id=?");
    q.addBindValue(id);
    return q.exec() ? true : echec(erreur, q);
}
