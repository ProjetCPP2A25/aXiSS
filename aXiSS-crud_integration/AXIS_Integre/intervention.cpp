#include "intervention.h"
#include "connexion.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <QDateTime>
#include <QFontMetrics>
#include <QJsonArray>
#include <QLocale>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QStandardPaths>
#include <QToolTip>
#include <QUrl>
#include <QUrlQuery>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

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

// =============================================================================================
// FONCTIONNALITÉS AVANCÉES
// (travaillent sur la liste déjà lue par lister() : aucune requête SQL supplémentaire)
// =============================================================================================

// ---------------------------------------------------------------------------------------------
// 6 - Distance
// ---------------------------------------------------------------------------------------------

double Intervention::distanceKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double rayonTerreKm = 6371.0088;
    constexpr double pi = 3.14159265358979323846;
    const double r = pi / 180.0;
    const double dLat = (lat2 - lat1) * r;
    const double dLon = (lon2 - lon1) * r;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                     + std::cos(lat1 * r) * std::cos(lat2 * r) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * rayonTerreKm * std::asin(std::min(1.0, std::sqrt(a)));
}

// ---------------------------------------------------------------------------------------------
// 1 - Tri
// ---------------------------------------------------------------------------------------------

void Intervention::trierParGravite(QVector<Intervention> &liste)
{
    std::stable_sort(liste.begin(), liste.end(), [](const Intervention &a, const Intervention &b) {
        if (a.graviteRang() != b.graviteRang())
            return a.graviteRang() < b.graviteRang();          // Élevée (0) d'abord
        return a.reception > b.reception;                      // à gravité égale : la plus récente d'abord
    });
}

void Intervention::trierParGraviteCroissante(QVector<Intervention> &liste)
{
    std::stable_sort(liste.begin(), liste.end(), [](const Intervention &a, const Intervention &b) {
        if (a.graviteRang() != b.graviteRang())
            return a.graviteRang() > b.graviteRang();          // Faible (2) d'abord
        return a.reception > b.reception;
    });
}

void Intervention::trierParDate(QVector<Intervention> &liste)
{
    std::stable_sort(liste.begin(), liste.end(),
                     [](const Intervention &a, const Intervention &b) { return a.reception > b.reception; });
}

void Intervention::trierParId(QVector<Intervention> &liste)
{
    std::stable_sort(liste.begin(), liste.end(),
                     [](const Intervention &a, const Intervention &b) { return a.id < b.id; });
}

// ---------------------------------------------------------------------------------------------
// 2 - Recherche par coordonnées GPS + historique
// ---------------------------------------------------------------------------------------------

QVector<Intervention> Intervention::rechercherGps(const QVector<Intervention> &liste, const QString &saisie,
                                                  double toleranceM)
{
    const QString texte = saisie.trimmed();
    if (texte.isEmpty())
        return liste;

    QVector<Intervention> trouvees;

    // Point précis « 36.8065, 10.1815 » (deux nombres décimaux) : interventions proches de ce point
    double la = 0.0, lo = 0.0;
    if (texte.count(QLatin1Char('.')) >= 2 && lireGps(texte, la, lo)) {
        for (const Intervention &i : liste)
            if (distanceKm(la, lo, i.lat, i.lon) * 1000.0 <= toleranceM)
                trouvees.push_back(i);
        std::stable_sort(trouvees.begin(), trouvees.end(), [la, lo](const Intervention &a, const Intervention &b) {
            return distanceKm(la, lo, a.lat, a.lon) < distanceKm(la, lo, b.lat, b.lon);
        });
        return trouvees;
    }

    // Saisie partielle « 36.80 » ou « 10.18 » : correspondance dans le texte des coordonnées
    QString motif = texte;
    if (!motif.contains(QLatin1Char('.')))
        motif.replace(QLatin1Char(','), QLatin1Char('.'));       // « 36,80 » saisi à la française
    for (const Intervention &i : liste)
        if (i.gps().contains(motif, Qt::CaseInsensitive))
            trouvees.push_back(i);
    return trouvees;
}

QVector<Intervention> Intervention::historiqueProche(const QVector<Intervention> &liste, const Intervention &ref,
                                                     double rayonM)
{
    QVector<Intervention> historique;
    for (const Intervention &i : liste)
        if (i.id != ref.id && distanceKm(ref.lat, ref.lon, i.lat, i.lon) * 1000.0 <= rayonM)
            historique.push_back(i);
    trierParDate(historique);
    return historique;
}

// ---------------------------------------------------------------------------------------------
// 4 - Statistiques par gravité
// ---------------------------------------------------------------------------------------------

Intervention::StatsGravite Intervention::statistiques(const QVector<Intervention> &liste)
{
    StatsGravite s;
    for (const Intervention &i : liste) {
        if (i.gravite == Grav::Elevee)       ++s.elevee;
        else if (i.gravite == Grav::Moyenne) ++s.moyenne;
        else                                 ++s.faible;
    }
    return s;
}

// ---------------------------------------------------------------------------------------------
// 5 - Zones à risque (historique + météo)
// ---------------------------------------------------------------------------------------------

Intervention::Meteo Intervention::lireMeteoJson(const QByteArray &json)
{
    Meteo m;
    const QJsonObject courant = QJsonDocument::fromJson(json).object().value(QStringLiteral("current")).toObject();
    if (courant.isEmpty() || !courant.contains(QStringLiteral("temperature_2m")))
        return m;

    m.disponible = true;
    m.temperatureC = courant.value(QStringLiteral("temperature_2m")).toDouble();
    m.ventKmh = courant.value(QStringLiteral("wind_speed_10m")).toDouble();
    m.pluieMm = courant.value(QStringLiteral("precipitation")).toDouble();
    m.resume = QStringLiteral("%1 °C, vent %2 km/h, %3")
                   .arg(qRound(m.temperatureC))
                   .arg(qRound(m.ventKmh))
                   .arg(m.pluieMm >= 0.1 ? QStringLiteral("pluie %1 mm").arg(m.pluieMm, 0, 'f', 1)
                                         : QStringLiteral("sans pluie"));
    return m;
}

QVector<Intervention::ZoneRisque> Intervention::detecterZonesRisque(const QVector<Intervention> &liste,
                                                                    const Meteo &meteo, int maxZones, double rayonM)
{
    struct Groupe {
        double sommeLat = 0.0, sommeLon = 0.0;
        QVector<int> index;
        double lat() const { return sommeLat / index.size(); }
        double lon() const { return sommeLon / index.size(); }
    };

    // 1) Regroupement : une intervention rejoint la première zone dont le centre est à moins de « rayonM » mètres
    QVector<Groupe> groupes;
    for (int k = 0; k < liste.size(); ++k) {
        const Intervention &i = liste[k];
        Groupe *cible = nullptr;
        for (Groupe &g : groupes) {
            if (distanceKm(g.lat(), g.lon(), i.lat, i.lon) * 1000.0 <= rayonM) {
                cible = &g;
                break;
            }
        }
        if (!cible) {
            groupes.push_back(Groupe());
            cible = &groupes.last();
        }
        cible->sommeLat += i.lat;
        cible->sommeLon += i.lon;
        cible->index.push_back(k);
    }

    // 2) Conditions météo favorables à certains types d'interventions
    const bool chaudEtSec = meteo.disponible && meteo.temperatureC >= 30.0 && meteo.pluieMm < 0.1;
    const bool ventFort = meteo.disponible && meteo.ventKmh >= 25.0;
    const bool pluie = meteo.disponible && (meteo.pluieMm >= 1.0 || meteo.ventKmh >= 50.0);

    const QDateTime maintenant = QDateTime::currentDateTime();
    QVector<ZoneRisque> zones;
    for (const Groupe &g : groupes) {
        ZoneRisque z;
        z.lat = g.lat();
        z.lon = g.lon();
        z.nbInterventions = g.index.size();

        QHash<QString, int> lieux;
        int nbFeu = 0, nbRoute = 0;
        double score = 0.0;
        for (int k : g.index) {
            const Intervention &i = liste[k];
            const double poids = i.gravite == Grav::Elevee ? 3.0 : (i.gravite == Grav::Moyenne ? 2.0 : 1.0);
            const qint64 jours = i.reception.isValid() ? i.reception.daysTo(maintenant) : 365;
            const double recence = jours <= 30 ? 1.5 : (jours <= 180 ? 1.0 : 0.6);   // l'historique récent compte plus
            score += poids * recence;
            if (i.gravite == Grav::Elevee)
                ++z.nbGraves;
            ++lieux[i.lieu.trimmed()];
            if (i.type.contains(QLatin1String("incend"), Qt::CaseInsensitive))
                ++nbFeu;
            if (i.type.contains(QLatin1String("accident"), Qt::CaseInsensitive)
                || i.type.contains(QLatin1String("inond"), Qt::CaseInsensitive))
                ++nbRoute;
        }

        // lieu le plus fréquent de la zone = nom affiché
        int meilleur = -1;
        for (auto it = lieux.cbegin(); it != lieux.cend(); ++it) {
            if (it.value() > meilleur || (it.value() == meilleur && it.key() < z.nom)) {
                meilleur = it.value();
                z.nom = it.key();
            }
        }

        // 3) Majoration météo, proportionnelle à la part des interventions concernées
        QStringList facteurs;
        facteurs << QStringLiteral("%1 intervention%2").arg(z.nbInterventions).arg(z.nbInterventions > 1 ? "s" : "");
        if (z.nbGraves > 0)
            facteurs << QStringLiteral("%1 grave%2").arg(z.nbGraves).arg(z.nbGraves > 1 ? "s" : "");
        const double partFeu = double(nbFeu) / z.nbInterventions;
        const double partRoute = double(nbRoute) / z.nbInterventions;
        QStringList meteoTxt;
        if (chaudEtSec && partFeu > 0.0) {
            score *= 1.0 + 0.5 * partFeu + (ventFort ? 0.25 * partFeu : 0.0);
            meteoTxt << (ventFort ? QStringLiteral("chaleur + vent") : QStringLiteral("chaleur"));
        }
        if (pluie && partRoute > 0.0) {
            score *= 1.0 + 0.5 * partRoute;
            meteoTxt << QStringLiteral("pluie / vent fort");
        }
        QString texte = facteurs.join(QStringLiteral(", "));
        if (!meteoTxt.isEmpty())
            texte += QStringLiteral(" · météo : ") + meteoTxt.join(QStringLiteral(", "));

        z.score = score;
        z.facteurs = texte;
        z.niveau = score >= 8.0 ? Grav::Elevee : (score >= 4.0 ? Grav::Moyenne : Grav::Faible);

        // Une zone n'est « exposée » que si elle est récurrente ou a déjà connu une intervention grave
        if (z.nbInterventions >= 2 || z.nbGraves >= 1)
            zones.push_back(z);
    }

    std::sort(zones.begin(), zones.end(), [](const ZoneRisque &a, const ZoneRisque &b) { return a.score > b.score; });
    if (zones.size() > maxZones)
        zones.resize(maxZones);
    return zones;
}

// ---------------------------------------------------------------------------------------------
// 3 - Export Excel (.xlsx écrit à la main : archive ZIP « sans compression » + XML SpreadsheetML)
// ---------------------------------------------------------------------------------------------

namespace {

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool pret = false;
    if (!pret) {
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        pret = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (char octet : data)
        c = table[(c ^ static_cast<quint8>(octet)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void ecrire16(QByteArray &b, quint16 v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}

void ecrire32(QByteArray &b, quint32 v)
{
    ecrire16(b, static_cast<quint16>(v & 0xFFFF));
    ecrire16(b, static_cast<quint16>((v >> 16) & 0xFFFF));
}

struct FichierZip {
    QByteArray nom;
    QByteArray contenu;
};

QByteArray creerZip(const QVector<FichierZip> &fichiers)
{
    constexpr quint16 dateDos = (0 << 9) | (1 << 5) | 1;      // 01/01/1980
    QByteArray zip, centrale;
    for (const FichierZip &f : fichiers) {
        const quint32 decalage = static_cast<quint32>(zip.size());
        const quint32 crc = crc32(f.contenu);
        const quint32 taille = static_cast<quint32>(f.contenu.size());

        ecrire32(zip, 0x04034B50);                 // en-tête local
        ecrire16(zip, 20); ecrire16(zip, 0); ecrire16(zip, 0);   // version, options, méthode 0 = stocké
        ecrire16(zip, 0); ecrire16(zip, dateDos);
        ecrire32(zip, crc); ecrire32(zip, taille); ecrire32(zip, taille);
        ecrire16(zip, static_cast<quint16>(f.nom.size())); ecrire16(zip, 0);
        zip.append(f.nom);
        zip.append(f.contenu);

        ecrire32(centrale, 0x02014B50);            // répertoire central
        ecrire16(centrale, 20); ecrire16(centrale, 20); ecrire16(centrale, 0); ecrire16(centrale, 0);
        ecrire16(centrale, 0); ecrire16(centrale, dateDos);
        ecrire32(centrale, crc); ecrire32(centrale, taille); ecrire32(centrale, taille);
        ecrire16(centrale, static_cast<quint16>(f.nom.size()));
        ecrire16(centrale, 0); ecrire16(centrale, 0); ecrire16(centrale, 0); ecrire16(centrale, 0);
        ecrire32(centrale, 0); ecrire32(centrale, decalage);
        centrale.append(f.nom);
    }
    const quint32 debutCentrale = static_cast<quint32>(zip.size());
    zip.append(centrale);
    ecrire32(zip, 0x06054B50);                     // fin de l'archive
    ecrire16(zip, 0); ecrire16(zip, 0);
    ecrire16(zip, static_cast<quint16>(fichiers.size())); ecrire16(zip, static_cast<quint16>(fichiers.size()));
    ecrire32(zip, static_cast<quint32>(centrale.size())); ecrire32(zip, debutCentrale);
    ecrire16(zip, 0);
    return zip;
}

QString xmlEchappe(const QString &s)
{
    QString r;
    r.reserve(s.size());
    for (QChar c : s) {
        if (c == QLatin1Char('&')) r += QLatin1String("&amp;");
        else if (c == QLatin1Char('<')) r += QLatin1String("&lt;");
        else if (c == QLatin1Char('>')) r += QLatin1String("&gt;");
        else if (c == QLatin1Char('"')) r += QLatin1String("&quot;");
        else if (c.unicode() < 0x20 && c != QLatin1Char('\t') && c != QLatin1Char('\n') && c != QLatin1Char('\r')) continue;
        else r += c;
    }
    return r;
}

// Styles (index dans cellXfs) : 0 normal, 1 en-tête, 2 date/heure, 3 coordonnée, 4/5/6 gravité élevée/moyenne/faible
enum StyleXlsx { sNormal = 0, sEntete = 1, sDate = 2, sCoord = 3, sGraveElevee = 4, sGraveMoyenne = 5, sGraveFaible = 6 };

QString celluleTexte(const QString &ref, const QString &texte, int style = sNormal)
{
    return QStringLiteral("<c r=\"%1\" s=\"%2\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%3</t></is></c>")
    .arg(ref).arg(style).arg(xmlEchappe(texte));
}

QString celluleNombre(const QString &ref, double valeur, int style = sNormal)
{
    return QStringLiteral("<c r=\"%1\" s=\"%2\"><v>%3</v></c>")
    .arg(ref).arg(style).arg(QString::number(valeur, 'f', 6));
}

double serieExcel(const QDateTime &d)                  // nombre de jours depuis le 30/12/1899
{
    return QDate(1899, 12, 30).daysTo(d.date()) + QTime(0, 0).msecsTo(d.time()) / 86400000.0;
}

} // namespace

bool Intervention::exporterExcel(const QVector<Intervention> &liste, const QString &chemin, QString *erreur)
{
    static const QStringList entetes = {
        QStringLiteral("ID"), QStringLiteral("Type"), QStringLiteral("Lieu"), QStringLiteral("Gravité"),
        QStringLiteral("Latitude"), QStringLiteral("Longitude"), QStringLiteral("Réception de l'alerte"),
        QStringLiteral("Fin de l'intervention"), QStringLiteral("Durée (min)"), QStringLiteral("État"),
        QStringLiteral("Distance caserne (km)")
    };
    static const QStringList colonnes = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K"};
    static const double largeurs[] = {8, 16, 28, 11, 11, 11, 20, 20, 12, 12, 20};

    QString feuille;
    feuille += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                              "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                              "<sheetViews><sheetView workbookViewId=\"0\" showGridLines=\"1\">"
                              "<pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" state=\"frozen\"/>"
                              "</sheetView></sheetViews><sheetFormatPr defaultRowHeight=\"15\"/><cols>");
    for (int c = 0; c < colonnes.size(); ++c)
        feuille += QStringLiteral("<col min=\"%1\" max=\"%1\" width=\"%2\" customWidth=\"1\"/>").arg(c + 1).arg(largeurs[c]);
    feuille += QStringLiteral("</cols><sheetData><row r=\"1\" ht=\"22\" customHeight=\"1\">");
    for (int c = 0; c < entetes.size(); ++c)
        feuille += celluleTexte(colonnes[c] + QStringLiteral("1"), entetes[c], sEntete);
    feuille += QStringLiteral("</row>");

    for (int r = 0; r < liste.size(); ++r) {
        const Intervention &i = liste[r];
        const QString n = QString::number(r + 2);
        const int styleGravite = i.gravite == Grav::Elevee ? sGraveElevee
                                                           : (i.gravite == Grav::Moyenne ? sGraveMoyenne : sGraveFaible);
        feuille += QStringLiteral("<row r=\"%1\">").arg(n);
        feuille += celluleTexte(QStringLiteral("A") + n, i.idText());
        feuille += celluleTexte(QStringLiteral("B") + n, i.type);
        feuille += celluleTexte(QStringLiteral("C") + n, i.lieu);
        feuille += celluleTexte(QStringLiteral("D") + n, i.gravite, styleGravite);
        feuille += celluleNombre(QStringLiteral("E") + n, i.lat, sCoord);
        feuille += celluleNombre(QStringLiteral("F") + n, i.lon, sCoord);
        if (i.reception.isValid())
            feuille += celluleNombre(QStringLiteral("G") + n, serieExcel(i.reception), sDate);
        if (i.fin.isValid())
            feuille += celluleNombre(QStringLiteral("H") + n, serieExcel(i.fin), sDate);
        feuille += celluleNombre(QStringLiteral("I") + n, static_cast<double>(i.dureeMinutes()));
        feuille += celluleTexte(QStringLiteral("J") + n, i.etat);
        feuille += celluleNombre(QStringLiteral("K") + n, std::round(i.distanceDepuisCaserneKm() * 100.0) / 100.0);
        feuille += QStringLiteral("</row>");
    }
    feuille += QStringLiteral("</sheetData><autoFilter ref=\"A1:K%1\"/></worksheet>").arg(liste.size() + 1);

    const QString styles = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<numFmts count=\"2\"><numFmt numFmtId=\"164\" formatCode=\"dd/mm/yyyy\\ hh:mm\"/>"
        "<numFmt numFmtId=\"165\" formatCode=\"0.0000\"/></numFmts>"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><color rgb=\"FFFFFFFF\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"6\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFC00000\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFF7D6D6\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFFCE8CC\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFDDF3E4\"/><bgColor indexed=\"64\"/></patternFill></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"7\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyAlignment=\"1\">"
        "<alignment horizontal=\"center\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"165\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"3\" borderId=\"0\" xfId=\"0\" applyFill=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"4\" borderId=\"0\" xfId=\"0\" applyFill=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"5\" borderId=\"0\" xfId=\"0\" applyFill=\"1\"/>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles></styleSheet>");

    QVector<FichierZip> fichiers;
    fichiers.push_back({"[Content_Types].xml", QByteArray(
                                                   "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                                                   "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                                                   "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                                                   "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                                                   "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
                                                   "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
                                                   "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
                                                   "</Types>")});
    fichiers.push_back({"_rels/.rels", QByteArray(
                                           "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                                           "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                                           "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
                                           "</Relationships>")});
    fichiers.push_back({"xl/workbook.xml", QByteArray(
                                               "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                                               "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                                               "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                                               "<sheets><sheet name=\"Interventions\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>")});
    fichiers.push_back({"xl/_rels/workbook.xml.rels", QByteArray(
                                                          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                                                          "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                                                          "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
                                                          "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
                                                          "</Relationships>")});
    fichiers.push_back({"xl/styles.xml", styles.toUtf8()});
    fichiers.push_back({"xl/worksheets/sheet1.xml", feuille.toUtf8()});

    QSaveFile fichier(chemin);
    if (!fichier.open(QIODevice::WriteOnly)) {
        if (erreur)
            *erreur = QStringLiteral("Impossible de créer le fichier :\n%1\n%2").arg(chemin, fichier.errorString());
        return false;
    }
    fichier.write(creerZip(fichiers));
    if (!fichier.commit()) {
        if (erreur)
            *erreur = QStringLiteral("Impossible d'écrire le fichier (est-il ouvert dans Excel ?) :\n%1\n%2")
                          .arg(chemin, fichier.errorString());
        return false;
    }
    return true;
}


// =============================================================================================
// CARTE INTERACTIVE (CarteWidget : tuiles OpenStreetMap, marqueurs, zones, itinéraire OSRM)
// =============================================================================================

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kTaille = 256;                       // taille d'une tuile OpenStreetMap
constexpr int kZoomMin = 2;
constexpr int kZoomMax = 18;
const char *const kUserAgent = "AXIS-RedFlame/1.0 (projet etudiant Qt; gestion de caserne)";   // exigé par OSM

quint64 cleTuile(int z, int x, int y)
{
    return (quint64(z) << 48) | (quint64(x) << 24) | quint64(y);
}

QString km(double valeur)
{
    return QLocale(QLocale::French).toString(valeur, 'f', valeur < 10.0 ? 1 : 0);
}

QString dureeTexte(int minutes)
{
    if (minutes < 60)
        return QStringLiteral("%1 min").arg(minutes);
    return QStringLiteral("%1 h %2").arg(minutes / 60).arg(minutes % 60, 2, 10, QChar('0'));
}

const char *const kStyleBouton =
    "QPushButton { background-color: #FFFFFF; color: #222; border: 1px solid #9AA0A6; border-radius: 4px;"
    "  font-size: 18px; font-weight: bold; }"
    "QPushButton:hover { background-color: #EDEDED; }";

} // namespace

// ---------------------------------------------------------------------------------------------

CarteWidget::CarteWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(300, 200);

    m_reseau = new QNetworkAccessManager(this);
    auto *cache = new QNetworkDiskCache(this);          // les tuiles déjà vues restent disponibles (et on ménage OSM)
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/tuiles_osm"));
    cache->setMaximumCacheSize(150LL * 1024 * 1024);
    m_reseau->setCache(cache);

    auto creer = [this](const QString &texte, const QString &infobulle) {
        auto *b = new QPushButton(texte, this);
        b->setFixedSize(34, 34);
        b->setToolTip(infobulle);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setStyleSheet(QLatin1String(kStyleBouton));
        return b;
    };
    m_btnPlus = creer(QStringLiteral("+"), QStringLiteral("Zoom avant"));
    m_btnMoins = creer(QStringLiteral("−"), QStringLiteral("Zoom arrière"));
    m_btnCadrer = creer(QStringLiteral("⌖"), QStringLiteral("Voir toutes les interventions"));
    connect(m_btnPlus, &QPushButton::clicked, this, [this] { definirZoom(m_zoom + 1, QPointF(width() / 2.0, height() / 2.0)); });
    connect(m_btnMoins, &QPushButton::clicked, this, [this] { definirZoom(m_zoom - 1, QPointF(width() / 2.0, height() / 2.0)); });
    connect(m_btnCadrer, &QPushButton::clicked, this, [this] { ajusterVue(); });
}

// ---------------------------------------------------------------------------------------------
// Données
// ---------------------------------------------------------------------------------------------

void CarteWidget::setCaserne(double lat, double lon, const QString &nom)
{
    m_casLat = lat;
    m_casLon = lon;
    m_casNom = nom;
    m_lat = lat;
    m_lon = lon;
    update();
}

void CarteWidget::setMarqueurs(const QVector<Marqueur> &marqueurs)
{
    m_marqueurs = marqueurs;
    // la sélection ne survit que si l'intervention existe toujours
    bool trouve = false;
    for (const Marqueur &m : m_marqueurs)
        trouve = trouve || m.id == m_selection;
    if (!trouve) {
        m_selection = 0;
        m_route.clear();
        m_infoTrajet.clear();
        if (m_reponseRoute) {
            m_reponseRoute->disconnect(this);
            m_reponseRoute->abort();
            m_reponseRoute->deleteLater();
            m_reponseRoute = nullptr;
        }
    }
    update();
}

void CarteWidget::setZones(const QVector<Zone> &zones)
{
    m_zones = zones;
    update();
}

void CarteWidget::ajusterVue()
{
    QVector<QPointF> points;
    points.push_back(QPointF(m_casLat, m_casLon));
    for (const Marqueur &m : m_marqueurs)
        points.push_back(QPointF(m.lat, m.lon));
    cadrer(points, 15);
}

void CarteWidget::selectionner(int id)
{
    const Marqueur *cible = nullptr;
    for (const Marqueur &m : m_marqueurs)
        if (m.id == id)
            cible = &m;

    if (m_reponseRoute) {                         // on abandonne le calcul précédent
        m_reponseRoute->disconnect(this);
        m_reponseRoute->abort();
        m_reponseRoute->deleteLater();
        m_reponseRoute = nullptr;
    }
    m_route.clear();
    m_infoTrajet.clear();

    if (!cible) {
        m_selection = 0;
        update();
        return;
    }
    m_selection = id;
    cadrer({QPointF(m_casLat, m_casLon), QPointF(cible->lat, cible->lon)}, 16);
    calculerItineraire(*cible);
}

// ---------------------------------------------------------------------------------------------
// Projection Web Mercator
// ---------------------------------------------------------------------------------------------

QPointF CarteWidget::projeter(double lat, double lon, int zoom)
{
    const double n = double(kTaille) * double(1 << zoom);
    const double latR = std::clamp(lat, -85.0511, 85.0511) * kPi / 180.0;
    const double x = (lon + 180.0) / 360.0 * n;
    const double y = (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / kPi) / 2.0 * n;
    return QPointF(x, y);
}

void CarteWidget::deprojeter(const QPointF &pixel, int zoom, double &lat, double &lon)
{
    const double n = double(kTaille) * double(1 << zoom);
    lon = pixel.x() / n * 360.0 - 180.0;
    lat = std::atan(std::sinh(kPi * (1.0 - 2.0 * pixel.y() / n))) * 180.0 / kPi;
}

QPointF CarteWidget::versEcran(double lat, double lon) const
{
    const QPointF centre = projeter(m_lat, m_lon, m_zoom);
    const QPointF p = projeter(lat, lon, m_zoom);
    return QPointF(p.x() - centre.x() + width() / 2.0, p.y() - centre.y() + height() / 2.0);
}

void CarteWidget::ecranVersGeo(const QPointF &ecran, double &lat, double &lon) const
{
    const QPointF centre = projeter(m_lat, m_lon, m_zoom);
    deprojeter(QPointF(centre.x() + ecran.x() - width() / 2.0, centre.y() + ecran.y() - height() / 2.0),
               m_zoom, lat, lon);
}

// ---------------------------------------------------------------------------------------------
// Vue : zoom, cadrage
// ---------------------------------------------------------------------------------------------

void CarteWidget::definirZoom(int zoom, const QPointF &ancrageEcran)
{
    zoom = std::clamp(zoom, kZoomMin, kZoomMax);
    if (zoom == m_zoom)
        return;
    // le point géographique sous le curseur reste sous le curseur
    double lat, lon;
    ecranVersGeo(ancrageEcran, lat, lon);
    m_zoom = zoom;
    const QPointF p = projeter(lat, lon, m_zoom);
    deprojeter(QPointF(p.x() - (ancrageEcran.x() - width() / 2.0), p.y() - (ancrageEcran.y() - height() / 2.0)),
               m_zoom, m_lat, m_lon);
    update();
}

void CarteWidget::cadrer(const QVector<QPointF> &pointsLatLon, int zoomMax)
{
    if (pointsLatLon.isEmpty())
        return;
    double minLat = 90, maxLat = -90, minLon = 180, maxLon = -180;
    for (const QPointF &p : pointsLatLon) {
        minLat = std::min(minLat, p.x());
        maxLat = std::max(maxLat, p.x());
        minLon = std::min(minLon, p.y());
        maxLon = std::max(maxLon, p.y());
    }
    const double marge = 90.0;                    // place pour les boutons et le panneau d'information
    const double largeur = std::max(50.0, width() - 2 * marge);
    const double hauteur = std::max(50.0, height() - 2 * marge);

    int zoom = std::min(zoomMax, kZoomMax);
    for (; zoom > kZoomMin; --zoom) {
        const QPointF a = projeter(maxLat, minLon, zoom);        // coin nord-ouest
        const QPointF b = projeter(minLat, maxLon, zoom);        // coin sud-est
        if (b.x() - a.x() <= largeur && b.y() - a.y() <= hauteur)
            break;
    }
    m_zoom = zoom;
    m_lat = (minLat + maxLat) / 2.0;
    m_lon = (minLon + maxLon) / 2.0;
    update();
}

void CarteWidget::resizeEvent(QResizeEvent *)
{
    const int x = width() - 44;
    m_btnPlus->move(x, 10);
    m_btnMoins->move(x, 48);
    m_btnCadrer->move(x, 86);
}

// ---------------------------------------------------------------------------------------------
// Tuiles OpenStreetMap
// ---------------------------------------------------------------------------------------------

const QPixmap *CarteWidget::tuile(int z, int x, int y)
{
    const quint64 cle = cleTuile(z, x, y);
    const auto it = m_tuiles.constFind(cle);
    if (it != m_tuiles.constEnd())
        return &it.value();
    demanderTuile(z, x, y);
    return nullptr;
}

void CarteWidget::demanderTuile(int z, int x, int y)
{
    const quint64 cle = cleTuile(z, x, y);
    if (m_enCours.contains(cle))
        return;
    const auto echec = m_echecs.constFind(cle);
    if (echec != m_echecs.constEnd() && QDateTime::currentMSecsSinceEpoch() - echec.value() < 15000)
        return;

    QNetworkRequest req(QUrl(QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png").arg(z).arg(x).arg(y)));
    req.setRawHeader("User-Agent", kUserAgent);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    req.setTransferTimeout(15000);

    m_enCours.insert(cle);
    QNetworkReply *reponse = m_reseau->get(req);
    connect(reponse, &QNetworkReply::finished, this, [this, reponse, cle] {
        reponse->deleteLater();
        m_enCours.remove(cle);
        QPixmap pm;
        if (reponse->error() == QNetworkReply::NoError && pm.loadFromData(reponse->readAll())) {
            if (m_tuiles.size() > 500)             // cache mémoire borné
                m_tuiles.clear();
            m_tuiles.insert(cle, pm);
            m_echecs.remove(cle);
            update();
        } else {
            m_echecs.insert(cle, QDateTime::currentMSecsSinceEpoch());
        }
    });
}

// ---------------------------------------------------------------------------------------------
// Itinéraire (OSRM) et distance
// ---------------------------------------------------------------------------------------------

void CarteWidget::calculerItineraire(const Marqueur &cible)
{
    const double vol = Intervention::distanceKm(m_casLat, m_casLon, cible.lat, cible.lon);
    m_infoTrajet = QStringLiteral("Calcul de l'itinéraire… (à vol d'oiseau : %1 km)").arg(km(vol));
    update();

    QUrl url(QStringLiteral("https://router.project-osrm.org/route/v1/driving/%1,%2;%3,%4")
                 .arg(QString::number(m_casLon, 'f', 6), QString::number(m_casLat, 'f', 6),
                      QString::number(cible.lon, 'f', 6), QString::number(cible.lat, 'f', 6)));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("overview"), QStringLiteral("full"));
    q.addQueryItem(QStringLiteral("geometries"), QStringLiteral("geojson"));
    q.addQueryItem(QStringLiteral("alternatives"), QStringLiteral("true"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", kUserAgent);
    req.setTransferTimeout(12000);

    const int id = cible.id;
    QNetworkReply *reponse = m_reseau->get(req);
    m_reponseRoute = reponse;
    connect(reponse, &QNetworkReply::finished, this, [this, reponse, id, vol] {
        reponse->deleteLater();
        if (id != m_selection)                     // une autre intervention a été choisie entre-temps
            return;

        QJsonArray routes;
        if (reponse->error() == QNetworkReply::NoError)
            routes = QJsonDocument::fromJson(reponse->readAll()).object().value(QStringLiteral("routes")).toArray();

        if (routes.isEmpty()) {
            m_route.clear();
            m_infoTrajet = QStringLiteral("Itinéraire indisponible (hors ligne ?) · à vol d'oiseau : %1 km").arg(km(vol));
            update();
            return;
        }

        // « Trajet le plus adapté » : parmi les itinéraires proposés, le plus rapide (puis le plus court)
        QJsonObject meilleur;
        for (const QJsonValue &v : routes) {
            const QJsonObject r = v.toObject();
            if (meilleur.isEmpty()
                || r.value(QStringLiteral("duration")).toDouble() < meilleur.value(QStringLiteral("duration")).toDouble()
                || (r.value(QStringLiteral("duration")).toDouble() == meilleur.value(QStringLiteral("duration")).toDouble()
                    && r.value(QStringLiteral("distance")).toDouble() < meilleur.value(QStringLiteral("distance")).toDouble()))
                meilleur = r;
        }

        m_route.clear();
        const QJsonArray coords = meilleur.value(QStringLiteral("geometry")).toObject()
                                      .value(QStringLiteral("coordinates")).toArray();
        m_route.reserve(coords.size());
        for (const QJsonValue &c : coords) {
            const QJsonArray lonLat = c.toArray();
            if (lonLat.size() >= 2)
                m_route.push_back(QPointF(lonLat.at(1).toDouble(), lonLat.at(0).toDouble()));     // (lat, lon)
        }
        const double distanceKm = meilleur.value(QStringLiteral("distance")).toDouble() / 1000.0;
        const int minutes = std::max(1, qRound(meilleur.value(QStringLiteral("duration")).toDouble() / 60.0));
        m_infoTrajet = QStringLiteral("Distance par la route : %1 km · durée estimée : %2")
                           .arg(km(distanceKm), dureeTexte(minutes));
        update();
    });
}

// ---------------------------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------------------------

void CarteWidget::dessinerMarqueur(QPainter &p, const QPointF &pointe, const QColor &couleur, bool selectionne) const
{
    const double r = selectionne ? 11.5 : 9.0;
    const QPointF centre(pointe.x(), pointe.y() - r - 6.0);

    QPainterPath goutte;
    goutte.moveTo(pointe);
    goutte.lineTo(centre.x() - r * 0.8, centre.y() + r * 0.6);
    goutte.arcTo(QRectF(centre.x() - r, centre.y() - r, 2 * r, 2 * r), 216, -252);
    goutte.closeSubpath();

    p.setPen(QPen(Qt::white, selectionne ? 2.5 : 2.0));
    p.setBrush(couleur);
    p.drawPath(goutte);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(centre, r * 0.38, r * 0.38);
}

void CarteWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(rect(), QColor("#E5E3DF"));

    // --- tuiles ---
    const QPointF centre = projeter(m_lat, m_lon, m_zoom);
    const double x0 = centre.x() - width() / 2.0;
    const double y0 = centre.y() - height() / 2.0;
    const int n = 1 << m_zoom;
    const int tx0 = int(std::floor(x0 / kTaille)), tx1 = int(std::floor((x0 + width()) / kTaille));
    const int ty0 = std::max(0, int(std::floor(y0 / kTaille)));
    const int ty1 = std::min(n - 1, int(std::floor((y0 + height()) / kTaille)));
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const int txm = ((tx % n) + n) % n;
            const QPointF coin(tx * double(kTaille) - x0, ty * double(kTaille) - y0);
            if (const QPixmap *pm = tuile(m_zoom, txm, ty)) {
                p.drawPixmap(coin, *pm);
            } else {
                p.fillRect(QRectF(coin, QSizeF(kTaille, kTaille)), QColor("#ECEAE5"));
                p.setPen(QColor("#B8B4AC"));
                p.drawRect(QRectF(coin, QSizeF(kTaille, kTaille)));
            }
        }
    }

    // --- zones à risque ---
    const double metresParPixel = 156543.03392 * std::cos(m_lat * kPi / 180.0) / double(1 << m_zoom);
    for (const Zone &z : m_zones) {
        const QPointF c = versEcran(z.lat, z.lon);
        const double rayon = std::max(16.0, z.rayonM / metresParPixel);
        QColor fond = z.couleur;
        fond.setAlpha(55);
        QColor trait = z.couleur;
        trait.setAlpha(190);
        p.setBrush(fond);
        p.setPen(QPen(trait, 2, Qt::DashLine));
        p.drawEllipse(c, rayon, rayon);
    }

    // --- itinéraire ---
    if (!m_route.isEmpty()) {
        QPolygonF ligne;
        ligne.reserve(m_route.size());
        for (const QPointF &pt : m_route)
            ligne << versEcran(pt.x(), pt.y());
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(ligne);
        p.setPen(QPen(QColor("#1A73E8"), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(ligne);
    } else if (m_selection != 0) {                 // repli : ligne droite pointillée
        for (const Marqueur &m : m_marqueurs) {
            if (m.id != m_selection)
                continue;
            p.setPen(QPen(QColor("#1A73E8"), 3, Qt::DashLine, Qt::RoundCap));
            p.drawLine(versEcran(m_casLat, m_casLon), versEcran(m.lat, m.lon));
        }
    }

    // --- caserne ---
    {
        const QPointF c = versEcran(m_casLat, m_casLon);
        p.setPen(QPen(Qt::white, 2.5));
        p.setBrush(QColor("#1A56DB"));
        p.drawRoundedRect(QRectF(c.x() - 12, c.y() - 12, 24, 24), 6, 6);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(14);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(c.x() - 12, c.y() - 12, 24, 24), Qt::AlignCenter, QStringLiteral("C"));
        if (!m_casNom.isEmpty()) {
            f.setPixelSize(11);
            p.setFont(f);
            const QRectF r(c.x() - 70, c.y() + 14, 140, 16);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 215));
            const int w = QFontMetrics(f).horizontalAdvance(m_casNom) + 10;
            p.drawRoundedRect(QRectF(c.x() - w / 2.0, r.y(), w, 16), 4, 4);
            p.setPen(QColor("#1A3C8F"));
            p.drawText(r, Qt::AlignCenter, m_casNom);
        }
    }

    // --- interventions (la sélection est dessinée en dernier, au-dessus des autres) ---
    for (int passe = 0; passe < 2; ++passe) {
        for (const Marqueur &m : m_marqueurs) {
            const bool sel = (m.id == m_selection);
            if (sel != (passe == 1))
                continue;
            dessinerMarqueur(p, versEcran(m.lat, m.lon), m.couleur, sel);
        }
    }

    // --- panneau d'information ---
    {
        QStringList lignes;
        const Marqueur *sel = nullptr;
        for (const Marqueur &m : m_marqueurs)
            if (m.id == m_selection)
                sel = &m;
        if (sel) {
            lignes << sel->titre;
            if (!sel->lieu.isEmpty())
                lignes << sel->lieu;
            lignes << m_infoTrajet;
        } else {
            lignes << QStringLiteral("Cliquez sur une intervention pour afficher");
            lignes << QStringLiteral("la distance et l'itinéraire depuis la caserne.");
        }
        QFont f = p.font();
        f.setPixelSize(12);
        f.setBold(false);
        p.setFont(f);
        const QFontMetrics fm(f);
        int largeur = 0;
        for (const QString &l : lignes)
            largeur = std::max(largeur, fm.horizontalAdvance(l));
        largeur = std::min(largeur + 24, std::max(120, width() - 70));
        const int hauteur = int(lignes.size()) * 18 + 14;
        const QRectF panneau(10, 10, largeur, hauteur);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(24, 27, 31, 225));
        p.drawRoundedRect(panneau, 6, 6);
        p.setPen(QColor("#FFFFFF"));
        for (int i = 0; i < lignes.size(); ++i) {
            QFont fl = f;
            fl.setBold(sel && i == 0);
            p.setFont(fl);
            p.drawText(QRectF(panneau.x() + 12, panneau.y() + 7 + i * 18, panneau.width() - 16, 18),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       QFontMetrics(fl).elidedText(lignes[i], Qt::ElideRight, int(panneau.width()) - 20));
        }
    }

    // --- mention légale obligatoire des données OpenStreetMap ---
    {
        QFont f = p.font();
        f.setPixelSize(10);
        f.setBold(false);
        p.setFont(f);
        const QString txt = QStringLiteral("© OpenStreetMap contributors");
        const int w = QFontMetrics(f).horizontalAdvance(txt) + 10;
        const QRectF r(width() - w, height() - 16, w, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 200));
        p.drawRect(r);
        p.setPen(QColor("#333333"));
        p.drawText(r, Qt::AlignCenter, txt);
    }
}

// ---------------------------------------------------------------------------------------------
// Souris
// ---------------------------------------------------------------------------------------------

int CarteWidget::marqueurSous(const QPointF &ecran) const
{
    int meilleur = -1;
    double meilleureDistance = 16.0;               // rayon de capture en pixels
    for (int i = 0; i < m_marqueurs.size(); ++i) {
        const QPointF pointe = versEcran(m_marqueurs[i].lat, m_marqueurs[i].lon);
        const QPointF tete(pointe.x(), pointe.y() - 15.0);
        const double d = std::hypot(ecran.x() - tete.x(), ecran.y() - tete.y());
        if (d < meilleureDistance) {
            meilleureDistance = d;
            meilleur = i;
        }
    }
    return meilleur;
}

void CarteWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_glisse = true;
    m_aBouge = false;
    m_pressPos = m_dernierePos = e->position().toPoint();
    setCursor(Qt::ClosedHandCursor);
}

void CarteWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_glisse) {
        const QPoint pos = e->position().toPoint();
        if ((pos - m_pressPos).manhattanLength() > 4)
            m_aBouge = true;
        if (m_aBouge) {
            const QPoint delta = pos - m_dernierePos;
            const QPointF c = projeter(m_lat, m_lon, m_zoom);
            deprojeter(QPointF(c.x() - delta.x(), c.y() - delta.y()), m_zoom, m_lat, m_lon);
            m_lat = std::clamp(m_lat, -85.0, 85.0);
            update();
        }
        m_dernierePos = pos;
        return;
    }
    const int i = marqueurSous(e->position());
    setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::OpenHandCursor);
    if (i >= 0)
        QToolTip::showText(e->globalPosition().toPoint(), m_marqueurs[i].etiquette, this);
    else
        QToolTip::hideText();
}

void CarteWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_glisse)
        return;
    m_glisse = false;
    setCursor(Qt::OpenHandCursor);
    if (!m_aBouge) {                               // simple clic : sélection d'une intervention
        const int i = marqueurSous(e->position());
        if (i >= 0) {
            const int id = m_marqueurs[i].id;
            selectionner(id);
            emit marqueurSelectionne(id);
        }
    }
}

void CarteWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && marqueurSous(e->position()) < 0)
        definirZoom(m_zoom + 1, e->position());
}

void CarteWidget::wheelEvent(QWheelEvent *e)
{
    const int pas = e->angleDelta().y();
    if (pas > 0)
        definirZoom(m_zoom + 1, e->position());
    else if (pas < 0)
        definirZoom(m_zoom - 1, e->position());
    e->accept();
}
