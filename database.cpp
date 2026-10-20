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
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dir + "/axis.db");
    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    // Mode autonome uniquement : table employes minimale, même structure que celle d'AXIS
    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS employes (id TEXT PRIMARY KEY, nom TEXT NOT NULL)");
    q.exec("SELECT COUNT(*) FROM employes");
    if (q.next() && q.value(0).toInt() == 0) {
        q.exec("INSERT INTO employes(id, nom) VALUES "
               "('E001','Sami Gharbi'),('E002','Rim Trabelsi'),('E003','Karim Majri'),"
               "('E004','Leila Mansouri'),('E006','Youssef Ben Salah')");
    }
    return init(error);
}

bool Database::init(QString *error)
{
    QSqlDatabase db = QSqlDatabase::database();   // connexion par défaut (déjà ouverte)
    if (!db.isOpen()) {
        if (error) *error = "Aucune connexion ouverte : appelez Connexion::ouvrir() avant Database::init().";
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

    // Jeu de données de démonstration (identique à la maquette)
    q.exec("SELECT COUNT(*) FROM materiels");
    if (q.next() && q.value(0).toInt() == 0) {
        q.exec("INSERT INTO materiels(id, reference, designation, categorie, quantite, seuil, etat, "
               "disponibilite, date_acquisition, emplacement) VALUES "
               "(1,'MAT-0001','Tuyau d''incendie 40 m','Lutte incendie',24,10,'Bon','Disponible','2024-03-12','Rayon A'),"
               "(2,'MAT-0002','Extincteur CO2','Lutte incendie',6,8,'Bon','Disponible','2024-06-05','Rayon B'),"
               "(3,'MAT-0003','Masque respiratoire','Protection',15,10,'Bon','Disponible','2025-01-20','Rayon C'),"
               "(4,'MAT-0004','Trousse de secours','Secours',3,5,'Bon','Disponible','2025-09-14','Rayon D')");
        // L'employé est cherché par son nom (NULL s'il n'existe pas)
        q.exec("INSERT INTO mouvements(date, type, materiel_id, quantite, motif, employe_id) VALUES "
               "('2026-09-29','Sortie',2,2,'Intervention',(SELECT id FROM employes WHERE nom IN ('Karim Majri','Majri') LIMIT 1)),"
               "('2026-09-25','Entrée',1,10,'Réapprovisionnement',(SELECT id FROM employes WHERE nom IN ('Sami Gharbi','Gharbi') LIMIT 1))");
    }
    return true;
}

QList<Materiel> Database::materiels(const QString &search, const QString &orderColumn)
{
    // Liste blanche pour éviter toute injection via le nom de colonne
    static const QStringList allowed = {"reference", "designation", "categorie", "quantite", "date_acquisition", "etat"};
    const QString col = allowed.contains(orderColumn) ? orderColumn : "reference";

    QSqlQuery q;
    // Tri par état : ordre logique Bon, Moyen, Mauvais, Hors service (puis par référence)
    const QString orderBy = (col == "etat")
        ? QString("CASE etat WHEN 'Bon' THEN 1 WHEN 'Moyen' THEN 2 WHEN 'Mauvais' THEN 3 ELSE 4 END, reference COLLATE NOCASE")
        : QString("%1 COLLATE NOCASE").arg(col);
    q.prepare(QString("%1 WHERE reference LIKE :s OR designation LIKE :s ORDER BY %2")
                  .arg(SELECT_MAT, orderBy));
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
    q.prepare("INSERT INTO materiels(reference, designation, categorie, quantite, seuil, etat, "
              "disponibilite, date_acquisition, emplacement) VALUES (?,?,?,?,?,?,?,?,?)");
    q.addBindValue(m.reference);
    q.addBindValue(m.designation);
    q.addBindValue(m.categorie);
    q.addBindValue(m.quantite);
    q.addBindValue(m.seuil);
    q.addBindValue(m.etat);
    q.addBindValue(m.disponibilite);
    q.addBindValue(m.dateAcquisition.toString(ISO));
    q.addBindValue(m.emplacement);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateMateriel(const Materiel &m, QString *error)
{
    QSqlQuery q;
    q.prepare("UPDATE materiels SET reference=?, designation=?, categorie=?, quantite=?, seuil=?, "
              "etat=?, disponibilite=?, date_acquisition=?, emplacement=? WHERE id=?");
    q.addBindValue(m.reference);
    q.addBindValue(m.designation);
    q.addBindValue(m.categorie);
    q.addBindValue(m.quantite);
    q.addBindValue(m.seuil);
    q.addBindValue(m.etat);
    q.addBindValue(m.disponibilite);
    q.addBindValue(m.dateAcquisition.toString(ISO));
    q.addBindValue(m.emplacement);
    q.addBindValue(m.id);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
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

QList<EmployeLite> Database::employes()
{
    QList<EmployeLite> list;
    QSqlQuery q("SELECT id, nom FROM employes ORDER BY nom");
    while (q.next()) list << EmployeLite{q.value(0).toString(), q.value(1).toString()};
    return list;
}

QList<Mouvement> Database::mouvements(int limit)
{
    QList<Mouvement> list;
    QSqlQuery q;
    q.prepare("SELECT mv.id, mv.date, mv.type, mv.materiel_id, m.designation, mv.quantite, mv.motif, "
              "IFNULL(e.id,''), IFNULL(e.nom,'') "
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
            m.employeId = q.value(7).toString();
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
        q.addBindValue(m.employeId.isEmpty() ? QVariant() : QVariant(m.employeId));
        ok = q.exec();
    }
    if (!ok) {
        db.rollback();
        if (error) *error = q.lastError().text();
        return false;
    }
    return db.commit();
}
