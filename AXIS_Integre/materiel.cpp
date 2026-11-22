#include "materiel.h"
#include "connexion.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QStringList>
#include <QSet>
#include <QDebug>
#include <QtWidgets>

// Les noms de colonnes « seuil » et « emplacement » diffèrent selon les versions de la table MATERIEL :
// on regarde dans Oracle (USER_TAB_COLUMNS) lequel existe, une seule fois.
static QString colonne(const QStringList &candidats)
{
    static QSet<QString> colonnes;
    static bool charge = false;
    if (!charge) {
        charge = true;
        QSqlQuery q(Connexion::oracle());
        if (q.exec("SELECT COLUMN_NAME FROM USER_TAB_COLUMNS WHERE TABLE_NAME = 'MATERIEL'"))
            while (q.next()) colonnes.insert(q.value(0).toString().toUpper());
    }
    for (const QString &c : candidats)
        if (colonnes.contains(c)) return c;
    return candidats.first();
}
static QString colSeuil()       { return colonne({"SEUIL_MINIMAL_STOCK", "SEUIL_MINIMAL"}); }
static QString colEmplacement() { return colonne({"EMPLACEMENT_STOCKAGE", "EMPLACEMENT_STOCK"}); }

static QString selectMat()
{
    return "SELECT ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK, " + colSeuil() +
           ", ETAT, DISPONIBILITE, DATE_ACQUISITION, " + colEmplacement() + " FROM MATERIEL";
}

static Materiel fromQuery(const QSqlQuery &q)
{
    Materiel m;
    m.id = q.value(0).toInt();
    m.reference = q.value(1).toString();
    m.designation = q.value(2).toString();
    m.categorie = q.value(3).toString();
    m.quantite = q.value(4).toInt();
    m.seuil = q.value(5).toInt();
    m.etat = q.value(6).toString();
    m.disponibilite = q.value(7).toString();
    m.dateAcquisition = q.value(8).toDate();
    m.emplacement = q.value(9).toString();
    return m;
}

// ---------- Règles métier ----------

// Durée de vie par catégorie (en années) : à adapter aux normes de l'entreprise
int Materiel::dureeVieAns() const
{
    if (categorie == "Lutte incendie") return 10;
    if (categorie == "Protection")     return 8;
    if (categorie == "Secours")        return 5;
    return 0;
}

int Materiel::ageAns() const
{
    if (!dateAcquisition.isValid()) return 0;
    return int(dateAcquisition.daysTo(QDate::currentDate()) / 365.25);
}

QString Materiel::statutVie() const
{
    const int duree = dureeVieAns();
    if (duree <= 0 || !dateAcquisition.isValid()) return "ok";
    const double age = dateAcquisition.daysTo(QDate::currentDate()) / 365.25;
    if (age >= duree)     return "fin";
    if (age >= duree - 1) return "bientot";
    return "ok";
}

// ---------- CRUD ----------

bool Materiel::ajouter(QString *error)
{
    // Pas d'auto-incrément sur ID_MATERIEL : on prend le prochain numéro libre
    QSqlQuery qid(Connexion::oracle());
    if (!qid.exec("SELECT NVL(MAX(ID_MATERIEL), 0) + 1 FROM MATERIEL") || !qid.next()) {
        if (error) *error = qid.lastError().text();
        return false;
    }
    const int newId = qid.value(0).toInt();

    QSqlQuery q(Connexion::oracle());
    q.prepare("INSERT INTO MATERIEL (ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK, " +
              colSeuil() + ", ETAT, DISPONIBILITE, DATE_ACQUISITION, " + colEmplacement() + ") "
                                                                                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(newId);
    q.addBindValue(reference);
    q.addBindValue(designation);
    q.addBindValue(categorie);
    q.addBindValue(quantite);
    q.addBindValue(seuil);
    q.addBindValue(etat);
    q.addBindValue(disponibilite);
    q.addBindValue(dateAcquisition);
    q.addBindValue(emplacement);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    id = newId;
    return true;
}

bool Materiel::modifier(QString *error) const
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("UPDATE MATERIEL SET REFERENCE=?, DESIGNATION=?, CATEGORIE=?, QUANTITE_STOCK=?, " +
              colSeuil() + "=?, ETAT=?, DISPONIBILITE=?, DATE_ACQUISITION=?, " + colEmplacement() + "=? "
                                                                                                    "WHERE ID_MATERIEL=?");
    q.addBindValue(reference);
    q.addBindValue(designation);
    q.addBindValue(categorie);
    q.addBindValue(quantite);
    q.addBindValue(seuil);
    q.addBindValue(etat);
    q.addBindValue(disponibilite);
    q.addBindValue(dateAcquisition);
    q.addBindValue(emplacement);
    q.addBindValue(id);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

bool Materiel::supprimer(int id, QString *error)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("DELETE FROM MATERIEL WHERE ID_MATERIEL = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        // Ex. ORA-02292 : matériel déjà utilisé dans une autre table (clé étrangère)
        if (error) *error = q.lastError().text();
        return false;
    }
    return true;
}

QList<Materiel> Materiel::afficher(const QString &search, const QString &orderColumn)
{
    // Liste blanche : on ne met jamais le texte de l'utilisateur dans l'ORDER BY
    QString orderBy;
    if (orderColumn == "etat")
        orderBy = "CASE ETAT WHEN 'Bon' THEN 1 WHEN 'Moyen' THEN 2 WHEN 'Mauvais' THEN 3 ELSE 4 END, UPPER(REFERENCE)";
    else if (orderColumn == "designation")      orderBy = "UPPER(DESIGNATION)";
    else if (orderColumn == "categorie")        orderBy = "UPPER(CATEGORIE), UPPER(REFERENCE)";
    else if (orderColumn == "quantite")         orderBy = "QUANTITE_STOCK";
    else if (orderColumn == "date_acquisition") orderBy = "DATE_ACQUISITION";
    else                                        orderBy = "UPPER(REFERENCE)";

    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("%1 WHERE UPPER(REFERENCE) LIKE UPPER(?) OR UPPER(DESIGNATION) LIKE UPPER(?) ORDER BY %2")
                  .arg(selectMat(), orderBy));
    q.addBindValue("%" + search + "%");
    q.addBindValue("%" + search + "%");
    QList<Materiel> list;
    if (q.exec())
        while (q.next()) list << fromQuery(q);
    else
        qWarning() << "Materiel::afficher :" << q.lastError().text();
    return list;
}

// ---------- Utilitaires ----------

Materiel Materiel::trouver(int id)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare(QString("%1 WHERE ID_MATERIEL = ?").arg(selectMat()));
    q.addBindValue(id);
    if (q.exec() && q.next()) return fromQuery(q);
    return Materiel();
}

bool Materiel::referenceExiste(const QString &reference, int ignoreId)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT COUNT(*) FROM MATERIEL WHERE UPPER(REFERENCE) = UPPER(?) AND ID_MATERIEL <> ?");
    q.addBindValue(reference);
    q.addBindValue(ignoreId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

// ---------- Mouvements de stock ----------

// Une seule transaction : mise à jour du stock + ligne d'historique, ou rien du tout.
bool Materiel::enregistrerMouvement(int idMateriel, int idEmploye, const QString &type,
                                    int quantite, const QString &motif, QString *error)
{
    if (quantite <= 0 || (type != "ENTREE" && type != "SORTIE") || motif.trimmed().isEmpty()) {
        if (error) *error = "Mouvement invalide (quantité > 0, type et motif obligatoires).";
        return false;
    }

    QSqlDatabase db = Connexion::oracle();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    // 1) Mise à jour du stock. Pour une sortie, la condition empêche un stock négatif.
    QSqlQuery up(Connexion::oracle());
    if (type == "ENTREE")
        up.prepare("UPDATE MATERIEL SET QUANTITE_STOCK = QUANTITE_STOCK + ? WHERE ID_MATERIEL = ?");
    else
        up.prepare("UPDATE MATERIEL SET QUANTITE_STOCK = QUANTITE_STOCK - ? "
                   "WHERE ID_MATERIEL = ? AND QUANTITE_STOCK >= ?");
    up.addBindValue(quantite);
    up.addBindValue(idMateriel);
    if (type == "SORTIE") up.addBindValue(quantite);

    if (!up.exec() || up.numRowsAffected() != 1) {
        if (error) *error = up.lastError().isValid() ? up.lastError().text()
                                              : "Stock insuffisant pour cette sortie.";
        db.rollback();
        return false;
    }

    // 2) Ligne d'historique (pas de SEQUENCE : on prend le prochain id libre, dans la même transaction)
    QSqlQuery qid(Connexion::oracle());
    if (!qid.exec("SELECT NVL(MAX(ID_MOUVEMENT), 0) + 1 FROM MOUVEMENT_STOCK") || !qid.next()) {
        if (error) *error = qid.lastError().text();
        db.rollback();
        return false;
    }
    const int newId = qid.value(0).toInt();

    QSqlQuery ins(Connexion::oracle());
    ins.prepare("INSERT INTO MOUVEMENT_STOCK (ID_MOUVEMENT, ID_MATERIEL, ID_EMPLOYE, TYPE_MOUVEMENT, QUANTITE, MOTIF) "
                "VALUES (?, ?, ?, ?, ?, ?)");
    ins.addBindValue(newId);
    ins.addBindValue(idMateriel);
    ins.addBindValue(idEmploye);
    ins.addBindValue(type);
    ins.addBindValue(quantite);
    ins.addBindValue(motif.trimmed());
    if (!ins.exec()) {
        if (error) *error = ins.lastError().text();
        db.rollback();
        return false;
    }
    return db.commit();
}

QList<Mouvement> Materiel::historique(int idMateriel)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT M.DATE_MOUVEMENT, M.TYPE_MOUVEMENT, M.QUANTITE, M.MOTIF, E.PRENOM, E.NOM "
              "FROM MOUVEMENT_STOCK M JOIN EMPLOYE E ON E.ID_EMPLOYE = M.ID_EMPLOYE "
              "WHERE M.ID_MATERIEL = ? ORDER BY M.DATE_MOUVEMENT DESC, M.ID_MOUVEMENT DESC");
    q.addBindValue(idMateriel);
    QList<Mouvement> list;
    if (q.exec())
        while (q.next()) {
            Mouvement m;
            m.date = q.value(0).toDateTime();
            m.type = q.value(1).toString();
            m.quantite = q.value(2).toInt();
            m.motif = q.value(3).toString();
            m.employe = q.value(4).toString() + " " + q.value(5).toString();
            list << m;
        }
    return list;
}

QList<Mouvement> Materiel::historiqueGlobal(const QString &type, const QString &search)
{
    QSqlQuery q(Connexion::oracle());
    q.prepare("SELECT M.DATE_MOUVEMENT, M.TYPE_MOUVEMENT, M.QUANTITE, M.MOTIF, E.PRENOM, E.NOM, "
              "MAT.REFERENCE, MAT.DESIGNATION "
              "FROM MOUVEMENT_STOCK M "
              "JOIN EMPLOYE E ON E.ID_EMPLOYE = M.ID_EMPLOYE "
              "JOIN MATERIEL MAT ON MAT.ID_MATERIEL = M.ID_MATERIEL "
              "WHERE (? = 'TOUS' OR M.TYPE_MOUVEMENT = ?) "
              "AND (UPPER(MAT.REFERENCE) LIKE UPPER(?) OR UPPER(MAT.DESIGNATION) LIKE UPPER(?) "
              "     OR UPPER(M.MOTIF) LIKE UPPER(?) OR UPPER(E.PRENOM || ' ' || E.NOM) LIKE UPPER(?)) "
              "ORDER BY M.DATE_MOUVEMENT DESC, M.ID_MOUVEMENT DESC");
    q.addBindValue(type);
    q.addBindValue(type);
    const QString like = "%" + search + "%";
    for (int i = 0; i < 4; ++i) q.addBindValue(like);
    QList<Mouvement> list;
    if (q.exec())
        while (q.next()) {
            Mouvement m;
            m.date = q.value(0).toDateTime();
            m.type = q.value(1).toString();
            m.quantite = q.value(2).toInt();
            m.motif = q.value(3).toString();
            m.employe = q.value(4).toString() + " " + q.value(5).toString();
            m.reference = q.value(6).toString();
            m.designation = q.value(7).toString();
            list << m;
        }
    return list;
}

QStringList Materiel::emailsMagasiniers()
{
    QStringList list;
    QSqlQuery q(Connexion::oracle());
    q.exec("SELECT DISTINCT TRIM(EMAIL) FROM EMPLOYE "
           "WHERE UPPER(TRIM(GRADE)) = 'MAGASINIER' AND EMAIL IS NOT NULL");
    while (q.next()) {
        const QString e = q.value(0).toString().trimmed();
        if (e.contains('@')) list << e;
    }
    return list;
}

QMap<int, double> Materiel::consommationParJour(int jours)
{
    QMap<int, double> conso;
    QSqlQuery q(Connexion::oracle());
    q.exec(QString("SELECT ID_MATERIEL, SUM(QUANTITE) FROM MOUVEMENT_STOCK "
                   "WHERE TYPE_MOUVEMENT = 'SORTIE' AND DATE_MOUVEMENT >= SYSDATE - %1 "
                   "GROUP BY ID_MATERIEL").arg(jours));
    while (q.next())
        conso[q.value(0).toInt()] = q.value(1).toDouble() / jours;
    return conso;    // vide si la table n'existe pas encore ou s'il n'y a aucune sortie
}

QList<QPair<int, QString>> Materiel::employes(QString *error)
{
    QList<QPair<int, QString>> list;
    QSqlQuery q(Connexion::oracle());
    if (!q.exec("SELECT ID_EMPLOYE, PRENOM || ' ' || NOM FROM EMPLOYE ORDER BY NOM, PRENOM")) {
        if (error) *error = q.lastError().text();   // ex. ORA-00904 (colonne inconnue), ORA-00942 (table introuvable)
        return list;
    }
    while (q.next()) list.append({q.value(0).toInt(), q.value(1).toString()});
    return list;
}

// ---------- Boîtes de dialogue ----------

bool dialogueMouvement(QWidget *parent, const Materiel &m)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QString("Mouvement de stock – %1").arg(m.designation));
    auto *form = new QFormLayout(&dlg);

    auto *cbType = new QComboBox;
    cbType->addItem("Entrée", "ENTREE");
    cbType->addItem("Sortie", "SORTIE");

    auto *sbQte = new QSpinBox;
    sbQte->setRange(1, 1000000);

    auto *leMotif = new QLineEdit;
    leMotif->setMaxLength(200);
    leMotif->setPlaceholderText("Ex. Intervention incendie, achat, retour de mission...");

    auto *cbEmp = new QComboBox;
    QString empErr;
    for (const auto &e : Materiel::employes(&empErr))
        cbEmp->addItem(e.second, e.first);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    form->addRow("Stock actuel :", new QLabel(QString::number(m.quantite)));
    form->addRow("Type :", cbType);
    form->addRow("Quantité :", sbQte);
    form->addRow("Motif :", leMotif);
    form->addRow("Employé :", cbEmp);
    form->addRow(bb);

    if (dlg.exec() != QDialog::Accepted) return false;
    if (cbEmp->count() == 0) {
        QMessageBox::warning(parent, "Mouvement",
                             empErr.isEmpty() ? "Aucun employé dans la table EMPLOYE."
                                              : "Impossible de lire les employés :\n" + empErr);
        return false;
    }

    QString err;
    if (!Materiel::enregistrerMouvement(m.id, cbEmp->currentData().toInt(),
                                        cbType->currentData().toString(),
                                        sbQte->value(), leMotif->text(), &err)) {
        QMessageBox::warning(parent, "Mouvement refusé", err);
        return false;
    }
    return true;
}

void dialogueHistorique(QWidget *parent, const Materiel &m)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QString("Historique – %1 (%2)").arg(m.designation, m.reference));
    dlg.resize(720, 380);
    auto *lay = new QVBoxLayout(&dlg);

    const QList<Mouvement> items = Materiel::historique(m.id);
    auto *t = new QTableWidget(items.size(), 5);
    t->setHorizontalHeaderLabels({"Date", "Type", "Quantité", "Motif", "Employé"});
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (int r = 0; r < items.size(); ++r) {
        const Mouvement &mv = items[r];
        const bool entree = mv.type == "ENTREE";
        t->setItem(r, 0, new QTableWidgetItem(mv.date.toString("dd/MM/yyyy HH:mm")));
        t->setItem(r, 1, new QTableWidgetItem(entree ? "Entrée" : "Sortie"));
        t->setItem(r, 2, new QTableWidgetItem((entree ? "+" : "-") + QString::number(mv.quantite)));
        t->setItem(r, 3, new QTableWidgetItem(mv.motif));
        t->setItem(r, 4, new QTableWidgetItem(mv.employe));
    }
    lay->addWidget(t);
    dlg.exec();
}

void dialogueHistoriqueGlobal(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("Historique des mouvements de stock");
    dlg.resize(1000, 520);
    auto *lay = new QVBoxLayout(&dlg);

    auto *bar = new QHBoxLayout;
    auto *cbType = new QComboBox;
    cbType->addItem("Tous les mouvements", "TOUS");
    cbType->addItem("Entrées", "ENTREE");
    cbType->addItem("Sorties", "SORTIE");
    auto *leSearch = new QLineEdit;
    leSearch->setPlaceholderText("Rechercher (matériel, motif, employé)...");
    auto *btnCsv = new QPushButton("Exporter (CSV)");
    bar->addWidget(cbType);
    bar->addWidget(leSearch, 1);
    bar->addWidget(btnCsv);
    lay->addLayout(bar);

    auto *t = new QTableWidget(0, 6);
    t->setHorizontalHeaderLabels({"Date", "Matériel", "Type", "Quantité", "Motif", "Employé"});
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->verticalHeader()->setVisible(false);
    lay->addWidget(t, 1);

    auto *lblTotal = new QLabel;
    lay->addWidget(lblTotal);

    QList<Mouvement> current;
    auto reload = [&] {
        current = Materiel::historiqueGlobal(cbType->currentData().toString(), leSearch->text().trimmed());
        t->setRowCount(current.size());
        int in = 0, out = 0;
        for (int r = 0; r < current.size(); ++r) {
            const Mouvement &mv = current[r];
            const bool entree = mv.type == "ENTREE";
            (entree ? in : out) += mv.quantite;
            t->setItem(r, 0, new QTableWidgetItem(mv.date.toString("dd/MM/yyyy HH:mm")));
            t->setItem(r, 1, new QTableWidgetItem(mv.reference + " – " + mv.designation));
            t->setItem(r, 2, new QTableWidgetItem(entree ? "Entrée" : "Sortie"));
            auto *q = new QTableWidgetItem((entree ? "+" : "-") + QString::number(mv.quantite));
            q->setForeground(QColor(entree ? "#1e9e4a" : "#ff4d4d"));
            t->setItem(r, 3, q);
            t->setItem(r, 4, new QTableWidgetItem(mv.motif));
            t->setItem(r, 5, new QTableWidgetItem(mv.employe));
        }
        lblTotal->setText(QString("%1 mouvement(s)  ·  entrées : +%2  ·  sorties : -%3")
                              .arg(current.size()).arg(in).arg(out));
    };
    QObject::connect(cbType, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, reload);
    QObject::connect(leSearch, &QLineEdit::textChanged, &dlg, reload);

    QObject::connect(btnCsv, &QPushButton::clicked, &dlg, [&] {
        const QString path = QFileDialog::getSaveFileName(
            &dlg, "Exporter l'historique", QDir::homePath() + "/historique_mouvements.csv", "Fichier CSV (*.csv)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(&dlg, "Export", "Impossible d'écrire dans le fichier :\n" + path);
            return;
        }
        auto esc = [](QString s) { s.replace('"', "\"\""); return "\"" + s + "\""; };
        QString out = "Date;Référence;Désignation;Type;Quantité;Motif;Employé\r\n";
        for (const Mouvement &mv : current)
            out += QStringList{mv.date.toString("dd/MM/yyyy HH:mm"), esc(mv.reference), esc(mv.designation),
                               mv.type == "ENTREE" ? "Entrée" : "Sortie", QString::number(mv.quantite),
                               esc(mv.motif), esc(mv.employe)}.join(';') + "\r\n";
        f.write("\xEF\xBB\xBF");
        f.write(out.toUtf8());
        f.close();
        QMessageBox::information(&dlg, "Export", "Historique exporté :\n" + path);
    });

    reload();
    dlg.exec();
}
