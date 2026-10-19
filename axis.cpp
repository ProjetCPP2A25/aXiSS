#include "axis.h"
#include "ui_axis.h"
#include "database.h"

// ===== MODULE EMPLOYÉS (début) : includes à ajouter dans votre axis.cpp =====
#include "conge.h"
#include "smtp.h"
#include <QtWidgets>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QThread>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <memory>
#include <QScrollArea>
#include "interventionpage.h"
#include "vehiclespage.h"
#include "sidebar.h"
// ===== MODULE EMPLOYÉS (fin des includes) =====

// ===== MODULE EMPLOYÉS : classes graphiques DonutWidget et BarWidget (à copier avant le constructeur) =====
// ------------------------------------------------------- Widgets graphiques
class EmployeDonutWidget : public QWidget
{
public:
explicit EmployeDonutWidget(QWidget *p = nullptr) : QWidget(p) { setMinimumSize(130, 130); }
    void setData(int a, int b, int c) { v_[0] = a; v_[1] = b; v_[2] = c; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int total = v_[0] + v_[1] + v_[2];
        const int s = qMin(width(), height()) - 10;
        const double w = s * 0.2;
        QRectF r((width() - s) / 2.0 + w / 2, (height() - s) / 2.0 + w / 2, s - w, s - w);
        const QColor col[3] = {QColor("#1e8e4e"), QColor("#c00000"), QColor("#c06000")};
        if (total == 0) {
            p.setPen(QPen(QColor("#2a2d34"), w));
            p.drawEllipse(r);
        } else {
            double start = 90 * 16;
            for (int i = 0; i < 3; ++i) {
                const double span = -360.0 * 16 * v_[i] / total;
                p.setPen(QPen(col[i], w, Qt::SolidLine, Qt::FlatCap));
                p.drawArc(r, int(start), int(span));
                start += span;
            }
        }
        p.setPen(Qt::white);
        QFont f = font(); f.setPixelSize(22); f.setBold(true); p.setFont(f);
        p.drawText(rect().adjusted(0, -8, 0, 0), Qt::AlignCenter, QString::number(total));
        f.setPixelSize(10); f.setBold(false); p.setFont(f);
        p.drawText(rect().adjusted(0, 28, 0, 0), Qt::AlignCenter, "total");
    }

private:
    int v_[3] = {0, 0, 0};
};

class BarWidget : public QWidget
{
public:
    explicit BarWidget(QWidget *p = nullptr) : QWidget(p) { setMinimumSize(200, 130); }
    void setData(const QStringList &labels, const QVector<int> &values) { labels_ = labels; values_ = values; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int n = values_.size();
        if (n == 0) return;
        const int mx = qMax(1, *std::max_element(values_.begin(), values_.end()));
        const int base = height() - 22, top = 22;
        const double slot = double(width()) / n, bw = slot * 0.6;
        for (int i = 0; i < n; ++i) {
            const double h = (base - top) * double(values_[i]) / mx;
            QRectF r(i * slot + (slot - bw) / 2, base - h, bw, h);
            p.fillRect(r, QColor("#ff0000"));
            p.setPen(Qt::white);
            p.drawText(QRectF(i * slot, r.top() - 18, slot, 16), Qt::AlignCenter, QString::number(values_[i]));
            p.setPen(QColor("#aaaaaa"));
            p.drawText(QRectF(i * slot, base + 4, slot, 16), Qt::AlignCenter, labels_[i]);
        }
        p.setPen(QColor("#2a2d34"));
        p.drawLine(0, base, width(), base);
    }

private:
    QStringList labels_;
    QVector<int> values_;
};

// ===== FIN classes graphiques =====

// Supprime le contenu « placeholder » d'une page du .ui (widgets et layouts imbriqués compris)
// pour pouvoir y placer le widget réel d'un module.
static void viderLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        } else if (QLayout *sub = item->layout()) {
            viderLayout(sub);
        }
        delete item;
    }
}

static void viderPage(QWidget *page)
{
    if (QLayout *old = page->layout()) {
        viderLayout(old);
        delete old;
    }
}

// ------------------------------------------------------------ Construction
AXIS::AXIS(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::AXIS)
{
    ui->setupUi(this);


    setWindowTitle("A.X.I.S - RedFlame Protection");

    // ===== MODULE EMPLOYÉS : initialisation (à copier dans le constructeur, après ui->setupUi(this)) =====
    // ===== MODULE MATÉRIELS =====
    if (QLayout *old = ui->PMateriel->layout()) {   // نفسخو الـ layout القديم إذا موجود
        while (QLayoutItem *it = old->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        delete old;
    }
    materielPage_ = new MaterielPage(ui->PMateriel);
    auto *layoutMateriel = new QVBoxLayout(ui->PMateriel);
    layoutMateriel->setContentsMargins(0, 0, 0, 0);
    layoutMateriel->addWidget(materielPage_);

    // ===== MODULE INTERVENTIONS =====
    viderPage(ui->PInterv);
    interventionPage_ = new InterventionPage(ui->PInterv);

    auto *layoutIntervention = new QVBoxLayout(ui->PInterv);
    layoutIntervention->setContentsMargins(0, 0, 0, 0);
    layoutIntervention->addWidget(interventionPage_);

    // ===== MODULE VÉHICULES =====
    viderPage(ui->PVehicule);
    vehiclesPage_ = new VehiclesPage(ui->PVehicule);

    auto *layoutVehicule = new QVBoxLayout(ui->PVehicule);
    layoutVehicule->setContentsMargins(0, 0, 0, 0);
    layoutVehicule->addWidget(vehiclesPage_);

    // Barre latérale du module Employés (même structure que celle des interventions), à gauche de la page CRUD
    sidebarEmpl_ = new SidebarFrame(ui->PCrudEmpl);
    auto *layoutEmpl = static_cast<QHBoxLayout *>(ui->PCrudEmpl->layout());
    layoutEmpl->insertWidget(0, sidebarEmpl_);
    layoutEmpl->setStretch(0, 0);
    layoutEmpl->setStretch(1, 1);

    // Logos (ressources.qrc : :/logo.png)
    chargerLogo(ui->lblLogoLoginEmpl, 260);
    chargerLogo(ui->lblLogoMenuEmpl, 200);

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

    // Graphiques
    donut_ = new EmployeDonutWidget(this);
    ui->layoutDonutEmpl->addWidget(donut_);
    bars_ = new BarWidget;
    ui->layoutBarresEmpl->addWidget(bars_);

    ui->dteDebutCongeEmpl->setDate(QDate::currentDate());
    ui->dteFinCongeEmpl->setDate(QDate::currentDate());

    // ----- Navigation (connect explicites, un seul .ui avec des Stacked Widget) -----
    // Login
    connect(ui->btnConnexionEmpl, &QPushButton::clicked, this, &AXIS::connexionEmploye);
    connect(ui->txtMdpLoginEmpl, &QLineEdit::returnPressed, this, &AXIS::connexionEmploye);
    connect(ui->txtEmailLoginEmpl, &QLineEdit::returnPressed, this, [this] { ui->txtMdpLoginEmpl->setFocus(); });
    connect(ui->btnMdpOublieEmpl, &QPushButton::clicked, this, [this] { ui->stackedWidgetEmpl->setCurrentIndex(2); });
    connect(ui->btnQuitterLoginEmpl, &QPushButton::clicked, this, &QWidget::close);
    // Mot de passe oublié
    connect(ui->btnEnvoyerMdpOublieEmpl, &QPushButton::clicked, this, &AXIS::envoyerMdpOublie);
    connect(ui->btnRetourLoginMdpOublieEmpl, &QPushButton::clicked, this, [this] { ui->stackedWidgetEmpl->setCurrentIndex(0); });
    // Menu principal : un bouton par module
    connect(ui->btnMenuEmpl, &QPushButton::clicked, this, [this] { allerAuModule(0); });
    connect(ui->btnMenuInterv, &QPushButton::clicked, this, [this] { allerAuModule(1); });
    connect(ui->btnMenuVehicule, &QPushButton::clicked, this, [this] { allerAuModule(2); });
    connect(ui->btnMenuMateriel, &QPushButton::clicked, this, [this] { allerAuModule(3); });
    connect(ui->btnQuitterMenuEmpl, &QPushButton::clicked, this, &AXIS::deconnexionEmploye);
    // Barre latérale du module Employés
    connect(sidebarEmpl_, &SidebarFrame::menuRequested, this, &AXIS::allerAuMenu);
    connect(sidebarEmpl_, &SidebarFrame::logoutRequested, this, &AXIS::deconnexionEmploye);
    // Pages Interventions et Matériels : boutons « Menu principal » et « Déconnexion »
    connect(interventionPage_, &InterventionPage::menuRequested, this, &AXIS::allerAuMenu);
    connect(interventionPage_, &InterventionPage::logoutRequested, this, &AXIS::deconnexionEmploye);
    connect(materielPage_, &MaterielPage::menuRequested, this, &AXIS::allerAuMenu);
    connect(materielPage_, &MaterielPage::logoutRequested, this, &AXIS::deconnexionEmploye);
    connect(vehiclesPage_, &VehiclesPage::menuRequested, this, &AXIS::allerAuMenu);
    connect(vehiclesPage_, &VehiclesPage::logoutRequested, this, &AXIS::deconnexionEmploye);


    // ----- Module Employés -----
    connect(ui->btnAjouterEmpl, &QPushButton::clicked, this, &AXIS::enregistrerEmploye);
    connect(ui->btnViderEmpl, &QPushButton::clicked, this, &AXIS::viderFormulaire);
    connect(ui->txtRechercheEmpl, &QLineEdit::textChanged, this, [this] { rafraichirListe(); });
    connect(ui->cmbTriEmpl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { rafraichirListe(); });
    connect(ui->btnExporterEmpl, &QPushButton::clicked, this, &AXIS::exporterAttestation);
    connect(ui->btnEnregistrerCongeEmpl, &QPushButton::clicked, this, &AXIS::enregistrerConge);

    // Surveillance de la surcharge horaire (démarrée après la connexion d'un chef / administrateur)
    timerSurcharge_ = new QTimer(this);
    connect(timerSurcharge_, &QTimer::timeout, this, [this] { majAlerte(); });

    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(0);   // page de connexion
    // ===== FIN initialisation module Employés =====
}

AXIS::~AXIS()
{
    delete ui;
}

// ===== MODULE EMPLOYÉS : slots et fonctions (à copier à la fin de votre axis.cpp) =====
// ------------------------------------------------------------ Connexion / navigation
void AXIS::chargerLogo(QLabel *label, int largeur)
{
    const QPixmap logo(":/logo.png");
    if (logo.isNull()) return;   // sinon on garde le texte « A.X.I.S »
    label->setPixmap(logo.scaledToWidth(largeur, Qt::SmoothTransformation));
    label->setAlignment(Qt::AlignCenter);
}

void AXIS::connexionEmploye()
{
    const QString email = ui->txtEmailLoginEmpl->text().trimmed();
    const QString mdp = ui->txtMdpLoginEmpl->text();
    if (email.isEmpty() || mdp.isEmpty()) {
        QMessageBox::warning(this, "Connexion", "Veuillez saisir l'email et le mot de passe.");
        return;
    }
    const Employe e = Employe::authentifier(email, mdp);
    if (e.id.isEmpty()) {
        QMessageBox::warning(this, "Connexion", "Email ou mot de passe incorrect.");
        ui->txtMdpLoginEmpl->clear();
        return;
    }
    // Droits : seuls les employés ayant un rôle (poste) autorisé utilisent l'application.
    if (!Employe::estUtilisateur(e.poste)) {
        QMessageBox::warning(this, "Accès refusé",
                             QString("Le poste « %1 » ne donne pas accès à l'application.\n"
                                     "Seuls l'administrateur, le chef de caserne, l'opérateur de dispatch "
                                     "et le magasinier peuvent se connecter.").arg(e.poste));
        ui->txtMdpLoginEmpl->clear();
        return;
    }
    user_ = e;
    ui->txtMdpLoginEmpl->clear();
    appliquerDroits();
    rafraichirListe();
    rafraichirCombos();
    afficherConges();
    allerAuMenu();
}

void AXIS::appliquerDroits()
{
    const QString connecte = QString("Connecté : %1 (%2)").arg(user_.nomComplet(), user_.poste);
    sidebarEmpl_->setInfo(connecte);
    vehiclesPage_->setUserInfo(connecte);
    ui->lblConnecteMenuEmpl->setText(connecte);
    ui->lblBienvenueEmpl->setText("Bienvenue " + user_.nomComplet());

    // Seul le chef de caserne enregistre et décide les congés
    peutDecider_ = (user_.poste == "Chef de caserne");
    ui->btnEnregistrerCongeEmpl->setEnabled(peutDecider_);
    ui->btnEnregistrerCongeEmpl->setToolTip(peutDecider_ ? QString() : "Réservé au chef de caserne");

    // Modules accessibles selon le rôle (menu principal)
    const QList<QPushButton *> menu = {ui->btnMenuEmpl, ui->btnMenuInterv, ui->btnMenuVehicule, ui->btnMenuMateriel};
    for (int i = 0; i < 4; ++i) {
        const bool ok = Employe::aAcces(user_.poste, i);
        menu[i]->setEnabled(ok);
        menu[i]->setToolTip(ok ? QString() : "Accès refusé pour votre rôle (" + user_.poste + ")");
    }

    // Alerte de surcharge : uniquement pour les rôles qui gèrent les employés
    timerSurcharge_->stop();
    if (Employe::aAcces(user_.poste, 0)) {
        QTimer::singleShot(600, this, [this] { if (!user_.id.isEmpty()) majAlerte(); });
        timerSurcharge_->start(60000);
    }
}

void AXIS::deconnexionEmploye()
{
    timerSurcharge_->stop();
    user_ = Employe();
    peutDecider_ = false;
    viderFormulaire();
    ui->txtEmailLoginEmpl->clear();
    ui->txtMdpLoginEmpl->clear();
    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(0);
}

void AXIS::allerAuMenu()
{
    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(1);
}

void AXIS::allerAuModule(int module)
{
    if (!Employe::aAcces(user_.poste, module)) return;
    if (module == 0) {                              // module Employés : page CRUD
        ui->SWAxis->setCurrentIndex(0);
        ui->stackedWidgetEmpl->setCurrentIndex(3);
        rafraichirListe();
    } else {                                        // autres modules : pages du SWAxis
        ui->SWAxis->setCurrentIndex(module);
    }
}

// Mot de passe oublié : un mot de passe temporaire est généré et envoyé par email
void AXIS::envoyerMdpOublie()
{
    const QString email = ui->txtEmailMdpOublieEmpl->text().trimmed();
    if (!Employe::emailValide(email)) {
        QMessageBox::warning(this, "Mot de passe oublié", "Saisissez une adresse email valide (nom@gmail.com).");
        return;
    }
    Employe e = Employe::trouverParEmail(email);
    if (e.id.isEmpty() || !Employe::estUtilisateur(e.poste)) {
        QMessageBox::warning(this, "Mot de passe oublié", "Aucun compte utilisateur ne correspond à cet email.");
        return;
    }
    QSettings s;
    SmtpConfig cfg;
    cfg.hote = s.value("smtp/host", "smtp.gmail.com").toString();
    cfg.port = s.value("smtp/port", 465).toInt();
    cfg.user = s.value("smtp/user").toString().trimmed();
    cfg.pass = s.value("smtp/pass").toString();
    QString from = s.value("smtp/from").toString().trimmed();
    if (from.isEmpty() && cfg.user.contains('@')) from = cfg.user;
    if (cfg.user.isEmpty() || cfg.pass.trimmed().isEmpty() || from.isEmpty()) {
        QMessageBox::warning(this, "Mot de passe oublié",
                             "L'envoi d'email n'est pas encore configuré (Paramètres email). Contactez l'administrateur.");
        return;
    }

    // Mot de passe temporaire de 8 caractères
    const QString alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    QString temp;
    for (int i = 0; i < 8; ++i) temp += alphabet.at(int(QRandomGenerator::system()->bounded(alphabet.size())));

    EmailMessage m;
    m.from = from;
    m.nomFrom = "A.X.I.S - RedFlame Protection";
    m.to = e.email;
    m.sujet = "Votre mot de passe temporaire A.X.I.S";
    m.corps = QString("Bonjour %1,\n\nVoici votre mot de passe temporaire : %2\n\n"
                      "Connectez-vous avec cet email et ce mot de passe. "
                      "Demandez ensuite à l'administrateur de le remplacer par un mot de passe personnel.\n\n"
                      "A.X.I.S - RedFlame Protection").arg(e.nomComplet(), temp);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    const bool ok = envoyerEmail(cfg, m, err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, "Mot de passe oublié", "Envoi impossible :\n" + err);
        return;
    }
    e.motDePasse = Employe::hash(temp);     // le mot de passe n'est changé qu'après l'envoi réussi
    e.modifier();
    ui->txtEmailMdpOublieEmpl->clear();
    ui->txtEmailLoginEmpl->setText(e.email);
    ui->stackedWidgetEmpl->setCurrentIndex(0);
    QMessageBox::information(this, "Mot de passe oublié", "Un mot de passe temporaire a été envoyé à " + e.email + ".");
}

// ------------------------------------------------------------------ Liste
void AXIS::rafraichirListe()
{
    QTableWidget *t = ui->tblEmpl;
    t->setRowCount(0);
    const QVector<Employe> liste = Employe::lister(ui->txtRechercheEmpl->text().trimmed(), ui->cmbTriEmpl->currentIndex());

    for (const Employe &e : liste) {
        const int r = t->rowCount();
        t->insertRow(r);
        t->setRowHeight(r, 76);
        const QStringList v = {e.id, e.nom, e.prenom, e.telephone, e.email, e.grade, QString::number(e.age), e.poste,
                               QString::number(e.heures) + " h", QString::number(e.conges) + " j"};
        for (int c = 0; c < v.size(); ++c) {
            auto *it = new QTableWidgetItem(v[c]);
            if (c == 8 && e.heures > 48) it->setForeground(QColor("#ff2a2a"));
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
    majAlerte();
    rafraichirStats();
}

void AXIS::majAlerte()
{
    const int over = Employe::surcharge(false).size();
    ui->lblAlerteEmpl->setVisible(over > 0);
    if (over == 0) return;
    ui->lblAlerteEmpl->setText(QString("Alerte : %1 employé(s) dépassent 48 h/semaine.").arg(over));
}

void AXIS::rafraichirStats()
{
    const QMap<QString, int> d = Employe::statsDisponibilite();   // COUNT + GROUP BY
    const QMap<QString, int> g = Employe::statsGrade();
    const int dispo = d.value("Disponible"), interv = d.value("En intervention"), indispo = d.value("Indisponible");
    const int total = qMax(1, dispo + interv + indispo);
    donut_->setData(dispo, interv, indispo);
    auto ligne = [&](const char *couleur, const QString &txt, int n) {
        return QString("<span style='color:%1;'>&#9679;</span> %2 <b>%3</b> (%4%)<br>")
            .arg(couleur, txt).arg(n).arg(100 * n / total);
    };
    ui->lblLegendeEmpl->setText(ligne("#1e8e4e", "Disponibles", dispo) + ligne("#c00000", "En intervention", interv) +
                            ligne("#c06000", "Indisponibles", indispo));
    bars_->setData({"Sergents", "Caporaux", "Lieutenants", "Capitaines"},
                   {g.value("Sergent"), g.value("Caporal"), g.value("Lieutenant"), g.value("Capitaine")});
}

void AXIS::rafraichirCombos()
{
    ui->cmbCongeEmpl->clear();
    for (const Employe &e : Employe::lister("", 2)) ui->cmbCongeEmpl->addItem(e.nomComplet(), e.id);
}

// ------------------------------------------------------------ Employés
void AXIS::viderFormulaire()
{
    editingId_.clear();
    ui->txtNomEmpl->clear(); ui->txtPrenomEmpl->clear(); ui->txtEmailEmpl->clear(); ui->txtTelEmpl->clear(); ui->txtMdpEmpl->clear();
    ui->txtMdpEmpl->setPlaceholderText(QString());
    ui->spnAgeEmpl->setValue(30); ui->spnHeuresEmpl->setValue(40); ui->spnCongesEmpl->setValue(18);
    ui->cmbGradeEmpl->setCurrentIndex(0); ui->cmbPosteEmpl->setCurrentIndex(0); ui->cmbDispoEmpl->setCurrentIndex(0);
    ui->btnAjouterEmpl->setText("Ajouter");
}

void AXIS::enregistrerEmploye()
{
    const bool edition = !editingId_.isEmpty();
    const QString email = ui->txtEmailEmpl->text().trimmed();
    const QString mdp = ui->txtMdpEmpl->text();

    // Contrôle de saisie : on affiche toutes les erreurs d'un coup
    QStringList erreurs;
    if (ui->txtNomEmpl->text().trimmed().isEmpty())
        erreurs << "Le nom est obligatoire.";
    if (ui->txtPrenomEmpl->text().trimmed().isEmpty())
        erreurs << "Le prénom est obligatoire.";
    if (email.isEmpty())
        erreurs << "L'email est obligatoire.";
    else if (!Employe::emailValide(email))
        erreurs << "L'email doit avoir la forme nom@gmail.com ou nom@email.com.";
    else if (Employe::emailExiste(email, editingId_))
        erreurs << "Un employé utilise déjà cet email.";
    if (!Employe::telephoneValide(ui->txtTelEmpl->text()))
        erreurs << "Le téléphone doit contenir uniquement des chiffres, au moins 8 (15 au plus).";
    if (!edition && mdp.length() < 4)
        erreurs << "Le mot de passe doit contenir au moins 4 caractères.";
    else if (edition && !mdp.isEmpty() && mdp.length() < 4)
        erreurs << "Le nouveau mot de passe doit contenir au moins 4 caractères.";
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
        const Employe ancien = Employe::trouver(editingId_);
        e.id = editingId_;
        e.motDePasse = ui->txtMdpEmpl->text().isEmpty() ? ancien.motDePasse : Employe::hash(ui->txtMdpEmpl->text());
        e.alerte = (e.heures > 48) ? ancien.alerte : false;
        ok = e.modifier();
    } else {
        e.motDePasse = Employe::hash(ui->txtMdpEmpl->text());
        ok = e.ajouter();
    }
    if (!ok) {
        QMessageBox::critical(this, "Erreur", "Opération impossible : " + Employe::derniereErreur());
        return;
    }
    viderFormulaire();
    rafraichirListe();
    rafraichirCombos();
}

void AXIS::modifierEmploye(const QString &id)
{
    const Employe e = Employe::trouver(id);
    if (e.id.isEmpty()) return;
    editingId_ = id;
    ui->txtNomEmpl->setText(e.nom); ui->txtPrenomEmpl->setText(e.prenom); ui->txtEmailEmpl->setText(e.email); ui->txtTelEmpl->setText(e.telephone);
    ui->txtMdpEmpl->clear();
    ui->txtMdpEmpl->setPlaceholderText("(inchangé)");
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
    if (id == user_.id) {
        QMessageBox::warning(this, "Suppression impossible", "Vous ne pouvez pas supprimer votre propre compte.");
        return;
    }
    if (QMessageBox::question(this, "Supprimer", "Supprimer cet employé ?") != QMessageBox::Yes) return;
    if (!Employe::supprimer(id)) {
        QMessageBox::critical(this, "Erreur", "Suppression impossible : " + Employe::derniereErreur());
        return;
    }
    rafraichirListe();
    rafraichirCombos();
    afficherConges();
}

// ------------------------------------------------------------ Export
void AXIS::exporterAttestation()
{
    const QVector<Employe> liste = Employe::lister("", 2);
    if (liste.isEmpty()) return;
    QStringList noms;
    for (const Employe &e : liste) noms << QString("%1 - %2").arg(e.id, e.nomComplet());
    bool ok = false;
    const QString choix = QInputDialog::getItem(this, "Attestation de présence", "Choisir l'employé :", noms, 0, false, &ok);
    if (!ok) return;
    const Employe e = liste[noms.indexOf(choix)];

    QString nomFichier = e.nomComplet();
    nomFichier.replace(' ', '_');
    const QString chemin = QFileDialog::getSaveFileName(this, "Exporter l'attestation de présence",
                                                        QString("attestation_%1.csv").arg(nomFichier), "Excel / CSV (*.csv)");
    if (chemin.isEmpty()) return;

    const QStringList lignes = {
        "ATTESTATION DE PRÉSENCE;",
        "Caserne;RedFlame Protection",
        "Date d'édition;" + QDate::currentDate().toString("dd/MM/yyyy"),
        ";",
        "ID;" + e.id,
        "Nom;" + e.nom,
        "Prénom;" + e.prenom,
        "Grade;" + e.grade,
        "Poste;" + e.poste,
        "Âge;" + QString::number(e.age),
        "Heures de travail / semaine;" + QString::number(e.heures),
        "Congés restants (jours);" + QString::number(e.conges),
        "Disponibilité;" + e.disponibilite,
        ";",
        "Le chef de caserne atteste que l'employé ci-dessus fait partie de l'effectif de la caserne.;",
    };
    QFile f(chemin);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Erreur", "Impossible d'écrire le fichier.");
        return;
    }
    f.write("\xEF\xBB\xBF");   // BOM UTF-8 : Excel affiche correctement les accents
    f.write(lignes.join("\r\n").toUtf8());
    f.close();
    QMessageBox::information(this, "Export", "Attestation exportée. Vous pouvez l'ouvrir avec Excel.");
}

// ------------------------------------------------------------ Congés
void AXIS::enregistrerConge()
{
    if (ui->cmbCongeEmpl->currentIndex() < 0) return;
    if (ui->dteFinCongeEmpl->date() < ui->dteDebutCongeEmpl->date()) {
        QMessageBox::warning(this, "Dates invalides", "La date de fin précède la date de début.");
        return;
    }
    Conge c;
    c.idEmploye = ui->cmbCongeEmpl->currentData().toString();
    c.nomEmploye = ui->cmbCongeEmpl->currentText();
    c.jours = ui->dteDebutCongeEmpl->date().daysTo(ui->dteFinCongeEmpl->date()) + 1;
    c.details = QString("%1 au %2 · %3 j · Motif : %4")
                    .arg(ui->dteDebutCongeEmpl->date().toString("dd/MM"), ui->dteFinCongeEmpl->date().toString("dd/MM"))
                    .arg(c.jours).arg(ui->txtMotifCongeEmpl->text());
    if (c.jours > Employe::trouver(c.idEmploye).conges) {   // refus automatique
        c.statut = "Refusée";
        c.details += " · Solde insuffisant";
    } else {
        c.statut = "En attente";
    }
    if (!c.ajouter()) {
        QMessageBox::critical(this, "Erreur", "Enregistrement impossible : " + Employe::derniereErreur());
        return;
    }
    ui->txtMotifCongeEmpl->clear();
    afficherConges();
}

void AXIS::deciderConge(int id, bool accepter)
{
    QString message;
    if (!Conge::decider(id, accepter, message))
        QMessageBox::critical(this, "Erreur", "Opération impossible : " + Employe::derniereErreur());
    else if (!message.isEmpty())
        QMessageBox::information(this, "Congé refusé", message);
    rafraichirListe();    // met à jour la colonne « Congés »
    afficherConges();
}

void AXIS::afficherConges()
{
    while (QLayoutItem *it = ui->layoutDemandesEmpl->takeAt(0)) {
        if (it->widget()) { it->widget()->hide(); it->widget()->deleteLater(); }
        delete it;
    }
    for (const Conge &c : Conge::lister(8)) {
        auto *carte = new QFrame;
        carte->setStyleSheet("QFrame{border:1px solid #2a2d34;border-radius:8px;}");
        auto *h = new QHBoxLayout(carte);
        h->setContentsMargins(10, 8, 10, 8);
        auto *txt = new QVBoxLayout;
        auto *titre = new QLabel(c.nomEmploye);
        titre->setStyleSheet("border:none;font-size:14px;");
        auto *det = new QLabel(c.details);
        det->setStyleSheet("border:none;color:#aaaaaa;font-size:12px;");
        det->setWordWrap(true);
        txt->addWidget(titre);
        txt->addWidget(det);
        h->addLayout(txt, 1);

        if (c.statut == "En attente") {
            if (peutDecider_) {
                auto *ok = new QPushButton("Accepter");
                ok->setProperty("primary", true);
                auto *ko = new QPushButton("Refuser");
                const int id = c.id;
                connect(ok, &QPushButton::clicked, this, [this, id] { deciderConge(id, true); });
                connect(ko, &QPushButton::clicked, this, [this, id] { deciderConge(id, false); });
                h->addWidget(ok);
                h->addWidget(ko);
            } else {
                auto *p = new QLabel("En attente");
                p->setStyleSheet("border:none;background:#fbe6cf;color:#b45f00;border-radius:9px;padding:2px 10px;");
                h->addWidget(p);
            }
        } else {
            const bool bon = (c.statut == "Acceptée");
            auto *p = new QLabel(c.statut);
            p->setStyleSheet(QString("border:none;background:%1;color:%2;border-radius:9px;padding:2px 10px;")
                                 .arg(bon ? "#d6f0df" : "#fbdcdc", bon ? "#1e7a45" : "#c00000"));
            h->addWidget(p);
        }
        ui->layoutDemandesEmpl->addWidget(carte);
    }
}

// ===== FIN MODULE EMPLOYÉS =====
