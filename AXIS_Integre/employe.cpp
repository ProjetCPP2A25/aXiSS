#include "employe.h"
#include "connexion.h"
#include "axis.h"
#include "ui_axis.h"
#include "charts.h"
#include <QtWidgets>
#include <QTimer>
#include <QLocale>
#include <QPainter>
#include <QLabel>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>
#include <QFile>
#include <QDate>
#include <QPair>
#include <QCryptographicHash>
#include <QList>
#include <QSettings>

// Table Oracle EMPLOYE (SQL Developer, connexion « axis »).
// Ordre des colonnes = ordre lu par depuis().
static const char *kCols =
    "ID_EMPLOYE,NOM,PRENOM,TELEPHONE,EMAIL,GRADE,DISPONIBILITE,AGE,"
    "HEURES_TRAVAIL_SEMAINE,JOURS_CONGE_RESTANTS,POSTE";

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

    e.id = q.value(0).toString();
    e.nom = q.value(1).toString();
    e.prenom = q.value(2).toString();
    e.telephone = q.value(3).toString();
    e.email = q.value(4).toString();
    e.grade = q.value(5).toString();

    e.poste = q.value(10).toString();

    e.disponibilite = q.value(6).toString();
    e.age = entier(q.value(7));
    e.heures = entier(q.value(8));
    e.conges = entier(q.value(9));

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

    q.prepare(QString(
                  "INSERT INTO EMPLOYE(%1) VALUES(?,?,?,?,?,?,?,?,?,?,?)"
                  ).arg(kCols));

    q.addBindValue(id.toInt());
    q.addBindValue(nom);
    q.addBindValue(prenom);
    q.addBindValue(telephone);
    q.addBindValue(email);
    q.addBindValue(grade);
    q.addBindValue(disponibilite);
    q.addBindValue(age);
    q.addBindValue(heures);
    q.addBindValue(conges);
    q.addBindValue(poste);

    return lancer(q);
}


// ------------------------------------------------------------------ UPDATE
bool Employe::modifier() const
{

    QSqlQuery q(Connexion::oracle());

    q.prepare("UPDATE EMPLOYE SET "
              "NOM=?, PRENOM=?, TELEPHONE=?, EMAIL=?, "
              "GRADE=?, DISPONIBILITE=?, AGE=?, "
              "HEURES_TRAVAIL_SEMAINE=?, JOURS_CONGE_RESTANTS=?, POSTE=? "
              "WHERE ID_EMPLOYE=?");

    q.addBindValue(nom);
    q.addBindValue(prenom);
    q.addBindValue(telephone);
    q.addBindValue(email);
    q.addBindValue(grade);
    q.addBindValue(disponibilite);
    q.addBindValue(age);
    q.addBindValue(heures);
    q.addBindValue(conges);
    q.addBindValue(poste);
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

// ======================================================================
// MÉTIERS INNOVANTS : recherche, tri par grade, export Excel, statistiques
// ======================================================================

// Ordre hiérarchique des grades pour le tri (cohérent avec la contrainte CK_EMPLOYE_GRADE)
static const char *kOrdreGrade =
    "CASE GRADE WHEN 'Caporal' THEN 1 WHEN 'Sergent' THEN 2 "
    "WHEN 'Lieutenant' THEN 3 WHEN 'Capitaine' THEN 4 ELSE 5 END";

// Prépare le texte pour un LIKE : majuscules, et % _ \ sont traités comme des caractères ordinaires
static QString motifLike(const QString &texte)
{
    QString t = texte.trimmed().toUpper();
    t.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    t.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    t.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return t;
}

// Requête commune à la recherche et au tri
static QVector<Employe> requeteEmployes(const QString &texte, const QString &orderBy)
{
    QVector<Employe> r;
    const QString motif = motifLike(texte);
    const bool filtre = !motif.isEmpty();

    QString sql = QString("SELECT %1 FROM EMPLOYE").arg(kCols);
    if (filtre)
        sql += " WHERE UPPER(NOM) LIKE ? ESCAPE '\\'";      // recherche sur le nom uniquement
    sql += " ORDER BY " + orderBy;

    QSqlQuery q(Connexion::oracle());
    q.prepare(sql);
    if (filtre) {
        const QString p = "%" + motif + "%";
        q.addBindValue(p);
    }
    if (!lancer(q)) return r;
    while (q.next()) r.append(depuis(q));
    return r;
}

// ------------------------------------------------------------------ 1) RECHERCHE
QVector<Employe> Employe::rechercher(const QString &nom)
{
    return requeteEmployes(nom, "NOM, PRENOM");
}

// ------------------------------------------------------------------ 2) TRI PAR GRADE
QVector<Employe> Employe::trierParGrade(bool croissant, const QString &nom)
{
    const QString ordre = QString("%1 %2, NOM, PRENOM").arg(QString::fromLatin1(kOrdreGrade), croissant ? QStringLiteral("ASC") : QStringLiteral("DESC"));
    return requeteEmployes(nom, ordre);
}

// ------------------------------------------------------------------ 4) STATISTIQUES
QStringList Employe::disponibilites() { return {"Disponible", "En intervention", "Indisponible"}; }
QStringList Employe::grades()         { return {"Caporal", "Sergent", "Lieutenant", "Capitaine"}; }

static QMap<QString, int> compterPar(const char *colonne)
{
    QMap<QString, int> m;
    QSqlQuery q(Connexion::oracle());
    if (!q.exec(QString("SELECT %1, COUNT(*) FROM EMPLOYE GROUP BY %1").arg(colonne))) {
        s_erreur = q.lastError().text();
        return m;
    }
    while (q.next()) m[q.value(0).toString()] = q.value(1).toInt();
    return m;
}

QMap<QString, int> Employe::statistiquesParDisponibilite() { return compterPar("DISPONIBILITE"); }
QMap<QString, int> Employe::statistiquesParGrade()         { return compterPar("GRADE"); }

// ------------------------------------------------------------------ 3) EXPORT EXCEL (.xlsx)
// Un fichier .xlsx est une archive ZIP de fichiers XML : on l'écrit directement (ZIP « stocké »,
// sans compression) afin de ne dépendre d'aucune bibliothèque externe.

static quint32 crc32Xlsx(const QByteArray &donnees)
{
    static quint32 table[256];
    static bool prete = false;
    if (!prete) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        prete = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (char b : donnees) crc = table[(crc ^ quint8(b)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static void ecrire16(QByteArray &o, quint16 v)
{
    o.append(char(v & 0xFF));
    o.append(char((v >> 8) & 0xFF));
}

static void ecrire32(QByteArray &o, quint32 v)
{
    ecrire16(o, quint16(v & 0xFFFF));
    ecrire16(o, quint16(v >> 16));
}

static QByteArray creerZip(const QVector<QPair<QString, QByteArray>> &fichiers)
{
    QByteArray zip, central;
    for (const auto &f : fichiers) {
        const QByteArray nom = f.first.toUtf8();
        const quint32 crc = crc32Xlsx(f.second);
        const quint32 taille = quint32(f.second.size());
        const quint32 decalage = quint32(zip.size());

        // En-tête local
        ecrire32(zip, 0x04034b50);
        ecrire16(zip, 20); ecrire16(zip, 0); ecrire16(zip, 0);      // version, options, méthode 0 = stocké
        ecrire16(zip, 0); ecrire16(zip, 0x21);                      // heure, date (01/01/1980)
        ecrire32(zip, crc); ecrire32(zip, taille); ecrire32(zip, taille);
        ecrire16(zip, quint16(nom.size())); ecrire16(zip, 0);
        zip.append(nom);
        zip.append(f.second);

        // Entrée du répertoire central
        ecrire32(central, 0x02014b50);
        ecrire16(central, 20); ecrire16(central, 20); ecrire16(central, 0); ecrire16(central, 0);
        ecrire16(central, 0); ecrire16(central, 0x21);
        ecrire32(central, crc); ecrire32(central, taille); ecrire32(central, taille);
        ecrire16(central, quint16(nom.size())); ecrire16(central, 0); ecrire16(central, 0);
        ecrire16(central, 0); ecrire16(central, 0); ecrire32(central, 0);
        ecrire32(central, decalage);
        central.append(nom);
    }
    const quint32 debutCentral = quint32(zip.size());
    zip.append(central);
    ecrire32(zip, 0x06054b50);
    ecrire16(zip, 0); ecrire16(zip, 0);
    ecrire16(zip, quint16(fichiers.size())); ecrire16(zip, quint16(fichiers.size()));
    ecrire32(zip, quint32(central.size()));
    ecrire32(zip, debutCentral);
    ecrire16(zip, 0);
    return zip;
}

// Styles de cellules (index dans cellXfs de styles.xml ci-dessous)
enum StyleXlsx { StDefaut = 0, StTitre, StSousTitre, StLibelle, StValeur, StParagraphe, StSignature };

static QString celluleTexte(const QString &ref, const QString &texte, int style)
{
    return QString("<c r=\"%1\" s=\"%2\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%3</t></is></c>")
        .arg(ref, QString::number(style), texte.toHtmlEscaped());
}

static QString ligneXlsx(int numero, const QString &cellules, int hauteur = 0)
{
    const QString ht = hauteur > 0 ? QString(" ht=\"%1\" customHeight=\"1\"").arg(hauteur) : QString();
    return QString("<row r=\"%1\"%2>%3</row>").arg(QString::number(numero), ht, cellules);
}

bool Employe::exporterAttestationExcel(const QString &chemin) const
{
    if (id.isEmpty()) { s_erreur = "aucun employé sélectionné."; return false; }

    const QString date = QDate::currentDate().toString("dd/MM/yyyy");
    const QString posteAff = poste.isEmpty() ? QStringLiteral("-") : poste;

    QString lignes;
    QStringList fusions;
    int n = 1;

    lignes += ligneXlsx(n, celluleTexte("A1", "A.X.I.S - RedFlame Protection", StTitre), 34);
    fusions << "A1:B1"; ++n;
    lignes += ligneXlsx(n, celluleTexte("A2", "ATTESTATION DE PRÉSENCE", StSousTitre), 28);
    fusions << "A2:B2"; ++n;
    lignes += ligneXlsx(n, celluleTexte("A3", "Établie le " + date, StDefaut)); ++n;
    ++n;                                                            // ligne vide

    const QVector<QPair<QString, QString>> champs = {
        {"Matricule", id}, {"Nom", nom}, {"Prénom", prenom}, {"Grade", grade}, {"Poste", posteAff},
        {"Email", email}, {"Téléphone", telephone}, {"Âge", QString::number(age) + " ans"},
        {"Disponibilité", disponibilite},
        {"Heures de travail par semaine", QString::number(heures) + " h"},
        {"Congés restants", QString::number(conges) + " j"}};
    for (const auto &ch : champs) {
        const QString r = QString::number(n);
        lignes += ligneXlsx(n, celluleTexte("A" + r, ch.first, StLibelle) + celluleTexte("B" + r, ch.second, StValeur), 22);
        ++n;
    }
    ++n;                                                            // ligne vide

    const QString texte = QString("Nous attestons que %1, de grade %2, fait partie de l'effectif de la caserne "
                                  "RedFlame Protection. Statut actuel : %3 ; durée de travail hebdomadaire : %4 heures.")
                              .arg(nomComplet(), grade, disponibilite, QString::number(heures));
    lignes += ligneXlsx(n, celluleTexte("A" + QString::number(n), texte, StParagraphe), 64);
    fusions << QString("A%1:B%1").arg(n); ++n;
    ++n;
    lignes += ligneXlsx(n, celluleTexte("B" + QString::number(n), "Le chef de caserne", StSignature)); ++n;

    QString fusionsXml = QString("<mergeCells count=\"%1\">").arg(fusions.size());
    for (const QString &f : fusions) fusionsXml += "<mergeCell ref=\"" + f + "\"/>";
    fusionsXml += "</mergeCells>";

    const QString entete = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    const QString nsMain = QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    const QString nsRel  = QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships");
    const QString nsDoc  = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");

    const QString feuille = entete +
        "<worksheet xmlns=\"" + nsMain + "\">"
        "<sheetViews><sheetView workbookViewId=\"0\" showGridLines=\"0\"/></sheetViews>"
        "<sheetFormatPr defaultRowHeight=\"18\"/>"
        "<cols><col min=\"1\" max=\"1\" width=\"32\" customWidth=\"1\"/><col min=\"2\" max=\"2\" width=\"48\" customWidth=\"1\"/></cols>"
        "<sheetData>" + lignes + "</sheetData>" + fusionsXml +
        "<pageMargins left=\"0.7\" right=\"0.7\" top=\"0.75\" bottom=\"0.75\" header=\"0.3\" footer=\"0.3\"/>"
        "</worksheet>";

    const QString styles = entete + QStringLiteral(R"xml(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<fonts count="4">
<font><sz val="11"/><name val="Calibri"/></font>
<font><b/><sz val="18"/><color rgb="FFC00000"/><name val="Calibri"/></font>
<font><b/><sz val="14"/><name val="Calibri"/></font>
<font><b/><sz val="11"/><name val="Calibri"/></font>
</fonts>
<fills count="3">
<fill><patternFill patternType="none"/></fill>
<fill><patternFill patternType="gray125"/></fill>
<fill><patternFill patternType="solid"><fgColor rgb="FFEEEEEE"/><bgColor indexed="64"/></patternFill></fill>
</fills>
<borders count="2">
<border><left/><right/><top/><bottom/><diagonal/></border>
<border><left style="thin"><color auto="1"/></left><right style="thin"><color auto="1"/></right><top style="thin"><color auto="1"/></top><bottom style="thin"><color auto="1"/></bottom><diagonal/></border>
</borders>
<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>
<cellXfs count="7">
<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>
<xf numFmtId="0" fontId="1" fillId="0" borderId="0" xfId="0" applyFont="1" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf>
<xf numFmtId="0" fontId="2" fillId="0" borderId="0" xfId="0" applyFont="1" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf>
<xf numFmtId="0" fontId="3" fillId="2" borderId="1" xfId="0" applyFont="1" applyFill="1" applyBorder="1" applyAlignment="1"><alignment vertical="center"/></xf>
<xf numFmtId="0" fontId="0" fillId="0" borderId="1" xfId="0" applyBorder="1" applyAlignment="1"><alignment horizontal="left" vertical="center" wrapText="1"/></xf>
<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0" applyAlignment="1"><alignment horizontal="left" vertical="top" wrapText="1"/></xf>
<xf numFmtId="0" fontId="3" fillId="0" borderId="0" xfId="0" applyFont="1" applyAlignment="1"><alignment horizontal="right"/></xf>
</cellXfs>
<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>
</styleSheet>)xml");

    const QString typesXml = entete +
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>";

    const QString relsRacine = entete +
        "<Relationships xmlns=\"" + nsRel + "\">"
        "<Relationship Id=\"rId1\" Type=\"" + nsDoc + "/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>";

    const QString classeur = entete +
        "<workbook xmlns=\"" + nsMain + "\" xmlns:r=\"" + nsDoc + "\">"
        "<sheets><sheet name=\"Attestation\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";

    const QString relsClasseur = entete +
        "<Relationships xmlns=\"" + nsRel + "\">"
        "<Relationship Id=\"rId1\" Type=\"" + nsDoc + "/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"" + nsDoc + "/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";

    const QByteArray zip = creerZip({
        {"[Content_Types].xml", typesXml.toUtf8()},
        {"_rels/.rels", relsRacine.toUtf8()},
        {"xl/workbook.xml", classeur.toUtf8()},
        {"xl/_rels/workbook.xml.rels", relsClasseur.toUtf8()},
        {"xl/styles.xml", styles.toUtf8()},
        {"xl/worksheets/sheet1.xml", feuille.toUtf8()}});

    QFile f(chemin);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        s_erreur = "impossible d'écrire le fichier (est-il ouvert dans Excel ?).";
        return false;
    }
    const bool ok = f.write(zip) == zip.size();
    f.close();
    if (!ok) s_erreur = "écriture incomplète du fichier.";
    return ok;
}

// ======================================================================
// AUTHENTIFICATION, MOT DE PASSE OUBLIÉ, DROITS D'ACCÈS
// (colonnes POSTE et MOT_DE_PASSE : voir sql/oracle_login.sql)
// ======================================================================

// Empreinte SHA-256 salée avec l'identifiant : le mot de passe n'est jamais stocké en clair
static QString hacherMotDePasse(const QString &id, const QString &motDePasse)
{
    const QByteArray donnees = (QString::number(id.toInt()) + ":" + motDePasse).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(donnees, QCryptographicHash::Sha256).toHex());
}

bool Employe::authentifier(const QString &email, const QString &motDePasse, SessionEmploye &session)
{
    static const QString refus = QStringLiteral("Email ou mot de passe incorrect.");

    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT ID_EMPLOYE, NOM, PRENOM, MOT_DE_PASSE, POSTE "
              "FROM EMPLOYE WHERE LOWER(EMAIL)=?");
    q.addBindValue(email.trimmed().toLower());
    if (!lancer(q)) return false;                       // erreur SQL (ex. script oracle_login.sql non exécuté)
    if (!q.next()) { s_erreur = refus; return false; }

    const QString id = q.value(0).toString();
    const QString hash = q.value(3).toString();
    if (hash.isEmpty()) {
        s_erreur = "Aucun mot de passe n'est défini pour ce compte : cliquez sur « Mot de passe oublié ? ».";
        return false;
    }
    if (hash != hacherMotDePasse(id, motDePasse)) { s_erreur = refus; return false; }

    // Le poste de l'employé donne ses droits : il doit avoir accès à au moins un module
    const QString poste = q.value(4).toString();
    bool acces = false;
    for (int m = 0; m < 5; ++m) acces = acces || moduleAutorise(poste, m);
    if (!acces) {
        s_erreur = poste.isEmpty()
            ? QStringLiteral("Aucun poste n'est défini pour ce compte.")
            : QStringLiteral("Le poste « %1 » n'a accès à aucun module de l'application.").arg(poste);
        return false;
    }

    session.id = id;
    session.nomComplet = (q.value(2).toString() + " " + q.value(1).toString()).trimmed();
    session.role = poste;
    return true;
}

bool Employe::reinitialiserMotDePasse(const QString &email, const QString &idEmploye, const QString &nouveau)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT ID_EMPLOYE FROM EMPLOYE WHERE LOWER(EMAIL)=? AND ID_EMPLOYE=?");
    q.addBindValue(email.trimmed().toLower());
    q.addBindValue(idEmploye.trimmed().toInt());
    if (!lancer(q)) return false;
    if (!q.next()) { s_erreur = "Email ou ID incorrect."; return false; }

    return changerMotDePasse(q.value(0).toString(), nouveau);
}

bool Employe::changerMotDePasse(const QString &id, const QString &nouveau)
{
    QSqlQuery u(Connexion::oracle());
    u.prepare("UPDATE EMPLOYE SET MOT_DE_PASSE=? WHERE ID_EMPLOYE=?");
    u.addBindValue(hacherMotDePasse(id, nouveau));
    u.addBindValue(id.toInt());
    return lancer(u) && u.numRowsAffected() > 0;
}

// Droits d'accès (cahier de spécification, tableau des exigences fonctionnelles) :
//   Employés      : Administrateur, Chef de caserne
//   Interventions : Opérateur de dispatch, Chef de caserne
//   Véhicules     : Chef de caserne, Administrateur
//   Matériels     : Magasinier, Chef de caserne
//   Gardes        : Administrateur, Chef de caserne
bool Employe::moduleAutorise(const QString &poste, int module)
{
    static const QMap<QString, QList<int>> droits = {
        {"Administrateur",         {0, 2, 4}},
        {"Chef de caserne",        {0, 1, 2, 3, 4}},
        {"Opérateur de dispatch",  {1}},
        {"Magasinier",             {3}}
    };
    return droits.value(poste).contains(module);
}

// ======================================================================
// MINI-JOURNAL D'ACTIVITÉ
// Aucune table Oracle ni fichier à créer : les 50 dernières actions sont gardées dans les
// paramètres de l'application (QSettings), donc conservées après la fermeture du programme.
// ======================================================================

static const int kMaxJournal = 50;

bool Employe::journaliser(const QString &utilisateur, const QString &action)
{
    QSettings p("RedFlame", "AXIS");
    QStringList lignes = p.value("journal").toStringList();
    lignes.prepend(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "|" + utilisateur + "|" + action);
    while (lignes.size() > kMaxJournal) lignes.removeLast();
    p.setValue("journal", lignes);
    return true;
}

QVector<EntreeJournal> Employe::dernieresActivites(int nombre)
{
    QVector<EntreeJournal> r;
    const QStringList lignes = QSettings("RedFlame", "AXIS").value("journal").toStringList();   // du plus récent au plus ancien
    for (const QString &l : lignes) {
        if (r.size() >= nombre) break;
        const QStringList champs = l.split('|');
        if (champs.size() < 3) continue;
        EntreeJournal e;
        e.date = QDateTime::fromString(champs[0], "yyyy-MM-dd HH:mm:ss");
        e.utilisateur = champs[1];
        e.action = champs.mid(2).join("|");
        r.append(e);
    }
    return r;
}

// ======================================================================
// INTERFACE DU MODULE EMPLOYÉS (fonctions de la fenêtre AXIS liées aux employés) :
// recherche / tri, statistiques, export de l'attestation, bandeau et journal du menu principal
// ======================================================================

// ------------------------------------------------------------------ Métiers innovants (interface)
void AXIS::initialiserMetiersEmployes()
{
    // --- Recherche + tri : barre d'outils entre le titre et le tableau
    leRechercheEmpl_ = new QLineEdit(ui->frmListeEmpl);
    leRechercheEmpl_->setPlaceholderText("Rechercher un employé par nom");
    leRechercheEmpl_->setClearButtonEnabled(true);
    cmbTriEmpl_ = new QComboBox(ui->frmListeEmpl);
    cmbTriEmpl_->addItems({"Tri par grade : croissant", "Tri par grade : décroissant"});

    auto *barre = new QHBoxLayout;
    barre->setSpacing(8);
    barre->addWidget(leRechercheEmpl_, 1);
    barre->addWidget(cmbTriEmpl_);
    auto *btnExport = new QPushButton("Exporter l'attestation (Excel)", ui->frmListeEmpl);
    btnExport->setCursor(Qt::PointingHandCursor);
    barre->addWidget(btnExport);

    // L'attestation concerne l'employé dont la ligne est sélectionnée dans le tableau
    ui->tblEmpl->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblEmpl->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(btnExport, &QPushButton::clicked, this, [this] {
        const int r = ui->tblEmpl->currentRow();
        if (r < 0 || !ui->tblEmpl->item(r, 0)) {
            QMessageBox::information(this, "Attestation", "Cliquez d'abord sur la ligne de l'employé dans le tableau.");
            return;
        }
        exporterAttestationEmploye(ui->tblEmpl->item(r, 0)->text());
    });
    qobject_cast<QVBoxLayout *>(ui->frmListeEmpl->layout())->insertLayout(1, barre);

    connect(leRechercheEmpl_, &QLineEdit::textChanged, this, [this] { rafraichirListe(); });
    connect(cmbTriEmpl_, &QComboBox::currentIndexChanged, this, [this] { rafraichirListe(); });

    // --- Statistiques : panneau sous la liste (anneau par disponibilité, barres verticales par grade)
    auto *panneau = new QFrame(ui->contentEmpl);
    panneau->setProperty("panel", true);
    auto *vPanneau = new QVBoxLayout(panneau);
    vPanneau->setContentsMargins(16, 14, 16, 16);
    vPanneau->setSpacing(10);

    auto *titre = new QLabel("Statistiques des employés", panneau);
    titre->setProperty("h2", true);
    vPanneau->addWidget(titre);

    const QString styleSousTitre = "font-weight: bold; color: #b0b3b8; background: transparent; border: none;";

    auto *colDispo = new QVBoxLayout;
    auto *lblDispo = new QLabel("Répartition par disponibilité", panneau);
    lblDispo->setStyleSheet(styleSousTitre);
    colDispo->addWidget(lblDispo);
    auto *ligneAnneau = new QHBoxLayout;
    ligneAnneau->setSpacing(14);
    donutDispoEmpl_ = new QLabel(panneau);
    donutDispoEmpl_->setFixedSize(120, 120);
    donutDispoEmpl_->setStyleSheet("background: transparent; border: none;");
    legendDispoEmpl_ = new QLabel(panneau);
    legendDispoEmpl_->setStyleSheet("background: transparent; border: none;");
    ligneAnneau->addWidget(donutDispoEmpl_, 0, Qt::AlignTop);
    ligneAnneau->addWidget(legendDispoEmpl_, 1, Qt::AlignVCenter);
    colDispo->addLayout(ligneAnneau);
    colDispo->addStretch(1);

    auto *colGrade = new QVBoxLayout;
    auto *lblGrade = new QLabel("Répartition par grade", panneau);
    lblGrade->setStyleSheet(styleSousTitre);
    colGrade->addWidget(lblGrade);
    barresGradeEmpl_ = new BarChartWidget(panneau);
    barresGradeEmpl_->setMinimumHeight(230);
    colGrade->addWidget(barresGradeEmpl_, 1);

    auto *ligne = new QHBoxLayout;
    ligne->setSpacing(24);
    ligne->addLayout(colDispo, 1);
    ligne->addLayout(colGrade, 1);
    vPanneau->addLayout(ligne);

    qobject_cast<QVBoxLayout *>(ui->contentEmpl->layout())->addWidget(panneau);
}

// Anneau (répartition des employés par disponibilité) : arcs colorés avec le total au centre
static QPixmap anneauEmployes(const QVector<int> &valeurs, const QVector<QColor> &couleurs, int taille)
{
    QPixmap px(taille, taille);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    int total = 0;
    for (int v : valeurs) total += v;
    const int epaisseur = taille / 6;
    const QRectF r(epaisseur / 2.0, epaisseur / 2.0, taille - epaisseur, taille - epaisseur);

    if (total == 0) {
        p.setPen(QPen(QColor("#3a3f45"), epaisseur));
        p.drawEllipse(r);
    } else {
        int debut = 90 * 16;                                       // départ en haut, sens horaire
        for (int i = 0; i < valeurs.size() && i < couleurs.size(); ++i) {
            const int etendue = -qRound(5760.0 * valeurs[i] / total);
            QPen stylo(couleurs[i], epaisseur);
            stylo.setCapStyle(Qt::FlatCap);
            p.setPen(stylo);
            p.drawArc(r, debut, etendue);
            debut += etendue;
        }
    }
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(taille / 4);
    p.setFont(f);
    p.setPen(QColor("#E5E7EB"));
    p.drawText(px.rect(), Qt::AlignCenter, QString::number(total));
    return px;
}

void AXIS::rafraichirStatsEmployes()
{
    if (!donutDispoEmpl_ || !barresGradeEmpl_) return;

    // Anneau : disponibilité (vert / rouge / orange, comme les pastilles du tableau)
    const QStringList dispos = Employe::disponibilites();
    const QVector<QColor> couleurs = {QColor("#1f9d55"), QColor("#c81e1e"), QColor("#c2670f")};
    const QMap<QString, int> sd = Employe::statistiquesParDisponibilite();
    int total = 0;
    for (const QString &k : dispos) total += sd.value(k);

    QVector<int> valeurs;
    QString html = "<table cellspacing='4'>";
    for (int i = 0; i < dispos.size(); ++i) {
        const int n = sd.value(dispos[i]);
        valeurs.push_back(n);
        const int pct = total ? qRound(100.0 * n / total) : 0;
        html += QString("<tr><td><span style='color:%1'>●</span></td><td>%2 <b>%3</b> (%4%)</td></tr>")
                    .arg(couleurs[i].name(), dispos[i]).arg(n).arg(pct);
    }
    donutDispoEmpl_->setPixmap(anneauEmployes(valeurs, couleurs, 120));
    legendDispoEmpl_->setTextFormat(Qt::RichText);
    legendDispoEmpl_->setText(html + "</table>");

    // Barres verticales : effectif par grade
    const QMap<QString, int> sg = Employe::statistiquesParGrade();
    QVector<QPair<QString, int>> barres;
    for (const QString &g : Employe::grades()) barres.append({g, sg.value(g)});
    barresGradeEmpl_->setData(barres);
}

void AXIS::exporterAttestationEmploye(const QString &id)
{
    const Employe e = Employe::trouver(id);
    if (e.id.isEmpty()) return;

    QString base = e.nom + "_" + e.prenom;
    base.replace(QRegularExpression(QStringLiteral("[^\\p{L}0-9]+")), "_");
    QString chemin = QFileDialog::getSaveFileName(this, "Exporter l'attestation de présence",
                                                  "attestation_presence_" + base + ".xlsx", "Excel (*.xlsx)");
    if (chemin.isEmpty()) return;
    if (!chemin.endsWith(".xlsx", Qt::CaseInsensitive)) chemin += ".xlsx";

    if (!e.exporterAttestationExcel(chemin)) {
        QMessageBox::critical(this, "Erreur", "Export impossible : " + Employe::derniereErreur());
        return;
    }
    journaliserActivite("a exporté l'attestation de présence de " + e.nomComplet());
    QMessageBox::information(this, "Export", "Attestation de présence créée (ouvrable dans Excel) :\n" + chemin);
}

// ------------------------------------------------------------------ Menu principal : bandeau + mini-journal
void AXIS::initialiserMenuPrincipal()
{
    auto *lay = qobject_cast<QVBoxLayout *>(ui->PMenuEmpl->layout());

    // Bandeau : sous le titre « Bonjour Karim, Chef de caserne », la date, l'heure en direct et « Bonne garde / Bonne nuit »
    lblHorlogeMenu_ = new QLabel(ui->PMenuEmpl);
    lblHorlogeMenu_->setTextFormat(Qt::RichText);
    lblHorlogeMenu_->setStyleSheet("color: #9AA4AE; font-size: 14px; background: transparent; border: none;");
    lay->insertWidget(1, lblHorlogeMenu_);

    // Mini-journal d'activité : panneau placé au-dessus du bouton Quitter
    auto *panneau = new QFrame(ui->PMenuEmpl);
    panneau->setProperty("panel", true);
    panneau->setMinimumWidth(640);
    panneau->setMaximumWidth(900);
    auto *v = new QVBoxLayout(panneau);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(6);
    auto *titre = new QLabel("Activité récente", panneau);
    titre->setProperty("h2", true);
    lblJournalMenu_ = new QLabel(panneau);
    lblJournalMenu_->setTextFormat(Qt::RichText);
    lblJournalMenu_->setWordWrap(true);
    lblJournalMenu_->setStyleSheet("color: #E5E7EB; font-size: 13px; background: transparent; border: none;");
    v->addWidget(titre);
    v->addWidget(lblJournalMenu_);
    lay->insertWidget(lay->count() - 1, panneau, 0, Qt::AlignHCenter);

    // L'heure se met à jour chaque seconde
    auto *horloge = new QTimer(this);
    connect(horloge, &QTimer::timeout, this, [this] { majBandeauMenu(); });
    horloge->start(1000);

    majBandeauMenu();
    rafraichirJournalMenu();
}

void AXIS::majBandeauMenu()
{
    if (!lblHorlogeMenu_) return;
    const QDateTime maintenant = QDateTime::currentDateTime();
    const int h = maintenant.time().hour();
    const bool jour = h >= 6 && h < 20;
    const QString salut = h < 18 ? QStringLiteral("Bonjour") : QStringLiteral("Bonsoir");
    const QString prenom = session_.nomComplet.section(' ', 0, 0);

    ui->lblBienvenueEmpl->setText(prenom.isEmpty() ? salut : salut + " " + prenom + ", " + session_.role);

    QString date = QLocale(QLocale::French).toString(maintenant.date(), "dddd d MMMM yyyy");
    if (!date.isEmpty()) date[0] = date[0].toUpper();
    const QString couleur = jour ? QStringLiteral("#f5b301") : QStringLiteral("#6ea8fe");
    const QString souhait = jour ? QStringLiteral("☀ Bonne garde") : QStringLiteral("☾ Bonne nuit");
    lblHorlogeMenu_->setText(QString("%1 &nbsp;·&nbsp; <b style='color:#E5E7EB'>%2</b> &nbsp;·&nbsp; <span style='color:%3'>%4</span>")
                                 .arg(date, maintenant.toString("HH:mm:ss"), couleur, souhait));
}

void AXIS::rafraichirJournalMenu()
{
    if (!lblJournalMenu_) return;
    const QVector<EntreeJournal> entrees = Employe::dernieresActivites(8);
    if (entrees.isEmpty()) {
        lblJournalMenu_->setText("<span style='color:#9AA4AE'>Aucune activité enregistrée pour le moment.</span>");
        return;
    }
    const QDate aujourdhui = QDate::currentDate();
    QString html;
    for (const EntreeJournal &e : entrees) {
        const QString quand = e.date.date() == aujourdhui
            ? "à " + e.date.toString("HH:mm")
            : "le " + e.date.toString("dd/MM") + " à " + e.date.toString("HH:mm");
        html += QString("• <b>%1</b> %2 <span style='color:#9AA4AE'>%3</span><br>")
                    .arg(e.utilisateur.section(' ', 0, 0).toHtmlEscaped(), e.action.toHtmlEscaped(), quand);
    }
    lblJournalMenu_->setText(html);
}

void AXIS::journaliserActivite(const QString &action)
{
    if (session_.nomComplet.isEmpty()) return;
    Employe::journaliser(session_.nomComplet, action);
    rafraichirJournalMenu();
}

void AXIS::definirUtilisateur(const SessionEmploye &session)
{
    session_ = session;
    majBandeauMenu();
    journaliserActivite("s'est connecté(e)");
    setWindowTitle("A.X.I.S - RedFlame Protection  |  " + session.nomComplet + " (" + session.role + ")");

    // Un bouton de module est désactivé si le rôle n'y a pas droit
    const QList<QPair<QPushButton *, int>> boutons = {
        {ui->btnMenuEmpl, 0}, {ui->btnMenuInterv, 1}, {ui->btnMenuVehicule, 2},
        {ui->btnMenuMateriel, 3}, {ui->btnMenuGarde, 4}};
    for (const auto &b : boutons) {
        const bool ok = Employe::moduleAutorise(session.role, b.second);
        b.first->setEnabled(ok);
        b.first->setToolTip(ok ? QString() : QStringLiteral("Accès refusé pour votre rôle (%1).").arg(session.role));
    }
}

// ======================================================================
// PAGE DE CONNEXION (LoginDialog)
// ======================================================================

static const int kMaxEchecs = 5;

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("A.X.I.S - Connexion");
    setMinimumSize(560, 780);

    auto *racine = new QVBoxLayout(this);
    auto *carte = new QFrame(this);
    carte->setProperty("panel", true);
    carte->setFixedWidth(470);
    racine->addStretch(1);
    racine->addWidget(carte, 0, Qt::AlignHCenter);
    racine->addStretch(1);

    auto *v = new QVBoxLayout(carte);
    v->setContentsMargins(36, 36, 36, 30);
    v->setSpacing(12);

    const QString transparent = "background: transparent; border: none;";

    auto *logo = new QLabel(carte);
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet(transparent);
    const QPixmap px(":/logo.png");
    if (!px.isNull()) logo->setPixmap(px.scaledToWidth(300, Qt::SmoothTransformation));
    v->addWidget(logo);

    auto *societe = new QLabel("RedFlame Protection", carte);
    societe->setAlignment(Qt::AlignCenter);
    societe->setStyleSheet("color: #9AA4AE; " + transparent);
    v->addWidget(societe);

    auto *titre = new QLabel("Connexion", carte);
    titre->setProperty("h2", true);
    titre->setStyleSheet(transparent);
    v->addWidget(titre);

    leEmail_ = new QLineEdit(carte);
    leEmail_->setPlaceholderText("Email");
    leEmail_->setMaxLength(80);
    leMdp_ = new QLineEdit(carte);
    leMdp_->setPlaceholderText("Mot de passe");
    leMdp_->setEchoMode(QLineEdit::Password);
    leMdp_->setMaxLength(60);
    v->addWidget(leEmail_);
    v->addWidget(leMdp_);

    auto *btnConnexion = new QPushButton("Se connecter", carte);
    btnConnexion->setProperty("primary", true);
    btnConnexion->setCursor(Qt::PointingHandCursor);
    btnConnexion->setDefault(true);
    v->addWidget(btnConnexion);

    auto *btnOublie = new QPushButton("Mot de passe oublié ?", carte);
    btnOublie->setProperty("link", true);
    btnOublie->setCursor(Qt::PointingHandCursor);
    v->addWidget(btnOublie);

    auto *btnQuitter = new QPushButton("Quitter", carte);
    btnQuitter->setCursor(Qt::PointingHandCursor);
    v->addWidget(btnQuitter);

    connect(btnConnexion, &QPushButton::clicked, this, &LoginDialog::seConnecter);
    connect(leEmail_, &QLineEdit::returnPressed, this, [this] { leMdp_->setFocus(); });
    connect(leMdp_, &QLineEdit::returnPressed, this, &LoginDialog::seConnecter);
    connect(btnOublie, &QPushButton::clicked, this, &LoginDialog::motDePasseOublie);
    connect(btnQuitter, &QPushButton::clicked, this, &QDialog::reject);
}

void LoginDialog::seConnecter()
{
    const QString email = leEmail_->text().trimmed();
    const QString mdp = leMdp_->text();

    if (email.isEmpty() || mdp.isEmpty()) {
        QMessageBox::warning(this, "Connexion", "Saisissez votre email et votre mot de passe.");
        return;
    }
    if (!Employe::emailValide(email)) {
        QMessageBox::warning(this, "Connexion", "L'email doit avoir la forme nom@email.com.");
        return;
    }

    SessionEmploye s;
    if (!Employe::authentifier(email, mdp, s)) {
        leMdp_->clear();
        leMdp_->setFocus();
        if (++echecs_ >= kMaxEchecs) {
            QMessageBox::critical(this, "Connexion", "Trop de tentatives échouées. L'application va se fermer.");
            reject();
            return;
        }
        QMessageBox::warning(this, "Connexion refusée",
                             Employe::derniereErreur() + QString("\n\nTentative %1 sur %2.").arg(echecs_).arg(kMaxEchecs));
        return;
    }

    session_ = s;
    accept();
}

// « Mot de passe oublié ? » : vérification de l'identité (email + téléphone du compte, dans Oracle),
// puis choix direct d'un nouveau mot de passe.
void LoginDialog::motDePasseOublie()
{
    QDialog d(this);
    d.setWindowTitle("Mot de passe oublié");
    d.setMinimumWidth(430);
    auto *v = new QVBoxLayout(&d);
    v->setSpacing(10);

    auto *info = new QLabel("Pour vérifier votre identité, saisissez l'email et l'ID de votre compte, "
                            "puis choisissez un nouveau mot de passe.", &d);
    info->setWordWrap(true);
    v->addWidget(info);

    auto *leMail = new QLineEdit(leEmail_->text().trimmed(), &d);
    leMail->setPlaceholderText("Email");
    auto *leId = new QLineEdit(&d);
    leId->setPlaceholderText("ID de l'employé");
    leId->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), &d));
    leId->setMaxLength(9);
    auto *leNouveau = new QLineEdit(&d);
    leNouveau->setPlaceholderText("Nouveau mot de passe (4 caractères minimum)");
    leNouveau->setEchoMode(QLineEdit::Password);
    auto *leConfirm = new QLineEdit(&d);
    leConfirm->setPlaceholderText("Confirmer le nouveau mot de passe");
    leConfirm->setEchoMode(QLineEdit::Password);
    for (QLineEdit *e : {leMail, leId, leNouveau, leConfirm}) v->addWidget(e);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    btns->button(QDialogButtonBox::Ok)->setText("Valider");
    btns->button(QDialogButtonBox::Cancel)->setText("Annuler");
    v->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, &d, &QDialog::reject);

    // La fenêtre ne se ferme que si les champs sont valides et que l'identité est confirmée par Oracle
    connect(btns, &QDialogButtonBox::accepted, &d, [&] {
        const QString email = leMail->text().trimmed();
        if (!Employe::emailValide(email)) {
            QMessageBox::warning(&d, "Mot de passe oublié", "L'email doit avoir la forme nom@email.com.");
            return;
        }
        if (leId->text().trimmed().isEmpty()) {
            QMessageBox::warning(&d, "Mot de passe oublié", "Saisissez l'ID de l'employé (nombre).");
            return;
        }
        if (leNouveau->text().length() < 4) {
            QMessageBox::warning(&d, "Mot de passe oublié", "Le mot de passe doit contenir au moins 4 caractères.");
            return;
        }
        if (leNouveau->text() != leConfirm->text()) {
            QMessageBox::warning(&d, "Mot de passe oublié", "Les deux mots de passe sont différents.");
            return;
        }
        if (!Employe::reinitialiserMotDePasse(email, leId->text(), leNouveau->text())) {
            QMessageBox::warning(&d, "Mot de passe oublié", Employe::derniereErreur());
            return;
        }
        d.accept();
    });

    if (d.exec() != QDialog::Accepted) return;

    leEmail_->setText(leMail->text().trimmed());
    leMdp_->clear();
    leMdp_->setFocus();
    QMessageBox::information(this, "Mot de passe oublié", "Votre mot de passe a été modifié. Vous pouvez vous connecter.");
}
