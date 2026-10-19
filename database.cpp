#include "database.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

static const char *ISO = "yyyy-MM-dd";

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
    m.dateAcquisition = QDate::fromString(q.value(8).toString(), ISO);
    m.emplacement = q.value(9).toString();
    return m;
}

static const char *SELECT_MAT =
    "SELECT id, reference, designation, categorie, quantite, seuil, etat, disponibilite, "
    "date_acquisition, emplacement FROM materiels";

bool Database::open(QString *error)
{
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.isValid() || !db.isOpen()) {
        if (error)
            *error = "La base de données du projet n'est pas ouverte.";
        return false;
    }

    QSqlQuery q(db);

    q.exec("PRAGMA foreign_keys = ON");

    q.exec("CREATE TABLE IF NOT EXISTS materiels ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "reference TEXT NOT NULL UNIQUE,"
           "designation TEXT NOT NULL,"
           "categorie TEXT NOT NULL,"
           "quantite INTEGER NOT NULL DEFAULT 0,"
           "seuil INTEGER NOT NULL DEFAULT 0,"
           "etat TEXT NOT NULL,"
           "disponibilite TEXT NOT NULL,"
           "date_acquisition TEXT NOT NULL,"
           "emplacement TEXT)");

    q.exec("CREATE TABLE IF NOT EXISTS mouvements ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "date TEXT NOT NULL,"
           "type TEXT NOT NULL,"
           "materiel_id INTEGER NOT NULL REFERENCES materiels(id) ON DELETE CASCADE,"
           "quantite INTEGER NOT NULL,"
           "motif TEXT,"
           "employe_id TEXT REFERENCES employes(id) ON DELETE SET NULL)");

    return true;
}

QList<Materiel> Database::materiels(const QString &search, const QString &orderColumn)
{
    // Liste blanche pour éviter toute injection via le nom de colonne
    static const QStringList allowed = {"reference", "designation", "categorie", "quantite", "date_acquisition"};
    const QString col = allowed.contains(orderColumn) ? orderColumn : "reference";

    QSqlQuery q;
    q.prepare(QString("%1 WHERE reference LIKE :s OR designation LIKE :s ORDER BY %2 COLLATE NOCASE")
                  .arg(SELECT_MAT, col));
    q.bindValue(":s", "%" + search + "%");
    QList<Materiel> list;
    if (q.exec())
        while (q.next()) list << fromQuery(q);
    return list;
}

Materiel Database::materiel(int id)
{
    QSqlQuery q;
    q.prepare(QString("%1 WHERE id = ?").arg(SELECT_MAT));
    q.addBindValue(id);
    if (q.exec() && q.next()) return fromQuery(q);
    return Materiel();
}

bool Database::referenceExists(const QString &reference, int ignoreId)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM materiels WHERE UPPER(reference) = UPPER(?) AND id <> ?");
    q.addBindValue(reference);
    q.addBindValue(ignoreId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool Database::addMateriel(const Materiel &m, QString *error)
{
    QSqlQuery q;

    QString sql =
        "INSERT INTO materiels "
        "(reference, designation, categorie, quantite, seuil, etat, "
        "disponibilite, date_acquisition, emplacement) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (!q.prepare(sql)) {
        if (error)
            *error = "Prepare error : " + q.lastError().text();

        qDebug() << "PREPARE ERROR:" << q.lastError().text();
        return false;
    }

    q.addBindValue(m.reference);
    q.addBindValue(m.designation);
    q.addBindValue(m.categorie);
    q.addBindValue(m.quantite);
    q.addBindValue(m.seuil);
    q.addBindValue(m.etat);
    q.addBindValue(m.disponibilite);
    q.addBindValue(m.dateAcquisition.toString(Qt::ISODate));
    q.addBindValue(m.emplacement);

    qDebug() << "SQL:" << q.lastQuery();
    qDebug() << "VALUES COUNT:" << q.boundValues().size();
    qDebug() << "VALUES:" << q.boundValues();

    if (!q.exec()) {
        if (error)
            *error = "Execute error : " + q.lastError().text();

        qDebug() << "EXEC ERROR:" << q.lastError().text();
        return false;
    }

    return true;
}
bool Database::updateMateriel(const Materiel &m, QString *error)
{
    QSqlQuery q;

    q.prepare(
        "UPDATE materiels SET "
        "reference = :reference, "
        "designation = :designation, "
        "categorie = :categorie, "
        "quantite = :quantite, "
        "seuil = :seuil, "
        "etat = :etat, "
        "disponibilite = :disponibilite, "
        "date_acquisition = :date_acquisition, "
        "emplacement = :emplacement "
        "WHERE id = :id"
        );

    q.bindValue(":reference", m.reference);
    q.bindValue(":designation", m.designation);
    q.bindValue(":categorie", m.categorie);
    q.bindValue(":quantite", m.quantite);
    q.bindValue(":seuil", m.seuil);
    q.bindValue(":etat", m.etat);
    q.bindValue(":disponibilite", m.disponibilite);
    q.bindValue(":date_acquisition", m.dateAcquisition.toString(Qt::ISODate));
    q.bindValue(":emplacement", m.emplacement);
    q.bindValue(":id", m.id);

    if (!q.exec()) {
        if (error)
            *error = q.lastError().text();

        qDebug() << "Erreur UPDATE materiel :" << q.lastError().text();
        return false;
    }

    return true;
}

bool Database::deleteMateriel(int id, QString *error)
{
    QSqlQuery q;
    q.exec("PRAGMA foreign_keys = ON");
    q.prepare("DELETE FROM materiels WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

QList<DatabaseEmploye> Database::employes()
{
    QList<DatabaseEmploye> list;

    QSqlQuery q("SELECT id, TRIM(prenom || ' ' || nom) FROM employes ORDER BY nom, prenom");

    while (q.next()) {
        DatabaseEmploye e;
        e.id = q.value(0).toInt();
        e.nom = q.value(1).toString();
        list << e;
    }

    return list;
}

QList<Mouvement> Database::mouvements(int limit)
{
    QList<Mouvement> list;
    QSqlQuery q;
    q.prepare("SELECT mv.id, mv.date, mv.type, mv.materiel_id, m.designation, mv.quantite, mv.motif, "
              "IFNULL(e.id,0), IFNULL(TRIM(e.prenom || ' ' || e.nom),'') "
              "FROM mouvements mv JOIN materiels m ON m.id = mv.materiel_id "
              "LEFT JOIN employes e ON e.id = mv.employe_id "
              "ORDER BY mv.date DESC, mv.id DESC LIMIT ?");
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            Mouvement m;
            m.id = q.value(0).toInt();
            m.date = QDate::fromString(q.value(1).toString(), ISO);
            m.type = q.value(2).toString();
            m.materielId = q.value(3).toInt();
            m.materielNom = q.value(4).toString();
            m.quantite = q.value(5).toInt();
            m.motif = q.value(6).toString();
            m.employeId = q.value(7).toInt();
            m.employeNom = q.value(8).toString();
            list << m;
        }
    }
    return list;
}

bool Database::addMouvement(const Mouvement &m, QString *error)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
    q.prepare("SELECT quantite FROM materiels WHERE id = ?");
    q.addBindValue(m.materielId);
    if (!q.exec() || !q.next()) {
        db.rollback();
        if (error) *error = "Matériel introuvable.";
        return false;
    }
    const int stock = q.value(0).toInt();
    const int delta = (m.type == "Entrée") ? m.quantite : -m.quantite;
    if (stock + delta < 0) {
        db.rollback();
        if (error) *error = QString("Stock insuffisant (disponible : %1).").arg(stock);
        return false;
    }

    q.prepare("UPDATE materiels SET quantite = quantite + ? WHERE id = ?");
    q.addBindValue(delta);
    q.addBindValue(m.materielId);
    bool ok = q.exec();

    if (ok) {
        q.prepare("INSERT INTO mouvements(date, type, materiel_id, quantite, motif, employe_id) "
                  "VALUES (?,?,?,?,?,?)");
        q.addBindValue(m.date.toString(ISO));
        q.addBindValue(m.type);
        q.addBindValue(m.materielId);
        q.addBindValue(m.quantite);
        q.addBindValue(m.motif);
        q.addBindValue(m.employeId > 0 ? QVariant(m.employeId) : QVariant());
        ok = q.exec();
    }
    if (!ok) {
        db.rollback();
        if (error) *error = q.lastError().text();
        return false;
    }
    return db.commit();
}
