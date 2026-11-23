#include "vehicule.h"
#include "connexion.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
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


const QString COLONNES = QStringLiteral(
    "ID_VEHICULE, IMMATRICULATION, TYPE, KILOMETRAGE, ETAT, DISPONIBILITE, "
    "DATE_DERNIERE_MAINTENANCE, DATE_PROCHAINE_MAINTENANCE, "
    "DATE_EXPIRATION_ASSURANCE, DATE_VISITE_TECHNIQUE");


const QString ID_TEXTE = QStringLiteral(
    "('V' || LPAD(ID_VEHICULE, GREATEST(3, LENGTH(ID_VEHICULE)), '0'))");


const QString DATE_SQL = QStringLiteral("TO_DATE(?, 'YYYY-MM-DD')");

void setErreur(QString *erreur, const QString &message)
{
    if (erreur) *erreur = message;
}


QString idTexte(int n)
{
    return QString("V%1").arg(n, 3, 10, QChar('0'));
}

int idNombre(const QString &id)
{
    return id.mid(1).toInt();   
}


QVariant dateSql(const QDate &d)
{
    return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : QVariant(QString());
}


QDate versDate(const QVariant &valeur)
{
    if (valeur.isNull()) return QDate();
    QDate d = valeur.toDate();
    if (!d.isValid())
        d = QDate::fromString(valeur.toString().left(10), Qt::ISODate);
    return d;
}

Vehicule depuisRequete(const QSqlQuery &q)
{
    Vehicule v;
    v.id             = idTexte(q.value(0).toInt());
    v.plaque         = q.value(1).toString();
    v.type           = q.value(2).toString();
    v.km             = q.value(3).toInt();
    v.etat           = q.value(4).toString();
    v.dispo          = q.value(5).toString();
    v.derniereMaint  = versDate(q.value(6));
    v.prochaineMaint = versDate(q.value(7));
    v.assurance      = versDate(q.value(8));
    v.visiteTech     = versDate(q.value(9));
    return v;
}

void lierChamps(QSqlQuery &q, const Vehicule &v)
{
    q.addBindValue(v.plaque);
    q.addBindValue(v.type);
    q.addBindValue(v.km);
    q.addBindValue(v.etat);
    q.addBindValue(v.dispo);
    q.addBindValue(dateSql(v.derniereMaint));
    q.addBindValue(dateSql(v.prochaineMaint));
    q.addBindValue(dateSql(v.assurance));
    q.addBindValue(dateSql(v.visiteTech));
}

} 


const QStringList &Vehicule::types()
{
    static const QStringList l{"Camion-citerne", "Ambulance", "Échelle aérienne", "Fourgon pompe",
                               "Véhicule léger"};
    return l;
}

const QStringList &Vehicule::etats()
{
    static const QStringList l{"Bon", "À surveiller", "Hors service"};
    return l;
}

const QStringList &Vehicule::disponibilites()
{
    static const QStringList l{"Disponible", "En intervention", "En maintenance"};
    return l;
}


QString Vehicule::normaliserPlaque(const QString &texte)
{
    static const QRegularExpression re("^(\\d{1,3}) ?TUN ?(\\d{1,4})$",
                                       QRegularExpression::CaseInsensitiveOption);
    const QString t = texte.trimmed();
    const QRegularExpressionMatch m = re.match(t);
    if (!m.hasMatch()) return t;
    return m.captured(1) + " TUN " + m.captured(2);
}

bool Vehicule::valider(QString *erreur) const
{
    static const QRegularExpression re("^\\d{1,3} TUN \\d{1,4}$");

    if (plaque.trimmed().isEmpty()) {
        setErreur(erreur, "L'immatriculation est obligatoire.");
        return false;
    }
    if (!re.match(normaliserPlaque(plaque)).hasMatch()) {
        setErreur(erreur, "Immatriculation invalide : utilisez le format 123 TUN 4567.");
        return false;
    }
    if (!types().contains(type)) {
        setErreur(erreur, "Type de véhicule invalide.");
        return false;
    }
    if (km < 0 || km > 2000000) {
        setErreur(erreur, "Le kilométrage doit être compris entre 0 et 2 000 000.");
        return false;
    }
    if (!etats().contains(etat)) {
        setErreur(erreur, "État invalide.");
        return false;
    }
    if (!disponibilites().contains(dispo)) {
        setErreur(erreur, "Disponibilité invalide.");
        return false;
    }
    if (derniereMaint.isValid() && prochaineMaint.isValid() && prochaineMaint < derniereMaint) {
        setErreur(erreur, "La prochaine maintenance ne peut pas être avant la dernière maintenance.");
        return false;
    }
    return true;
}


bool Vehicule::plaqueExiste(const QString &plaque, const QString &sauf)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT COUNT(*) FROM VEHICULE WHERE UPPER(IMMATRICULATION) = UPPER(?) AND ID_VEHICULE <> ?");
    q.addBindValue(plaque);
    q.addBindValue(idNombre(sauf));
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

QString Vehicule::prochainId()
{
    QSqlQuery q(Connexion::oracle());
    int max = 0;
    if (q.exec("SELECT NVL(MAX(ID_VEHICULE), 0) FROM VEHICULE") && q.next())
        max = q.value(0).toInt();
    return idTexte(max + 1);
}








bool Vehicule::ajouter(QString *erreur)
{
    plaque = normaliserPlaque(plaque);
    if (!valider(erreur)) return false;
    if (plaqueExiste(plaque, QString())) {
        setErreur(erreur, "Cette immatriculation existe déjà.");
        return false;
    }

    const QString nouvelId = prochainId();
    QSqlQuery q(Connexion::oracle());
    q.prepare("INSERT INTO VEHICULE (ID_VEHICULE, IMMATRICULATION, TYPE, KILOMETRAGE, ETAT, DISPONIBILITE, "
              "DATE_DERNIERE_MAINTENANCE, DATE_PROCHAINE_MAINTENANCE, "
              "DATE_EXPIRATION_ASSURANCE, DATE_VISITE_TECHNIQUE) "
              "VALUES (?, ?, ?, ?, ?, ?, " + DATE_SQL + ", " + DATE_SQL + ", " + DATE_SQL + ", " + DATE_SQL + ")");
    q.addBindValue(idNombre(nouvelId));
    lierChamps(q, *this);
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    id = nouvelId;
    return true;
}


bool Vehicule::afficher(const QString &id, Vehicule &resultat, QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT " + COLONNES + " FROM VEHICULE WHERE ID_VEHICULE = ?");
    q.addBindValue(idNombre(id));
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    if (!q.next()) {
        setErreur(erreur, "Véhicule introuvable.");
        return false;
    }
    resultat = depuisRequete(q);
    return true;
}



bool Vehicule::modifier(QString *erreur) const
{
    Vehicule copie = *this;
    copie.plaque = normaliserPlaque(plaque);
    if (!copie.valider(erreur)) return false;
    if (plaqueExiste(copie.plaque, id)) {
        setErreur(erreur, "Cette immatriculation existe déjà.");
        return false;
    }

    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE VEHICULE SET IMMATRICULATION=?, TYPE=?, KILOMETRAGE=?, ETAT=?, DISPONIBILITE=?, "
              "DATE_DERNIERE_MAINTENANCE=" + DATE_SQL + ", "
                           "DATE_PROCHAINE_MAINTENANCE=" + DATE_SQL + ", "
                           "DATE_EXPIRATION_ASSURANCE=" + DATE_SQL + ", "
                           "DATE_VISITE_TECHNIQUE=" + DATE_SQL + " "
                           "WHERE ID_VEHICULE=?");
    lierChamps(q, copie);
    q.addBindValue(idNombre(id));
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() == 0) {
        setErreur(erreur, "Véhicule introuvable.");
        return false;
    }
    return true;
}



bool Vehicule::supprimer(const QString &id, QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM VEHICULE WHERE ID_VEHICULE = ?");
    q.addBindValue(idNombre(id));
    if (!q.exec()) {
        const QString texte = q.lastError().text();
        if (texte.contains("ORA-02292"))   
            setErreur(erreur, "Ce véhicule est utilisé dans une intervention : suppression impossible.");
        else
            setErreur(erreur, texte);
        return false;
    }
    if (q.numRowsAffected() == 0) {
        setErreur(erreur, "Véhicule introuvable.");
        return false;
    }
    return true;
}


QVector<Vehicule> Vehicule::lister(const QString &recherche, int tri, QString *erreur)
{
    QString ordre;
    switch (tri) {
    case 0:  ordre = "KILOMETRAGE"; break;
    case 1:  ordre = "ID_VEHICULE"; break;
    case 2:  ordre = "IMMATRICULATION"; break;
    default: ordre = "DATE_PROCHAINE_MAINTENANCE"; break;
    }

    
    
    const bool filtrer = !recherche.isEmpty();

    QString sql = "SELECT " + COLONNES + " FROM VEHICULE";
    if (filtrer)
        sql += " WHERE " + ID_TEXTE + " LIKE UPPER(?) OR UPPER(IMMATRICULATION) LIKE UPPER(?) "
                                      "OR UPPER(TYPE) LIKE UPPER(?)";
    sql += " ORDER BY " + ordre + ", ID_VEHICULE";

    QSqlQuery q(Connexion::oracle());
    q.prepare(sql);
    if (filtrer) {
        const QString motif = "%" + recherche + "%";
        q.addBindValue(motif);
        q.addBindValue(motif);
        q.addBindValue(motif);
    }

    QVector<Vehicule> liste;
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return liste;
    }
    while (q.next()) liste.push_back(depuisRequete(q));
    return liste;
}

QVector<Vehicule> Vehicule::maintenanceRequise(QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT " + COLONNES + " FROM VEHICULE "
                                     "WHERE DATE_PROCHAINE_MAINTENANCE IS NOT NULL AND DATE_PROCHAINE_MAINTENANCE <= " + DATE_SQL + " "
                           "ORDER BY DATE_PROCHAINE_MAINTENANCE");
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));

    QVector<Vehicule> liste;
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return liste;
    }
    while (q.next()) liste.push_back(depuisRequete(q));
    return liste;
}

QMap<QString, int> Vehicule::statistiques(bool parEtat, QString *erreur)
{
    const QString colonne = parEtat ? "ETAT" : "DISPONIBILITE";   
    QMap<QString, int> resultat;
    QSqlQuery q(Connexion::oracle());
    if (!q.exec("SELECT " + colonne + ", COUNT(*) FROM VEHICULE GROUP BY " + colonne)) {
        setErreur(erreur, q.lastError().text());
        return resultat;
    }
    while (q.next()) resultat[q.value(0).toString()] = q.value(1).toInt();
    return resultat;
}

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

}

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

namespace {

bool ignorerObjetExistant(const QSqlError &erreur)
{
    return erreur.text().contains("ORA-00955");
}

} 

bool GestionEntretiens::initialiser(QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    const QString creationTable =
        "CREATE TABLE ENTRETIEN_VEHICULE ("
        "ID_ENTRETIEN NUMBER(10) PRIMARY KEY, "
        "ID_VEHICULE NUMBER(10) NOT NULL, "
        "ID_EMPLOYE NUMBER(10), "
        "TYPE_ENTRETIEN VARCHAR2(60) NOT NULL, "
        "DESCRIPTION VARCHAR2(500), DATE_PREVUE DATE NOT NULL, KM_PREVU NUMBER(10) DEFAULT 0, "
        "STATUT VARCHAR2(20) DEFAULT 'Planifié' NOT NULL, GARAGE VARCHAR2(120), "
        "DATE_REALISEE DATE, KM_REALISE NUMBER(10) DEFAULT 0, COUT NUMBER(12,2) DEFAULT 0, "
        "PROCHAINE_DATE DATE, PROCHAIN_KM NUMBER(10) DEFAULT 0, "
        "CONSTRAINT FK_ENT_VEHICULE FOREIGN KEY (ID_VEHICULE) REFERENCES VEHICULE(ID_VEHICULE), "
        "CONSTRAINT FK_ENT_EMPLOYE FOREIGN KEY (ID_EMPLOYE) REFERENCES EMPLOYE(ID_EMPLOYE))";
    if (!q.exec(creationTable) && !ignorerObjetExistant(q.lastError())) {
        setErreur(erreur, "Impossible de créer la table d'entretiens : " + q.lastError().text());
        return false;
    }

    if (!q.exec("CREATE SEQUENCE SEQ_ENTRETIEN_VEHICULE START WITH 1 INCREMENT BY 1") &&
        !ignorerObjetExistant(q.lastError())) {
        setErreur(erreur, "Impossible de créer la séquence des entretiens : " + q.lastError().text());
        return false;
    }
    return true;
}

QVector<Entretien> GestionEntretiens::lister(const QString &recherche, QString *erreur)
{
    QVector<Entretien> resultat;
    const bool filtrer = !recherche.trimmed().isEmpty();
    QString sql =
        "SELECT E.ID_ENTRETIEN, E.ID_VEHICULE, V.IMMATRICULATION, E.TYPE_ENTRETIEN, "
        "E.DESCRIPTION, E.GARAGE, E.STATUT, E.DATE_PREVUE, E.DATE_REALISEE, "
        "E.KM_PREVU, E.KM_REALISE, E.COUT, E.PROCHAINE_DATE, E.PROCHAIN_KM, V.KILOMETRAGE "
        "FROM ENTRETIEN_VEHICULE E JOIN VEHICULE V ON V.ID_VEHICULE=E.ID_VEHICULE ";
    if (filtrer)
        sql += "WHERE UPPER(V.IMMATRICULATION) LIKE UPPER(?) OR UPPER(E.TYPE_ENTRETIEN) LIKE UPPER(?) ";
    sql += "ORDER BY CASE E.STATUT WHEN 'Planifié' THEN 0 WHEN 'En retard' THEN 1 ELSE 2 END, E.DATE_PREVUE";

    QSqlQuery q(Connexion::oracle());
    q.prepare(sql);
    if (filtrer) {
        const QString motif = "%" + recherche.trimmed() + "%";
        q.addBindValue(motif);
        q.addBindValue(motif);
    }
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return resultat;
    }

    while (q.next()) {
        Entretien e;
        e.id = q.value(0).toString();
        e.vehiculeId = q.value(1).toString();
        e.plaque = q.value(2).toString();
        e.type = q.value(3).toString();
        e.description = q.value(4).toString();
        e.garage = q.value(5).toString();
        e.statut = q.value(6).toString();
        e.datePrevue = versDate(q.value(7));
        e.dateRealisee = versDate(q.value(8));
        e.kmPrevu = q.value(9).toInt();
        e.kmRealise = q.value(10).toInt();
        e.cout = q.value(11).toDouble();
        e.prochaineDate = versDate(q.value(12));
        e.prochainKm = q.value(13).toInt();
        e.kmVehicule = q.value(14).toInt();
        resultat.push_back(e);
    }
    return resultat;
}

bool GestionEntretiens::vehicules(QVector<QPair<QString, QString>> &resultat, QString *erreur)
{
    resultat.clear();
    QSqlQuery q(Connexion::oracle());
    if (!q.exec("SELECT ID_VEHICULE, IMMATRICULATION FROM VEHICULE ORDER BY IMMATRICULATION")) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    while (q.next()) resultat.push_back(qMakePair(q.value(0).toString(), q.value(1).toString()));
    return true;
}

bool GestionEntretiens::ajouter(const Entretien &e, QString *erreur)
{
    if (e.vehiculeId.isEmpty() || e.type.trimmed().isEmpty() || !e.datePrevue.isValid()) {
        setErreur(erreur, "Le véhicule, le type et la date prévue sont obligatoires.");
        return false;
    }
    if (e.kmPrevu < 0 || e.kmPrevu > 2000000) {
        setErreur(erreur, "Le kilométrage prévu doit être entre 0 et 2 000 000.");
        return false;
    }

    QSqlQuery idQuery(Connexion::oracle());
    if (!idQuery.exec("SELECT SEQ_ENTRETIEN_VEHICULE.NEXTVAL FROM DUAL") || !idQuery.next()) {
        setErreur(erreur, idQuery.lastError().text());
        return false;
    }

    QSqlQuery q(Connexion::oracle());
    q.prepare("INSERT INTO ENTRETIEN_VEHICULE (ID_ENTRETIEN, ID_VEHICULE, TYPE_ENTRETIEN, DESCRIPTION, "
              "DATE_PREVUE, KM_PREVU, STATUT, GARAGE, COUT) "
              "VALUES (?, ?, ?, ?, TO_DATE(?, 'YYYY-MM-DD'), ?, 'Planifié', ?, 0)");
    q.addBindValue(idQuery.value(0));
    q.addBindValue(e.vehiculeId.toInt());
    q.addBindValue(e.type.trimmed());
    q.addBindValue(e.description.trimmed());
    q.addBindValue(e.datePrevue.toString(Qt::ISODate));
    q.addBindValue(e.kmPrevu);
    q.addBindValue(e.garage.trimmed());
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    return true;
}

bool GestionEntretiens::terminer(const Entretien &e, QString *erreur)
{
    if (!e.dateRealisee.isValid() || e.kmRealise < 0 || e.kmRealise > 2000000 || e.cout < 0) {
        setErreur(erreur, "Vérifiez la date, le kilométrage et le coût de l'entretien.");
        return false;
    }
    QSqlDatabase db = Connexion::oracle();
    if (!db.transaction()) {
        setErreur(erreur, "Impossible de démarrer la transaction : " + db.lastError().text());
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE ENTRETIEN_VEHICULE SET STATUT='Terminé', DATE_REALISEE=TO_DATE(?, 'YYYY-MM-DD'), "
              "KM_REALISE=?, COUT=?, PROCHAINE_DATE=TO_DATE(?, 'YYYY-MM-DD'), PROCHAIN_KM=? "
              "WHERE ID_ENTRETIEN=? AND STATUT='Planifié'");
    q.addBindValue(e.dateRealisee.toString(Qt::ISODate));
    q.addBindValue(e.kmRealise);
    q.addBindValue(e.cout);
    q.addBindValue(dateSql(e.prochaineDate));
    q.addBindValue(e.prochainKm);
    q.addBindValue(e.id.toInt());
    if (!q.exec() || q.numRowsAffected() == 0) {
        const QString message = q.lastError().isValid() ? q.lastError().text()
                                                        : "Cet entretien n'est plus planifié.";
        db.rollback();
        setErreur(erreur, message);
        return false;
    }

    QSqlQuery majVehicule(db);
    majVehicule.prepare("UPDATE VEHICULE SET DATE_DERNIERE_MAINTENANCE=TO_DATE(?, 'YYYY-MM-DD'), "
                        "DATE_PROCHAINE_MAINTENANCE=TO_DATE(?, 'YYYY-MM-DD'), "
                        "KILOMETRAGE=CASE WHEN KILOMETRAGE < ? THEN ? ELSE KILOMETRAGE END "
                        "WHERE ID_VEHICULE=?");
    majVehicule.addBindValue(e.dateRealisee.toString(Qt::ISODate));
    majVehicule.addBindValue(dateSql(e.prochaineDate));
    majVehicule.addBindValue(e.kmRealise);
    majVehicule.addBindValue(e.kmRealise);
    majVehicule.addBindValue(e.vehiculeId.toInt());
    if (!majVehicule.exec() || majVehicule.numRowsAffected() == 0) {
        const QString message = majVehicule.lastError().isValid() ? majVehicule.lastError().text()
                                                                  : "Véhicule associé introuvable.";
        db.rollback();
        setErreur(erreur, message);
        return false;
    }
    if (!db.commit()) {
        setErreur(erreur, "Impossible de valider l'entretien : " + db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

bool GestionEntretiens::supprimer(const QString &id, QString *erreur)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM ENTRETIEN_VEHICULE WHERE ID_ENTRETIEN=?");
    q.addBindValue(id.toInt());
    if (!q.exec()) {
        setErreur(erreur, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() == 0) {
        setErreur(erreur, "Entretien introuvable.");
        return false;
    }
    return true;
}