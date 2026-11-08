#include "axis.h"
#include "ui_axis.h"

#include <QtWidgets>
#include <QRegularExpressionValidator>
#include <QSslSocket>
#include <QTcpSocket>
#include <QThreadPool>

#include <algorithm>
#include <functional>

// ==========================================
// Outils d'affichage du module Interventions
// ==========================================
namespace {

// Colonnes du tableau des interventions (tableInterventions)
enum Col { ColId, ColType, ColLieu, ColGravite, ColGps, ColReception, ColFin, ColDuree, ColEtat, ColActions,
           NbColonnes };

const char *const kFormatDate = "dd/MM/yyyy HH:mm";

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

    // ==========================================
    // AUTRES MODULES (interface dans axis.ui)
    // ==========================================
    initialiserModuleInterventions();
    initialiserModuleVehicules();
    initialiserModuleMateriels();

    // ----- Navigation (connect explicites, un seul .ui avec des Stacked Widget) -----
    // Menu principal : un bouton par module
    connect(ui->btnMenuEmpl, &QPushButton::clicked, this, [this] { allerAuModule(0); });
    connect(ui->btnMenuInterv, &QPushButton::clicked, this, [this] { allerAuModule(1); });
    connect(ui->btnMenuVehicule, &QPushButton::clicked, this, [this] { allerAuModule(2); });
    connect(ui->btnMenuMateriel, &QPushButton::clicked, this, [this] { allerAuModule(3); });
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
    const QVector<Employe> liste = Employe::lister();

    for (const Employe &e : liste) {
        const int r = t->rowCount();
        t->insertRow(r);
        t->setRowHeight(r, 76);
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
        t->setCellWidget(r, 11, aw);
    }
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
    if (!Employe::supprimer(id)) {
        QMessageBox::critical(this, "Erreur", "Suppression impossible : " + Employe::derniereErreur());
        return;
    }
    rafraichirListe();
}

// ==========================================
// MODULE INTERVENTIONS
// ==========================================
// Présentation du tableau / du formulaire et connexions des widgets de la page PInterv (axis.ui).
// Recherche, tri, pagination, export, statistiques, zones à risque et carte : interface conservée, sans logique.
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
void AXIS::rafraichirListeInterventions()
{
    QString erreur;
    const QVector<Intervention> liste = Intervention::lister(&erreur);
    QTableWidget *t = ui->tableInterventions;
    t->clearContents();
    t->setRowCount(0);
    if (!erreur.isEmpty()) {
        ui->labelPagination->setText(QStringLiteral("Lecture impossible"));
        QMessageBox::critical(this, QStringLiteral("Erreur de la base de données"), erreur);
        return;
    }

    // Liste vide : message explicite à la place de la pagination
    ui->labelPagination->setText(liste.isEmpty()
                                     ? QStringLiteral("Aucune intervention enregistrée")
                                     : QStringLiteral("%1 intervention%2").arg(liste.size()).arg(liste.size() > 1 ? "s" : ""));

    auto creerItem = [](const QString &texte, const QString &infobulle) {
        auto *item = new QTableWidgetItem(texte);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setToolTip(infobulle);
        return item;
    };

    t->setRowCount(liste.size());
    for (int r = 0; r < liste.size(); ++r) {
        const Intervention &i = liste[r];
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

// ==========================================
// MODULE VEHICULES
// ==========================================
// Interface : page PVehicule de axis.ui (barre latérale + QScrollArea). AFFICHAGE SEUL : les boutons du formulaire
// sont présents mais non fonctionnels ; seule la navigation (« Menu principal », « Quitter ») est active.
void AXIS::initialiserModuleVehicules()
{
    // Barre latérale à gauche, page défilante à droite
    sidebarVeh_ = new SidebarFrame(ui->PVehicule);
    auto *lay = static_cast<QHBoxLayout *>(ui->PVehicule->layout());
    lay->insertWidget(0, sidebarVeh_);
    lay->setStretch(0, 0);
    lay->setStretch(1, 1);
    connect(sidebarVeh_, &SidebarFrame::menuRequested, this, &AXIS::allerAuMenu);
    connect(sidebarVeh_, &SidebarFrame::logoutRequested, this, &QWidget::close);

    // Présentation du tableau (aucune donnée : affichage seul)
    QHeaderView *h = ui->tableVeh->horizontalHeader();
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::ResizeToContents);
    h->setFixedHeight(44);

    ui->bannerVeh->hide();   // le bandeau « maintenance requise » n'a pas de sens sans données
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
    const int largeurs[] = {48, 80, 0, 92, 66, 46, 100, 98, 82, 76, 112};
    for (int col = 0; col < 11; ++col)
        if (largeurs[col]) ui->tableMat->setColumnWidth(col, largeurs[col]);
    ui->tableMat->verticalHeader()->hide();
    ui->tableMat->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->btnAddMat, &QPushButton::clicked, this, &AXIS::enregistrerMateriel);
    connect(ui->btnClearMat, &QPushButton::clicked, this, &AXIS::viderFormulaireMateriel);
    connect(ui->btnExport, &QPushButton::clicked, this, &AXIS::exporterMateriels);
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
            {"👁️", "Afficher", 0}, {"✏️", "Modifier", 1}, {"🗑️", "Supprimer", 2}};
        for (const auto &a : actions) {
            auto *btn = new QToolButton;
            btn->setText(QString::fromUtf8(a.emoji));
            btn->setToolTip(QString::fromUtf8(a.tip));
            btn->setCursor(Qt::PointingHandCursor);
            const int kind = a.kind;
            connect(btn, &QToolButton::clicked, this, [this, id, kind] {
                if (kind == 0) afficherMateriel(id);
                else if (kind == 1) chargerMateriel(id);
                else supprimerMateriel(id);
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
    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(0);
}

void AXIS::allerAuModule(int module)
{
    if (module == 0) {                              // module Employés : page CRUD
        ui->SWAxis->setCurrentIndex(0);
        ui->stackedWidgetEmpl->setCurrentIndex(1);
        rafraichirListe();
    } else {                                        // autres modules : pages du SWAxis
        ui->SWAxis->setCurrentIndex(module);
        if (module == 1)                            // module Interventions : tableau à jour à l'ouverture
            rafraichirListeInterventions();
        else if (module == 3) {                      // module Matériels : liste et statistiques à jour
            rafraichirListeMateriels();
            rafraichirStatsMateriels();
        }
    }
}
