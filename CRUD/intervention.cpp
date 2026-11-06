#include "intervention.h"
#include "connection.h"

#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

#include <QDebug>
#include <QVariant>

namespace {

// "45 min", "1 h 15", "2 h"
QString formatDuration(qint64 minutes)
{
    if (minutes < 60)
        return QStringLiteral("%1 min").arg(minutes);
    const qint64 h = minutes / 60;
    const qint64 m = minutes % 60;
    return m == 0 ? QStringLiteral("%1 h").arg(h)
                  : QStringLiteral("%1 h %2").arg(h).arg(m, 2, 10, QChar('0'));
}

// Toutes les requêtes SQL de l'application sont écrites dans cette classe (architecture modèle-vue).
// Elles sont toutes préparées (prepare + bindValue) : les valeurs saisies ne sont jamais concaténées
// dans le texte SQL, ce qui protège la base contre les injections SQL.
const QString kColonnes = QStringLiteral(
    "ID_INTERVENTION, TYPE, LIEU, GRAVITE, "
    "DATE_RECEPTION_APPEL, HEURE_RECEPTION_APPEL, "
    "DATE_FIN_INTERVENTION, HEURE_FIN_INTERVENTION, "
    "ETAT, DUREE, COORDONNEES_GPS, ID_GARDE"
    );

// La connexion unique (patron Singleton, voir connection.h) doit avoir été ouverte au démarrage.
bool baseOuverte(QString *erreur)
{
    if (Connection::instance().estOuverte())
        return true;
    if (erreur)
        *erreur = QStringLiteral("La connexion à la base de données n'est pas établie.");
    return false;
}

void signaler(QString *erreur, const QString &contexte, const QSqlQuery &query)
{
    if (erreur)
        *erreur = contexte + QStringLiteral("\n") + query.lastError().text();
}

// Date absente (intervention en cours) -> NULL en base
QVariant dateOuNull(const QDateTime &d)
{
    return d.isValid() ? QVariant(d) : QVariant(QMetaType::fromType<QDateTime>());
}

// Ligne de résultat (colonnes dans l'ordre de kColonnes) -> objet Intervention
Intervention depuisLigne(const QSqlQuery &q)
{
    Intervention i;
    i.id = q.value(0).toInt();
    i.type = q.value(1).toString();
    i.lieu = q.value(2).toString();
    i.gravite = q.value(3).toString();
    i.etat = q.value(4).toString();
    i.lat = q.value(5).toDouble();
    i.lon = q.value(6).toDouble();
    i.reception = q.value(7).toDateTime();
    i.fin = q.value(8).isNull() ? QDateTime() : q.value(8).toDateTime();
    return i;
}

// Lit toutes les interventions de la table (requête préparée, sans paramètre)
QVector<Intervention> lireToutes(QString *erreur)
{
    QVector<Intervention> lignes;
    if (!baseOuverte(erreur))
        return lignes;

    QSqlQuery query(Connection::instance().database());
    query.setForwardOnly(true);
    query.prepare(QStringLiteral("SELECT ") + kColonnes + QStringLiteral(" FROM INTERVENTION ORDER BY ID_INTERVENTION"));
    if (!query.exec()) {
        signaler(erreur, QStringLiteral("Lecture des interventions impossible."), query);
        return {};
    }
    while (query.next())
        lignes.push_back(depuisLigne(query));
    return lignes;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Valeurs calculées
// ---------------------------------------------------------------------------------------------

QString Intervention::idText() const
{
    return QStringLiteral("I%1").arg(id, 3, 10, QChar('0'));
}

QString Intervention::gps() const
{
    return QString::number(lat, 'f', 4) + QStringLiteral(", ") + QString::number(lon, 'f', 4);
}

qint64 Intervention::dureeMinutes() const
{
    const QDateTime fin_ = fin.isValid() ? fin : QDateTime::currentDateTime();
    return std::max<qint64>(0, reception.secsTo(fin_) / 60);
}

QString Intervention::dureeTexte() const
{
    return formatDuration(dureeMinutes());
}

int Intervention::graviteRang() const
{
    if (gravite == Grav::Elevee)  return 0;
    if (gravite == Grav::Moyenne) return 1;
    return 2;
}

bool Intervention::valider(QString *erreur) const
{
    auto refuser = [erreur](const QString &message) {
        if (erreur)
            *erreur = message;
        return false;
    };

    if (type.trimmed().isEmpty())
        return refuser(QStringLiteral("Veuillez choisir le type de l'intervention."));
    if (lieu.trimmed().isEmpty())
        return refuser(QStringLiteral("Veuillez saisir le lieu de l'intervention."));
    if (lieu.size() > 100)
        return refuser(QStringLiteral("Le lieu ne doit pas dépasser 100 caractères."));
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0)
        return refuser(QStringLiteral("Coordonnées GPS hors limites."));
    if (!reception.isValid())
        return refuser(QStringLiteral("La date de réception de l'alerte est obligatoire."));
    if (etat == Etat::Terminee) {
        if (!fin.isValid())
            return refuser(QStringLiteral("La date de fin est obligatoire pour une intervention terminée."));
        if (fin < reception)
            return refuser(QStringLiteral("La date de fin doit être postérieure à la réception de l'alerte."));
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// CRUD (table INTERVENTION, requêtes préparées)
// ---------------------------------------------------------------------------------------------

bool Intervention::ajouter(QString *erreur)
{
    // Contrôle de saisie dans le code C++ (les contraintes de la table le répètent côté base)
    if (!valider(erreur) || !baseOuverte(erreur))
        return false;

    // Prochain ID_INTERVENTION_INTERVENTIONentifiant
    QSqlQuery suivant(Connection::instance().database());
    suivant.prepare(QStringLiteral("SELECT COALESCE(MAX(ID_INTERVENTION), 0) + 1 FROM INTERVENTION"));
    if (!suivant.exec() || !suivant.next()) {
        signaler(erreur, QStringLiteral("Ajout impossible : ID_INTERVENTIONentifiant non attribué."), suivant);
        return false;
    }
    const int nouvelID = suivant.value(0).toInt();


    QSqlQuery query(Connection::instance().database());

    if (!query.prepare(QStringLiteral(
            "INSERT INTO INTERVENTION ("
            "ID_INTERVENTION, TYPE, LIEU, GRAVITE, "
            "DATE_RECEPTION_APPEL, HEURE_RECEPTION_APPEL, "
            "ETAT, DUREE, COORDONNEES_GPS, ID_GARDE"
            ") VALUES ("
            "?, ?, ?, ?, "
            "TO_DATE(?, 'YYYY-MM-DD'), ?, "
            "?, ?, ?, ?)"
            ))) {
        qDebug() << "ERREUR PREPARE :"
                 << query.lastError().text();

        if (erreur)
            *erreur = query.lastError().text();

        return false;
    }

    // Parametres dans le meme ordre que les ?
    query.addBindValue(nouvelID);
    query.addBindValue(type);
    query.addBindValue(lieu);
    query.addBindValue(gravite);

    query.addBindValue(
        reception.date().toString("yyyy-MM-dd")
        );

    query.addBindValue(
        reception.time().toString("HH:mm")
        );

    query.addBindValue(etat);
    query.addBindValue(static_cast<int>(dureeMinutes()));
    query.addBindValue(gps());
    query.addBindValue(1); // ID_GARDE existant

    qDebug() << "===== PARAMETRES INSERT =====";

    const auto noms = query.boundValueNames();

    for (const QString &nom : noms) {
        QVariant valeur = query.boundValue(nom);

        qDebug() << nom
                 << "Valeur :" << valeur
                 << "Type :" << valeur.metaType().name()
                 << "NULL :" << valeur.isNull();
    }

    qDebug() << "============================";
    if (!query.exec()) {
        qDebug() << "REQUETE SQL :" << query.lastQuery();
        qDebug() << "ERREUR SQL :" << query.lastError().text();
        qDebug() << "ERREUR NATIVE :" << query.lastError().nativeErrorCode();

        signaler(erreur, QStringLiteral("Ajout impossible."), query);
        return false;
    }
    id = nouvelID;
    return true;
}

bool Intervention::afficher(int ID_INTERVENTION, Intervention &resultat, QString *erreur)
{
    if (!baseOuverte(erreur))
        return false;

    QSqlQuery query(Connection::instance().database());
    query.prepare(QStringLiteral("SELECT ") + kColonnes + QStringLiteral(" FROM INTERVENTION WHERE ID_INTERVENTION = :ID_INTERVENTION"));
    query.bindValue(QStringLiteral(":ID_INTERVENTION"), ID_INTERVENTION);
    if (!query.exec()) {
        signaler(erreur, QStringLiteral("Lecture de l'intervention impossible."), query);
        return false;
    }
    if (query.next()) {
        resultat = depuisLigne(query);
        return true;
    }
    if (erreur)
        *erreur = QStringLiteral("L'intervention demandée n'existe plus.");
    return false;
}

bool Intervention::modifier(QString *erreur) const
{
    if (!valider(erreur) || !baseOuverte(erreur))
        return false;

    QSqlQuery query(Connection::instance().database());
    query.prepare(QStringLiteral(
        "UPDATE INTERVENTION SET TYPE_INTERVENTION = :type, LIEU = :lieu, GRAVITE = :gravite, ETAT = :etat, "
        "LATITUDE = :lat, LONGITUDE = :lon, DATE_RECEPTION = :reception, DATE_FIN_INTERVENTION = :fin "
        "WHERE ID_INTERVENTION = :ID_INTERVENTION"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":lieu"), lieu);
    query.bindValue(QStringLiteral(":gravite"), gravite);
    query.bindValue(QStringLiteral(":etat"), etat);
    query.bindValue(QStringLiteral(":lat"), lat);
    query.bindValue(QStringLiteral(":lon"), lon);
    query.bindValue(QStringLiteral(":reception"), reception);
    query.bindValue(QStringLiteral(":fin"), dateOuNull(etat == Etat::Terminee ? fin : QDateTime()));
    query.bindValue(QStringLiteral(":ID_INTERVENTION"), id);
    if (!query.exec()) {
        signaler(erreur, QStringLiteral("Modification impossible."), query);
        return false;
    }
    if (query.numRowsAffected() == 0) {
        if (erreur)
            *erreur = QStringLiteral("L'intervention %1 n'existe plus : modification impossible.").arg(idText());
        return false;
    }
    return true;
}

bool Intervention::supprimer(int ID_INTERVENTION, QString *erreur)
{
    if (!baseOuverte(erreur))
        return false;

    QSqlQuery query(Connection::instance().database());
    query.prepare(QStringLiteral("DELETE FROM INTERVENTION WHERE ID_INTERVENTION = :ID_INTERVENTION"));
    query.bindValue(QStringLiteral(":ID_INTERVENTION"), ID_INTERVENTION);
    if (!query.exec()) {
        signaler(erreur, QStringLiteral("Suppression impossible."), query);
        return false;
    }
    if (query.numRowsAffected() == 0) {
        if (erreur)
            *erreur = QStringLiteral("L'intervention demandée n'existe plus.");
        return false;
    }
    return true;
}

QVector<Intervention> Intervention::lister(QString *erreur)
{
    return lireToutes(erreur);
}

bool Intervention::roleAutorise(const QString &role)
{
    return role == Role::OperateurDispatch || role == Role::ChefCaserne;
}
