#include "intervention.h"
#include "helpers.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace {

// Stockage en mémoire (aucune base de données) : les données vivent le temps de l'exécution.
QVector<Intervention> &stock()
{
    static QVector<Intervention> lignes = [] {
        auto make = [](int id, const char *type, const char *lieu, const QString &grav, const char *rec,
                       const char *fin, double lat, double lon) {
            Intervention i;
            i.id = id;
            i.type = QString::fromUtf8(type);
            i.lieu = QString::fromUtf8(lieu);
            i.gravite = grav;
            i.reception = QDateTime::fromString(QString::fromLatin1(rec), QStringLiteral("yyyy-MM-dd HH:mm"));
            if (fin) {
                i.fin = QDateTime::fromString(QString::fromLatin1(fin), QStringLiteral("yyyy-MM-dd HH:mm"));
                i.etat = Etat::Terminee;
            } else {
                i.etat = Etat::EnCours;
            }
            i.lat = lat;
            i.lon = lon;
            return i;
        };
        QVector<Intervention> v;
        v << make(1, "Incendie",   "Ben Arous",    Grav::Elevee,  "2026-09-30 14:32", nullptr,            36.7534, 10.2231)
          << make(2, "Accident",   "Tunis centre", Grav::Moyenne, "2026-09-30 11:05", "2026-09-30 11:35", 36.8065, 10.1815)
          << make(3, "Inondation", "Ariana",       Grav::Faible,  "2026-09-30 09:40", "2026-09-30 12:50", 36.8625, 10.1956)
          << make(4, "Incendie",   "Ben Arous",    Grav::Elevee,  "2026-09-12 18:20", "2026-09-12 19:55", 36.7480, 10.2290)
          << make(5, "Accident",   "Ben Arous",    Grav::Moyenne, "2026-08-27 07:45", "2026-08-27 08:25", 36.7560, 10.2180)
          << make(6, "Inondation", "Ariana",       Grav::Moyenne, "2026-09-18 06:30", "2026-09-18 10:50", 36.8601, 10.1932)
          << make(7, "Accident",   "Carthage",     Grav::Faible,  "2026-09-22 20:05", "2026-09-22 20:40", 36.8528, 10.3233)
          << make(8, "Incendie",   "Tunis centre", Grav::Elevee,  "2026-10-01 16:40", nullptr,            36.8100, 10.1850);
        return v;
    }();
    return lignes;
}

int prochainId()
{
    int maxId = 0;
    for (const Intervention &i : std::as_const(stock()))
        maxId = std::max(maxId, i.id);
    return maxId + 1;
}

// Poids de la gravité : 1 = faible, 2 = moyenne, 3 = élevée
int graviteVersCode(const QString &g)
{
    if (g == Grav::Elevee)  return 3;
    if (g == Grav::Moyenne) return 2;
    return 1;
}

// Recherche par coordonnées GPS : "36.8065" (latitude ou longitude) ou "36.8065, 10.18" (les deux).
bool correspond(const Intervention &i, const QString &gps)
{
    const QString texte = gps.trimmed();
    if (texte.isEmpty())
        return true;

    static const QRegularExpression caracteresValides(QStringLiteral("^[-0-9.,;\\s]+$"));
    if (!caracteresValides.match(texte).hasMatch())
        return false;

    QStringList parties;
    for (const QString &p : texte.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
        const QString nettoyee = QString(p).remove(QRegularExpression(QStringLiteral("\\s")));
        if (!nettoyee.isEmpty())
            parties << nettoyee;
    }
    if (parties.isEmpty())
        return false;

    const QString latitude = QString::number(i.lat, 'f', 4);
    const QString longitude = QString::number(i.lon, 'f', 4);
    if (parties.size() >= 2)
        return latitude.contains(parties[0]) && longitude.contains(parties[1]);
    return latitude.contains(parties[0]) || longitude.contains(parties[0]);
}

QVector<Intervention> filtrer(const Intervention::Filtre &filtre)
{
    QVector<Intervention> res;
    for (const Intervention &i : std::as_const(stock()))
        if (correspond(i, filtre.gps))
            res.push_back(i);

    using T = Intervention::Tri;
    switch (filtre.tri) {
    case T::GraviteDecroissante:
        std::stable_sort(res.begin(), res.end(), [](const Intervention &a, const Intervention &b) {
            const int ga = graviteVersCode(a.gravite), gb = graviteVersCode(b.gravite);
            if (ga != gb) return ga > gb;
            if (a.reception != b.reception) return a.reception > b.reception;
            return a.id < b.id;
        });
        break;
    case T::GraviteCroissante:
        std::stable_sort(res.begin(), res.end(), [](const Intervention &a, const Intervention &b) {
            const int ga = graviteVersCode(a.gravite), gb = graviteVersCode(b.gravite);
            if (ga != gb) return ga < gb;
            if (a.reception != b.reception) return a.reception > b.reception;
            return a.id < b.id;
        });
        break;
    case T::PlusRecentes:
        std::stable_sort(res.begin(), res.end(), [](const Intervention &a, const Intervention &b) {
            if (a.reception != b.reception) return a.reception > b.reception;
            return a.id > b.id;
        });
        break;
    case T::Aucun:
        std::stable_sort(res.begin(), res.end(),
                         [](const Intervention &a, const Intervention &b) { return a.id < b.id; });
        break;
    }
    return res;
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
    return Helpers::formatDuration(dureeMinutes());
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
// CRUD (en mémoire)
// ---------------------------------------------------------------------------------------------

bool Intervention::ajouter(QString *)
{
    id = prochainId();
    stock().push_back(*this);
    return true;
}

bool Intervention::afficher(int id, Intervention &resultat, QString *erreur)
{
    for (const Intervention &i : std::as_const(stock())) {
        if (i.id == id) {
            resultat = i;
            return true;
        }
    }
    if (erreur)
        *erreur = QStringLiteral("L'intervention demandée n'existe plus.");
    return false;
}

bool Intervention::modifier(QString *erreur) const
{
    for (Intervention &i : stock()) {
        if (i.id == id) {
            i = *this;
            return true;
        }
    }
    if (erreur)
        *erreur = QStringLiteral("L'intervention %1 n'existe plus : modification impossible.").arg(idText());
    return false;
}

bool Intervention::supprimer(int id, QString *)
{
    auto &v = stock();
    v.erase(std::remove_if(v.begin(), v.end(), [id](const Intervention &i) { return i.id == id; }), v.end());
    return true;
}

// ---------------------------------------------------------------------------------------------
// Fonctionnalités avancées (filtre, tri, pagination, statistiques, zones à risque)
// ---------------------------------------------------------------------------------------------

QVector<Intervention> Intervention::lister(const Filtre &filtre, int debut, int nombre, QString *)
{
    QVector<Intervention> lignes = filtrer(filtre);
    if (nombre <= 0)
        return lignes;
    if (debut >= lignes.size())
        return {};
    return lignes.mid(std::max(0, debut), nombre);
}

int Intervention::compter(const Filtre &filtre, QString *)
{
    return int(filtrer(filtre).size());
}

QMap<QString, int> Intervention::statistiquesParGravite(QString *)
{
    QMap<QString, int> resultat{{Grav::Elevee, 0}, {Grav::Moyenne, 0}, {Grav::Faible, 0}};
    for (const Intervention &i : std::as_const(stock())) {
        const int code = graviteVersCode(i.gravite);
        resultat[code >= 3 ? Grav::Elevee : code == 2 ? Grav::Moyenne : Grav::Faible] += 1;
    }
    return resultat;
}

// Historique par lieu depuis la date donnée : nombre d'interventions, gravité cumulée, position moyenne.
QVector<Intervention::ZoneAgregee> Intervention::zonesARisque(const QDateTime &depuis, QString *)
{
    QHash<QString, ZoneAgregee> parLieu;
    for (const Intervention &i : std::as_const(stock())) {
        if (i.reception < depuis)
            continue;
        const QString cle = i.lieu.toLower();
        ZoneAgregee &z = parLieu[cle];
        z.cle = cle;
        if (z.nom.isEmpty() || i.lieu < z.nom)
            z.nom = i.lieu;
        z.nombre += 1;
        z.poids += graviteVersCode(i.gravite);
        z.lat += i.lat;
        z.lon += i.lon;
    }
    QVector<ZoneAgregee> zones;
    for (ZoneAgregee z : std::as_const(parLieu)) {
        z.lat /= z.nombre;
        z.lon /= z.nombre;
        zones.push_back(z);
    }
    return zones;
}

bool Intervention::roleAutorise(const QString &role)
{
    return role == Role::OperateurDispatch || role == Role::ChefCaserne;
}
