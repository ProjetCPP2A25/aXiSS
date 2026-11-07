#include "axis.h"
#include "ui_axis.h"
#include "database.h"

// ===== MODULE EMPLOYÉS (début) : includes à ajouter dans votre axis.cpp =====
#include <QtWidgets>
#include <QRegularExpressionValidator>
#include <QFile>
#include <QThread>
#include <QScrollArea>
#include "interventionpage.h"
#include "vehiculepage.h"
// ===== MODULE EMPLOYÉS (fin des includes) =====

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

    interventionPage_ = new InterventionPage(ui->PInterv);

    auto *layoutIntervention = new QVBoxLayout(ui->PInterv);
    layoutIntervention->setContentsMargins(0, 0, 0, 0);
    layoutIntervention->addWidget(interventionPage_);

    // ===== MODULE VÉHICULES : remplace la page provisoire de PVehicule =====
    ui->lblTitreVehicule->deleteLater();
    ui->lblInfoVehicule->deleteLater();
    ui->btnRetourMenuVehicule->deleteLater();
    if (QLayout *old = ui->PVehicule->layout()) {
        while (QLayoutItem *it = old->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        delete old;
    }
    vehiculePage_ = new VehiculePage(ui->PVehicule);
    auto *layoutVehicule = new QVBoxLayout(ui->PVehicule);
    layoutVehicule->setContentsMargins(0, 0, 0, 0);
    layoutVehicule->addWidget(vehiculePage_);

    // Barre latérale du module Employés (même structure que celle des interventions), à gauche de la page CRUD
    sidebarEmpl_ = new SidebarFrame(ui->PCrudEmpl);
    auto *layoutEmpl = static_cast<QHBoxLayout *>(ui->PCrudEmpl->layout());
    layoutEmpl->insertWidget(0, sidebarEmpl_);
    layoutEmpl->setStretch(0, 0);
    layoutEmpl->setStretch(1, 1);

    // Logos (ressources.qrc : :/logo.png)
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

    // ----- Navigation (connect explicites, un seul .ui avec des Stacked Widget) -----
    // Menu principal : un bouton par module
    connect(ui->btnMenuEmpl, &QPushButton::clicked, this, [this] { allerAuModule(0); });
    connect(ui->btnMenuInterv, &QPushButton::clicked, this, [this] { allerAuModule(1); });
    connect(ui->btnMenuVehicule, &QPushButton::clicked, this, [this] { allerAuModule(2); });
    connect(ui->btnMenuMateriel, &QPushButton::clicked, this, [this] { allerAuModule(3); });
    connect(ui->btnQuitterMenuEmpl, &QPushButton::clicked, this, &QWidget::close);
    // Barre latérale du module Employés
    connect(sidebarEmpl_, &SidebarFrame::menuRequested, this, &AXIS::allerAuMenu);
    connect(sidebarEmpl_, &SidebarFrame::logoutRequested, this, &QWidget::close);
    // Pages Interventions et Matériels : boutons « Menu principal » et « Quitter »
    connect(interventionPage_, &InterventionPage::menuRequested, this, &AXIS::allerAuMenu);
    connect(interventionPage_, &InterventionPage::logoutRequested, this, &QWidget::close);
    connect(materielPage_, &MaterielPage::menuRequested, this, &AXIS::allerAuMenu);
    connect(materielPage_, &MaterielPage::logoutRequested, this, &QWidget::close);
    connect(vehiculePage_, &VehiculePage::menuRequested, this, &AXIS::allerAuMenu);
    connect(vehiculePage_, &VehiculePage::logoutRequested, this, &QWidget::close);
    // Pages des autres modules (à remplacer par les widgets de vos camarades)
    connect(ui->btnRetourMenuInterv, &QPushButton::clicked, this, &AXIS::allerAuMenu);


    // ----- Module Employés -----
    connect(ui->btnAjouterEmpl, &QPushButton::clicked, this, &AXIS::enregistrerEmploye);
    connect(ui->btnViderEmpl, &QPushButton::clicked, this, &AXIS::viderFormulaire);

    ui->SWAxis->setCurrentIndex(0);
    ui->stackedWidgetEmpl->setCurrentIndex(0);   // l'application s'ouvre directement sur le menu principal
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
        if (module == 2) vehiculePage_->reload();   // alertes de maintenance à jour
    }
}

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

// ------------------------------------------------------------ Employés
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

// ===== FIN MODULE EMPLOYÉS =====
