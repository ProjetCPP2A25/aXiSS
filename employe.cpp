#include "employe.h"
#include <QLabel>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QVariant>
#include <QRegularExpression>

static const char *kCols = "ID_EMPLOYE,NOM,PRENOM,TELEPHONE,EMAIL,GRADE,POSTE,DISPONIBILITE,MOT_DE_PASSE,AGE,HEURES_TRAVAIL_SEMAINE,JOURS_CONGE_RESTANTS,ALERTE";

// Derniere erreur SQL (les erreurs de QSqlQuery ne remontent pas toujours dans QSqlDatabase)
static QString s_erreur;
static bool lancer(QSqlQuery &q)
{
    if (q.exec()) return true;
    s_erreur = q.lastError().text();
    return false;
}


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

QString Employe::derniereErreur() { return s_erreur; }

QString Employe::hash(const QString &mdp)
{
    return QString::fromLatin1(QCryptographicHash::hash(mdp.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool Employe::reinitialiserMotDePasse(const QString &email, const QString &nouveauMotDePasse)
{
    if (!emailValide(email) || nouveauMotDePasse.trimmed().size() < 4) return false;
    QSqlQuery q;
    q.prepare("UPDATE EMPLOYE SET MOT_DE_PASSE=? WHERE LOWER(EMAIL)=?");
    q.addBindValue(hash(nouveauMotDePasse));
    q.addBindValue(email.trimmed().toLower());
    return lancer(q) && q.numRowsAffected() == 1;
}

// Barre latérale du module Employés (implémentée ici comme demandé).
SidebarFrame::SidebarFrame(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("sidebarFrame"));
    setFixedWidth(200);
    setStyleSheet(QStringLiteral("QFrame#sidebarFrame { background-color: #23282D; border: none; }"));

    auto *logo = new QLabel(QStringLiteral("A.X.I.S"), this);
    logo->setGeometry(22, 22, 150, 32);
    logo->setStyleSheet(QStringLiteral("color: white; font-size: 24px; font-weight: bold; background: transparent; border: none;"));
    auto *company = new QLabel(QStringLiteral("RedFlame Protection"), this);
    company->setGeometry(22, 62, 160, 22);
    company->setStyleSheet(QStringLiteral("color: #9AA4AE; font-size: 13px; background: transparent; border: none;"));

    const QString buttonStyle = QStringLiteral("QPushButton { background-color: transparent; color: #E5E7EB; border: none; text-align: left; padding-left: 12px; font-size: 14px; } QPushButton:hover { background-color: #30363C; border-radius: 7px; }");
    auto *menu = new QPushButton(QStringLiteral("Menu principal"), this);
    menu->setGeometry(14, 105, 184, 42); menu->setStyleSheet(buttonStyle); menu->setCursor(Qt::PointingHandCursor);
    auto *logout = new QPushButton(QStringLiteral("Déconnexion"), this);
    logout->setGeometry(14, 155, 184, 42); logout->setStyleSheet(buttonStyle); logout->setCursor(Qt::PointingHandCursor);

    m_info = new QLabel(this);
    m_info->setGeometry(22, 210, 166, 60); m_info->setWordWrap(true);
    m_info->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_info->setStyleSheet(QStringLiteral("color: #9AA4AE; font-size: 12px; background: transparent; border: none;"));
    m_info->hide();
    connect(menu, &QPushButton::clicked, this, &SidebarFrame::menuRequested);
    connect(logout, &QPushButton::clicked, this, &SidebarFrame::logoutRequested);
}

void SidebarFrame::setInfo(const QString &text)
{
    m_info->setText(text);
    m_info->setVisible(!text.isEmpty());
}

QString Employe::prochainId()
{
    QSqlQuery q;
    if (q.exec("SELECT SEQ_EMPLOYE.NEXTVAL FROM DUAL") && q.next())
        return q.value(0).toString();
    s_erreur = q.lastError().text();
    return QString();
}

bool Employe::ajouter()
{
    if (id.isEmpty()) id = prochainId();
    if (id.isEmpty()) return false;
    QSqlQuery q;
    q.prepare(QString("INSERT INTO EMPLOYE(%1) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)").arg(kCols));
    q.addBindValue(id.toInt()); q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(telephone); q.addBindValue(email);
    q.addBindValue(grade); q.addBindValue(poste); q.addBindValue(disponibilite); q.addBindValue(motDePasse);
    q.addBindValue(age); q.addBindValue(heures); q.addBindValue(conges); q.addBindValue(alerte ? 1 : 0);
    return lancer(q);
}

bool Employe::modifier() const
{
    QSqlQuery q;
    q.prepare("UPDATE EMPLOYE SET NOM=?,PRENOM=?,TELEPHONE=?,EMAIL=?,GRADE=?,POSTE=?,DISPONIBILITE=?,MOT_DE_PASSE=?,"
              "AGE=?,HEURES_TRAVAIL_SEMAINE=?,JOURS_CONGE_RESTANTS=?,ALERTE=? WHERE ID_EMPLOYE=?");
    q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(telephone); q.addBindValue(email); q.addBindValue(grade);
    q.addBindValue(poste); q.addBindValue(disponibilite); q.addBindValue(motDePasse);
    q.addBindValue(age); q.addBindValue(heures); q.addBindValue(conges); q.addBindValue(alerte ? 1 : 0);
    q.addBindValue(id.toInt());
    return lancer(q);
}

bool Employe::supprimer(const QString &id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM EMPLOYE WHERE ID_EMPLOYE=?");
    q.addBindValue(id.toInt());
    return lancer(q) && q.numRowsAffected() > 0;
}

Employe Employe::trouver(const QString &id)
{
    QSqlQuery q;
    q.prepare(QString("SELECT %1 FROM EMPLOYE WHERE ID_EMPLOYE=?").arg(kCols));
    q.addBindValue(id.toInt());
    q.exec();
    return q.next() ? depuis(q) : Employe();
}

Employe Employe::trouverParEmail(const QString &email)
{
    QSqlQuery q;
    q.prepare(QString("SELECT %1 FROM EMPLOYE WHERE LOWER(EMAIL)=?").arg(kCols));
    q.addBindValue(email.trimmed().toLower());
    q.exec();
    return q.next() ? depuis(q) : Employe();
}

QVector<Employe> Employe::lister(const QString &recherche, int tri)
{
    QString sql = QString("SELECT %1 FROM EMPLOYE").arg(kCols);
    if (!recherche.isEmpty()) sql += " WHERE LOWER(NOM || ' ' || PRENOM) LIKE ? OR LOWER(PRENOM || ' ' || NOM) LIKE ?";
    const QString rang = "CASE GRADE WHEN 'Capitaine' THEN 4 WHEN 'Lieutenant' THEN 3 "
                         "WHEN 'Sergent' THEN 2 WHEN 'Caporal' THEN 1 ELSE 0 END";
    switch (tri) {
    case 0: sql += " ORDER BY " + rang + " DESC, NOM, PRENOM"; break;
    case 1: sql += " ORDER BY " + rang + " ASC, NOM, PRENOM"; break;
    default: sql += " ORDER BY NOM, PRENOM"; break;
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
    QString sql = QString("SELECT %1 FROM EMPLOYE WHERE HEURES_TRAVAIL_SEMAINE > 48").arg(kCols);
    if (seulementNonNotifies) sql += " AND ALERTE = 0";
    QSqlQuery q(sql);
    QVector<Employe> r;
    while (q.next()) r.append(depuis(q));
    return r;
}

bool Employe::emailExiste(const QString &email, const QString &sauf)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM EMPLOYE WHERE LOWER(EMAIL)=? AND id<>?");
    q.addBindValue(email.toLower());
    q.addBindValue(sauf);
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
}

static QMap<QString, int> compter(const QString &colonne)
{
    QMap<QString, int> m;
    QSqlQuery q(QString("SELECT %1, COUNT(*) FROM EMPLOYE GROUP BY %1").arg(colonne));
    while (q.next()) m[q.value(0).toString()] = q.value(1).toInt();
    return m;
}
QMap<QString, int> Employe::statsDisponibilite() { return compter("disponibilite"); }
QMap<QString, int> Employe::statsGrade() { return compter("grade"); }

Employe Employe::authentifier(const QString &email, const QString &mdp)
{
    QSqlQuery q;
    q.prepare(QString("SELECT %1 FROM EMPLOYE WHERE LOWER(EMAIL)=? AND mot_de_passe=?").arg(kCols));
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

bool Employe::nomValide(const QString &nom)
{
    const QString n = nom.trimmed();
    static const QRegularExpression re(QStringLiteral("^\\p{L}[\\p{L} '\\-]*$"));
    return n.length() >= 2 && n.length() <= 40 && re.match(n).hasMatch();
}
