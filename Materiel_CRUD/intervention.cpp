#include "intervention.h"
#include "connexion.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

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

// Toutes les requêtes SQL du module Interventions sont écrites dans cette classe. Elles sont toutes préparées
// (prepare + bindValue) : les valeurs saisies ne sont jamais concaténées dans le texte SQL (pas d'injection SQL).
// Noms de colonnes = schéma Oracle de référence (TYPE, LIEU, COORDONNEES_GPS, DATE_RECEPTION_APPEL, ...).
const QString kColonnes = QStringLiteral(
    "ID_INTERVENTION, TYPE, LIEU, GRAVITE, "
    "DATE_RECEPTION_APPEL, HEURE_RECEPTION_APPEL, "
    "DATE_FIN_INTERVENTION, HEURE_FIN_INTERVENTION, "
    "ETAT, DUREE, COORDONNEES_GPS, ID_GARDE"
    );

// Index des colonnes dans kColonnes
enum Col { cId, cType, cLieu, cGravite, cDateRec, cHeureRec, cDateFin, cHeureFin, cEtat, cDuree, cGps, cGarde };

// Connexion Oracle commune du projet (la même que celle du module Employés), ouverte au démarrage par main().
QSqlDatabase base()
{
    return Connexion::oracle();
}

bool baseOuverte(QString *erreur)
{
    if (base().isValid() && base().isOpen())
        return true;
    if (erreur)
        *erreur = QStringLiteral("La connexion à la base de données Oracle n'est pas établie.");
    return false;
}

void signaler(QString *erreur, const QString &contexte, const QSqlQuery &query)
{
    if (!erreur)
        return;
    const QString detail = query.lastError().text();
    *erreur = contexte + QStringLiteral("\n") + detail;

    // Messages explicites pour les erreurs de clés étrangères / contraintes Oracle les plus courantes
    if (detail.contains(QLatin1String("ORA-02292")))
        *erreur = contexte + QStringLiteral("\nCette intervention est encore référencée par d'autres enregistrements "
                                            "(contrainte de clé étrangère) : elle ne peut pas être supprimée.\n") + detail;
    else if (detail.contains(QLatin1String("ORA-02291")))
        *erreur = contexte + QStringLiteral("\nLa garde (ID_GARDE) n'existe pas dans la table GARDE "
                                            "(contrainte de clé étrangère).\n") + detail;
    else if (detail.contains(QLatin1String("ORA-00001")))
        *erreur = contexte + QStringLiteral("\nUn enregistrement avec le même identifiant existe déjà.\n") + detail;
    else if (detail.contains(QLatin1String("ORA-12899")))
        *erreur = contexte + QStringLiteral("\nUne valeur saisie est trop longue pour la colonne Oracle.\n") + detail;
}

// Valeur NULL typée (texte) pour les colonnes optionnelles : date/heure de fin d'une intervention en cours
QVariant texteOuNull(const QString &s, bool present)
{
    return present ? QVariant(s) : QVariant(QMetaType::fromType<QString>());
}

// Ligne de résultat (colonnes dans l'ordre de kColonnes) -> objet Intervention
Intervention depuisLigne(const QSqlQuery &q)
{
    Intervention i;
    i.id = q.value(cId).toInt();
    i.type = q.value(cType).toString();
    i.lieu = q.value(cLieu).toString();
    i.gravite = q.value(cGravite).toString();
    i.etat = q.value(cEtat).toString();
    i.idGarde = q.value(cGarde).toInt();

    // Date et heure de réception (colonnes distinctes : DATE + "HH:mm")
    i.reception = QDateTime(q.value(cDateRec).toDate(),
                            QTime::fromString(q.value(cHeureRec).toString(), QStringLiteral("HH:mm")));

    // Date et heure de fin (absentes tant que l'intervention est en cours)
    if (!q.value(cDateFin).isNull() && !q.value(cHeureFin).isNull()) {
        i.fin = QDateTime(q.value(cDateFin).toDate(),
                          QTime::fromString(q.value(cHeureFin).toString(), QStringLiteral("HH:mm")));
    }

    // Coordonnées GPS : texte "36.8532, 10.5563"
    Intervention::lireGps(q.value(cGps).toString(), i.lat, i.lon);
    return i;
}

// Lit toutes les interventions de la table (requête préparée, sans paramètre)
QVector<Intervention> lireToutes(QString *erreur)
{
    QVector<Intervention> lignes;
    if (!baseOuverte(erreur))
        return lignes;

    QSqlQuery query(base());
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

bool Intervention::lireGps(const QString &texte, double &lat, double &lon, QString *erreur)
{
    auto refuser = [erreur](const QString &message) {
        if (erreur)
            *erreur = message;
        return false;
    };

    static const QRegularExpression re(
        QStringLiteral("^\\s*(-?\\d+(?:\\.\\d+)?)\\s*[,;]\\s*(-?\\d+(?:\\.\\d+)?)\\s*$"));
    const QRegularExpressionMatch m = re.match(texte);
    if (!m.hasMatch())
        return refuser(QStringLiteral("Coordonnées GPS invalides.\n"
                                      "Format attendu : latitude, longitude (ex. 36.8065, 10.1815)."));

    const double la = m.captured(1).toDouble();
    const double lo = m.captured(2).toDouble();
    if (la < -90.0 || la > 90.0)
        return refuser(QStringLiteral("La latitude doit être comprise entre -90 et 90."));
    if (lo < -180.0 || lo > 180.0)
        return refuser(QStringLiteral("La longitude doit être comprise entre -180 et 180."));

    lat = la;
    lon = lo;
    return true;
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
    if (type.size() > 50)
        return refuser(QStringLiteral("Le type ne doit pas dépasser 50 caractères."));
    if (lieu.trimmed().isEmpty())
        return refuser(QStringLiteral("Veuillez saisir le lieu de l'intervention."));
    if (lieu.size() > 100)
        return refuser(QStringLiteral("Le lieu ne doit pas dépasser 100 caractères."));
    if (gravite.trimmed().isEmpty())
        return refuser(QStringLiteral("Veuillez choisir la gravité de l'intervention."));
    if (lat < -90.0 || lat > 90.0)
        return refuser(QStringLiteral("La latitude doit être comprise entre -90 et 90."));
    if (lon < -180.0 || lon > 180.0)
        return refuser(QStringLiteral("La longitude doit être comprise entre -180 et 180."));
    if (!reception.isValid())
        return refuser(QStringLiteral("La date de réception de l'alerte est obligatoire."));
    if (etat != Etat::EnCours && etat != Etat::Terminee)
        return refuser(QStringLiteral("L'état doit être « En cours » ou « Terminée »."));
    if (etat == Etat::Terminee) {
        if (!fin.isValid())
            return refuser(QStringLiteral("La date de fin est obligatoire pour une intervention terminée."));
        if (fin < reception)
            return refuser(QStringLiteral("La date de fin doit être postérieure ou égale à la réception de l'alerte."));
    }
    if (idGarde < 0)
        return refuser(QStringLiteral("L'identifiant de garde est invalide."));
    return true;
}

// ---------------------------------------------------------------------------------------------
// Garde (clé étrangère ID_GARDE)
// ---------------------------------------------------------------------------------------------

bool Intervention::gardeExiste(int idGarde, QString *erreur)
{
    if (!baseOuverte(erreur))
        return false;

    QSqlQuery query(base());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM GARDE WHERE ID_GARDE = :id"));
    query.bindValue(QStringLiteral(":id"), idGarde);
    if (!query.exec() || !query.next()) {
        signaler(erreur, QStringLiteral("Vérification de la garde impossible."), query);
        return false;
    }
    if (query.value(0).toInt() == 0) {
        if (erreur)
            *erreur = QStringLiteral("La garde n° %1 n'existe pas.").arg(idGarde);
        return false;
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

    // Garde : celle indiquée, sinon (provisoire) la première garde existante
    int garde = idGarde;
    if (garde == 0) {
        QSqlQuery premiere(base());
        premiere.prepare(QStringLiteral("SELECT MIN(ID_GARDE) FROM GARDE"));
        if (!premiere.exec() || !premiere.next()) {
            signaler(erreur, QStringLiteral("Ajout impossible : lecture de la table GARDE impossible."), premiere);
            return false;
        }
        if (premiere.value(0).isNull()) {
            if (erreur)
                *erreur = QStringLiteral("Ajout impossible : aucune garde n'existe dans la table GARDE.\n"
                                         "Une intervention doit être rattachée à une garde existante.");
            return false;
        }
        garde = premiere.value(0).toInt();
    }
    if (!gardeExiste(garde, erreur))
        return false;

    // Prochain identifiant
    QSqlQuery suivant(base());
    suivant.prepare(QStringLiteral("SELECT COALESCE(MAX(ID_INTERVENTION), 0) + 1 FROM INTERVENTION"));
    if (!suivant.exec() || !suivant.next()) {
        signaler(erreur, QStringLiteral("Ajout impossible : identifiant non attribué."), suivant);
        return false;
    }
    const int nouvelId = suivant.value(0).toInt();

    const bool termine = (etat == Etat::Terminee && fin.isValid());

    QSqlQuery query(base());
    if (!query.prepare(QStringLiteral(
            "INSERT INTO INTERVENTION ("
            "ID_INTERVENTION, TYPE, LIEU, GRAVITE, "
            "DATE_RECEPTION_APPEL, HEURE_RECEPTION_APPEL, "
            "DATE_FIN_INTERVENTION, HEURE_FIN_INTERVENTION, "
            "ETAT, DUREE, COORDONNEES_GPS, ID_GARDE"
            ") VALUES ("
            ":id, :type, :lieu, :gravite, "
            "TO_DATE(:dateReception, 'YYYY-MM-DD'), :heureReception, "
            "TO_DATE(:dateFin, 'YYYY-MM-DD'), :heureFin, "
            ":etat, :duree, :gps, :garde)"))) {
        signaler(erreur, QStringLiteral("Ajout impossible (requête non préparée)."), query);
        return false;
    }

    query.bindValue(QStringLiteral(":id"), nouvelId);
    query.bindValue(QStringLiteral(":type"), type.trimmed());
    query.bindValue(QStringLiteral(":lieu"), lieu.trimmed());
    query.bindValue(QStringLiteral(":gravite"), gravite);
    query.bindValue(QStringLiteral(":dateReception"), reception.date().toString(QStringLiteral("yyyy-MM-dd")));
    query.bindValue(QStringLiteral(":heureReception"), reception.time().toString(QStringLiteral("HH:mm")));
    query.bindValue(QStringLiteral(":dateFin"),
                    texteOuNull(fin.date().toString(QStringLiteral("yyyy-MM-dd")), termine));
    query.bindValue(QStringLiteral(":heureFin"),
                    texteOuNull(fin.time().toString(QStringLiteral("HH:mm")), termine));
    query.bindValue(QStringLiteral(":etat"), etat);
    query.bindValue(QStringLiteral(":duree"), static_cast<int>(dureeMinutes()));
    query.bindValue(QStringLiteral(":gps"), gps());
    query.bindValue(QStringLiteral(":garde"), garde);

    if (!query.exec()) {
        signaler(erreur, QStringLiteral("Ajout impossible."), query);
        return false;
    }
    id = nouvelId;
    idGarde = garde;
    return true;
}

bool Intervention::afficher(int id, Intervention &resultat, QString *erreur)
{
    if (!baseOuverte(erreur))
        return false;

    QSqlQuery query(base());
    query.prepare(QStringLiteral("SELECT ") + kColonnes + QStringLiteral(" FROM INTERVENTION WHERE ID_INTERVENTION = :id"));
    query.bindValue(QStringLiteral(":id"), id);
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

    // Si une garde est précisée, elle doit exister (sinon ID_GARDE reste inchangée)
    if (idGarde > 0 && !gardeExiste(idGarde, erreur))
        return false;

    const bool termine = (etat == Etat::Terminee && fin.isValid());

    QSqlQuery query(base());
    if (!query.prepare(QStringLiteral(
            "UPDATE INTERVENTION SET "
            "TYPE = :type, "
            "LIEU = :lieu, "
            "GRAVITE = :gravite, "
            "DATE_RECEPTION_APPEL = TO_DATE(:dateReception, 'YYYY-MM-DD'), "
            "HEURE_RECEPTION_APPEL = :heureReception, "
            "DATE_FIN_INTERVENTION = TO_DATE(:dateFin, 'YYYY-MM-DD'), "
            "HEURE_FIN_INTERVENTION = :heureFin, "
            "ETAT = :etat, "
            "DUREE = :duree, "
            "COORDONNEES_GPS = :gps "
            "WHERE ID_INTERVENTION = :id"))) {
        signaler(erreur, QStringLiteral("Modification impossible (requête non préparée)."), query);
        return false;
    }

    query.bindValue(QStringLiteral(":type"), type.trimmed());
    query.bindValue(QStringLiteral(":lieu"), lieu.trimmed());
    query.bindValue(QStringLiteral(":gravite"), gravite);
    query.bindValue(QStringLiteral(":dateReception"), reception.date().toString(QStringLiteral("yyyy-MM-dd")));
    query.bindValue(QStringLiteral(":heureReception"), reception.time().toString(QStringLiteral("HH:mm")));
    // Date de fin uniquement si l'intervention est terminée, sinon NULL
    query.bindValue(QStringLiteral(":dateFin"),
                    texteOuNull(fin.date().toString(QStringLiteral("yyyy-MM-dd")), termine));
    query.bindValue(QStringLiteral(":heureFin"),
                    texteOuNull(fin.time().toString(QStringLiteral("HH:mm")), termine));
    query.bindValue(QStringLiteral(":etat"), etat);
    query.bindValue(QStringLiteral(":duree"), static_cast<int>(dureeMinutes()));
    query.bindValue(QStringLiteral(":gps"), gps());
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        signaler(erreur, QStringLiteral("Modification impossible."), query);
        return false;
    }
    if (query.numRowsAffected() == 0) {
        if (erreur)
            *erreur = QStringLiteral("L'intervention %1 n'existe plus.").arg(idText());
        return false;
    }
    return true;
}

bool Intervention::supprimer(int id, QString *erreur)
{
    if (!baseOuverte(erreur))
        return false;

    QSqlQuery query(base());
    query.prepare(QStringLiteral("DELETE FROM INTERVENTION WHERE ID_INTERVENTION = :id"));
    query.bindValue(QStringLiteral(":id"), id);
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
