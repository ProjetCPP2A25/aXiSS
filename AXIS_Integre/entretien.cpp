#include "entretien.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include "connexion.h"
#include <QVariant>

namespace {

void setErreur(QString *erreur, const QString &message)
{
    if (erreur) *erreur = message;
}

QVariant dateSql(const QDate &date)
{
    return date.isValid() ? QVariant(date.toString(Qt::ISODate)) : QVariant(QString());
}

QDate versDate(const QVariant &valeur)
{
    if (valeur.isNull()) return {};
    QDate date = valeur.toDate();
    if (!date.isValid()) date = QDate::fromString(valeur.toString().left(10), Qt::ISODate);
    return date;
}

bool ignorerObjetExistant(const QSqlError &erreur)
{
    return erreur.text().contains("ORA-00955");
}

} // namespace

bool GestionEntretiens::initialiser(QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    const QString creationTable =
        "CREATE TABLE ENTRETIEN_VEHICULE ("
        "ID_ENTRETIEN NUMBER(10) PRIMARY KEY, "
        "ID_VEHICULE NUMBER(10) NOT NULL, "
        "TYPE_ENTRETIEN VARCHAR2(60) NOT NULL, "
        "DESCRIPTION VARCHAR2(500), DATE_PREVUE DATE NOT NULL, KM_PREVU NUMBER(10) DEFAULT 0, "
        "STATUT VARCHAR2(20) DEFAULT 'Planifié' NOT NULL, GARAGE VARCHAR2(120), "
        "DATE_REALISEE DATE, KM_REALISE NUMBER(10) DEFAULT 0, COUT NUMBER(12,2) DEFAULT 0, "
        "PROCHAINE_DATE DATE, PROCHAIN_KM NUMBER(10) DEFAULT 0, "
        "CONSTRAINT FK_ENT_VEHICULE FOREIGN KEY (ID_VEHICULE) REFERENCES VEHICULE(ID_VEHICULE))";
    if (!q.exec(creationTable) && !ignorerObjetExistant(q.lastError())) {
        setErreur(erreur, "Impossible de créer la table d'entretiens : " + q.lastError().text());
        return false;
    }

    if (!q.exec("CREATE SEQUENCE SEQ_ENTRETIEN_VEHICULE START WITH 1 INCREMENT BY 1") &&
        !ignorerObjetExistant(q.lastError())) {
        setErreur(erreur, "Impossible de créer la séquence des entretiens : " + q.lastError().text());
        return false;
    }
    return true;
}

QVector<Entretien> GestionEntretiens::lister(const QString &recherche, QString *erreur)
{
    QVector<Entretien> resultat;
    const bool filtrer = !recherche.trimmed().isEmpty();
    QString sql =
        "SELECT E.ID_ENTRETIEN, E.ID_VEHICULE, V.IMMATRICULATION, E.TYPE_ENTRETIEN, "
        "E.DESCRIPTION, E.GARAGE, E.STATUT, E.DATE_PREVUE, E.DATE_REALISEE, "
        "E.KM_PREVU, E.KM_REALISE, E.COUT, E.PROCHAINE_DATE, E.PROCHAIN_KM, V.KILOMETRAGE "
        "FROM ENTRETIEN_VEHICULE E JOIN VEHICULE V ON V.ID_VEHICULE=E.ID_VEHICULE ";
    if (filtrer)
        sql += "WHERE UPPER(V.IMMATRICULATION) LIKE UPPER(?) OR UPPER(E.TYPE_ENTRETIEN) LIKE UPPER(?) ";
    sql += "ORDER BY CASE E.STATUT WHEN 'Planifié' THEN 0 WHEN 'En retard' THEN 1 ELSE 2 END, E.DATE_PREVUE";

    QSqlQuery q(Connexion::oracle());
    q.prepare(sql);
    if (filtrer) {
        const QString motif = "%" + recherche.trimmed() + "%";
        q.addBindValue(motif);
        q.addBindValue(motif);
    }
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return resultat;
    }

    while (q.next()) {
        Entretien e;
        e.id = q.value(0).toString();
        e.vehiculeId = q.value(1).toString();
        e.plaque = q.value(2).toString();
        e.type = q.value(3).toString();
        e.description = q.value(4).toString();
        e.garage = q.value(5).toString();
        e.statut = q.value(6).toString();
        e.datePrevue = versDate(q.value(7));
        e.dateRealisee = versDate(q.value(8));
        e.kmPrevu = q.value(9).toInt();
        e.kmRealise = q.value(10).toInt();
        e.cout = q.value(11).toDouble();
        e.prochaineDate = versDate(q.value(12));
        e.prochainKm = q.value(13).toInt();
        e.kmVehicule = q.value(14).toInt();
        resultat.push_back(e);
    }
    return resultat;
}

bool GestionEntretiens::vehicules(QVector<QPair<QString, QString>> &resultat, QString *erreur)
{
    resultat.clear();
    QSqlQuery q(Connexion::oracle());
    if (!q.exec("SELECT ID_VEHICULE, IMMATRICULATION FROM VEHICULE ORDER BY IMMATRICULATION")) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    while (q.next()) resultat.push_back(qMakePair(q.value(0).toString(), q.value(1).toString()));
    return true;
}

bool GestionEntretiens::ajouter(const Entretien &e, QString *erreur)
{
    if (e.vehiculeId.isEmpty() || e.type.trimmed().isEmpty() || !e.datePrevue.isValid()) {
        setErreur(erreur, "Le véhicule, le type et la date prévue sont obligatoires.");
        return false;
    }
    if (e.kmPrevu < 0 || e.kmPrevu > 2000000) {
        setErreur(erreur, "Le kilométrage prévu doit être entre 0 et 2 000 000.");
        return false;
    }

    QSqlQuery idQuery(Connexion::oracle());
    if (!idQuery.exec("SELECT SEQ_ENTRETIEN_VEHICULE.NEXTVAL FROM DUAL") || !idQuery.next()) {
        setErreur(erreur, idQuery.lastError().text());
        return false;
    }

    QSqlQuery q(Connexion::oracle());
    q.prepare("INSERT INTO ENTRETIEN_VEHICULE (ID_ENTRETIEN, ID_VEHICULE, TYPE_ENTRETIEN, DESCRIPTION, "
              "DATE_PREVUE, KM_PREVU, STATUT, GARAGE, COUT) "
              "VALUES (?, ?, ?, ?, TO_DATE(?, 'YYYY-MM-DD'), ?, 'Planifié', ?, 0)");
    q.addBindValue(idQuery.value(0));
    q.addBindValue(e.vehiculeId.toInt());
    q.addBindValue(e.type.trimmed());
    q.addBindValue(e.description.trimmed());
    q.addBindValue(e.datePrevue.toString(Qt::ISODate));
    q.addBindValue(e.kmPrevu);
    q.addBindValue(e.garage.trimmed());
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    return true;
}

bool GestionEntretiens::terminer(const Entretien &e, QString *erreur)
{
    if (!e.dateRealisee.isValid() || e.kmRealise < 0 || e.kmRealise > 2000000 || e.cout < 0) {
        setErreur(erreur, "Vérifiez la date, le kilométrage et le coût de l'entretien.");
        return false;
    }
    QSqlDatabase db = Connexion::oracle();
    if (!db.transaction()) {
        setErreur(erreur, "Impossible de démarrer la transaction : " + db.lastError().text());
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE ENTRETIEN_VEHICULE SET STATUT='Terminé', DATE_REALISEE=TO_DATE(?, 'YYYY-MM-DD'), "
              "KM_REALISE=?, COUT=?, PROCHAINE_DATE=TO_DATE(?, 'YYYY-MM-DD'), PROCHAIN_KM=? "
              "WHERE ID_ENTRETIEN=? AND STATUT='Planifié'");
    q.addBindValue(e.dateRealisee.toString(Qt::ISODate));
    q.addBindValue(e.kmRealise);
    q.addBindValue(e.cout);
    q.addBindValue(dateSql(e.prochaineDate));
    q.addBindValue(e.prochainKm);
    q.addBindValue(e.id.toInt());
    if (!q.exec() || q.numRowsAffected() == 0) {
        const QString message = q.lastError().isValid() ? q.lastError().text()
                                                        : "Cet entretien n'est plus planifié.";
        db.rollback();
        setErreur(erreur, message);
        return false;
    }

    QSqlQuery majVehicule(db);
    majVehicule.prepare("UPDATE VEHICULE SET DATE_DERNIERE_MAINTENANCE=TO_DATE(?, 'YYYY-MM-DD'), "
                        "DATE_PROCHAINE_MAINTENANCE=TO_DATE(?, 'YYYY-MM-DD'), "
                        "KILOMETRAGE=CASE WHEN KILOMETRAGE < ? THEN ? ELSE KILOMETRAGE END "
                        "WHERE ID_VEHICULE=?");
    majVehicule.addBindValue(e.dateRealisee.toString(Qt::ISODate));
    majVehicule.addBindValue(dateSql(e.prochaineDate));
    majVehicule.addBindValue(e.kmRealise);
    majVehicule.addBindValue(e.kmRealise);
    majVehicule.addBindValue(e.vehiculeId.toInt());
    if (!majVehicule.exec() || majVehicule.numRowsAffected() == 0) {
        const QString message = majVehicule.lastError().isValid() ? majVehicule.lastError().text()
                                                                  : "Véhicule associé introuvable.";
        db.rollback();
        setErreur(erreur, message);
        return false;
    }
    if (!db.commit()) {
        setErreur(erreur, "Impossible de valider l'entretien : " + db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

bool GestionEntretiens::supprimer(const QString &id, QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM ENTRETIEN_VEHICULE WHERE ID_ENTRETIEN=?");
    q.addBindValue(id.toInt());
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() == 0) {
        setErreur(erreur, "Entretien introuvable.");
        return false;
    }
    return true;
}
