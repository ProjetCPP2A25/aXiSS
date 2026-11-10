#include "garde.h"
#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QSqlQueryModel>
#include <QRegularExpression>

// Table Oracle GARDE (DDL : ID_GARDE, DATE_GARDE, TYPE, HEURE_DEBUT, HEURE_FIN, EFFECTIF, STATUT).
static const char *kColsGarde =
    "ID_GARDE,DATE_GARDE,TYPE,HEURE_DEBUT,HEURE_FIN,EFFECTIF,STATUT";

static QString s_erreur;

// Traduit les erreurs Oracle les plus fréquentes en message compréhensible (identique à employe.cpp)
static QString messageSql(const QSqlQuery &q)
{
    const QString t = q.lastError().text();
    if (t.contains("ORA-02292"))
        return "cette garde est utilisée ailleurs (par exemple dans des affectations ou interventions).";
    if (t.contains("ORA-00001"))
        return "cette valeur existe déjà (identifiant de garde en double).";
    if (t.contains("ORA-02290"))
        return "une valeur est refusée par une contrainte de la base (type, statut ou effectif).";
    if (t.contains("ORA-12899"))
        return "une valeur est trop longue pour sa colonne.";
    if (t.contains("ORA-01843") || t.contains("ORA-01861"))
        return "format de date invalide.";
    return t;
}

static bool lancer(QSqlQuery &q)
{
    if (q.exec()) return true;
    s_erreur = messageSql(q);
    return false;
}

static Garde depuis(const QSqlQuery &q)
{
    Garde g;
    g.id = q.value(0).toString();

    QVariant d = q.value(1);
    if (d.typeId() == QMetaType::QDate)
        g.date = d.toDate();
    else if (d.typeId() == QMetaType::QDateTime)
        g.date = d.toDateTime().date();
    else
        g.date = QDate::fromString(d.toString(), "yyyy-MM-dd");

    g.type = q.value(2).toString();
    g.heureDebut = q.value(3).toString();
    g.heureFin = q.value(4).toString();
    g.effectif = q.value(5).toInt();
    g.statut = q.value(6).toString();
    return g;
}

QString Garde::derniereErreur() { return s_erreur; }

// ============================================================================
// CONSTRUCTEURS
// ============================================================================
Garde::Garde()
    : id(QString()), date(QDate::currentDate()), type(QStringLiteral("Jour")),
      heureDebut(QStringLiteral("07:00")), heureFin(QStringLiteral("19:00")),
      effectif(4), statut(QStringLiteral("Planifiée"))
{
}

Garde::Garde(const QString &id, const QDate &date, const QString &type,
             const QString &heureDebut, const QString &heureFin,
             int effectif, const QString &statut)
    : id(id), date(date), type(type),
      heureDebut(heureDebut), heureFin(heureFin),
      effectif(effectif), statut(statut)
{
}

// Identifiant généré (séquence Oracle SEQ_GARDE ou MAX(ID_GARDE)+1)
QString Garde::prochainId()
{
    QSqlQuery q(Connexion::oracle());
    if (q.exec("SELECT SEQ_GARDE.NEXTVAL FROM DUAL") && q.next()) {
        int v = q.value(0).toInt();
        if (v > 0) return QString::number(v);
    }

    if (q.exec("SELECT COALESCE(MAX(ID_GARDE), 0) + 1 FROM GARDE") && q.next()) {
        return q.value(0).toString();
    }
    s_erreur = q.lastError().text();
    return QStringLiteral("1");
}

// ------------------------------------------------------------------ CREATE
bool Garde::ajouter()
{
    if (id.isEmpty()) id = prochainId();
    if (id.isEmpty()) return false;

    QSqlQuery q(Connexion::oracle());
    q.prepare(QString(
        "INSERT INTO GARDE(%1) VALUES(?, ?, ?, ?, ?, ?, ?)"
    ).arg(kColsGarde));

    q.addBindValue(id.toInt());
    q.addBindValue(date.isValid() ? date : QDate::currentDate());
    q.addBindValue(type);
    q.addBindValue(heureDebut);
    q.addBindValue(heureFin);
    q.addBindValue(effectif);
    q.addBindValue(statut);

    return lancer(q);
}

bool Garde::ajouter_garde(QString *err)
{
    if (id.isEmpty()) id = prochainId();
    if (id.isEmpty()) {
        if (err) *err = QStringLiteral("Impossible de générer un ID");
        return false;
    }

    QSqlQuery q(Connexion::oracle());
    q.prepare(QString(
        "INSERT INTO GARDE(%1) VALUES(?, ?, ?, ?, ?, ?, ?)"
    ).arg(kColsGarde));

    q.addBindValue(id.toInt());
    q.addBindValue(date.isValid() ? date : QDate::currentDate());
    q.addBindValue(type);
    q.addBindValue(heureDebut);
    q.addBindValue(heureFin);
    q.addBindValue(effectif);
    q.addBindValue(statut);

    if (!lancer(q)) {
        if (err) *err = s_erreur;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ UPDATE
bool Garde::modifier() const
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE GARDE SET "
              "DATE_GARDE=?, TYPE=?, HEURE_DEBUT=?, HEURE_FIN=?, "
              "EFFECTIF=?, STATUT=? "
              "WHERE ID_GARDE=?");

    q.addBindValue(date.isValid() ? date : QDate::currentDate());
    q.addBindValue(type);
    q.addBindValue(heureDebut);
    q.addBindValue(heureFin);
    q.addBindValue(effectif);
    q.addBindValue(statut);
    q.addBindValue(id.toInt());

    return lancer(q) && q.numRowsAffected() > 0;
}

bool Garde::modifier_garde(QString *err)
{
    QString cleanId = id;
    cleanId.remove('#');
    cleanId = cleanId.trimmed();

    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE GARDE SET "
              "DATE_GARDE=?, TYPE=?, HEURE_DEBUT=?, HEURE_FIN=?, "
              "EFFECTIF=?, STATUT=? "
              "WHERE ID_GARDE=?");

    q.addBindValue(date.isValid() ? date : QDate::currentDate());
    q.addBindValue(type);
    q.addBindValue(heureDebut);
    q.addBindValue(heureFin);
    q.addBindValue(effectif);
    q.addBindValue(statut);
    q.addBindValue(cleanId.toInt());

    if (!lancer(q)) {
        if (err) *err = s_erreur;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ DELETE
bool Garde::supprimer(const QString &id)
{
    QString cleanId = id;
    cleanId.remove('#');
    cleanId = cleanId.trimmed();
    int numId = cleanId.toInt();

    // Supprime d'abord les liaisons associées dans la table AFFECTE si existantes
    QSqlQuery qAff(Connexion::oracle());
    qAff.prepare("DELETE FROM AFFECTE WHERE ID_GARDE=?");
    qAff.addBindValue(numId);
    qAff.exec();

    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM GARDE WHERE ID_GARDE=?");
    q.addBindValue(numId);

    return lancer(q);
}

bool Garde::supprimer_garde(const QString &id, QString *err)
{
    QString cleanId = id;
    cleanId.remove('#');
    cleanId = cleanId.trimmed();
    int numId = cleanId.toInt();

    // Supprime d'abord les liaisons associées dans la table AFFECTE si existantes
    QSqlQuery qAff(Connexion::oracle());
    qAff.prepare("DELETE FROM AFFECTE WHERE ID_GARDE=?");
    qAff.addBindValue(numId);
    qAff.exec();

    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM GARDE WHERE ID_GARDE=?");
    q.addBindValue(numId);

    if (!lancer(q)) {
        if (err) *err = s_erreur;
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ READ
Garde Garde::trouver(const QString &id)
{
    QString cleanId = id;
    cleanId.remove('#');
    cleanId = cleanId.trimmed();

    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("SELECT %1 FROM GARDE WHERE ID_GARDE=?").arg(kColsGarde));
    q.addBindValue(cleanId.toInt());
    if (q.exec() && q.next()) {
        return depuis(q);
    }
    return Garde();
}

QVector<Garde> Garde::lister()
{
    QSqlQuery q(Connexion::oracle());
    q.exec(QString("SELECT %1 FROM GARDE ORDER BY ID_GARDE ASC").arg(kColsGarde));
    QVector<Garde> r;
    while (q.next()) r.append(depuis(q));
    return r;
}

QSqlQueryModel* Garde::afficher_garde(QString *err)
{
    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery(
        QString("SELECT %1 FROM GARDE ORDER BY ID_GARDE ASC").arg(kColsGarde),
        Connexion::oracle()
    );

    if (model->lastError().isValid()) {
        s_erreur = model->lastError().text();
        if (err) *err = s_erreur;
        delete model;
        return nullptr;
    }

    // En-têtes lisibles pour la vue
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Date");
    model->setHeaderData(2, Qt::Horizontal, "Type");
    model->setHeaderData(3, Qt::Horizontal, "Heure début");
    model->setHeaderData(4, Qt::Horizontal, "Heure fin");
    model->setHeaderData(5, Qt::Horizontal, "Effectif");
    model->setHeaderData(6, Qt::Horizontal, "Statut");

    return model;
}

// ------------------------------------------------------------------ VALIDATIONS
bool Garde::typeValide(const QString &type)
{
    return type.compare("Jour", Qt::CaseInsensitive) == 0 ||
           type.compare("Nuit", Qt::CaseInsensitive) == 0;
}

bool Garde::heureValide(const QString &heure)
{
    static const QRegularExpression re(R"(^([01]?[0-9]|2[0-3]):[0-5][0-9]$)");
    return re.match(heure.trimmed()).hasMatch();
}

bool Garde::effectifValide(int effectif)
{
    return effectif >= 1 && effectif <= 50;
}

bool Garde::existe(const QString &id)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT COUNT(*) FROM GARDE WHERE ID_GARDE=?");
    q.addBindValue(id.toInt());
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
}
