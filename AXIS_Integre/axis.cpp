#include "axis.h"
#include "ui_axis.h"
#include "garde.h"

#include <QtWidgets>
#include <QDesktopServices>
#include <QRegularExpressionValidator>
#include <QSslSocket>
#include <QTcpSocket>
#include <QThreadPool>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QUrlQuery>

#include "vehicule.h"
#include "entretiendialog.h"

#include <algorithm>
#include <functional>
#include <QBoxLayout>

// ==========================================
// Outils d'affichage du module Interventions
// ==========================================
namespace {

// Colonnes du tableau des interventions (tableInterventions)
enum Col { ColId, ColType, ColLieu, ColGravite, ColGps, ColReception, ColFin, ColDuree, ColEtat, ColActions,
           NbColonnes };

const char *const kFormatDate = "dd/MM/yyyy HH:mm";
const int kInterventionsParPage = 9;              // lignes visibles dans le tableau (pagination)
const int kBaseBarres = 185;                      // ligne de base et hauteur maximale des barres de statistiques
const int kHauteurMaxBarre = 140;

QColor couleurMarqueur(const QString &gravite)    // même code couleur que la légende de la carte
{
    if (gravite == Grav::Elevee)  return QColor("#D93025");
    if (gravite == Grav::Moyenne) return QColor("#F29900");
    return QColor("#1E8E3E");
}

struct Couleurs {
    const char *fond;
    const char *texte;
};

QString formatDateHeure(const QDateTime &d)         // "-" si la date est invalide
{
    return d.isValid() ? d.toString(QLatin1String(kFormatDate)) : QStringLiteral("-");
}

QString formatDateTableau(const QDateTime &d)       // « 30/09/26 » puis « 14:32 » sur deux lignes (tableau compact)
{
    return d.isValid() ? d.toString(QStringLiteral("dd/MM/yy\nHH:mm")) : QStringLiteral("-");
}

Couleurs couleursGravite(const QString &gravite)
{
    if (gravite == Grav::Elevee)  return {"#F7D6D6", "#A61B1B"};
    if (gravite == Grav::Moyenne) return {"#FCE8CC", "#9A5A00"};
    return {"#DDF3E4", "#1B7A3E"};
}

Couleurs couleursEtat(const QString &etat)
{
    return etat == Etat::EnCours ? Couleurs{"#FCE8CC", "#9A5A00"} : Couleurs{"#DDF3E4", "#1B7A3E"};
}

QLabel *creerEtiquettePastille(const QString &texte, const Couleurs &c)
{
    auto *l = new QLabel(texte);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral("background-color: %1; color: %2; border: none; border-radius: 9px;"
                                    "padding: 2px 8px; font-size: 11px;")
                         .arg(QLatin1String(c.fond), QLatin1String(c.texte)));
    return l;
}

QWidget *creerPastille(const QString &texte, const Couleurs &c)   // pastille alignée dans une cellule du tableau
{
    auto *boite = new QWidget;
    boite->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *lay = new QHBoxLayout(boite);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(creerEtiquettePastille(texte, c), 0, Qt::AlignLeft | Qt::AlignVCenter);
    return boite;
}

QToolButton *creerBoutonAction(const QString &glyphe, const QString &infobulle, const QString &survol)
{
    auto *b = new QToolButton;
    b->setText(glyphe);
    b->setToolTip(infobulle);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(26, 26);
    b->setStyleSheet(QStringLiteral("QToolButton { background-color: #181B1F; color: white; border: 1px solid #343A40;"
                                    "  border-radius: 5px; font-size: 12px; }"
                                    "QToolButton:hover { background-color: %1; border: 1px solid %1; }").arg(survol));
    return b;
}

} // namespace


// ---------- Outils du module Matériels ----------
static const QStringList kCategories = {"Lutte incendie", "Protection", "Secours"};
static const char *kDateFmt = "dd/MM/yyyy";
static const int kRowH = 58;          // hauteur des lignes du tableau des matériels

// Cellule contenant un badge (kind = ok | crit | warn)
static QWidget *badgeCell(const QString &text, const QString &kind)
{
    auto *w = new QWidget;
    w->setObjectName("cell");
    w->setAttribute(Qt::WA_StyledBackground, true);
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(4, 0, 4, 0);
    auto *b = new QLabel(text);
    const QString bg = kind == "ok" ? "#e3f4e8" : kind == "crit" ? "#fde8e8" : "#fdeedb";
    const QString fg = kind == "ok" ? "#1e7e34" : kind == "crit" ? "#c0392b" : "#b9650b";
    b->setStyleSheet(QString("QLabel { background:%1; color:%2; border:1px solid %1; border-radius:9px; padding:1px 8px; "
                             "font-size:11px; %3 }")
                         .arg(bg, fg, kind == "crit" ? "font-weight:600;" : ""));
    b->adjustSize();
    l->addWidget(b, 0, Qt::AlignLeft | Qt::AlignVCenter);
    return w;
}

static QString csvField(const QString &s)
{
    QString v = s;
    v.replace('"', "\"\"");
    return '"' + v + '"';
}

// ---------- Envoi d'email (SMTP) ----------
// Gmail : activer la validation en 2 étapes, puis créer un « mot de passe d'application »
// (compte Google > Sécurité > Mots de passe des applications) et le mettre dans SMTP_PASSWORD.
// >>> NE JAMAIS COMMITER LE VRAI MOT DE PASSE SUR GITHUB <<<
static const char *SMTP_HOST     = "smtp.gmail.com";
static const quint16 SMTP_PORT   = 465;                       // SSL direct
static const char *SMTP_USER     = "rebhia001@gmail.com";     // compte expéditeur
static const char *SMTP_PASSWORD = "hkoszkgrwoyfepev";
static const char *ALERT_TO      = "rebhia001@gmail.com";     // destinataire des alertes

// SMTP-BEGIN
// Lit une réponse SMTP (peut tenir sur plusieurs lignes « 250-... »). Renvoie le code, ou -1 en cas d'erreur.
static int smtpRead(QTcpSocket &s, QString *text = nullptr)
{
    QString all;
    for (;;) {
        while (!s.canReadLine()) {
            if (!s.waitForReadyRead(15000)) return -1;
        }
        const QString line = QString::fromUtf8(s.readLine()).trimmed();
        all += line + "\n";
        if (line.size() >= 4 && line[3] != '-') {
            if (text) *text = all;
            return line.left(3).toInt();
        }
        if (line.size() < 4) return -1;
    }
}

static bool smtpCmd(QTcpSocket &s, const QByteArray &cmd, int expected, QString *err)
{
    if (!cmd.isEmpty()) {
        s.write(cmd + "\r\n");
        if (!s.waitForBytesWritten(15000)) { *err = "Envoi impossible"; return false; }
    }
    QString txt;
    const int code = smtpRead(s, &txt);
    if (code != expected) {
        *err = QString("Réponse inattendue du serveur (%1) : %2").arg(code).arg(txt.trimmed());
        return false;
    }
    return true;
}

static bool smtpSend(const QString &host, quint16 port, bool ssl, const QString &user, const QString &password,
                     const QString &to, const QString &subject, const QString &body, QString *err)
{
    QSslSocket sock;
    if (ssl) {
        if (!QSslSocket::supportsSsl()) {
            *err = "SSL indisponible (OpenSSL manquant pour Qt)";
            return false;
        }
        sock.connectToHostEncrypted(host, port);
        if (!sock.waitForEncrypted(15000)) { *err = "Connexion SSL impossible : " + sock.errorString(); return false; }
    } else {
        sock.connectToHost(host, port);
        if (!sock.waitForConnected(15000)) { *err = "Connexion impossible : " + sock.errorString(); return false; }
    }

    if (!smtpCmd(sock, "", 220, err)) return false;
    if (!smtpCmd(sock, "EHLO localhost", 250, err)) return false;
    if (!smtpCmd(sock, "AUTH LOGIN", 334, err)) return false;
    if (!smtpCmd(sock, user.toUtf8().toBase64(), 334, err)) return false;
    if (!smtpCmd(sock, password.toUtf8().toBase64(), 235, err)) {
        *err = "Authentification refusée (vérifie l'email et le mot de passe d'application) : " + *err;
        return false;
    }
    if (!smtpCmd(sock, "MAIL FROM:<" + user.toUtf8() + ">", 250, err)) return false;
    if (!smtpCmd(sock, "RCPT TO:<" + to.toUtf8() + ">", 250, err)) return false;
    if (!smtpCmd(sock, "DATA", 354, err)) return false;

    QByteArray msg;
    msg += "From: AXIS <" + user.toUtf8() + ">\r\n";
    msg += "To: <" + to.toUtf8() + ">\r\n";
    msg += "Subject: =?UTF-8?B?" + subject.toUtf8().toBase64() + "?=\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: base64\r\n\r\n";
    const QByteArray b64 = body.toUtf8().toBase64();
    for (int i = 0; i < b64.size(); i += 76) msg += b64.mid(i, 76) + "\r\n";
    msg += ".";   // fin du message : « \r\n.\r\n »
    if (!smtpCmd(sock, msg, 250, err)) return false;

    smtpCmd(sock, "QUIT", 221, err);
    sock.disconnectFromHost();
    return true;
}
// SMTP-END

// ------------------------------------------------------------ Construction
AXIS::AXIS(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::AXIS)
{
    ui->setupUi(this);
    setWindowTitle("A.X.I.S - RedFlame Protection");

    // Logos (ressources.qrc : :/logo.png)
    chargerLogo(ui->lblLogoMenuEmpl, 200);

    // ==========================================
    // MODULE EMPLOYES
    // ==========================================
    // Barre latérale du module Employés (même structure que celle des interventions), à gauche de la page CRUD
    sidebarEmpl_ = new SidebarFrame(ui->PCrudEmpl);
    auto *layoutEmpl = static_cast<QHBoxLayout *>(ui->PCrudEmpl->layout());
    layoutEmpl->insertWidget(0, sidebarEmpl_);
    layoutEmpl->setStretch(0, 0);
    layoutEmpl->setStretch(1, 1);

    // Tableau
    QTableWidget *t = ui->tblEmpl;
    t->setColumnCount(12);
    t->setHorizontalHeaderLabels({"ID", "Nom", "Prénom", "Téléphone", "Email", "Grade", "Âge", "Poste",
                                  "H/sem", "Congés", "Disponibilité", "Actions"});
    t->verticalHeader()->hide();
    t->setShowGrid(false);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setFocusPolicy(Qt::NoFocus);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    // Contrôle de saisie à la frappe (la validation finale reste dans enregistrerEmploye)
    const QRegularExpression reNom(QStringLiteral("^[\\p{L} '\\-]*$"));
    const QRegularExpression reTel(QStringLiteral("^[0-9 ]*$"));
    ui->txtNomEmpl->setValidator(new QRegularExpressionValidator(reNom, this));
    ui->txtPrenomEmpl->setValidator(new QRegularExpressionValidator(reNom, this));
    ui->txtTelEmpl->setValidator(new QRegularExpressionValidator(reTel, this));
    ui->txtNomEmpl->setMaxLength(40);
    ui->txtPrenomEmpl->setMaxLength(40);
    ui->txtTelEmpl->setMaxLength(20);
    ui->txtEmailEmpl->setMaxLength(80);

    connect(ui->btnAjouterEmpl, &QPushButton::clicked, this, &AXIS::enregistrerEmploye);
    connect(ui->btnViderEmpl, &QPushButton::clicked, this, &AXIS::viderFormulaire);
    connect(sidebarEmpl_, &SidebarFrame::menuRequested, this, &AXIS::allerAuMenu);
    connect(sidebarEmpl_, &SidebarFrame::logoutRequested, this, &QWidget::close);
    initialiserMetiersEmployes();
    initialiserMenuPrincipal();

    // ==========================================
    // AUTRES MODULES (interface dans axis.ui)
    // ==========================================
    initialiserModuleInterventions();
    initialiserModuleVehicules();
    initialiserModuleMateriels();
    initialiserModuleGardes();

    // ----- Navigation (connect explicites, un seul .ui avec des Stacked Widget) -----
    // Menu principal : un bouton par module
    connect(ui->btnMenuEmpl, &QPushButton::clicked, this, [this] { allerAuModule(0); });
    connect(ui->btnMenuInterv, &QPushButton::clicked, this, [this] { allerAuModule(1); });
    connect(ui->btnMenuVehicule, &QPushButton::clicked, this, [this] { allerAuModule(2); });
    connect(ui->btnMenuMateriel, &QPushButton::clicked, this, [this] { allerAuModule(3); });
    connect(ui->btnMenuGarde, &QPushButton::clicked, this, [this] { allerAuModule(4); });
    connect(ui->btnQuitterMenuEmpl, &QPushButton::clicked, this, &QWidget::close);

    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(0);   // l'application s'ouvre directement sur le menu principal
}

AXIS::~AXIS()
{
    delete ui;
}

// ==========================================
// MODULE EMPLOYES
// ==========================================
// ------------------------------------------------------------------ Liste
void AXIS::rafraichirListe()
{
    QTableWidget *t = ui->tblEmpl;
    t->setRowCount(0);
    // Recherche par nom + tri par grade (fonctions de la classe Employe)
    const QString recherche = leRechercheEmpl_ ? leRechercheEmpl_->text().trimmed() : QString();
    const int tri = cmbTriEmpl_ ? cmbTriEmpl_->currentIndex() : 0;
    const QVector<Employe> liste = Employe::trierParGrade(tri == 0, recherche);   // 0 : croissant, 1 : décroissant

    for (const Employe &e : liste) {
        const int r = t->rowCount();
        t->insertRow(r);
        t->setRowHeight(r, 100);
        const QStringList v = {e.id, e.nom, e.prenom, e.telephone, e.email, e.grade, QString::number(e.age), e.poste,
                               QString::number(e.heures) + " h", QString::number(e.conges) + " j"};
        for (int c = 0; c < v.size(); ++c) {
            auto *it = new QTableWidgetItem(v[c]);
            t->setItem(r, c, it);
        }

        QString bg, fg;
        if (e.disponibilite == "Disponible") { bg = "#d6f0df"; fg = "#1e7a45"; }
        else if (e.disponibilite == "En intervention") { bg = "#fbdcdc"; fg = "#c00000"; }
        else { bg = "#fbe6cf"; fg = "#b45f00"; }
        auto *pill = new QLabel(e.disponibilite);
        pill->setAlignment(Qt::AlignCenter);
        pill->setStyleSheet(QString("background:%1;color:%2;border-radius:9px;padding:2px 8px;font-size:12px;").arg(bg, fg));
        auto *pw = new QWidget;
        auto *ph = new QHBoxLayout(pw);
        ph->setContentsMargins(4, 0, 4, 0);
        ph->addWidget(pill, 0, Qt::AlignVCenter);
        t->setCellWidget(r, 10, pw);

        auto *aw = new QWidget;
        auto *av = new QVBoxLayout(aw);
        av->setContentsMargins(4, 2, 4, 2);
        av->setSpacing(0);
        const QString id = e.id;
        auto mk = [&](const QString &txt, std::function<void()> fn) {
            auto *b = new QPushButton(txt);
            b->setProperty("link", true);
            b->setCursor(Qt::PointingHandCursor);
            connect(b, &QPushButton::clicked, this, [fn] { fn(); });
            av->addWidget(b);
        };
        mk("Afficher", [this, id] { afficherEmploye(id); });
        mk("Modifier", [this, id] { modifierEmploye(id); });
        mk("Supprimer", [this, id] { supprimerEmploye(id); });
        mk("Attestation", [this, id] { exporterAttestationEmploye(id); });
        t->setCellWidget(r, 11, aw);
    }
    rafraichirStatsEmployes();
}

// ------------------------------------------------------------------ CRUD
void AXIS::viderFormulaire()
{
    editingId_.clear();
    ui->txtNomEmpl->clear(); ui->txtPrenomEmpl->clear(); ui->txtEmailEmpl->clear(); ui->txtTelEmpl->clear();
    ui->spnAgeEmpl->setValue(30); ui->spnHeuresEmpl->setValue(40); ui->spnCongesEmpl->setValue(18);
    ui->cmbGradeEmpl->setCurrentIndex(0); ui->cmbPosteEmpl->setCurrentIndex(0); ui->cmbDispoEmpl->setCurrentIndex(0);
    ui->btnAjouterEmpl->setText("Ajouter");
}

void AXIS::enregistrerEmploye()
{
    const bool edition = !editingId_.isEmpty();
    const QString email = ui->txtEmailEmpl->text().trimmed();

    // Contrôle de saisie : on affiche toutes les erreurs d'un coup
    QStringList erreurs;
    if (!Employe::nomValide(ui->txtNomEmpl->text()))
        erreurs << "Le nom est obligatoire : lettres uniquement, 2 caractères minimum.";
    if (!Employe::nomValide(ui->txtPrenomEmpl->text()))
        erreurs << "Le prénom est obligatoire : lettres uniquement, 2 caractères minimum.";
    if (email.isEmpty())
        erreurs << "L'email est obligatoire.";
    else if (!Employe::emailValide(email))
        erreurs << "L'email doit avoir la forme nom@gmail.com ou nom@email.com.";
    else if (Employe::emailExiste(email, editingId_))
        erreurs << "Un employé utilise déjà cet email.";
    if (!Employe::telephoneValide(ui->txtTelEmpl->text()))
        erreurs << "Le téléphone doit contenir uniquement des chiffres, au moins 8 (15 au plus).";
    if (!erreurs.isEmpty()) {
        QMessageBox::warning(this, "Données invalides", "• " + erreurs.join("\n• "));
        return;
    }

    Employe e;
    e.nom = ui->txtNomEmpl->text().trimmed();
    e.prenom = ui->txtPrenomEmpl->text().trimmed();
    e.email = email;
    e.telephone = ui->txtTelEmpl->text().trimmed();
    e.age = ui->spnAgeEmpl->value();
    e.heures = ui->spnHeuresEmpl->value();
    e.conges = ui->spnCongesEmpl->value();
    e.grade = ui->cmbGradeEmpl->currentText();
    e.poste = ui->cmbPosteEmpl->currentText();
    e.disponibilite = ui->cmbDispoEmpl->currentText();

    bool ok;
    if (edition) {
        e.id = editingId_;
        ok = e.modifier();
    } else {
        ok = e.ajouter();
    }
    if (!ok) {
        QMessageBox::critical(this, "Erreur", "Opération impossible : " + Employe::derniereErreur());
        return;
    }
    journaliserActivite(QString("a %1 l'employé %2").arg(edition ? QString("modifié") : QString("ajouté"), e.nomComplet()));
    viderFormulaire();
    rafraichirListe();
}

void AXIS::modifierEmploye(const QString &id)
{
    const Employe e = Employe::trouver(id);
    if (e.id.isEmpty()) return;
    editingId_ = id;
    ui->txtNomEmpl->setText(e.nom); ui->txtPrenomEmpl->setText(e.prenom); ui->txtEmailEmpl->setText(e.email); ui->txtTelEmpl->setText(e.telephone);
    ui->spnAgeEmpl->setValue(e.age); ui->spnHeuresEmpl->setValue(e.heures); ui->spnCongesEmpl->setValue(e.conges);
    ui->cmbGradeEmpl->setCurrentText(e.grade); ui->cmbPosteEmpl->setCurrentText(e.poste);
    ui->cmbDispoEmpl->setCurrentText(e.disponibilite);
    ui->btnAjouterEmpl->setText("Enregistrer");
}

void AXIS::afficherEmploye(const QString &id)
{
    const Employe e = Employe::trouver(id);
    if (e.id.isEmpty()) return;
    QMessageBox::information(this, e.nomComplet(),
                             QString("ID : %1\nTéléphone : %2\nEmail : %3\nGrade : %4\nPoste : %5\nÂge : %6\n"
                                     "Heures/semaine : %7\nCongés restants : %8\nDisponibilité : %9")
                                 .arg(e.id, e.telephone, e.email, e.grade, e.poste).arg(e.age).arg(e.heures).arg(e.conges).arg(e.disponibilite));
}

void AXIS::supprimerEmploye(const QString &id)
{
    if (QMessageBox::question(this, "Supprimer", "Supprimer cet employé ?") != QMessageBox::Yes) return;
    const QString nomSupprime = Employe::trouver(id).nomComplet();
    if (!Employe::supprimer(id)) {
        QMessageBox::critical(this, "Erreur", "Suppression impossible : " + Employe::derniereErreur());
        return;
    }
    journaliserActivite("a supprimé l'employé " + nomSupprime);
    rafraichirListe();
}

// ==========================================
// MODULE INTERVENTIONS
// ==========================================
// Présentation du tableau / du formulaire et connexions des widgets de la page PInterv (axis.ui).
// Recherche GPS, tri, pagination, export Excel, statistiques, zones à risque et carte : voir « Fonctionnalités avancées ».
void AXIS::initialiserModuleInterventions()
{
    // ----- Tableau -----
    QTableWidget *t = ui->tableInterventions;
    t->setColumnCount(NbColonnes);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setFocusPolicy(Qt::NoFocus);
    t->setWordWrap(false);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(46);

    // Largeurs prévues pour une fenêtre de 1366 px de large (dates sur deux lignes pour gagner de la place)
    QHeaderView *h = t->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Fixed);
    h->setStretchLastSection(true);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setHighlightSections(false);
    const int largeurs[NbColonnes - 1] = {46, 80, 84, 74, 124, 78, 78, 64, 78};
    for (int c = 0; c < NbColonnes - 1; ++c)
        t->setColumnWidth(c, largeurs[c]);

    connect(t, &QTableWidget::cellDoubleClicked, this, [this](int ligne, int) {
        if (auto *it = ui->tableInterventions->item(ligne, ColId))
            afficherIntervention(it->data(Qt::UserRole).toInt());
    });

    // ----- Formulaire -----
    ui->dateReception->setDisplayFormat(QLatin1String(kFormatDate));
    ui->dateFin->setDisplayFormat(QLatin1String(kFormatDate));
    ui->dateReception->setCalendarPopup(true);
    ui->dateFin->setCalendarPopup(true);
    ui->dateFin->setStyleSheet(ui->dateFin->styleSheet()
                               + QStringLiteral("\nQDateTimeEdit:disabled { color: #5E656C; }"));

    connect(ui->btnAjouter, &QPushButton::clicked, this, &AXIS::enregistrerIntervention);
    connect(ui->btnVider, &QPushButton::clicked, this, &AXIS::viderFormulaireIntervention);
    connect(ui->comboEtat, &QComboBox::currentTextChanged, this, &AXIS::majEtatFormulaireIntervention);
    connect(ui->dateReception, &QDateTimeEdit::dateTimeChanged, this, &AXIS::majDureeIntervention);
    connect(ui->dateFin, &QDateTimeEdit::dateTimeChanged, this, &AXIS::majDureeIntervention);
    for (QLineEdit *e : {ui->editLieu, ui->editGPS})
        connect(e, &QLineEdit::returnPressed, this, &AXIS::enregistrerIntervention);

    // ----- Navigation : barre latérale de la page -----
    connect(ui->btnMenuPrincipal, &QPushButton::clicked, this, &AXIS::allerAuMenu);
    connect(ui->btnDeconnexion, &QPushButton::clicked, this, &QWidget::close);

    initialiserFonctionsAvanceesInterventions();
    viderFormulaireIntervention();
}

// La date de fin n'est saisissable que pour une intervention terminée.
void AXIS::majEtatFormulaireIntervention()
{
    ui->dateFin->setEnabled(ui->comboEtat->currentText() == Etat::Terminee);
    majDureeIntervention();
}

// La durée n'est jamais saisie : fin - réception (terminée) ou temps écoulé depuis la réception (en cours).
void AXIS::majDureeIntervention()
{
    Intervention it;
    it.reception = ui->dateReception->dateTime();
    if (ui->comboEtat->currentText() == Etat::Terminee)
        it.fin = ui->dateFin->dateTime();
    const bool coherent = !it.fin.isValid() || it.fin >= it.reception;
    ui->editDuree->setText(coherent ? it.dureeTexte() : QString());
}

// Bouton « Vider » (mode ajout) ou « Annuler » (mode modification) : retour au formulaire d'ajout vide.
void AXIS::viderFormulaireIntervention()
{
    interventionEnEdition_ = 0;
    ui->labelAjouter->setText(QStringLiteral("Ajouter"));
    ui->labelAjouter->setGeometry(ui->labelAjouter->x(), ui->labelAjouter->y(), 100, ui->labelAjouter->height());
    ui->btnAjouter->setText(QStringLiteral("Ajouter"));
    ui->btnVider->setText(QStringLiteral("Vider"));

    ui->comboType->setCurrentIndex(0);
    ui->comboGravite->setCurrentIndex(0);
    ui->comboEtat->setCurrentIndex(0);
    ui->editLieu->clear();
    ui->editGPS->clear();
    const QDateTime maintenant = QDateTime::currentDateTime();
    ui->dateReception->setDateTime(maintenant);
    ui->dateFin->setDateTime(maintenant);
    majEtatFormulaireIntervention();
}

// Lit le formulaire dans une Intervention ; renvoie false (et le message dans « erreurs ») si la saisie est invalide.
bool AXIS::lireFormulaireIntervention(Intervention &it, QString &erreurs) const
{
    it.type = ui->comboType->currentText();
    it.gravite = ui->comboGravite->currentText();
    it.etat = ui->comboEtat->currentText();
    it.lieu = ui->editLieu->text().trimmed();

    if (it.lieu.isEmpty()) {
        erreurs = QStringLiteral("Veuillez saisir le lieu de l'intervention.");
        return false;
    }
    if (!Intervention::lireGps(ui->editGPS->text(), it.lat, it.lon, &erreurs))
        return false;

    it.reception = ui->dateReception->dateTime();
    it.fin = it.etat == Etat::Terminee ? ui->dateFin->dateTime() : QDateTime();

    // Contrôle final (champs obligatoires, cohérence des dates) porté par la classe Intervention
    return it.valider(&erreurs);
}

// Bouton Ajouter / Modifier du formulaire
void AXIS::enregistrerIntervention()
{
    Intervention it;
    QString erreur;
    if (!lireFormulaireIntervention(it, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Données invalides"), erreur);
        return;
    }

    if (interventionEnEdition_ == 0) {
        if (!it.ajouter(&erreur)) {
            QMessageBox::critical(this, QStringLiteral("Ajout impossible"), erreur);
            return;
        }
    } else {
        it.id = interventionEnEdition_;
        if (!it.modifier(&erreur)) {
            QMessageBox::critical(this, QStringLiteral("Modification impossible"), erreur);
            return;
        }
    }
    viderFormulaireIntervention();
    rafraichirListeInterventions();
}

// Détails d'une intervention (avec accès direct à la modification)
void AXIS::afficherIntervention(int id)
{
    Intervention it;
    QString erreur;
    if (!Intervention::afficher(id, it, &erreur)) {
        QMessageBox::critical(this, QStringLiteral("Lecture impossible"), erreur);
        rafraichirListeInterventions();
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Intervention %1").arg(it.idText()));
    dlg.setStyleSheet(QStringLiteral(
        "QDialog { background-color: #202428; }"
        "QLabel { color: #E5E7EB; background: transparent; font-size: 13px; border: none; }"
        "QPushButton { background-color: #181B1F; color: white; border: 1px solid #343A40;"
        "  border-radius: 5px; padding: 7px 18px; min-width: 70px; font-size: 13px; }"
        "QPushButton:hover { background-color: #30353A; }"
        "QPushButton:default { background-color: #FF1717; border: none; font-weight: bold; }"
        "QPushButton:default:hover { background-color: #E31313; }"));
    dlg.setMinimumWidth(380);

    auto *racine = new QVBoxLayout(&dlg);
    racine->setContentsMargins(22, 20, 22, 18);
    racine->setSpacing(14);

    auto *titre = new QLabel(QStringLiteral("Intervention %1").arg(it.idText()));
    titre->setStyleSheet(QStringLiteral("color: #FF1717; font-size: 18px; font-weight: bold;"
                                        "background: transparent; border: none;"));
    racine->addWidget(titre);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(8);
    auto ajouterLigne = [form](const QString &cle, QWidget *valeur) {
        auto *k = new QLabel(cle);
        k->setStyleSheet(QStringLiteral("color: #AAB0B6; background: transparent; border: none;"));
        form->addRow(k, valeur);
    };
    auto texte = [](const QString &s) {
        auto *l = new QLabel(s);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };
    ajouterLigne(QStringLiteral("Type"), texte(it.type));
    ajouterLigne(QStringLiteral("Lieu"), texte(it.lieu));
    ajouterLigne(QStringLiteral("Gravité"), creerEtiquettePastille(it.gravite, couleursGravite(it.gravite)));
    ajouterLigne(QStringLiteral("Coordonnées GPS"), texte(it.gps()));
    ajouterLigne(QStringLiteral("Réception de l'alerte"), texte(formatDateHeure(it.reception)));
    ajouterLigne(QStringLiteral("Fin"), texte(formatDateHeure(it.fin)));
    ajouterLigne(QStringLiteral("Durée"), texte(it.dureeTexte()));
    ajouterLigne(QStringLiteral("État"), creerEtiquettePastille(it.etat, couleursEtat(it.etat)));
    racine->addLayout(form);

    // Historique : autres interventions survenues au même endroit (rayon de 300 m)
    const QVector<Intervention> historique = Intervention::historiqueProche(interventions_, it, 300.0);
    auto *titreHisto = new QLabel(historique.isEmpty()
                                      ? QStringLiteral("Historique du lieu : aucune autre intervention (rayon 300 m)")
                                      : QStringLiteral("Historique du lieu (rayon 300 m) : %1 autre%2")
                                            .arg(historique.size()).arg(historique.size() > 1 ? "s" : ""));
    titreHisto->setStyleSheet(QStringLiteral("color: #AAB0B6; background: transparent; border: none;"));
    racine->addWidget(titreHisto);
    for (int k = 0; k < historique.size() && k < 6; ++k) {
        const Intervention &h = historique[k];
        auto *ligne = new QLabel(QStringLiteral("%1 · %2 · %3 · %4")
                                     .arg(h.idText(), formatDateHeure(h.reception), h.type, h.gravite));
        ligne->setStyleSheet(QStringLiteral("color: #E5E7EB; background: transparent; border: none; font-size: 12px;"));
        racine->addWidget(ligne);
    }

    auto *boutons = new QHBoxLayout;
    boutons->addStretch();
    auto *bFermer = new QPushButton(QStringLiteral("Fermer"));
    auto *bModifier = new QPushButton(QStringLiteral("Modifier"));
    bModifier->setDefault(true);
    boutons->addWidget(bFermer);
    boutons->addWidget(bModifier);
    racine->addLayout(boutons);
    connect(bFermer, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(bModifier, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted)
        modifierIntervention(id);
}

// Charge l'intervention dans le formulaire (mode modification)
void AXIS::modifierIntervention(int id)
{
    Intervention it;
    QString erreur;
    if (!Intervention::afficher(id, it, &erreur)) {
        QMessageBox::critical(this, QStringLiteral("Modification impossible"), erreur);
        rafraichirListeInterventions();
        return;
    }

    interventionEnEdition_ = it.id;
    ui->comboType->setCurrentIndex(std::max(0, ui->comboType->findText(it.type)));
    ui->comboGravite->setCurrentIndex(std::max(0, ui->comboGravite->findText(it.gravite)));
    ui->comboEtat->setCurrentIndex(std::max(0, ui->comboEtat->findText(it.etat)));
    ui->editLieu->setText(it.lieu);
    ui->editGPS->setText(it.gps());
    ui->dateReception->setDateTime(it.reception);
    ui->dateFin->setDateTime(it.fin.isValid() ? it.fin : QDateTime::currentDateTime());
    majEtatFormulaireIntervention();

    ui->labelAjouter->setGeometry(ui->labelAjouter->x(), ui->labelAjouter->y(), 218, ui->labelAjouter->height());
    ui->labelAjouter->setText(QStringLiteral("Modifier %1").arg(it.idText()));
    ui->btnAjouter->setText(QStringLiteral("Modifier"));
    ui->btnVider->setText(QStringLiteral("Annuler"));
    ui->scrollArea->verticalScrollBar()->setValue(0);
    ui->editLieu->setFocus();
}

// Suppression (avec confirmation)
void AXIS::supprimerIntervention(int id)
{
    Intervention it;
    QString erreur;
    if (!Intervention::afficher(id, it, &erreur)) {           // sélection d'une intervention existante
        QMessageBox::critical(this, QStringLiteral("Suppression impossible"), erreur);
        rafraichirListeInterventions();
        return;
    }

    // Confirmation obligatoire avant toute suppression
    QMessageBox boite(QMessageBox::Warning, QStringLiteral("Supprimer l'intervention"),
                      QStringLiteral("Supprimer l'intervention %1 (%2 – %3) ?\nCette action est définitive.")
                          .arg(it.idText(), it.type, it.lieu),
                      QMessageBox::NoButton, this);
    QPushButton *oui = boite.addButton(QStringLiteral("Supprimer"), QMessageBox::AcceptRole);
    QPushButton *non = boite.addButton(QStringLiteral("Annuler"), QMessageBox::RejectRole);
    boite.setDefaultButton(non);
    boite.exec();
    if (boite.clickedButton() != oui)
        return;

    if (!Intervention::supprimer(id, &erreur)) {              // inclut les erreurs de clé étrangère
        QMessageBox::critical(this, QStringLiteral("Suppression impossible"), erreur);
        return;
    }
    if (interventionEnEdition_ == id)
        viderFormulaireIntervention();
    rafraichirListeInterventions();
}

// ------------------------------------------------------------ Liste
// Relit la table Oracle puis rafraîchit tout ce qui en dépend : tableau (recherche, tri, pagination), statistiques,
// carte et zones à risque.
void AXIS::rafraichirListeInterventions()
{
    QString erreur;
    interventions_ = Intervention::lister(&erreur);
    if (!erreur.isEmpty()) {
        interventions_.clear();
        interventionsFiltrees_.clear();
        ui->tableInterventions->clearContents();
        ui->tableInterventions->setRowCount(0);
        ui->labelPagination->setText(QStringLiteral("Lecture impossible"));
        QMessageBox::critical(this, QStringLiteral("Erreur de la base de données"), erreur);
        return;
    }
    appliquerFiltreInterventions();
    majStatistiquesInterventions();
    majCarteInterventions();
    majZonesRisqueInterventions();
    chargerMeteoInterventions();
}

// Recherche (coordonnées GPS) + tri + pagination, à partir de la liste lue dans la base.
void AXIS::appliquerFiltreInterventions()
{
    QVector<Intervention> liste = Intervention::rechercherGps(interventions_, ui->editRecherche->text());
    switch (ui->comboTri->currentIndex()) {
    case 1:  Intervention::trierParGravite(liste); break;            // élevée → faible
    case 2:  Intervention::trierParGraviteCroissante(liste); break;  // faible → élevée
    default: Intervention::trierParId(liste); break;                 // « Aucun tri » : ordre des ID
    }
    interventionsFiltrees_ = liste;

    const int pages = std::max(1, int((liste.size() + kInterventionsParPage - 1) / kInterventionsParPage));
    pageIntervention_ = std::clamp(pageIntervention_, 0, pages - 1);
    remplirTableauInterventions(liste.mid(pageIntervention_ * kInterventionsParPage, kInterventionsParPage));

    if (interventions_.isEmpty())
        ui->labelPagination->setText(QStringLiteral("Aucune intervention enregistrée"));
    else if (liste.isEmpty())
        ui->labelPagination->setText(QStringLiteral("Aucun résultat"));
    else
        ui->labelPagination->setText(QStringLiteral("Page %1 / %2  ·  %3 intervention%4")
                                         .arg(pageIntervention_ + 1).arg(pages).arg(liste.size())
                                         .arg(liste.size() > 1 ? "s" : ""));
    ui->btnPagePrecedente->setEnabled(pageIntervention_ > 0);
    ui->btnPageSuivante->setEnabled(pageIntervention_ < pages - 1);
}

void AXIS::remplirTableauInterventions(const QVector<Intervention> &page)
{
    QTableWidget *t = ui->tableInterventions;
    t->clearContents();
    t->setRowCount(0);

    auto creerItem = [](const QString &texte, const QString &infobulle) {
        auto *item = new QTableWidgetItem(texte);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setToolTip(infobulle);
        return item;
    };

    t->setRowCount(page.size());
    for (int r = 0; r < page.size(); ++r) {
        const Intervention &i = page[r];
        t->setRowHeight(r, 46);

        auto *idItem = creerItem(i.idText(), i.idText());
        idItem->setData(Qt::UserRole, i.id);                    // identifiant réel, utilisé par les actions
        t->setItem(r, ColId, idItem);
        t->setItem(r, ColType, creerItem(i.type, i.type));
        t->setItem(r, ColLieu, creerItem(i.lieu, i.lieu));
        t->setItem(r, ColGravite, creerItem(QString(), i.gravite));
        t->setItem(r, ColGps, creerItem(i.gps(), i.gps()));
        t->setItem(r, ColReception, creerItem(formatDateTableau(i.reception), formatDateHeure(i.reception)));
        t->setItem(r, ColFin, creerItem(formatDateTableau(i.fin), formatDateHeure(i.fin)));
        t->setItem(r, ColDuree, creerItem(i.dureeTexte(), i.dureeTexte()));
        t->setItem(r, ColEtat, creerItem(QString(), i.etat));
        t->setItem(r, ColActions, creerItem(QString(), QString()));

        t->setCellWidget(r, ColGravite, creerPastille(i.gravite, couleursGravite(i.gravite)));
        t->setCellWidget(r, ColEtat, creerPastille(i.etat, couleursEtat(i.etat)));

        // Actions : Afficher / Modifier / Supprimer
        auto *actions = new QWidget;
        actions->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto *lay = new QHBoxLayout(actions);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        auto *bAfficher = creerBoutonAction(QStringLiteral("👁"), QStringLiteral("Afficher"), QStringLiteral("#30353A"));
        auto *bModifier = creerBoutonAction(QStringLiteral("✎"), QStringLiteral("Modifier"), QStringLiteral("#30353A"));
        auto *bSupprimer = creerBoutonAction(QStringLiteral("🗑"), QStringLiteral("Supprimer"), QStringLiteral("#A61B1B"));
        lay->addWidget(bAfficher);
        lay->addWidget(bModifier);
        lay->addWidget(bSupprimer);
        lay->addStretch();
        const int id = i.id;
        connect(bAfficher, &QToolButton::clicked, this, [this, id] { afficherIntervention(id); });
        connect(bModifier, &QToolButton::clicked, this, [this, id] { modifierIntervention(id); });
        connect(bSupprimer, &QToolButton::clicked, this, [this, id] { supprimerIntervention(id); });
        t->setCellWidget(r, ColActions, actions);
    }
}

// ------------------------------------------------------------ Fonctionnalités avancées
void AXIS::initialiserFonctionsAvanceesInterventions()
{
    // ----- Tri -----
    ui->comboTri->clear();
    ui->comboTri->addItems({QStringLiteral("Aucun tri"), QStringLiteral("Gravité : élevée → faible"),
                            QStringLiteral("Gravité : faible → élevée")});
    connect(ui->comboTri, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        pageIntervention_ = 0;
        appliquerFiltreInterventions();
    });

    // ----- Recherche par coordonnées GPS -----
    ui->editRecherche->setPlaceholderText(QStringLiteral("Coordonnées GPS (ex. 36.8065, 10.1815)"));
    ui->editRecherche->setClearButtonEnabled(true);
    connect(ui->editRecherche, &QLineEdit::textChanged, this, [this] {
        pageIntervention_ = 0;
        appliquerFiltreInterventions();
    });

    // ----- Pagination -----
    connect(ui->btnPagePrecedente, &QPushButton::clicked, this, [this] {
        --pageIntervention_;
        appliquerFiltreInterventions();
    });
    connect(ui->btnPageSuivante, &QPushButton::clicked, this, [this] {
        ++pageIntervention_;
        appliquerFiltreInterventions();
    });

    // ----- Export Excel -----
    connect(ui->btnExporterInterventions, &QPushButton::clicked, this, &AXIS::exporterInterventionsExcel);

    // ----- Zones à risque : zone de texte dans le cadre existant -----
    labelZonesContenu_ = new QLabel(ui->frameZonesRisque);
    labelZonesContenu_->setGeometry(14, 42, ui->frameZonesRisque->width() - 28, ui->frameZonesRisque->height() - 52);
    labelZonesContenu_->setTextFormat(Qt::RichText);
    labelZonesContenu_->setWordWrap(true);
    labelZonesContenu_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    labelZonesContenu_->setStyleSheet(QStringLiteral("color: #E5E7EB; font-size: 13px; background: transparent; border: none;"));

    // ----- Carte : remplace la grille factice du cadre « frameMap » par une vraie carte OpenStreetMap -----
    const QList<QFrame *> grille = ui->frameMap->findChildren<QFrame *>(QRegularExpression(QStringLiteral("^line[HV]\\d+$")));
    for (QFrame *f : grille)
        f->deleteLater();
    carte_ = new CarteWidget(ui->frameMap);
    carte_->setGeometry(1, 1, ui->frameMap->width() - 2, ui->frameMap->height() - 2);
    carte_->setCaserne(Caserne::Lat, Caserne::Lon, Caserne::Nom);
    carte_->show();
    ui->labelMapHint->setText(QStringLiteral("Cliquer sur un point : distance + itinéraire depuis la caserne"));
    connect(carte_, &CarteWidget::marqueurSelectionne, this, [this](int id) { selectionnerInterventionSurCarte(id); });

    // Clic sur une ligne du tableau : l'intervention est localisée sur la carte (distance + trajet)
    connect(ui->tableInterventions, &QTableWidget::cellClicked, this, [this](int ligne, int colonne) {
        if (colonne == ColActions)
            return;
        if (auto *it = ui->tableInterventions->item(ligne, ColId)) {
            const int id = it->data(Qt::UserRole).toInt();
            carte_->selectionner(id);
            ui->labelMapHint->setText(QStringLiteral("%1 localisée sur la carte (en bas de page)")
                                          .arg(it->text()));
        }
    });

    meteoReseau_ = new QNetworkAccessManager(this);
}

void AXIS::selectionnerInterventionSurCarte(int id)
{
    // Se place sur la page du tableau qui contient l'intervention et sélectionne sa ligne
    for (int k = 0; k < interventionsFiltrees_.size(); ++k) {
        if (interventionsFiltrees_[k].id != id)
            continue;
        pageIntervention_ = int(k / kInterventionsParPage);
        appliquerFiltreInterventions();
        ui->tableInterventions->selectRow(k % kInterventionsParPage);
        ui->labelMapHint->setText(QStringLiteral("%1 sélectionnée").arg(interventionsFiltrees_[k].idText()));
        return;
    }
}

// ----- 3 : export Excel -----
void AXIS::exporterInterventionsExcel()
{
    if (interventionsFiltrees_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Export Excel"), QStringLiteral("Aucune intervention à exporter."));
        return;
    }
    const QString proposition = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                                    .filePath(QStringLiteral("interventions_%1.xlsx")
                                                  .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    QString chemin = QFileDialog::getSaveFileName(this, QStringLiteral("Exporter les interventions"), proposition,
                                                  QStringLiteral("Classeur Excel (*.xlsx)"));
    if (chemin.isEmpty())
        return;
    if (!chemin.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive))
        chemin += QStringLiteral(".xlsx");

    QString erreur;
    if (!Intervention::exporterExcel(interventionsFiltrees_, chemin, &erreur)) {
        QMessageBox::critical(this, QStringLiteral("Export impossible"), erreur);
        return;
    }
    const QString pluriel = interventionsFiltrees_.size() > 1 ? QStringLiteral("s") : QString();
    QMessageBox boite(QMessageBox::Information, QStringLiteral("Export Excel"),
                      QStringLiteral("%1 intervention%2 exportée%2 :\n%3")
                          .arg(interventionsFiltrees_.size()).arg(pluriel).arg(chemin),
                      QMessageBox::NoButton, this);
    QPushButton *ouvrir = boite.addButton(QStringLiteral("Ouvrir le fichier"), QMessageBox::AcceptRole);
    boite.addButton(QStringLiteral("Fermer"), QMessageBox::RejectRole);
    boite.exec();
    if (boite.clickedButton() == ouvrir)
        QDesktopServices::openUrl(QUrl::fromLocalFile(chemin));
}

// ----- 4 : statistiques par gravité (barres verticales) -----
void AXIS::majStatistiquesInterventions()
{
    const Intervention::StatsGravite s = Intervention::statistiques(interventions_);
    const int maximum = std::max(1, s.maximum());
    auto maj = [maximum](QFrame *barre, QLabel *compteur, int n) {
        const int h = n == 0 ? 3 : std::max(6, kHauteurMaxBarre * n / maximum);
        barre->setGeometry(barre->x(), kBaseBarres - h, barre->width(), h);
        compteur->setText(QString::number(n));
        compteur->setGeometry(compteur->x(), kBaseBarres - h - 32, compteur->width(), compteur->height());
    };
    maj(ui->barElevee, ui->labelCountElevee, s.elevee);
    maj(ui->barMoyenne, ui->labelCountMoyenne, s.moyenne);
    maj(ui->barFaible, ui->labelCountFaible, s.faible);
}

// ----- 6 : marqueurs de la carte -----
void AXIS::majCarteInterventions()
{
    QVector<CarteWidget::Marqueur> marqueurs;
    for (const Intervention &i : interventions_) {
        if (i.lat == 0.0 && i.lon == 0.0)               // GPS illisible : pas de point sur la carte
            continue;
        CarteWidget::Marqueur m;
        m.id = i.id;
        m.lat = i.lat;
        m.lon = i.lon;
        m.couleur = couleurMarqueur(i.gravite);
        m.titre = QStringLiteral("%1 · %2 · gravité %3").arg(i.idText(), i.type, i.gravite.toLower());
        m.lieu = i.lieu;
        m.etiquette = QStringLiteral("%1\n%2\n%3").arg(m.titre, i.lieu, i.etat);
        marqueurs.push_back(m);
    }
    const bool premiere = !marqueurs.isEmpty() && carte_->selection() == 0;
    carte_->setMarqueurs(marqueurs);
    if (premiere && !carteCadree_) {                    // cadrage automatique une seule fois (ensuite : vue de l'utilisateur)
        carte_->ajusterVue();
        carteCadree_ = true;
    }
}

// ----- 5 : zones à risque (historique + météo) -----
void AXIS::chargerMeteoInterventions()
{
    // Météo actuelle à la caserne (Open-Meteo, gratuit, sans clé). Rechargée au plus toutes les 10 minutes.
    if (meteoHeure_.isValid() && meteoHeure_.secsTo(QDateTime::currentDateTime()) < 600)
        return;
    meteoHeure_ = QDateTime::currentDateTime();

    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(Caserne::Lat, 'f', 4));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(Caserne::Lon, 'f', 4));
    q.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,precipitation,wind_speed_10m"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setTransferTimeout(8000);
    QNetworkReply *reponse = meteoReseau_->get(req);
    connect(reponse, &QNetworkReply::finished, this, [this, reponse] {
        reponse->deleteLater();
        if (reponse->error() == QNetworkReply::NoError) {
            meteoInterventions_ = Intervention::lireMeteoJson(reponse->readAll());
        } else {
            meteoInterventions_ = Intervention::Meteo();
            meteoHeure_ = QDateTime();                  // on réessaiera à la prochaine ouverture
        }
        majZonesRisqueInterventions();
    });
}

void AXIS::majZonesRisqueInterventions()
{
    const QVector<Intervention::ZoneRisque> zones =
        Intervention::detecterZonesRisque(interventions_, meteoInterventions_, 5);

    QString html;
    html += meteoInterventions_.disponible
                ? QStringLiteral("<p style='margin:0 0 6px 0; color:#AAB0B6; font-size:12px'>Météo actuelle : %1</p>")
                      .arg(meteoInterventions_.resume.toHtmlEscaped())
                : QStringLiteral("<p style='margin:0 0 6px 0; color:#AAB0B6; font-size:12px'>"
                                 "Météo indisponible : analyse basée sur l'historique.</p>");
    if (zones.isEmpty()) {
        html += QStringLiteral("<p style='margin:0'>Aucune zone à risque détectée pour le moment.</p>");
    } else {
        for (const Intervention::ZoneRisque &z : zones) {
            const QString couleur = couleurMarqueur(z.niveau).name();
            html += QStringLiteral("<p style='margin:0 0 5px 0'><span style='color:%1'>&#9679;</span> <b>%2</b> "
                                   "<span style='color:%1'>risque %3</span><br>"
                                   "<span style='color:#AAB0B6; font-size:12px'>&nbsp;&nbsp;&nbsp;%4</span></p>")
                        .arg(couleur, z.nom.toHtmlEscaped(), z.niveau.toLower(), z.facteurs.toHtmlEscaped());
        }
    }
    labelZonesContenu_->setText(html);

    QVector<CarteWidget::Zone> zonesCarte;
    for (const Intervention::ZoneRisque &z : zones) {
        CarteWidget::Zone c;
        c.lat = z.lat;
        c.lon = z.lon;
        c.rayonM = 700.0;
        c.couleur = couleurMarqueur(z.niveau);
        c.nom = z.nom;
        zonesCarte.push_back(c);
    }
    carte_->setZones(zonesCarte);
}

// ==========================================
// MODULE VEHICULES
// ==========================================
// Interface : page PVehicule de axis.ui (nouvelle interface : formulaire, liste, statistiques, barre latérale).
// Classes métier : Vehicule (vehicule.h/.cpp : CRUD + recherche + tri + statistiques) et GestionEntretiens
// (entretien.h/.cpp + entretiendialog.h/.cpp). Aucune requête SQL dans ce fichier.
namespace {

const char *kFormatDateVeh = "dd/MM/yyyy";
const int kHauteurLigneVeh = 52;

// Feuille de style de la page Véhicules (identique à celle de AXIS_Vehicules/gaxis.cpp)
const char *kStyleVeh = R"QSS(
QWidget { background:#1c1e21; color:#e6e6e6; font-family:"Montserrat","Segoe UI","Inter",sans-serif; font-size:13px; }
QLabel { background:transparent; }
#sidebarVeh { background:#232528; }
#sidebarVeh QLabel { background:transparent; }
#sidebarVeh QPushButton { text-align:left; padding:9px 10px; border:none; border-radius:6px; background:transparent; color:#e6e6e6; font-size:14px; }
#sidebarVeh QPushButton:hover { background:#2e3033; }
QFrame[role="card"] { background:#232528; border:1px solid #33363a; border-radius:8px; }
QLabel[role="cardTitle"] { color:#d90000; font-size:15px; font-weight:700; }
QLabel[role="pageTitle"] { font-size:21px; font-weight:700; }
QLabel[role="muted"] { color:#b0b3b8; font-size:12px; }
QLabel[role="alert"] { background:#fdf0dc; color:#b45f06; border-left:4px solid #e8830c; border-radius:4px; padding:8px 12px; }
QLineEdit, QComboBox { background:#1c1e21; border:1px solid #3a3d41; border-radius:5px; padding:6px 8px; color:#e6e6e6; }
QLineEdit:focus, QComboBox:focus { border-color:#ff0000; }
QComboBox QAbstractItemView { background:#232528; selection-background-color:#ff0000; border:1px solid #3a3d41; }
QPushButton[role="primary"] { background:#ff0000; color:white; border:none; border-radius:5px; padding:7px 16px; font-weight:600; }
QPushButton[role="primary"]:hover { background:#d90000; }
QPushButton[role="ghost"] { background:transparent; border:1px solid #3a3d41; border-radius:5px; padding:7px 14px; }
QPushButton[role="ghost"]:hover { background:#2e3033; }
QTableWidget { background:transparent; border:none; gridline-color:transparent; outline:0; }
QTableWidget::item { border-bottom:1px solid #33363a; padding:4px; }
QHeaderView::section { background:transparent; color:#b0b3b8; border:none; padding:6px; font-weight:500; }
QScrollArea { border:none; }
QScrollBar:horizontal { background:#1c1e21; height:12px; }
QScrollBar::handle:horizontal { background:#4a4d52; border-radius:5px; min-width:30px; }
QScrollBar:vertical { background:#1c1e21; width:12px; }
QScrollBar::handle:vertical { background:#4a4d52; border-radius:5px; min-height:30px; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
)QSS";




struct CouleursVeh { QString fond, texte; };

CouleursVeh couleursVehicule(const QString &valeur)
{
    if (valeur == "Bon" || valeur == "Disponible") return {"#dff5e6", "#1b7f43"};
    if (valeur == "À surveiller" || valeur == "En maintenance") return {"#fdebd3", "#b45f06"};
    return {"#fde0e0", "#c0171d"};
}

QWidget *creerBadgeVehicule(const QString &texte)
{
    const CouleursVeh c = couleursVehicule(texte);
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(4, 0, 4, 0);
    auto *b = new QLabel(texte);
    b->setStyleSheet(QString("background:%1;color:%2;border-radius:9px;padding:2px 9px;"
                             "font-size:11px;font-weight:600;")
                         .arg(c.fond, c.texte));
    l->addWidget(b);
    l->addStretch();
    return w;
}

QString dateVeh(const QDate &d) { return d.isValid() ? d.toString(kFormatDateVeh) : QString("—"); }
QString kmVeh(int km) { return QLocale(QLocale::French).toString(km); }

struct PartVeh { int valeur; QColor couleur; };


QPixmap dessinerAnneauVeh(const QVector<PartVeh> &parts, qreal dpr)
{
    const int cote = 100;
    QPixmap pm(QSize(qRound(cote * dpr), qRound(cote * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    int total = 0;
    for (const PartVeh &s : parts) total += s.valeur;

    const qreal pw = cote * 0.17;
    const QRectF r(pw / 2, pw / 2, cote - pw, cote - pw);
    QPen pen;
    pen.setWidthF(pw);
    pen.setCapStyle(Qt::FlatCap);

    if (total == 0) {
        pen.setColor(QColor("#3a3d41"));
        p.setPen(pen);
        p.drawEllipse(r);
    } else {
        int debut = 90 * 16;
        for (const PartVeh &s : parts) {
            if (s.valeur <= 0) continue;
            const int angle = -qRound(5760.0 * s.valeur / total);
            pen.setColor(s.couleur);
            p.setPen(pen);
            p.drawArc(r, debut, angle);
            debut += angle;
        }
    }

    p.setPen(QColor("#e6e6e6"));
    QFont f = p.font();
    f.setPointSizeF(15);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(0, 0, cote, cote * 0.58), Qt::AlignHCenter | Qt::AlignBottom, QString::number(total));
    f.setPointSizeF(8);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#b0b3b8"));
    p.drawText(QRectF(0, cote * 0.58, cote, cote * 0.3), Qt::AlignHCenter | Qt::AlignTop, "total");
    return pm;
}

}

void AXIS::initialiserModuleVehicules()
{
    ui->PVehicule->setStyleSheet(QString::fromUtf8(kStyleVeh));   // style limité à la page Véhicules

    ui->cbTypeVeh->addItems(Vehicule::types());
    ui->cbEtatVeh->addItems(Vehicule::etats());
    ui->cbDispoVeh->addItems(Vehicule::disponibilites());

    auto *btnEntretiens = new QPushButton("🛠 Entretiens", ui->sidebarVeh);
    btnEntretiens->setCursor(Qt::PointingHandCursor);
    ui->sidebarLayoutVeh->insertWidget(5, btnEntretiens);
    connect(btnEntretiens, &QPushButton::clicked, this, [this] {
        GestionEntretiensDialog dialogue(ui->PVehicule);
        dialogue.exec();
        rafraichirListeVehicules();
    });


    ui->leKmVeh->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9 ]{0,9}"), this));
    ui->lePlaqueVeh->setMaxLength(14);

    QTableWidget *t = ui->tableVehiculesVeh;
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setShowGrid(false);
    t->setWordWrap(true);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(kHauteurLigneVeh);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setFixedHeight(44);

    connect(ui->btnMenuVeh, &QPushButton::clicked, this, &AXIS::allerAuMenu);
    connect(ui->btnDeconnexionVeh, &QPushButton::clicked, this, &QWidget::close);

    connect(ui->btnSubmitVeh, &QPushButton::clicked, this, &AXIS::enregistrerVehicule);
    connect(ui->btnViderVeh, &QPushButton::clicked, this, &AXIS::viderFormulaireVehicule);
    connect(ui->btnExcelVeh, &QPushButton::clicked, this, &AXIS::exporterVehiculesExcel);
    connect(ui->leRechercheVeh, &QLineEdit::textChanged, this, [this] { rafraichirListeVehicules(); });
    connect(ui->cbTriVeh, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { rafraichirListeVehicules(); });
    smsNetwork_ = new QNetworkAccessManager(this);
    connect(ui->btnSmsVeh, &QPushButton::clicked, this, &AXIS::envoyerAlerteSmsVehicules);
    // pas d'accès base ici : AXIS::allerAuModule() appelle rafraichirListeVehicules() à l'ouverture de la page
}


void AXIS::envoyerAlerteSmsVehicules()
{
    const QString accountSid = qEnvironmentVariable("TWILIO_ACCOUNT_SID").trimmed();
    const QString authToken = qEnvironmentVariable("TWILIO_AUTH_TOKEN").trimmed();
    const QString fromNumber = qEnvironmentVariable("TWILIO_FROM_NUMBER").trimmed();
    if (accountSid.isEmpty() || authToken.isEmpty() || fromNumber.isEmpty()) {
        QMessageBox::warning(ui->PVehicule, "Configuration SMS",
                             "Configurez les variables d'environnement TWILIO_ACCOUNT_SID, "
                             "TWILIO_AUTH_TOKEN et TWILIO_FROM_NUMBER, puis relancez l'application.\n\n"
                             "Un compte Twilio actif et un numéro expéditeur autorisé sont nécessaires.");
        return;
    }

    QString erreur;
    const QVector<Vehicule> aEntretenir = Vehicule::maintenanceRequise(&erreur);
    if (!erreur.isEmpty()) {
        QMessageBox::critical(ui->PVehicule, "Alerte SMS", "Impossible de lire les véhicules :\n" + erreur);
        return;
    }
    if (aEntretenir.isEmpty()) {
        QMessageBox::information(ui->PVehicule, "Alerte SMS", "Aucun véhicule n'a une maintenance échue à signaler.");
        return;
    }

    bool ok = false;
    const QString destinataire = QInputDialog::getText(ui->PVehicule, "Envoyer une alerte SMS", "Numéro du destinataire au format international (ex. +216XXXXXXXX) :",
                                                       QLineEdit::Normal, QString(), &ok).trimmed();
    static const QRegularExpression formatTelephone(R"(^\+[1-9][0-9]{7,14}$)");
    if (!ok) return;
    if (!formatTelephone.match(destinataire).hasMatch()) {
        QMessageBox::warning(ui->PVehicule, "Numéro invalide",
                             "Utilisez le format international E.164, par exemple +216XXXXXXXX.");
        return;
    }

    QStringList details;
    const int limite = qMin(aEntretenir.size(), 8);
    for (int i = 0; i < limite; ++i) {
        const Vehicule &vehicule = aEntretenir[i];
        details << QString("%1 : échéance %2").arg(vehicule.plaque, dateVeh(vehicule.prochaineMaint));
    }
    if (aEntretenir.size() > limite)
        details << QString("et %1 autre(s)").arg(aEntretenir.size() - limite);
    const QString message = QString("AXIS - %1 véhicule(s) nécessitent une maintenance :\n%2")
                                .arg(aEntretenir.size())
                                .arg(details.join("\n"));

    if (QMessageBox::question(ui->PVehicule, "Confirmer l'envoi",
                              QString("Envoyer cette alerte à %1 ?\n\n%2").arg(destinataire, message))
        != QMessageBox::Yes) {
        return;
    }

    const QUrl url(QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json").arg(accountSid));
    QNetworkRequest request(url);
    const QByteArray identifiants = (accountSid + ":" + authToken).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + identifiants);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery donnees;
    donnees.addQueryItem("From", fromNumber);
    donnees.addQueryItem("To", destinataire);
    donnees.addQueryItem("Body", message);
    QNetworkReply *reply = smsNetwork_->post(request, donnees.toString(QUrl::FullyEncoded).toUtf8());
    ui->btnSmsVeh->setEnabled(false);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray contenu = reply->readAll();
        QString erreur = reply->errorString();
        const QString detailApi = QJsonDocument::fromJson(contenu).object().value("message").toString();
        reply->deleteLater();
        ui->btnSmsVeh->setEnabled(true);

        if (status >= 200 && status < 300) {
            QMessageBox::information(ui->PVehicule, "Alerte SMS", "La demande d'envoi SMS a été acceptée par Twilio.");
        } else {
            if (!detailApi.isEmpty()) erreur = detailApi;
            QMessageBox::critical(ui->PVehicule, "Échec de l'envoi SMS",
                                  QString("Twilio n'a pas accepté le SMS (HTTP %1).\n%2").arg(status).arg(erreur));
        }
    });
}


void AXIS::rafraichirListeVehicules()
{
    QString erreur;
    const QVector<Vehicule> liste =
        Vehicule::lister(ui->leRechercheVeh->text().trimmed(), ui->cbTriVeh->currentIndex(), &erreur);
    if (!erreur.isEmpty()) {
        QMessageBox::critical(ui->PVehicule, "Base de données", erreur);
        return;
    }
    remplirTableauVehicules(liste);
    afficherAlerteVehicules();
    rafraichirStatsVehicules();
}

void AXIS::remplirTableauVehicules(const QVector<Vehicule> &liste)
{
    QTableWidget *t = ui->tableVehiculesVeh;
    t->setRowCount(liste.size());

    for (int r = 0; r < liste.size(); ++r) {
        const Vehicule &v = liste[r];
        const QStringList cellules = {v.id, v.plaque, v.type, kmVeh(v.km)};
        for (int c = 0; c < 4; ++c) t->setItem(r, c, new QTableWidgetItem(cellules[c]));
        t->setCellWidget(r, 4, creerBadgeVehicule(v.etat));
        t->setCellWidget(r, 5, creerBadgeVehicule(v.dispo));
        t->setItem(r, 6, new QTableWidgetItem(dateVeh(v.derniereMaint)));
        t->setItem(r, 7, new QTableWidgetItem(dateVeh(v.prochaineMaint)));
        t->setItem(r, 8, new QTableWidgetItem(dateVeh(v.assurance)));
        t->setItem(r, 9, new QTableWidgetItem(dateVeh(v.visiteTech)));

        const QString st = "style='color:#e6e6e6;text-decoration:none;'";
        auto *actions = new QLabel(QString("<a href='view' %1>Afficher</a> · <a href='edit' %1>Modifier</a> · "
                                           "<a href='del' %1>Supprimer</a>").arg(st));
        actions->setTextFormat(Qt::RichText);
        actions->setCursor(Qt::PointingHandCursor);
        actions->setContentsMargins(4, 0, 4, 0);
        const QString id = v.id;
        connect(actions, &QLabel::linkActivated, this, [this, id](const QString &lien) {
            if (lien == "view") afficherVehicule(id);
            else if (lien == "edit") modifierVehicule(id);
            else supprimerVehicule(id);
        });
        t->setCellWidget(r, 10, actions);
    }
    t->setFixedHeight(44 + qMax(1, int(liste.size())) * kHauteurLigneVeh + 16);
}

void AXIS::afficherAlerteVehicules()
{
    QString erreur;
    const QVector<Vehicule> liste = Vehicule::maintenanceRequise(&erreur);
    QStringList alertes;
    for (const Vehicule &v : liste)
        alertes << QString("%1 (état %2, prochaine maintenance le %3)")
                       .arg(v.id, v.etat.toLower(), dateVeh(v.prochaineMaint));
    ui->lblAlerteVeh->setVisible(!alertes.isEmpty());
    ui->lblAlerteVeh->setText("Maintenance requise : " + alertes.join(" ; ") + ".");
}

void AXIS::rafraichirStatsVehicules()
{
    const QColor vert("#1f9d55"), orange("#c2670f"), rouge("#c81e1e");

    auto remplir = [this](QLabel *anneau, QLabel *legende, bool parEtat, const QStringList &cles,
                          const QVector<QColor> &couleurs) {
        const QMap<QString, int> stats = Vehicule::statistiques(parEtat);
        int total = 0;
        for (const QString &k : cles) total += stats.value(k);

        QVector<PartVeh> parts;
        QString html = "<table cellspacing='4'>";
        for (int i = 0; i < cles.size(); ++i) {
            const int n = stats.value(cles[i]);
            parts.push_back({n, couleurs[i]});
            const int pct = total ? qRound(100.0 * n / total) : 0;
            html += QString("<tr><td><span style='color:%1'>●</span></td><td>%2 <b>%3</b> (%4%)</td></tr>")
                        .arg(couleurs[i].name(), cles[i]).arg(n).arg(pct);
        }
        anneau->setPixmap(dessinerAnneauVeh(parts, anneau->devicePixelRatioF()));
        legende->setText(html + "</table>");
    };

    remplir(ui->lblDonutEtatVeh, ui->lblLegendeEtatVeh, true, Vehicule::etats(), {vert, orange, rouge});
    remplir(ui->lblDonutDispoVeh, ui->lblLegendeDispoVeh, false, Vehicule::disponibilites(), {vert, rouge, orange});
}


void AXIS::viderFormulaireVehicule()
{
    vehiculeEnEdition_.clear();
    ui->btnSubmitVeh->setText("Ajouter");
    ui->lblFormTitreVeh->setText("Ajouter");
    for (QLineEdit *e : {ui->lePlaqueVeh, ui->leKmVeh, ui->leDerniereVeh, ui->leProchaineVeh, ui->leAssuranceVeh, ui->leVisiteVeh})
        e->clear();
    ui->cbTypeVeh->setCurrentIndex(0);
    ui->cbEtatVeh->setCurrentIndex(0);
    ui->cbDispoVeh->setCurrentIndex(0);
}

void AXIS::enregistrerVehicule()
{
    Vehicule v;
    v.plaque = Vehicule::normaliserPlaque(ui->lePlaqueVeh->text());

    QString kmTxt = ui->leKmVeh->text();
    kmTxt.remove(QRegularExpression("[\\s\\x{202F}\\x{00A0}]"));
    bool ok = false;
    v.km = kmTxt.toInt(&ok);
    if (!ok || v.km < 0) {
        QMessageBox::warning(ui->PVehicule, "Kilométrage invalide", "Saisissez un kilométrage numérique (ex. 42 300).");
        return;
    }

    struct Champ { QLineEdit *edit; QDate *date; const char *nom; };
    const Champ champs[] = {{ui->leDerniereVeh, &v.derniereMaint, "Dernière maintenance"},
                            {ui->leProchaineVeh, &v.prochaineMaint, "Prochaine maintenance"},
                            {ui->leAssuranceVeh, &v.assurance, "Assurance"},
                            {ui->leVisiteVeh, &v.visiteTech, "Prochaine visite technique"}};
    for (const Champ &c : champs) {
        if (c.edit->text().trimmed().isEmpty()) continue;
        *c.date = QDate::fromString(c.edit->text().trimmed(), kFormatDateVeh);
        if (!c.date->isValid()) {
            QMessageBox::warning(ui->PVehicule, "Date invalide",
                                 QString("« %1 » : utilisez le format jj/mm/aaaa.").arg(c.nom));
            return;
        }
    }
    v.type = ui->cbTypeVeh->currentText();
    v.etat = ui->cbEtatVeh->currentText();
    v.dispo = ui->cbDispoVeh->currentText();

    QString erreur;
    bool reussi;
    if (vehiculeEnEdition_.isEmpty()) {
        reussi = v.ajouter(&erreur);
    } else {
        v.id = vehiculeEnEdition_;
        reussi = v.modifier(&erreur);
    }
    if (!reussi) {
        QMessageBox::warning(ui->PVehicule, "Contrôle de saisie", erreur);
        return;
    }
    viderFormulaire();
    rafraichirListeVehicules();
}

void AXIS::afficherVehicule(const QString &id)
{
    Vehicule v;
    QString erreur;
    if (!Vehicule::afficher(id, v, &erreur)) {
        QMessageBox::warning(ui->PVehicule, "Erreur", erreur);
        return;
    }
    QMessageBox::information(ui->PVehicule, "Véhicule " + v.id,
                             QString("Immatriculation : %1\nType : %2\nKilométrage : %3 km\nÉtat : %4\n"
                                     "Disponibilité : %5\nDernière maintenance : %6\n"
                                     "Prochaine maintenance : %7\nAssurance : %8\nVisite technique : %9")
                                 .arg(v.plaque, v.type, kmVeh(v.km), v.etat, v.dispo, dateVeh(v.derniereMaint),
                                      dateVeh(v.prochaineMaint), dateVeh(v.assurance), dateVeh(v.visiteTech)));
}

void AXIS::modifierVehicule(const QString &id)
{
    Vehicule v;
    QString erreur;
    if (!Vehicule::afficher(id, v, &erreur)) {
        QMessageBox::warning(ui->PVehicule, "Erreur", erreur);
        return;
    }
    vehiculeEnEdition_ = id;
    ui->lePlaqueVeh->setText(v.plaque);
    ui->cbTypeVeh->setCurrentText(v.type);
    ui->leKmVeh->setText(QString::number(v.km));
    ui->cbEtatVeh->setCurrentText(v.etat);
    ui->cbDispoVeh->setCurrentText(v.dispo);
    ui->leDerniereVeh->setText(v.derniereMaint.isValid() ? v.derniereMaint.toString(kFormatDateVeh) : "");
    ui->leProchaineVeh->setText(v.prochaineMaint.isValid() ? v.prochaineMaint.toString(kFormatDateVeh) : "");
    ui->leAssuranceVeh->setText(v.assurance.isValid() ? v.assurance.toString(kFormatDateVeh) : "");
    ui->leVisiteVeh->setText(v.visiteTech.isValid() ? v.visiteTech.toString(kFormatDateVeh) : "");
    ui->btnSubmitVeh->setText("Enregistrer");
    ui->lblFormTitreVeh->setText("Modifier " + id);
}

void AXIS::supprimerVehicule(const QString &id)
{
    if (QMessageBox::question(ui->PVehicule, "Supprimer", "Supprimer le véhicule " + id + " ?") != QMessageBox::Yes)
        return;
    QString erreur;
    if (!Vehicule::supprimer(id, &erreur)) {
        QMessageBox::warning(ui->PVehicule, "Erreur", erreur);
        return;
    }
    if (vehiculeEnEdition_ == id) viderFormulaire();
    rafraichirListeVehicules();
}


void AXIS::exporterVehiculesExcel()
{
    QString erreur;
    const QVector<Vehicule> liste = Vehicule::lister(QString(), 1, &erreur);
    if (!erreur.isEmpty()) {
        QMessageBox::critical(ui->PVehicule, "Base de données", erreur);
        return;
    }
    const QString chemin = QFileDialog::getSaveFileName(ui->PVehicule, "Exporter pour Excel", "vehicules.csv",
                                                        "Excel / CSV (*.csv)");
    if (chemin.isEmpty()) return;

    QFile f(chemin);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(ui->PVehicule, "Erreur", "Impossible d'écrire le fichier.");
        return;
    }
    f.write("\xEF\xBB\xBF");
    QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "ID;Immatriculation;Type;Km;État;Disponibilité;Dernière maint.;Prochaine maint.;Assurance;Visite technique\n";
    for (const Vehicule &v : liste)
        out << v.id << ';' << v.plaque << ';' << v.type << ';' << v.km << ';' << v.etat << ';' << v.dispo
            << ';' << dateVeh(v.derniereMaint) << ';' << dateVeh(v.prochaineMaint) << ';' << dateVeh(v.assurance) << ';'
            << dateVeh(v.visiteTech) << '\n';
    QMessageBox::information(ui->PVehicule, "Export", "Fichier créé (ouvrable dans Excel) :\n" + chemin);
}

// ==========================================
// MODULE MATERIELS
// ==========================================
// Interface : page PMateriel de axis.ui (barre latérale + QScrollArea). AFFICHAGE SEUL : les boutons du formulaire
// sont présents mais non fonctionnels ; seule la navigation (« Menu principal », « Quitter ») est active.
void AXIS::initialiserModuleMateriels()
{
    sidebarMat_ = new SidebarFrame(ui->PMateriel);
    auto *lay = static_cast<QHBoxLayout *>(ui->PMateriel->layout());
    lay->insertWidget(0, sidebarMat_);
    lay->setStretch(0, 0);
    lay->setStretch(1, 1);
    connect(sidebarMat_, &SidebarFrame::menuRequested, this, &AXIS::allerAuMenu);
    connect(sidebarMat_, &SidebarFrame::logoutRequested, this, &QWidget::close);

    ui->leRef->setMaxLength(50);
    ui->leDesignation->setMaxLength(100);
    ui->leEmplacement->setMaxLength(100);
    ui->leQuantite->setValidator(new QIntValidator(0, 1000000, this));
    ui->leSeuil->setValidator(new QIntValidator(0, 1000000, this));
    ui->leDate->setMaxLength(10);
    ui->leDate->setPlaceholderText("jj/mm/aaaa");

    // Tableau des matériels : largeurs de colonnes (la désignation prend l'espace restant)
    QHeaderView *h = ui->tableMat->horizontalHeader();
    h->setFixedHeight(44);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::Fixed);
    h->setSectionResizeMode(2, QHeaderView::Stretch);
    const int largeurs[] = {48, 80, 0, 92, 66, 46, 100, 98, 82, 76,240 };
    for (int col = 0; col < 11; ++col)
        if (largeurs[col]) ui->tableMat->setColumnWidth(col, largeurs[col]);
    ui->tableMat->verticalHeader()->hide();
    ui->tableMat->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->btnAddMat, &QPushButton::clicked, this, &AXIS::enregistrerMateriel);
    connect(ui->btnClearMat, &QPushButton::clicked, this, &AXIS::viderFormulaireMateriel);
    // Bouton « Historique mouvements » : tous les mouvements de tous les matériels
    auto *btnHistAll = new QPushButton("Historique mouvements");
    btnHistAll->setToolTip("Toutes les entrées et sorties de stock (filtres + export)");
    if (auto *lay = qobject_cast<QBoxLayout *>(ui->btnExport->parentWidget()->layout()))
        lay->addWidget(btnHistAll);
    connect(btnHistAll, &QPushButton::clicked, this, [this] { dialogueHistoriqueGlobal(this); });
    connect(ui->leSearch, &QLineEdit::textChanged, this, &AXIS::rafraichirListeMateriels);
    connect(ui->cbSort, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AXIS::rafraichirListeMateriels);

    viderFormulaireMateriel();
    rafraichirListeMateriels();
    rafraichirStatsMateriels();
}

// ==========================================
// CRUD MATERIELS
// ==========================================
void AXIS::viderFormulaireMateriel()
{
    for (QLineEdit *e : {ui->leRef, ui->leDesignation, ui->leQuantite, ui->leSeuil, ui->leDate, ui->leEmplacement})
        e->clear();
    ui->cbCategorie->setCurrentIndex(0);
    ui->cbEtat->setCurrentIndex(0);
    ui->cbDispo->setCurrentIndex(0);
    materielEnEdition_ = 0;
    ui->btnAddMat->setText("Ajouter");
    ui->formTitleMat->setText("Ajouter");
}

void AXIS::enregistrerMateriel()
{
    Materiel m;
    m.id = materielEnEdition_;
    m.reference = ui->leRef->text().trimmed().toUpper();
    m.designation = ui->leDesignation->text().trimmed();
    m.categorie = ui->cbCategorie->currentText();
    m.etat = ui->cbEtat->currentText();
    m.disponibilite = ui->cbDispo->currentText();
    m.emplacement = ui->leEmplacement->text().trimmed();

    QStringList errors;
    if (m.reference.isEmpty())
        errors << "La référence est obligatoire.";
    else if (!QRegularExpression("^[A-Z0-9][A-Z0-9_-]*$").match(m.reference).hasMatch())
        errors << "La référence ne peut contenir que des lettres, chiffres, '-' ou '_' (ex. MAT-0001).";
    else if (Materiel::referenceExiste(m.reference, materielEnEdition_))
        errors << "Cette référence existe déjà.";
    if (m.designation.isEmpty())
        errors << "La désignation est obligatoire (nom du matériel).";
    else if (m.designation.size() < 2 || !m.designation.contains(QRegularExpression("\\p{L}")))
        errors << "La désignation doit être un nom de matériel (ex. Extincteur CO2), pas un nombre.";

    bool okQ = false, okS = false;
    m.quantite = ui->leQuantite->text().toInt(&okQ);
    m.seuil = ui->leSeuil->text().toInt(&okS);
    if (!okQ) errors << "La quantité en stock est obligatoire (nombre entier ≥ 0).";
    if (!okS) errors << "Le seuil minimal est obligatoire (nombre entier ≥ 0).";

    if (m.emplacement.isEmpty())
        errors << "L'emplacement de stockage est obligatoire.";

    m.dateAcquisition = QDate::fromString(ui->leDate->text().trimmed(), kDateFmt);
    if (!m.dateAcquisition.isValid())
        errors << "Date d'acquisition invalide (format jj/mm/aaaa).";
    else if (m.dateAcquisition > QDate::currentDate())
        errors << "La date d'acquisition ne peut pas être dans le futur.";

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, "Saisie invalide", errors.join("\n"));
        return;
    }

    QString err;
    const bool ok = materielEnEdition_ ? m.modifier(&err) : m.ajouter(&err);
    if (!ok) {
        QMessageBox::critical(this, "Erreur", err);
        return;
    }
    viderFormulaireMateriel();
    rafraichirListeMateriels();
    rafraichirStatsMateriels();
}

void AXIS::chargerMateriel(int id)
{
    const Materiel m = Materiel::trouver(id);
    if (m.id == 0) return;
    ui->leRef->setText(m.reference);
    ui->leDesignation->setText(m.designation);
    ui->cbCategorie->setCurrentIndex(qMax(0, ui->cbCategorie->findText(m.categorie)));
    ui->leQuantite->setText(QString::number(m.quantite));
    ui->leSeuil->setText(QString::number(m.seuil));
    ui->cbEtat->setCurrentIndex(qMax(0, ui->cbEtat->findText(m.etat)));
    ui->cbDispo->setCurrentIndex(qMax(0, ui->cbDispo->findText(m.disponibilite)));
    ui->leDate->setText(m.dateAcquisition.toString(kDateFmt));
    ui->leEmplacement->setText(m.emplacement);
    materielEnEdition_ = m.id;
    ui->btnAddMat->setText("Mettre à jour");
    ui->formTitleMat->setText("Modifier");
}

void AXIS::afficherMateriel(int id)
{
    const Materiel m = Materiel::trouver(id);
    if (m.id == 0) return;
    QMessageBox::information(
        this, "Détails du matériel",
        QString("<b>%1 – %2</b><br><br>Catégorie : %3<br>Quantité : %4 (seuil minimal : %5)<br>"
                "État : %6<br>Disponibilité : %7<br>Date d'acquisition : %8<br>Emplacement : %9")
            .arg(m.reference, m.designation, m.categorie).arg(m.quantite).arg(m.seuil)
            .arg(m.etat, m.disponibilite, m.dateAcquisition.toString(kDateFmt), m.emplacement));
}

void AXIS::supprimerMateriel(int id)
{
    const Materiel m = Materiel::trouver(id);
    if (m.id == 0) return;
    if (QMessageBox::question(this, "Supprimer", QString("Supprimer « %1 » ?").arg(m.designation)) != QMessageBox::Yes)
        return;
    QString err;
    if (!Materiel::supprimer(id, &err)) {
        QMessageBox::critical(this, "Erreur", err);
        return;
    }
    if (materielEnEdition_ == id) viderFormulaireMateriel();
    rafraichirListeMateriels();
    rafraichirStatsMateriels();
}

void AXIS::rafraichirListeMateriels()
{
    static const char *cols[] = {"etat", "designation", "categorie", "quantite", "date_acquisition"};
    const QList<Materiel> items = Materiel::afficher(ui->leSearch->text().trimmed(),
                                                     cols[qBound(0, ui->cbSort->currentIndex(), 4)]);
    QTableWidget *t = ui->tableMat;
    t->setRowCount(0);
    t->setRowCount(items.size());

    auto text = [&](int r, int c, const QString &s, bool bold = false) {
        auto *it = new QTableWidgetItem(s);
        it->setFlags(Qt::ItemIsEnabled);
        if (bold) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
        t->setItem(r, c, it);
    };

    for (int r = 0; r < items.size(); ++r) {
        const Materiel &m = items[r];
        text(r, 0, QString("M%1").arg(m.id, 3, 10, QChar('0')), true);
        text(r, 1, m.reference, true);
        text(r, 2, m.designation);
        text(r, 3, m.categorie);
        if (m.critique())
            t->setCellWidget(r, 4, badgeCell(QString::number(m.quantite), "crit"));
        else
            text(r, 4, QString::number(m.quantite));
        text(r, 5, QString::number(m.seuil));
        t->setCellWidget(r, 6, badgeCell(m.etat, m.etat == "Bon" ? "ok" : "warn"));
        t->setCellWidget(r, 7, badgeCell(m.disponibilite, m.disponibilite == "Disponible" ? "ok" : "warn"));
        text(r, 8, m.dateAcquisition.toString(kDateFmt));
        text(r, 9, m.emplacement);

        auto *cell = new QWidget;
        cell->setObjectName("cell");
        cell->setAttribute(Qt::WA_StyledBackground, true);
        auto *cl = new QHBoxLayout(cell);
        cl->setContentsMargins(4, 0, 0, 0);
        cl->setSpacing(2);
        const int id = m.id;
        const struct { const char *emoji, *tip; int kind; } actions[] = {
                        {"👁️", "Afficher", 0}, {"✏️", "Modifier", 1}, {"🗑️", "Supprimer", 2},
                        {"📦", "Mouvement de stock", 3}, {"📜", "Historique", 4}};
        for (const auto &a : actions) {
            auto *btn = new QToolButton;
            btn->setText(QString::fromUtf8(a.emoji));
            btn->setToolTip(QString::fromUtf8(a.tip));
            btn->setCursor(Qt::PointingHandCursor);
            const int kind = a.kind;
            connect(btn, &QToolButton::clicked, this, [this, id, kind] {
                if (kind == 0) afficherMateriel(id);
                else if (kind == 1) chargerMateriel(id);
                else if (kind == 2) supprimerMateriel(id);
                else if (kind == 3) {
                    const Materiel mat = Materiel::trouver(id);
                    if (mat.id != 0 && dialogueMouvement(this, mat)) {
                        if (materielEnEdition_ == id) chargerMateriel(id);
                        rafraichirListeMateriels();
                        rafraichirStatsMateriels();
                    }
                } else if (kind == 4) {
                    const Materiel mat = Materiel::trouver(id);
                    if (mat.id != 0) dialogueHistorique(this, mat);
                }
            });
            cl->addWidget(btn);
        }
        cl->addStretch();
        t->setCellWidget(r, 10, cell);
        t->setRowHeight(r, kRowH);
    }
    t->setFixedHeight(44 + qMax(items.size(), 2) * kRowH + 2);
}

void AXIS::rafraichirStatsMateriels()
{
    const QList<Materiel> all = Materiel::afficher("", "reference");

    QMap<QString, int> byCat;
    QList<Materiel> critical;
    for (const Materiel &m : all) {
        byCat[m.categorie] += m.quantite;
        if (m.critique()) critical << m;
    }

    QVector<QPair<QString, int>> bars;
    for (const QString &c : kCategories) bars.append({c, byCat.value(c)});
    ui->barChart->setData(bars);

    const int total = all.size();
    const int crit = critical.size();
    const int ok = total - crit;
    ui->donut->setData(crit, ok);
    auto pct = [&](int n) { return total ? qRound(100.0 * n / total) : 0; };
    ui->legendCrit->setText(QString("<span style='color:#d40000'>●</span> Stock critique <b>%1</b> (%2%)")
                                .arg(crit).arg(pct(crit)));
    ui->legendOk->setText(QString("<span style='color:#1e9e4a'>●</span> Stock suffisant <b>%1</b> (%2%)")
                              .arg(ok).arg(pct(ok)));
    ui->lblStockSub->setText(QString("%1 matériels enregistrés").arg(total));
    ui->lblCatSub->setText(QString("%1 catégories").arg(byCat.size()));

    // Bandeau + alerte magasinier
    if (crit > 0) {
        ui->bannerMat->setText(QString("Stock critique : %1 matériel%2 sous le seuil minimal.")
                                   .arg(crit).arg(crit > 1 ? "s" : ""));
        ui->bannerMat->show();
        QSet<int> ids;
        for (const Materiel &m : critical) ids.insert(m.id);
        if (ids != materielsCritiques_) {   // n'alerter que si la liste des matériels critiques change
            materielsCritiques_ = ids;
            envoyerAlerteStock(critical);
        }
    } else {
        ui->bannerMat->hide();
        materielsCritiques_.clear();
    }
}

void AXIS::exporterMateriels()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Exporter l'inventaire", QDir::homePath() + "/inventaire_axis.csv", "Fichier CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Export", "Impossible d'écrire dans le fichier :\n" + path);
        return;
    }
    QString out = "ID;Référence;Désignation;Catégorie;Quantité;Seuil minimal;État;Disponibilité;"
                  "Date d'acquisition;Emplacement;Niveau de stock\r\n";
    for (const Materiel &m : Materiel::afficher("", "reference")) {
        out += QStringList{QString("M%1").arg(m.id, 3, 10, QChar('0')), csvField(m.reference),
                           csvField(m.designation), csvField(m.categorie), QString::number(m.quantite),
                           QString::number(m.seuil), csvField(m.etat), csvField(m.disponibilite),
                           m.dateAcquisition.toString(kDateFmt), csvField(m.emplacement),
                           m.critique() ? "Critique" : "Suffisant"}
                   .join(';') + "\r\n";
    }
    f.write("\xEF\xBB\xBF");        // BOM UTF-8 : Excel affiche correctement les accents
    f.write(out.toUtf8());
    f.close();
    QMessageBox::information(this, "Export", "Inventaire exporté :\n" + path);
}

void AXIS::envoyerAlerteStock(const QList<Materiel> &critical)
{
    QString body = "Alerte AXIS : les matériels suivants sont sous le seuil minimal de stock.\n\n";
    for (const Materiel &m : critical)
        body += QString("- %1 (%2) : %3 en stock, seuil minimal %4\n")
                    .arg(m.designation, m.reference).arg(m.quantite).arg(m.seuil);

    const QString subject = QString("AXIS - Stock critique (%1 matériel%2)")
                                .arg(critical.size()).arg(critical.size() > 1 ? "s" : "");

    if (QString(SMTP_PASSWORD).startsWith("VOTRE_")) {
        qWarning() << "[ALERTE STOCK] Email non envoyé : renseigne SMTP_PASSWORD dans axis.cpp";
        ui->bannerMat->setText(ui->bannerMat->text() + " (email non configuré)");
        return;
    }

    // Envoi dans un thread séparé pour ne pas figer l'interface
    QThreadPool::globalInstance()->start([this, subject, body] {
        QString err;
        const bool ok = smtpSend(SMTP_HOST, SMTP_PORT, true, SMTP_USER, SMTP_PASSWORD, ALERT_TO, subject, body, &err);
        QMetaObject::invokeMethod(this, [this, ok, err] {
            const QString extra = ok ? QString(" Email envoyé à %1.").arg(ALERT_TO)
                                     : QString(" Email non envoyé : %1").arg(err);
            ui->bannerMat->setText(ui->bannerMat->text() + extra);
            if (!ok) qWarning().noquote() << "[ALERTE STOCK]" << err;
        }, Qt::QueuedConnection);
    });
}

// ==========================================
// SLOTS GENERAUX
// ==========================================
// Navigation entre les modules.
// ------------------------------------------------------------ Connexion / navigation
void AXIS::chargerLogo(QLabel *label, int largeur)
{
    const QPixmap logo(":/logo.png");
    if (logo.isNull()) return;   // sinon on garde le texte « A.X.I.S »
    label->setPixmap(logo.scaledToWidth(largeur, Qt::SmoothTransformation));
    label->setAlignment(Qt::AlignCenter);
}

void AXIS::allerAuMenu()
{
    rafraichirJournalMenu();
    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(0);
}

void AXIS::allerAuModule(int module)
{
    if (!Employe::moduleAutorise(session_.role, module)) {      // droits d'accès
        QMessageBox::warning(this, "Accès refusé", "Votre rôle (" + session_.role + ") ne donne pas accès à ce module.");
        return;
    }
    if (module == 0) {                              // module Employés : page CRUD
        ui->SWAxis->setCurrentIndex(0);
        ui->stackedWidgetEmpl->setCurrentIndex(1);
        rafraichirListe();
    } else {                                        // autres modules : pages du SWAxis
        ui->SWAxis->setCurrentIndex(module);
        if (module == 1)                            // module Interventions : tableau à jour à l'ouverture
            rafraichirListeInterventions();
        else if (module == 2)                       // module Véhicules : liste, alerte et statistiques à jour
            rafraichirListeVehicules();
        else if (module == 3) {                      // module Matériels : liste et statistiques à jour
            rafraichirListeMateriels();
            rafraichirStatsMateriels();
        } else if (module == 4) {                    // module Gardes : liste et statistiques à jour
            rafraichirListeGardes();
            rafraichirStatsGardes();
        }
    }
}

// ==========================================
// MODULE GARDES
// ==========================================
void AXIS::initialiserModuleGardes()
{
    sidebarGrd_ = new SidebarFrame(ui->PGarde);
    auto *lay = static_cast<QHBoxLayout *>(ui->PGarde->layout());
    lay->insertWidget(0, sidebarGrd_);
    lay->setStretch(0, 0);
    lay->setStretch(1, 1);
    connect(sidebarGrd_, &SidebarFrame::menuRequested, this, &AXIS::allerAuMenu);
    connect(sidebarGrd_, &SidebarFrame::logoutRequested, this, &QWidget::close);

    ui->deDateGrd->setDate(QDate::currentDate());
    ui->leDebutGrd->setMaxLength(5);
    ui->leFinGrd->setMaxLength(5);

    // Synchronisation automatique des heures selon le type de veille
    connect(ui->cbTypeGrd, &QComboBox::currentTextChanged, this, [this](const QString &type) {
        if (type == "Jour") {
            ui->leDebutGrd->setText("07:00");
            ui->leFinGrd->setText("19:00");
        } else {
            ui->leDebutGrd->setText("19:00");
            ui->leFinGrd->setText("07:00");
        }
    });

    // En-têtes du tableau
    QHeaderView *h = ui->tableGrd->horizontalHeader();
    h->setFixedHeight(38);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::Fixed);
    h->setSectionResizeMode(1, QHeaderView::Stretch); // Date prend le reste
    ui->tableGrd->setColumnWidth(0, 75);   // ID
    ui->tableGrd->setColumnWidth(2, 85);   // Type
    ui->tableGrd->setColumnWidth(3, 125);  // Horaires
    ui->tableGrd->setColumnWidth(4, 90);   // Effectif
    ui->tableGrd->setColumnWidth(5, 110);  // Statut
    ui->tableGrd->setColumnWidth(6, 115);  // Actions
    ui->tableGrd->verticalHeader()->hide();
    ui->tableGrd->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Boutons et filtres
    connect(ui->btnAddGrd, &QPushButton::clicked, this, &AXIS::enregistrerGarde);
    connect(ui->btnClearGrd, &QPushButton::clicked, this, &AXIS::viderFormulaireGarde);
    connect(ui->btnExportPdfGrd, &QPushButton::clicked, this, &AXIS::exporterGardesPdf);
    connect(ui->leSearchGrd, &QLineEdit::textChanged, this, &AXIS::rafraichirListeGardes);
    connect(ui->cbFilterTypeGrd, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AXIS::rafraichirListeGardes);
    connect(ui->cbFilterStatutGrd, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AXIS::rafraichirListeGardes);
    connect(ui->cbSortGrd, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AXIS::rafraichirListeGardes);

    viderFormulaireGarde();
    rafraichirListeGardes();
    rafraichirStatsGardes();
}

void AXIS::viderFormulaireGarde()
{
    gardeEnEdition_.clear();
    ui->leIdGrd->setText(Garde::prochainId());
    ui->deDateGrd->setDate(QDate::currentDate());
    ui->cbTypeGrd->setCurrentIndex(0); // Jour
    ui->leDebutGrd->setText("07:00");
    ui->leFinGrd->setText("19:00");
    ui->spnEffectifGrd->setValue(4);
    ui->cbStatutGrd->setCurrentIndex(0); // Planifiée
    ui->formTitleGrd->setText("Planifier une garde");
    ui->btnAddGrd->setText("Enregistrer");
}

void AXIS::enregistrerGarde()
{
    QStringList errors;
    const QString id = gardeEnEdition_.isEmpty() ? Garde::prochainId() : gardeEnEdition_;
    const QDate dt = ui->deDateGrd->date();
    const QString type = ui->cbTypeGrd->currentText();
    const QString hDeb = ui->leDebutGrd->text().trimmed();
    const QString hFin = ui->leFinGrd->text().trimmed();
    const int effectif = ui->spnEffectifGrd->value();
    const QString statut = ui->cbStatutGrd->currentText();

    if (!Garde::heureValide(hDeb))
        errors << "L'heure de début doit être au format valide HH:mm (ex: 07:00).";
    if (!Garde::heureValide(hFin))
        errors << "L'heure de fin doit être au format valide HH:mm (ex: 19:00).";
    if (!Garde::effectifValide(effectif))
        errors << "L'effectif doit être compris entre 1 et 50 sapeurs-pompiers.";

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, "Saisie invalide", errors.join("\n"));
        return;
    }

    Garde g(id, dt, type, hDeb, hFin, effectif, statut);
    QString err;
    const bool ok = gardeEnEdition_.isEmpty() ? g.ajouter_garde(&err) : g.modifier_garde(&err);

    if (!ok) {
        QMessageBox::critical(this, "Erreur base de données", QString("Impossible d'enregistrer la garde :\n%1").arg(err));
        return;
    }

    QMessageBox::information(this, "Succès",
                             gardeEnEdition_.isEmpty() ? "La garde a été enregistrée avec succès."
                                                       : QString("La garde #%1 a été modifiée.").arg(id));
    viderFormulaireGarde();
    rafraichirListeGardes();
    rafraichirStatsGardes();
}

void AXIS::chargerGarde(const QString &id)
{
    const Garde g = Garde::trouver(id);
    if (g.id.isEmpty()) return;

    gardeEnEdition_ = g.id;
    ui->leIdGrd->setText(g.id);
    ui->deDateGrd->setDate(g.date);
    ui->cbTypeGrd->setCurrentIndex(qMax(0, ui->cbTypeGrd->findText(g.type)));
    ui->leDebutGrd->setText(g.heureDebut);
    ui->leFinGrd->setText(g.heureFin);
    ui->spnEffectifGrd->setValue(g.effectif);
    ui->cbStatutGrd->setCurrentIndex(qMax(0, ui->cbStatutGrd->findText(g.statut)));

    ui->formTitleGrd->setText(QString("Modifier garde #%1").arg(g.id));
    ui->btnAddGrd->setText("Mettre à jour");
}

void AXIS::afficherGarde(const QString &id)
{
    const Garde g = Garde::trouver(id);
    if (g.id.isEmpty()) return;

    QMessageBox::information(
        this, "Détails de la garde",
        QString("<b>Tour de Garde #%1</b><br><br>"
                "• <b>Date :</b> %2<br>"
                "• <b>Type de veille :</b> %3<br>"
                "• <b>Horaires :</b> %4 à %5<br>"
                "• <b>Effectif requis :</b> %6 sapeurs-pompiers<br>"
                "• <b>Statut :</b> %7")
            .arg(g.id, g.date.toString("dd/MM/yyyy"), g.type,
                 g.heureDebut, g.heureFin, QString::number(g.effectif), g.statut));
}

void AXIS::supprimerGarde(const QString &id)
{
    if (QMessageBox::question(this, "Confirmation",
                              QString("Voulez-vous vraiment supprimer la garde #%1 ?").arg(id),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QString err;
    if (!Garde::supprimer_garde(id, &err)) {
        QMessageBox::critical(this, "Erreur", QString("Impossible de supprimer la garde :\n%1").arg(err));
        return;
    }

    if (gardeEnEdition_ == id) viderFormulaireGarde();
    rafraichirListeGardes();
    rafraichirStatsGardes();
    QMessageBox::information(this, "Supprimé", "La garde a été supprimée avec succès.");
}

void AXIS::rafraichirListeGardes()
{
    const QVector<Garde> all = Garde::lister();
    const QString search = ui->leSearchGrd->text().trimmed();
    const QString filterType = ui->cbFilterTypeGrd->currentText();
    const QString filterStatut = ui->cbFilterStatutGrd->currentText();
    const int sortIdx = ui->cbSortGrd->currentIndex();

    QVector<Garde> filtered;
    for (const Garde &g : all) {
        if (!search.isEmpty()) {
            const QString haystack = QString("%1 %2 %3 %4 %5")
            .arg(g.id, g.date.toString("dd/MM/yyyy"), g.type, g.statut, QString::number(g.effectif));
            if (!haystack.contains(search, Qt::CaseInsensitive))
                continue;
        }
        if (filterType != "Tous types" && g.type.compare(filterType, Qt::CaseInsensitive) != 0)
            continue;
        if (filterStatut != "Tous statuts" && g.statut.compare(filterStatut, Qt::CaseInsensitive) != 0)
            continue;
        filtered.append(g);
    }

    std::sort(filtered.begin(), filtered.end(), [sortIdx](const Garde &a, const Garde &b) {
        if (sortIdx == 0) return a.date < b.date;              // Date croissante
        if (sortIdx == 1) return a.date > b.date;              // Date décroissante
        if (sortIdx == 2) return a.id.toInt() < b.id.toInt();  // ID croissant
        if (sortIdx == 3) return a.effectif > b.effectif;      // Effectif
        return a.date < b.date;
    });

    QTableWidget *t = ui->tableGrd;
    t->setRowCount(0);
    t->setRowCount(filtered.size());

    const int kRowH = 42;
    for (int r = 0; r < filtered.size(); ++r) {
        const Garde &g = filtered[r];
        auto item = [t, r](int col, const QString &text, bool bold = false) {
            auto *it = new QTableWidgetItem(text);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (bold) {
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
            }
            t->setItem(r, col, it);
        };

        item(0, QString("#%1").arg(g.id), true);
        item(1, g.date.toString("dd/MM/yyyy"));
        item(2, g.type);
        item(3, QString("%1 - %2").arg(g.heureDebut, g.heureFin));
        item(4, QString("%1 pompiers").arg(g.effectif));

        // Statut badge
        QString bg = "#334155", fg = "#ffffff";
        if (g.statut.contains("cours", Qt::CaseInsensitive)) { bg = "#166534"; fg = "#dcfce7"; }
        else if (g.statut.contains("planifi", Qt::CaseInsensitive)) { bg = "#854d0e"; fg = "#fef9c3"; }
        else if (g.statut.contains("termin", Qt::CaseInsensitive)) { bg = "#1e293b"; fg = "#94a3b8"; }
        else if (g.statut.contains("annul", Qt::CaseInsensitive)) { bg = "#991b1b"; fg = "#fee2e2"; }

        auto *pill = new QLabel(g.statut);
        pill->setAlignment(Qt::AlignCenter);
        pill->setStyleSheet(QString("background:%1; color:%2; border-radius:8px; padding:2px 8px; font-size:11px; font-weight:bold;").arg(bg, fg));
        auto *pw = new QWidget;
        auto *ph = new QHBoxLayout(pw);
        ph->setContentsMargins(4, 0, 4, 0);
        ph->addWidget(pill, 0, Qt::AlignVCenter);
        t->setCellWidget(r, 5, pw);

        // Actions
        auto *cell = new QWidget;
        cell->setObjectName("cellGrd");
        cell->setAttribute(Qt::WA_StyledBackground, true);
        auto *cl = new QHBoxLayout(cell);
        cl->setContentsMargins(4, 0, 0, 0);
        cl->setSpacing(2);
        const QString gid = g.id;

        const struct { const char *emoji, *tip; int kind; } actions[] = {
            {"👁️", "Afficher", 0}, {"✏️", "Modifier", 1}, {"🗑️", "Supprimer", 2}
        };
        for (const auto &a : actions) {
            auto *btn = new QToolButton;
            btn->setText(QString::fromUtf8(a.emoji));
            btn->setToolTip(QString::fromUtf8(a.tip));
            btn->setCursor(Qt::PointingHandCursor);
            const int kind = a.kind;
            connect(btn, &QToolButton::clicked, this, [this, gid, kind] {
                if (kind == 0) afficherGarde(gid);
                else if (kind == 1) chargerGarde(gid);
                else supprimerGarde(gid);
            });
            cl->addWidget(btn);
        }
        cl->addStretch();
        t->setCellWidget(r, 6, cell);
        t->setRowHeight(r, kRowH);
    }
}

void AXIS::rafraichirStatsGardes()
{
    const QVector<Garde> all = Garde::lister();
    int tot = all.size();
    int jour = 0, nuit = 0, effTotal = 0;
    for (const Garde &g : all) {
        if (g.type.compare("Jour", Qt::CaseInsensitive) == 0) jour++;
        else if (g.type.compare("Nuit", Qt::CaseInsensitive) == 0) nuit++;
        effTotal += g.effectif;
    }

    ui->lblStatTotalGrd->setText(QString::number(tot));
    ui->lblStatJourGrd->setText(QString::number(jour));
    ui->lblStatNuitGrd->setText(QString::number(nuit));
    ui->lblStatEffectifGrd->setText(QString::number(effTotal));
}

void AXIS::exporterGardesPdf()
{
    const QVector<Garde> all = Garde::lister();
    if (all.isEmpty()) {
        QMessageBox::warning(this, "Export PDF", "Aucune garde à exporter.");
        return;
    }

    const QString chemin = QFileDialog::getSaveFileName(
        this, "Exporter le planning en PDF", "planning_gardes.pdf", "PDF (*.pdf)");
    if (chemin.isEmpty()) return;

    QString html = "<h2 style='color:#c00000'>A.X.I.S - Planning officiel des tours de garde</h2>"
                   "<p>Édité le " + QDateTime::currentDateTime().toString("dd/MM/yyyy à HH:mm") + "</p>"
                                                                                   "<table border='1' cellspacing='0' cellpadding='4' width='100%'>"
                                                                                   "<tr style='background:#eeeeee'><th>ID</th><th>Date</th><th>Type</th><th>Horaires</th>"
                                                                                   "<th>Effectif requis</th><th>Statut</th></tr>";

    for (const Garde &g : all) {
        html += QString("<tr><td align='center'><b>#%1</b></td><td align='center'>%2</td>"
                        "<td align='center'>%3</td><td align='center'>%4 - %5</td>"
                        "<td align='center'>%6 pompiers</td><td align='center'>%7</td></tr>")
                    .arg(g.id, g.date.toString("dd/MM/yyyy"), g.type,
                         g.heureDebut, g.heureFin, QString::number(g.effectif), g.statut);
    }
    html += "</table>";

    QPdfWriter pdf(chemin);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(QPageLayout::Landscape);
    QTextDocument doc;
    doc.setHtml("<body style='color:black;font-size:9pt'>" + html + "</body>");
    doc.print(&pdf);

    QMessageBox::information(this, "Export PDF", "Fichier PDF créé avec succès :\n" + chemin);
}

