#include "entretiendialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString dateTexte(const QDate &date)
{
    return date.isValid() ? date.toString("dd/MM/yyyy") : QStringLiteral("—");
}

QSpinBox *creerChampKm(QWidget *parent)
{
    auto *champ = new QSpinBox(parent);
    champ->setRange(0, 2000000);
    champ->setSingleStep(1000);
    champ->setSpecialValueText("Non renseigné");
    champ->setSuffix(" km");
    return champ;
}

QDateEdit *creerChampDate(QWidget *parent, const QDate &date = QDate::currentDate())
{
    auto *champ = new QDateEdit(date, parent);
    champ->setCalendarPopup(true);
    champ->setDisplayFormat("dd/MM/yyyy");
    return champ;
}

void afficherErreur(QWidget *parent, const QString &erreur)
{
    QMessageBox::warning(parent, "Gestion des entretiens", erreur);
}

} // namespace

GestionEntretiensDialog::GestionEntretiensDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Entretiens préventifs et historique");
    setMinimumSize(980, 590);

    auto *principal = new QVBoxLayout(this);
    auto *titre = new QLabel("Entretien intelligent de la flotte", this);
    titre->setStyleSheet("font-size:20px;font-weight:700;color:#ff3333;");
    principal->addWidget(titre);

    m_alerte = new QLabel(this);
    m_alerte->setWordWrap(true);
    m_alerte->setStyleSheet("background:#2b2d30;border-left:4px solid #ff0000;border-radius:4px;padding:9px;");
    principal->addWidget(m_alerte);

    auto *barre = new QHBoxLayout;
    m_recherche = new QLineEdit(this);
    m_recherche->setPlaceholderText("Filtrer par immatriculation ou type d'entretien");
    m_recherche->setClearButtonEnabled(true);
    barre->addWidget(m_recherche);
    principal->addLayout(barre);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"Véhicule", "Entretien", "Échéance", "Km prévu", "Statut", "Coût", "Priorité"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    principal->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *ajouter = new QPushButton("＋ Planifier", this);
    auto *terminer = new QPushButton("✓ Marquer terminé", this);
    auto *supprimer = new QPushButton("Supprimer", this);
    auto *fermer = new QPushButton("Fermer", this);
    ajouter->setProperty("role", "primary");
    terminer->setProperty("role", "ghost");
    supprimer->setProperty("role", "ghost");
    fermer->setProperty("role", "ghost");
    actions->addWidget(ajouter);
    actions->addWidget(terminer);
    actions->addWidget(supprimer);
    actions->addStretch();
    actions->addWidget(fermer);
    principal->addLayout(actions);

    connect(m_recherche, &QLineEdit::textChanged, this, &GestionEntretiensDialog::rafraichir);
    connect(ajouter, &QPushButton::clicked, this, &GestionEntretiensDialog::planifierEntretien);
    connect(terminer, &QPushButton::clicked, this, &GestionEntretiensDialog::terminerEntretien);
    connect(supprimer, &QPushButton::clicked, this, &GestionEntretiensDialog::supprimerEntretien);
    connect(fermer, &QPushButton::clicked, this, &QDialog::accept);

    QString erreur;
    if (!GestionEntretiens::initialiser(&erreur)) {
        QMessageBox::critical(this, "Initialisation des entretiens", erreur);
        ajouter->setEnabled(false);
        terminer->setEnabled(false);
        supprimer->setEnabled(false);
        m_alerte->setText("Le module nécessite les droits Oracle pour créer sa table et sa séquence.");
        return;
    }
    rafraichir();
}

void GestionEntretiensDialog::rafraichir()
{
    QString erreur;
    m_entretiens = GestionEntretiens::lister(m_recherche->text(), &erreur);
    if (!erreur.isEmpty()) {
        afficherErreur(this, erreur);
        return;
    }

    m_table->setRowCount(m_entretiens.size());
    int enRetard = 0;
    int aPrevoir = 0;
    const QDate aujourdHui = QDate::currentDate();

    for (int ligne = 0; ligne < m_entretiens.size(); ++ligne) {
        const Entretien &e = m_entretiens[ligne];
        QString priorite = "—";
        QColor couleur("#b0b3b8");
        if (e.statut == "Planifié") {
            const int jours = aujourdHui.daysTo(e.datePrevue);
            const int kmRestants = e.kmPrevu > 0 ? e.kmPrevu - e.kmVehicule : 10000000;
            if (jours < 0 || kmRestants <= 0) {
                priorite = "EN RETARD";
                couleur = QColor("#ff5555");
                ++enRetard;
            } else if (jours <= 30 || kmRestants <= 500) {
                priorite = "À PRÉVOIR";
                couleur = QColor("#ffb347");
                ++aPrevoir;
            } else {
                priorite = "À JOUR";
                couleur = QColor("#62c987");
            }
        }

        const QDate echeance = e.statut == "Terminé" ? e.dateRealisee : e.datePrevue;
        const QString km = e.statut == "Terminé" && e.kmRealise > 0
                               ? QLocale(QLocale::French).toString(e.kmRealise)
                               : (e.kmPrevu > 0 ? QLocale(QLocale::French).toString(e.kmPrevu) : QStringLiteral("—"));
        const QString cout = e.cout > 0
                     ? QLocale(QLocale::French).toString(e.cout, 'f', 2) + " TND"
                     : QStringLiteral("—");
        const QStringList valeurs = {e.plaque, e.type, dateTexte(echeance), km, e.statut, cout, priorite};
        for (int colonne = 0; colonne < valeurs.size(); ++colonne) {
            auto *item = new QTableWidgetItem(valeurs[colonne]);
            if (colonne == 0) item->setData(Qt::UserRole, e.id);
            if (colonne == 6) {
                item->setForeground(couleur);
                item->setFont(QFont(item->font().family(), item->font().pointSize(), QFont::DemiBold));
            }
            m_table->setItem(ligne, colonne, item);
        }
        m_table->setRowHeight(ligne, 38);
    }

    if (enRetard || aPrevoir) {
        m_alerte->setText(QString("⚠ %1 entretien(s) en retard · %2 à prévoir dans les 30 jours ou les 500 km.")
                              .arg(enRetard).arg(aPrevoir));
        m_alerte->setStyleSheet("background:#3a2925;border-left:4px solid #ff8c42;border-radius:4px;padding:9px;");
    } else {
        m_alerte->setText(QString("✓ Aucun entretien en retard ou proche de l'échéance · %1 intervention(s) suivie(s).")
                              .arg(m_entretiens.size()));
        m_alerte->setStyleSheet("background:#25352b;border-left:4px solid #62c987;border-radius:4px;padding:9px;");
    }
}

void GestionEntretiensDialog::planifierEntretien()
{
    QVector<QPair<QString, QString>> vehicules;
    QString erreur;
    if (!GestionEntretiens::vehicules(vehicules, &erreur)) {
        afficherErreur(this, erreur);
        return;
    }
    if (vehicules.isEmpty()) {
        QMessageBox::information(this, "Planifier", "Ajoutez d'abord un véhicule.");
        return;
    }

    QDialog formulaire(this);
    formulaire.setWindowTitle("Planifier un entretien");
    auto *layout = new QVBoxLayout(&formulaire);
    auto *form = new QFormLayout;
    auto *vehicule = new QComboBox(&formulaire);
    for (const auto &v : vehicules) vehicule->addItem(v.second, v.first);
    auto *type = new QComboBox(&formulaire);
    type->addItems({"Révision générale", "Vidange", "Freins", "Pneumatiques", "Batterie",
                    "Contrôle technique", "Réparation", "Autre"});
    auto *description = new QLineEdit(&formulaire);
    description->setMaxLength(500);
    description->setPlaceholderText("Détails ou pièces à contrôler");
    auto *date = creerChampDate(&formulaire, QDate::currentDate().addDays(7));
    auto *km = creerChampKm(&formulaire);
    auto *garage = new QLineEdit(&formulaire);
    garage->setMaxLength(120);

    form->addRow("Véhicule", vehicule);
    form->addRow("Type d'entretien", type);
    form->addRow("Description", description);
    form->addRow("Date prévue", date);
    form->addRow("Échéance kilométrique (optionnelle)", km);
    form->addRow("Garage / prestataire", garage);
    layout->addLayout(form);
    auto *boutons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &formulaire);
    boutons->button(QDialogButtonBox::Save)->setText("Planifier");
    connect(boutons, &QDialogButtonBox::accepted, &formulaire, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &formulaire, &QDialog::reject);
    layout->addWidget(boutons);

    if (formulaire.exec() != QDialog::Accepted) return;
    Entretien e;
    e.vehiculeId = vehicule->currentData().toString();
    e.type = type->currentText();
    e.description = description->text();
    e.datePrevue = date->date();
    e.kmPrevu = km->value();
    e.garage = garage->text();
    if (!GestionEntretiens::ajouter(e, &erreur)) {
        afficherErreur(this, erreur);
        return;
    }
    rafraichir();
}

void GestionEntretiensDialog::terminerEntretien()
{
    const int ligne = m_table->currentRow();
    if (ligne < 0 || ligne >= m_entretiens.size()) {
        QMessageBox::information(this, "Terminer", "Sélectionnez un entretien planifié.");
        return;
    }
    Entretien e = m_entretiens[ligne];
    if (e.statut != "Planifié") {
        QMessageBox::information(this, "Terminer", "Seuls les entretiens planifiés peuvent être terminés.");
        return;
    }

    QDialog formulaire(this);
    formulaire.setWindowTitle("Clôturer l'entretien — " + e.plaque);
    auto *layout = new QVBoxLayout(&formulaire);
    auto *form = new QFormLayout;
    auto *dateRealisee = creerChampDate(&formulaire);
    auto *kmRealise = creerChampKm(&formulaire);
    kmRealise->setValue(e.kmVehicule);
    auto *cout = new QDoubleSpinBox(&formulaire);
    cout->setRange(0, 10000000);
    cout->setDecimals(2);
    cout->setSuffix(" TND");
    auto *prochaineDateActive = new QCheckBox("Programmer une date de rappel", &formulaire);
    auto *prochaineDate = creerChampDate(&formulaire, QDate::currentDate().addMonths(6));
    prochaineDate->setEnabled(false);
    connect(prochaineDateActive, &QCheckBox::toggled, prochaineDate, &QDateEdit::setEnabled);
    auto *prochainKm = creerChampKm(&formulaire);
    form->addRow("Date de réalisation", dateRealisee);
    form->addRow("Kilométrage réel", kmRealise);
    form->addRow("Coût de l'intervention", cout);
    form->addRow(prochaineDateActive, prochaineDate);
    form->addRow("Rappel au kilométrage (optionnel)", prochainKm);
    layout->addLayout(form);
    auto *note = new QLabel("La clôture actualise aussi la dernière maintenance, la prochaine échéance et le kilométrage du véhicule.", &formulaire);
    note->setWordWrap(true);
    note->setStyleSheet("color:#b0b3b8;");
    layout->addWidget(note);
    auto *boutons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &formulaire);
    boutons->button(QDialogButtonBox::Save)->setText("Clôturer");
    connect(boutons, &QDialogButtonBox::accepted, &formulaire, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &formulaire, &QDialog::reject);
    layout->addWidget(boutons);

    if (formulaire.exec() != QDialog::Accepted) return;
    e.dateRealisee = dateRealisee->date();
    e.kmRealise = kmRealise->value();
    e.cout = cout->value();
    e.prochaineDate = prochaineDateActive->isChecked() ? prochaineDate->date() : QDate();
    e.prochainKm = prochainKm->value();
    if (e.prochaineDate.isValid() && e.prochaineDate < e.dateRealisee) {
        afficherErreur(this, "La prochaine date doit être postérieure ou égale à la date de réalisation.");
        return;
    }
    if (e.prochainKm > 0 && e.prochainKm <= e.kmRealise) {
        afficherErreur(this, "Le prochain kilométrage doit être supérieur au kilométrage réalisé.");
        return;
    }
    QString erreur;
    if (!GestionEntretiens::terminer(e, &erreur)) {
        afficherErreur(this, erreur);
        return;
    }
    rafraichir();
}

void GestionEntretiensDialog::supprimerEntretien()
{
    const int ligne = m_table->currentRow();
    if (ligne < 0 || ligne >= m_entretiens.size()) {
        QMessageBox::information(this, "Supprimer", "Sélectionnez un entretien.");
        return;
    }
    const Entretien &e = m_entretiens[ligne];
    if (QMessageBox::question(this, "Supprimer l'entretien",
                              "Supprimer l'entretien « " + e.type + " » du véhicule " + e.plaque + " ?") != QMessageBox::Yes)
        return;
    QString erreur;
    if (!GestionEntretiens::supprimer(e.id, &erreur)) {
        afficherErreur(this, erreur);
        return;
    }
    rafraichir();
}
