#include "vehicule.h"

#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include "connexion.h"
#include <QVariant>

namespace {

// Colonnes de la table Oracle VEHICULE, dans l'ordre lu par depuisRequete()
const QString COLONNES = QStringLiteral(
    "ID_VEHICULE, IMMATRICULATION, TYPE, KILOMETRAGE, ETAT, DISPONIBILITE, "
    "DATE_DERNIERE_MAINTENANCE, DATE_PROCHAINE_MAINTENANCE, "
    "DATE_EXPIRATION_ASSURANCE, DATE_VISITE_TECHNIQUE");

// Identifiant affiché ("V001") calculé côté Oracle, utilisé pour la recherche
const QString ID_TEXTE = QStringLiteral(
    "('V' || LPAD(ID_VEHICULE, GREATEST(3, LENGTH(ID_VEHICULE)), '0'))");

// Texte ISO (yyyy-MM-dd) -> DATE Oracle ; un texte vide donne NULL
const QString DATE_SQL = QStringLiteral("TO_DATE(?, 'YYYY-MM-DD')");

void setErreur(QString *erreur, const QString &message)
{
    if (erreur) *erreur = message;
}

// Dans Oracle l'identifiant est un nombre (1, 2, 3...), dans l'application il reste "V001"
QString idTexte(int n)
{
    return QString("V%1").arg(n, 3, 10, QChar('0'));
}

int idNombre(const QString &id)
{
    return id.mid(1).toInt();   // "V001" -> 1
}

// Date vide -> NULL en base ; sinon format ISO (yyyy-MM-dd)
QVariant dateSql(const QDate &d)
{
    return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : QVariant(QString());
}

// Oracle renvoie une vraie date (ou un texte selon le pilote) : on gère les deux cas
QDate versDate(const QVariant &valeur)
{
    if (valeur.isNull()) return QDate();
    QDate d = valeur.toDate();
    if (!d.isValid())
        d = QDate::fromString(valeur.toString().left(10), Qt::ISODate);
    return d;
}

Vehicule depuisRequete(const QSqlQuery &q)
{
    Vehicule v;
    v.id             = idTexte(q.value(0).toInt());
    v.plaque         = q.value(1).toString();
    v.type           = q.value(2).toString();
    v.km             = q.value(3).toInt();
    v.etat           = q.value(4).toString();
    v.dispo          = q.value(5).toString();
    v.derniereMaint  = versDate(q.value(6));
    v.prochaineMaint = versDate(q.value(7));
    v.assurance      = versDate(q.value(8));
    v.visiteTech     = versDate(q.value(9));
    return v;
}

void lierChamps(QSqlQuery &q, const Vehicule &v)
{
    q.addBindValue(v.plaque);
    q.addBindValue(v.type);
    q.addBindValue(v.km);
    q.addBindValue(v.etat);
    q.addBindValue(v.dispo);
    q.addBindValue(dateSql(v.derniereMaint));
    q.addBindValue(dateSql(v.prochaineMaint));
    q.addBindValue(dateSql(v.assurance));
    q.addBindValue(dateSql(v.visiteTech));
}

} // namespace

// ------------------------------------------------------------------ valeurs autorisées
const QStringList &Vehicule::types()
{
    static const QStringList l{"Camion-citerne", "Ambulance", "Échelle aérienne", "Fourgon pompe",
                               "Véhicule léger"};
    return l;
}

const QStringList &Vehicule::etats()
{
    static const QStringList l{"Bon", "À surveiller", "Hors service"};
    return l;
}

const QStringList &Vehicule::disponibilites()
{
    static const QStringList l{"Disponible", "En intervention", "En maintenance"};
    return l;
}

// ------------------------------------------------------------------ contrôle de saisie
QString Vehicule::normaliserPlaque(const QString &texte)
{
    static const QRegularExpression re("^(\\d{1,3}) ?TUN ?(\\d{1,4})$",
                                       QRegularExpression::CaseInsensitiveOption);
    const QString t = texte.trimmed();
    const QRegularExpressionMatch m = re.match(t);
    if (!m.hasMatch()) return t;
    return m.captured(1) + " TUN " + m.captured(2);
}

bool Vehicule::valider(QString *erreur) const
{
    static const QRegularExpression re("^\\d{1,3} TUN \\d{1,4}$");

    if (plaque.trimmed().isEmpty()) {
        setErreur(erreur, "L'immatriculation est obligatoire.");
        return false;
    }
    if (!re.match(normaliserPlaque(plaque)).hasMatch()) {
        setErreur(erreur, "Immatriculation invalide : utilisez le format 123 TUN 4567.");
        return false;
    }
    if (!types().contains(type)) {
        setErreur(erreur, "Type de véhicule invalide.");
        return false;
    }
    if (km < 0 || km > 2000000) {
        setErreur(erreur, "Le kilométrage doit être compris entre 0 et 2 000 000.");
        return false;
    }
    if (!etats().contains(etat)) {
        setErreur(erreur, "État invalide.");
        return false;
    }
    if (!disponibilites().contains(dispo)) {
        setErreur(erreur, "Disponibilité invalide.");
        return false;
    }
    if (derniereMaint.isValid() && prochaineMaint.isValid() && prochaineMaint < derniereMaint) {
        setErreur(erreur, "La prochaine maintenance ne peut pas être avant la dernière maintenance.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ outils privés
bool Vehicule::plaqueExiste(const QString &plaque, const QString &sauf)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT COUNT(*) FROM VEHICULE WHERE UPPER(IMMATRICULATION) = UPPER(?) AND ID_VEHICULE <> ?");
    q.addBindValue(plaque);
    q.addBindValue(idNombre(sauf));
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

QString Vehicule::prochainId()
{
    QSqlQuery q(Connexion::oracle());
    int max = 0;
    if (q.exec("SELECT NVL(MAX(ID_VEHICULE), 0) FROM VEHICULE") && q.next())
        max = q.value(0).toInt();
    return idTexte(max + 1);
}

// ------------------------------------------------------------------ CRUD
bool Vehicule::ajouter(QString *erreur)
{
    plaque = normaliserPlaque(plaque);
    if (!valider(erreur)) return false;
    if (plaqueExiste(plaque, QString())) {
        setErreur(erreur, "Cette immatriculation existe déjà.");
        return false;
    }

    const QString nouvelId = prochainId();
    QSqlQuery q(Connexion::oracle());
    q.prepare("INSERT INTO VEHICULE (ID_VEHICULE, IMMATRICULATION, TYPE, KILOMETRAGE, ETAT, DISPONIBILITE, "
              "DATE_DERNIERE_MAINTENANCE, DATE_PROCHAINE_MAINTENANCE, "
              "DATE_EXPIRATION_ASSURANCE, DATE_VISITE_TECHNIQUE) "
              "VALUES (?, ?, ?, ?, ?, ?, " + DATE_SQL + ", " + DATE_SQL + ", " + DATE_SQL + ", " + DATE_SQL + ")");
    q.addBindValue(idNombre(nouvelId));
    lierChamps(q, *this);
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    id = nouvelId;
    return true;
}

bool Vehicule::afficher(const QString &id, Vehicule &resultat, QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT " + COLONNES + " FROM VEHICULE WHERE ID_VEHICULE = ?");
    q.addBindValue(idNombre(id));
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    if (!q.next()) {
        setErreur(erreur, "Véhicule introuvable.");
        return false;
    }
    resultat = depuisRequete(q);
    return true;
}

bool Vehicule::modifier(QString *erreur) const
{
    Vehicule copie = *this;
    copie.plaque = normaliserPlaque(plaque);
    if (!copie.valider(erreur)) return false;
    if (plaqueExiste(copie.plaque, id)) {
        setErreur(erreur, "Cette immatriculation existe déjà.");
        return false;
    }

    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE VEHICULE SET IMMATRICULATION=?, TYPE=?, KILOMETRAGE=?, ETAT=?, DISPONIBILITE=?, "
              "DATE_DERNIERE_MAINTENANCE=" + DATE_SQL + ", "
                           "DATE_PROCHAINE_MAINTENANCE=" + DATE_SQL + ", "
                           "DATE_EXPIRATION_ASSURANCE=" + DATE_SQL + ", "
                           "DATE_VISITE_TECHNIQUE=" + DATE_SQL + " "
                           "WHERE ID_VEHICULE=?");
    lierChamps(q, copie);
    q.addBindValue(idNombre(id));
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() == 0) {
        setErreur(erreur, "Véhicule introuvable.");
        return false;
    }
    return true;
}

bool Vehicule::supprimer(const QString &id, QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM VEHICULE WHERE ID_VEHICULE = ?");
    q.addBindValue(idNombre(id));
    if (!q.exec()) {
        const QString texte = q.lastError().text();
        if (texte.contains("ORA-02292"))   // clé étrangère : le véhicule sert dans une intervention
            setErreur(erreur, "Ce véhicule est utilisé dans une intervention : suppression impossible.");
        else
            setErreur(erreur, texte);
        return false;
    }
    if (q.numRowsAffected() == 0) {
        setErreur(erreur, "Véhicule introuvable.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ métier
QVector<Vehicule> Vehicule::lister(const QString &recherche, int tri, QString *erreur)
{
    QString ordre;
    switch (tri) {
    case 0:  ordre = "KILOMETRAGE"; break;
    case 1:  ordre = "ID_VEHICULE"; break;
    case 2:  ordre = "IMMATRICULATION"; break;
    default: ordre = "DATE_PROCHAINE_MAINTENANCE"; break;
    }

    // Attention : en Oracle une chaîne vide ('') équivaut à NULL.
    // On n'ajoute donc le filtre que lorsqu'il y a vraiment une recherche.
    const bool filtrer = !recherche.isEmpty();

    QString sql = "SELECT " + COLONNES + " FROM VEHICULE";
    if (filtrer)
        sql += " WHERE " + ID_TEXTE + " LIKE UPPER(?) OR UPPER(IMMATRICULATION) LIKE UPPER(?) "
                                      "OR UPPER(TYPE) LIKE UPPER(?)";
    sql += " ORDER BY " + ordre + ", ID_VEHICULE";

    QSqlQuery q(Connexion::oracle());
    q.prepare(sql);
    if (filtrer) {
        const QString motif = "%" + recherche + "%";
        q.addBindValue(motif);
        q.addBindValue(motif);
        q.addBindValue(motif);
    }

    QVector<Vehicule> liste;
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return liste;
    }
    while (q.next()) liste.push_back(depuisRequete(q));
    return liste;
}

QVector<Vehicule> Vehicule::maintenanceRequise(QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT " + COLONNES + " FROM VEHICULE "
                                     "WHERE DATE_PROCHAINE_MAINTENANCE IS NOT NULL AND DATE_PROCHAINE_MAINTENANCE <= " + DATE_SQL + " "
                           "ORDER BY DATE_PROCHAINE_MAINTENANCE");
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));

    QVector<Vehicule> liste;
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return liste;
    }
    while (q.next()) liste.push_back(depuisRequete(q));
    return liste;
}

QMap<QString, int> Vehicule::statistiques(bool parEtat, QString *erreur)
{
    const QString colonne = parEtat ? "ETAT" : "DISPONIBILITE";   // valeurs fixes : pas d'injection possible
    QMap<QString, int> resultat;
    QSqlQuery q(Connexion::oracle());
    if (!q.exec("SELECT " + colonne + ", COUNT(*) FROM VEHICULE GROUP BY " + colonne)) {
        setErreur(erreur, q.lastError().text());
        return resultat;
    }
    while (q.next()) resultat[q.value(0).toString()] = q.value(1).toInt();
    return resultat;
}