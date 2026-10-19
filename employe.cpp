#include "employe.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QVariant>
#include <QRegularExpression>
#include <QRegularExpression>

static const char *kCols = "id,nom,prenom,telephone,email,grade,poste,disponibilite,mot_de_passe,age,heures,conges,alerte";

static Employe depuis(const QSqlQuery &q)
{
    Employe e;
    e.id = q.value(0).toString();   e.nom = q.value(1).toString();   e.prenom = q.value(2).toString();
    e.telephone = q.value(3).toString(); e.email = q.value(4).toString();
    e.grade = q.value(5).toString(); e.poste = q.value(6).toString();
    e.disponibilite = q.value(7).toString(); e.motDePasse = q.value(8).toString();
    e.age = q.value(9).toInt(); e.heures = q.value(10).toInt();
    e.conges = q.value(11).toInt(); e.alerte = q.value(12).toInt() != 0;
    return e;
}

QString Employe::derniereErreur() { return QSqlDatabase::database().lastError().text(); }

QString Employe::hash(const QString &mdp)
{
    return QString::fromLatin1(QCryptographicHash::hash(mdp.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString Employe::prochainId()
{
    QSqlQuery q("SELECT id FROM employes");
    int mx = 0;
    while (q.next()) mx = qMax(mx, q.value(0).toString().mid(1).toInt());
    return QString("E%1").arg(mx + 1, 3, 10, QChar('0'));
}

bool Employe::ajouter()
{
    if (id.isEmpty()) id = prochainId();
    QSqlQuery q;
    q.prepare(QString("INSERT INTO employes(%1) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)").arg(kCols));
    q.addBindValue(id); q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(telephone); q.addBindValue(email);
    q.addBindValue(grade); q.addBindValue(poste); q.addBindValue(disponibilite); q.addBindValue(motDePasse);
    q.addBindValue(age); q.addBindValue(heures); q.addBindValue(conges); q.addBindValue(alerte ? 1 : 0);
    return q.exec();
}

bool Employe::modifier() const
{
    QSqlQuery q;
    q.prepare("UPDATE employes SET nom=?,prenom=?,telephone=?,email=?,grade=?,poste=?,disponibilite=?,mot_de_passe=?,"
              "age=?,heures=?,conges=?,alerte=? WHERE id=?");
    q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(telephone); q.addBindValue(email); q.addBindValue(grade);
    q.addBindValue(poste); q.addBindValue(disponibilite); q.addBindValue(motDePasse);
    q.addBindValue(age); q.addBindValue(heures); q.addBindValue(conges); q.addBindValue(alerte ? 1 : 0);
    q.addBindValue(id);
    return q.exec();
}

bool Employe::supprimer(const QString &id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM employes WHERE id=?");
    q.addBindValue(id);
    return q.exec();
}

Employe Employe::trouver(const QString &id)
{
    QSqlQuery q;
    q.prepare(QString("SELECT %1 FROM employes WHERE id=?").arg(kCols));
    q.addBindValue(id);
    q.exec();
    return q.next() ? depuis(q) : Employe();
}

Employe Employe::trouverParEmail(const QString &email)
{
    QSqlQuery q;
    q.prepare(QString("SELECT %1 FROM employes WHERE LOWER(email)=?").arg(kCols));
    q.addBindValue(email.trimmed().toLower());
    q.exec();
    return q.next() ? depuis(q) : Employe();
}

QVector<Employe> Employe::lister(const QString &recherche, int tri)
{
    QString sql = QString("SELECT %1 FROM employes").arg(kCols);
    if (!recherche.isEmpty()) sql += " WHERE LOWER(nom || ' ' || prenom) LIKE ? OR LOWER(prenom || ' ' || nom) LIKE ?";
    const QString rang = "CASE grade WHEN 'Capitaine' THEN 4 WHEN 'Lieutenant' THEN 3 "
                         "WHEN 'Sergent' THEN 2 WHEN 'Caporal' THEN 1 ELSE 0 END";
    switch (tri) {
    case 0: sql += " ORDER BY " + rang + " DESC, nom, prenom"; break;
    case 1: sql += " ORDER BY " + rang + " ASC, nom, prenom"; break;
    default: sql += " ORDER BY nom, prenom"; break;
    }
    QSqlQuery q;
    q.prepare(sql);
    if (!recherche.isEmpty()) { q.addBindValue("%" + recherche.toLower() + "%"); q.addBindValue("%" + recherche.toLower() + "%"); }
    q.exec();
    QVector<Employe> r;
    while (q.next()) r.append(depuis(q));
    return r;
}

QVector<Employe> Employe::surcharge(bool seulementNonNotifies)
{
    QString sql = QString("SELECT %1 FROM employes WHERE heures > 48").arg(kCols);
    if (seulementNonNotifies) sql += " AND alerte = 0";
    QSqlQuery q(sql);
    QVector<Employe> r;
    while (q.next()) r.append(depuis(q));
    return r;
}

void Employe::reinitialiserAlertes()
{
    QSqlQuery q("UPDATE employes SET alerte=0 WHERE heures <= 48 AND alerte=1");
}

void Employe::effacerAlertes()
{
    QSqlQuery q("UPDATE employes SET alerte=0");
}

QString Employe::emailChefCaserne()
{
    QSqlQuery q("SELECT email FROM employes WHERE poste='Chef de caserne' ORDER BY id LIMIT 1");
    return q.next() ? q.value(0).toString() : QString();
}

void Employe::marquerAlerte(const QString &id, bool valeur)
{
    QSqlQuery q;
    q.prepare("UPDATE employes SET alerte=? WHERE id=?");
    q.addBindValue(valeur ? 1 : 0);
    q.addBindValue(id);
    q.exec();
}

bool Employe::emailExiste(const QString &email, const QString &sauf)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM employes WHERE LOWER(email)=? AND id<>?");
    q.addBindValue(email.toLower());
    q.addBindValue(sauf);
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
}

static QMap<QString, int> compter(const QString &colonne)
{
    QMap<QString, int> m;
    QSqlQuery q(QString("SELECT %1, COUNT(*) FROM employes GROUP BY %1").arg(colonne));
    while (q.next()) m[q.value(0).toString()] = q.value(1).toInt();
    return m;
}
QMap<QString, int> Employe::statsDisponibilite() { return compter("disponibilite"); }
QMap<QString, int> Employe::statsGrade() { return compter("grade"); }

Employe Employe::authentifier(const QString &email, const QString &mdp)
{
    QSqlQuery q;
    q.prepare(QString("SELECT %1 FROM employes WHERE LOWER(email)=? AND mot_de_passe=?").arg(kCols));
    q.addBindValue(email.trimmed().toLower());
    q.addBindValue(hash(mdp));
    q.exec();
    return q.next() ? depuis(q) : Employe();
}

// Matrice des droits (cahier de spécification, colonne « Utilisateur » de chaque module)
bool Employe::aAcces(const QString &poste, int module)
{
    switch (module) {
    case 0: return poste == "Administrateur" || poste == "Chef de caserne";            // Employés
    case 1: return poste == "Opérateur de dispatch" || poste == "Chef de caserne";     // Interventions
    case 2: return poste == "Chef de caserne" || poste == "Administrateur";            // Véhicules
    case 3: return poste == "Magasinier" || poste == "Chef de caserne";                // Matériels
    }
    return false;
}

bool Employe::estUtilisateur(const QString &poste)
{
    for (int m = 0; m < 4; ++m) if (aAcces(poste, m)) return true;
    return false;
}

Employe Employe::chefCaserne()
{
    QSqlQuery q(QString("SELECT %1 FROM employes WHERE poste='Chef de caserne' ORDER BY id LIMIT 1").arg(kCols));
    return q.next() ? depuis(q) : Employe();
}

bool Employe::emailValide(const QString &email)
{
    static const QRegularExpression re(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9\-]+(\.[A-Za-z0-9\-]+)*\.[A-Za-z]{2,}$)");
    return re.match(email.trimmed()).hasMatch();
}

bool Employe::telephoneValide(const QString &telephone)
{
    QString t = telephone.trimmed();
    t.remove(' ');                       // les espaces de présentation (20 123 456) sont tolérés
    static const QRegularExpression re("^[0-9]{8,15}$");
    return re.match(t).hasMatch();
}
