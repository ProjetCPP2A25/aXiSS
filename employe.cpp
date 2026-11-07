#include "employe.h"
#include "connexion.h"
#include <QLabel>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>

// Table Oracle EMPLOYE (SQL Developer, connexion « axis »).
// Ordre des colonnes = ordre lu par depuis().
static const char *kCols = "ID_EMPLOYE,NOM,PRENOM,TELEPHONE,EMAIL,GRADE,POSTE,DISPONIBILITE,AGE,"
                           "HEURES_TRAVAIL_SEMAINE,JOURS_CONGE_RESTANTS";

// Dernière erreur (les erreurs d'un QSqlQuery ne remontent pas toujours dans QSqlDatabase)
static QString s_erreur;

// Traduit les erreurs Oracle les plus fréquentes en message compréhensible
static QString messageSql(const QSqlQuery &q)
{
    const QString t = q.lastError().text();
    if (t.contains("ORA-02292"))
        return "cet employé est utilisé ailleurs (par exemple affecté à une garde).";
    if (t.contains("ORA-00001"))
        return "cette valeur existe déjà (email ou identifiant).";
    if (t.contains("ORA-02290"))
        return "une valeur est refusée par une contrainte de la base (grade, poste, âge, heures, congés ou disponibilité).";
    if (t.contains("ORA-12899"))
        return "une valeur est trop longue pour sa colonne.";
    return t;
}

static bool lancer(QSqlQuery &q)
{
    if (q.exec()) return true;
    s_erreur = messageSql(q);
    return false;
}

static int entier(const QVariant &v) { return qRound(v.toDouble()); }   // NUMBER(5,2) -> int

static Employe depuis(const QSqlQuery &q)
{
    Employe e;
    e.id = q.value(0).toString();   e.nom = q.value(1).toString();   e.prenom = q.value(2).toString();
    e.telephone = q.value(3).toString(); e.email = q.value(4).toString();
    e.grade = q.value(5).toString(); e.poste = q.value(6).toString();
    e.disponibilite = q.value(7).toString();
    e.age = entier(q.value(8)); e.heures = entier(q.value(9));
    e.conges = entier(q.value(10));
    return e;
}

QString Employe::derniereErreur() { return s_erreur; }

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
    auto *logout = new QPushButton(QStringLiteral("Quitter"), this);
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

// Identifiant généré par la séquence Oracle SEQ_EMPLOYE
QString Employe::prochainId()
{
    QSqlQuery q(Connexion::oracle());
    if (q.exec("SELECT SEQ_EMPLOYE.NEXTVAL FROM DUAL") && q.next())
        return q.value(0).toString();
    s_erreur = q.lastError().text();
    return QString();
}

// ------------------------------------------------------------------ CREATE
bool Employe::ajouter()
{
    if (id.isEmpty()) id = prochainId();
    if (id.isEmpty()) return false;
    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("INSERT INTO EMPLOYE(%1) VALUES(?,?,?,?,?,?,?,?,?,?,?)").arg(kCols));
    q.addBindValue(id.toInt()); q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(telephone); q.addBindValue(email);
    q.addBindValue(grade); q.addBindValue(poste); q.addBindValue(disponibilite);
    q.addBindValue(age); q.addBindValue(heures); q.addBindValue(conges);
    return lancer(q);
}

// ------------------------------------------------------------------ UPDATE
bool Employe::modifier() const
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE EMPLOYE SET NOM=?,PRENOM=?,TELEPHONE=?,EMAIL=?,GRADE=?,POSTE=?,DISPONIBILITE=?,"
              "AGE=?,HEURES_TRAVAIL_SEMAINE=?,JOURS_CONGE_RESTANTS=? WHERE ID_EMPLOYE=?");
    q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(telephone); q.addBindValue(email); q.addBindValue(grade);
    q.addBindValue(poste); q.addBindValue(disponibilite);
    q.addBindValue(age); q.addBindValue(heures); q.addBindValue(conges);
    q.addBindValue(id.toInt());
    return lancer(q) && q.numRowsAffected() > 0;
}

// ------------------------------------------------------------------ DELETE
bool Employe::supprimer(const QString &id)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM EMPLOYE WHERE ID_EMPLOYE=?");
    q.addBindValue(id.toInt());
    return lancer(q) && q.numRowsAffected() > 0;
}

// ------------------------------------------------------------------ READ
Employe Employe::trouver(const QString &id)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("SELECT %1 FROM EMPLOYE WHERE ID_EMPLOYE=?").arg(kCols));
    q.addBindValue(id.toInt());
    q.exec();
    return q.next() ? depuis(q) : Employe();
}

QVector<Employe> Employe::lister()
{
    QSqlQuery q(Connexion::oracle());
    q.exec(QString("SELECT %1 FROM EMPLOYE ORDER BY NOM, PRENOM").arg(kCols));
    QVector<Employe> r;
    while (q.next()) r.append(depuis(q));
    return r;
}

bool Employe::emailExiste(const QString &email, const QString &sauf)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT COUNT(*) FROM EMPLOYE WHERE LOWER(EMAIL)=? AND ID_EMPLOYE<>?");
    q.addBindValue(email.trimmed().toLower());
    q.addBindValue(sauf.toInt());     // 0 si vide (création)
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
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
