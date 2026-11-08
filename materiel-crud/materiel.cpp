#include "materiel.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QStringList>

static const char *SELECT_MAT =
    "SELECT ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK, SEUIL_MINIMAL_STOCK, "
    "ETAT, DISPONIBILITE, DATE_ACQUISITION, EMPLACEMENT_STOCKAGE FROM MATERIEL";

static Materiel fromQuery(const QSqlQuery &q)
{
    Materiel m;
    m.id = q.value(0).toInt();
    m.reference = q.value(1).toString();
    m.designation = q.value(2).toString();
    m.categorie = q.value(3).toString();
    m.quantite = q.value(4).toInt();
    m.seuil = q.value(5).toInt();
    m.etat = q.value(6).toString();
    m.disponibilite = q.value(7).toString();
    m.dateAcquisition = q.value(8).toDate();
    m.emplacement = q.value(9).toString();
    return m;
}

// ---------- CRUD ----------

bool Materiel::ajouter(QString *error)
{
    // Pas d'auto-incrément sur ID_MATERIEL : on prend le prochain numéro libre
    QSqlQuery qid;
    if (!qid.exec("SELECT NVL(MAX(ID_MATERIEL), 0) + 1 FROM MATERIEL") || !qid.next()) {
        if (error) *error = qid.lastError().text();
        return false;
    }
    const int newId = qid.value(0).toInt();

    QSqlQuery q;
    q.prepare("INSERT INTO MATERIEL (ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK, "
              "SEUIL_MINIMAL_STOCK, ETAT, DISPONIBILITE, DATE_ACQUISITION, EMPLACEMENT_STOCKAGE) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(newId);
    q.addBindValue(reference);
    q.addBindValue(designation);
    q.addBindValue(categorie);
    q.addBindValue(quantite);
    q.addBindValue(seuil);
    q.addBindValue(etat);
    q.addBindValue(disponibilite);
    q.addBindValue(dateAcquisition);
    q.addBindValue(emplacement);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    id = newId;
    return true;
}

bool Materiel::modifier(QString *error) const
{
    QSqlQuery q;
    q.prepare("UPDATE MATERIEL SET REFERENCE=?, DESIGNATION=?, CATEGORIE=?, QUANTITE_STOCK=?, "
              "SEUIL_MINIMAL_STOCK=?, ETAT=?, DISPONIBILITE=?, DATE_ACQUISITION=?, EMPLACEMENT_STOCKAGE=? "
              "WHERE ID_MATERIEL=?");
    q.addBindValue(reference);
    q.addBindValue(designation);
    q.addBindValue(categorie);
    q.addBindValue(quantite);
    q.addBindValue(seuil);
    q.addBindValue(etat);
    q.addBindValue(disponibilite);
    q.addBindValue(dateAcquisition);
    q.addBindValue(emplacement);
    q.addBindValue(id);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

bool Materiel::supprimer(int id, QString *error)
{
    QSqlQuery q;
    q.prepare("DELETE FROM MATERIEL WHERE ID_MATERIEL = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        // Ex. ORA-02292 : matériel déjà utilisé dans une autre table (clé étrangère)
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

QList<Materiel> Materiel::afficher(const QString &search, const QString &orderColumn)
{
    // Liste blanche : on ne met jamais le texte de l'utilisateur dans l'ORDER BY
    QString orderBy;
    if (orderColumn == "etat")
        orderBy = "CASE ETAT WHEN 'Bon' THEN 1 WHEN 'Moyen' THEN 2 WHEN 'Mauvais' THEN 3 ELSE 4 END, UPPER(REFERENCE)";
    else if (orderColumn == "designation")      orderBy = "UPPER(DESIGNATION)";
    else if (orderColumn == "categorie")        orderBy = "UPPER(CATEGORIE), UPPER(REFERENCE)";
    else if (orderColumn == "quantite")         orderBy = "QUANTITE_STOCK";
    else if (orderColumn == "date_acquisition") orderBy = "DATE_ACQUISITION";
    else                                        orderBy = "UPPER(REFERENCE)";

    QSqlQuery q;
    q.prepare(QString("%1 WHERE UPPER(REFERENCE) LIKE UPPER(:s) OR UPPER(DESIGNATION) LIKE UPPER(:s) ORDER BY %2")
                  .arg(SELECT_MAT, orderBy));
    q.bindValue(":s", "%" + search + "%");
    QList<Materiel> list;
    if (q.exec())
        while (q.next()) list << fromQuery(q);
    return list;
}

// ---------- Utilitaires ----------

Materiel Materiel::trouver(int id)
{
    QSqlQuery q;
    q.prepare(QString("%1 WHERE ID_MATERIEL = ?").arg(SELECT_MAT));
    q.addBindValue(id);
    if (q.exec() && q.next()) return fromQuery(q);
    return Materiel();
}

bool Materiel::referenceExiste(const QString &reference, int ignoreId)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM MATERIEL WHERE UPPER(REFERENCE) = UPPER(?) AND ID_MATERIEL <> ?");
    q.addBindValue(reference);
    q.addBindValue(ignoreId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}
