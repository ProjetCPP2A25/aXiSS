#include "conge.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

bool Conge::ajouter()
{
    QSqlQuery q;
    q.prepare("INSERT INTO demandes_conge(id_employe,nom_employe,details,jours,statut) VALUES(?,?,?,?,?)");
    q.addBindValue(idEmploye); q.addBindValue(nomEmploye); q.addBindValue(details);
    q.addBindValue(jours); q.addBindValue(statut);
    if (!q.exec()) return false;
    id = q.lastInsertId().toInt();
    return true;
}

QVector<Conge> Conge::lister(int limite)
{
    QSqlQuery q;
    q.prepare("SELECT id,id_employe,nom_employe,details,jours,statut FROM demandes_conge ORDER BY id DESC LIMIT ?");
    q.addBindValue(limite);
    q.exec();
    QVector<Conge> r;
    while (q.next()) {
        Conge c;
        c.id = q.value(0).toInt(); c.idEmploye = q.value(1).toString(); c.nomEmploye = q.value(2).toString();
        c.details = q.value(3).toString(); c.jours = q.value(4).toInt(); c.statut = q.value(5).toString();
        r.append(c);
    }
    return r;
}

bool Conge::decider(int id, bool accepter, QString &message)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
    q.prepare("SELECT id_employe,jours,details,statut FROM demandes_conge WHERE id=?");
    q.addBindValue(id);
    q.exec();
    if (!q.next() || q.value(3).toString() != "En attente") { db.rollback(); return false; }

    const QString idEmp = q.value(0).toString();
    const int jours = q.value(1).toInt();
    QString details = q.value(2).toString();
    QString statut = accepter ? "Acceptée" : "Refusée";

    if (accepter) {
        QSqlQuery s;
        s.prepare("SELECT conges FROM employes WHERE id=?");
        s.addBindValue(idEmp);
        s.exec();
        if (!s.next() || jours > s.value(0).toInt()) {          // refus automatique
            statut = "Refusée";
            details += " · Solde insuffisant";
            message = "Le solde de congés est insuffisant : la demande est refusée.";
        } else {
            QSqlQuery u;                                        // mise à jour automatique du solde
            u.prepare("UPDATE employes SET conges = conges - ? WHERE id=?");
            u.addBindValue(jours);
            u.addBindValue(idEmp);
            if (!u.exec()) { db.rollback(); return false; }
        }
    }
    QSqlQuery u2;
    u2.prepare("UPDATE demandes_conge SET details=?, statut=? WHERE id=?");
    u2.addBindValue(details); u2.addBindValue(statut); u2.addBindValue(id);
    if (!u2.exec()) { db.rollback(); return false; }
    return db.commit();
}
