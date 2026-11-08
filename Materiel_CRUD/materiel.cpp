#include "materiel.h"
#include "connexion.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QStringList>
#include <QSet>
#include <QDebug>

// Les noms de colonnes « seuil » et « emplacement » diffèrent selon les versions de la table MATERIEL :
// on regarde dans Oracle (USER_TAB_COLUMNS) lequel existe, une seule fois.
static QString colonne(const QStringList &candidats)
{
    static QSet<QString> colonnes;
    static bool charge = false;
    if (!charge) {
        charge = true;
        QSqlQuery q(Connexion::oracle());
        if (q.exec("SELECT COLUMN_NAME FROM USER_TAB_COLUMNS WHERE TABLE_NAME = 'MATERIEL'"))
            while (q.next()) colonnes.insert(q.value(0).toString().toUpper());
    }
    for (const QString &c : candidats)
        if (colonnes.contains(c)) return c;
    return candidats.first();
}
static QString colSeuil()       { return colonne({"SEUIL_MINIMAL_STOCK", "SEUIL_MINIMAL"}); }
static QString colEmplacement() { return colonne({"EMPLACEMENT_STOCKAGE", "EMPLACEMENT_STOCK"}); }

static QString selectMat()
{
    return "SELECT ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK, " + colSeuil() +
           ", ETAT, DISPONIBILITE, DATE_ACQUISITION, " + colEmplacement() + " FROM MATERIEL";
}

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
    QSqlQuery qid(Connexion::oracle());
    if (!qid.exec("SELECT NVL(MAX(ID_MATERIEL), 0) + 1 FROM MATERIEL") || !qid.next()) {
        if (error) *error = qid.lastError().text();
        return false;
    }
    const int newId = qid.value(0).toInt();

    QSqlQuery q(Connexion::oracle());
    q.prepare("INSERT INTO MATERIEL (ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK, " +
              colSeuil() + ", ETAT, DISPONIBILITE, DATE_ACQUISITION, " + colEmplacement() + ") "
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
    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE MATERIEL SET REFERENCE=?, DESIGNATION=?, CATEGORIE=?, QUANTITE_STOCK=?, " +
              colSeuil() + "=?, ETAT=?, DISPONIBILITE=?, DATE_ACQUISITION=?, " + colEmplacement() + "=? "
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
    QSqlQuery q(Connexion::oracle());
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

    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("%1 WHERE UPPER(REFERENCE) LIKE UPPER(?) OR UPPER(DESIGNATION) LIKE UPPER(?) ORDER BY %2")
                  .arg(selectMat(), orderBy));
    q.addBindValue("%" + search + "%");
    q.addBindValue("%" + search + "%");
    QList<Materiel> list;
    if (q.exec())
        while (q.next()) list << fromQuery(q);
    else
        qWarning() << "Materiel::afficher :" << q.lastError().text();
    return list;
}

// ---------- Utilitaires ----------

Materiel Materiel::trouver(int id)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("%1 WHERE ID_MATERIEL = ?").arg(selectMat()));
    q.addBindValue(id);
    if (q.exec() && q.next()) return fromQuery(q);
    return Materiel();
}

bool Materiel::referenceExiste(const QString &reference, int ignoreId)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT COUNT(*) FROM MATERIEL WHERE UPPER(REFERENCE) = UPPER(?) AND ID_MATERIEL <> ?");
    q.addBindValue(reference);
    q.addBindValue(ignoreId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}
