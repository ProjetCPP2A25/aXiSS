#include "materielpage.h"
#include "ui_materielpage.h"   // généré à partir de materielpage.ui
#include "charts.h"
#include "style.h"
#include "sidebar.h"

#include <QtWidgets>


static const QStringList kCategories = {"Lutte incendie", "Protection", "Secours"};
static const char *kDateFmt = "dd/MM/yyyy";
static const int kRowH = 58;          // hauteur des lignes du tableau des matériels

// ---------- Helpers ----------

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

// ---------- Construction ----------

MaterielPage::MaterielPage(QWidget *parent) : QWidget(parent), ui(new Ui::MaterielPage)
{
    // L'interface du .ui est construite dans un widget interne,
    // placé dans un QScrollArea : la page défile verticalement.
    auto *contenu = new QWidget;
    ui->setupUi(contenu);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(contenu);

    // Barre latérale à gauche (même structure que la gestion des interventions), page défilante à droite
    auto *sidebar = new SidebarFrame;
    connect(sidebar, &SidebarFrame::menuRequested, this, &MaterielPage::menuRequested);
    connect(sidebar, &SidebarFrame::logoutRequested, this, &MaterielPage::logoutRequested);

    auto *externe = new QHBoxLayout(this);
    externe->setContentsMargins(0, 0, 0, 0);
    externe->setSpacing(0);
    externe->addWidget(sidebar);
    externe->addWidget(scroll, 1);

    setStyleSheet(kStyle);

    ui->leQuantite->setValidator(new QIntValidator(0, 1000000, this));
    ui->leSeuil->setValidator(new QIntValidator(0, 1000000, this));

    // Tri : par état
    ui->cbSort->setItemText(0, "Trier par état");

    // Tableau des matériels : largeurs de colonnes (la désignation prend l'espace restant)
    QHeaderView *h = ui->table->horizontalHeader();
    h->setFixedHeight(44);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::Fixed);
    h->setSectionResizeMode(2, QHeaderView::Stretch);
    const int widths[] = {48, 80, 0, 92, 66, 46, 58, 98, 82, 76, 112};
    for (int c = 0; c < 11; ++c)
        if (widths[c]) ui->table->setColumnWidth(c, widths[c]);

    // Signaux / slots
    connect(ui->btnAdd, &QPushButton::clicked, this, &MaterielPage::submitForm);
    connect(ui->btnClear, &QPushButton::clicked, this, &MaterielPage::clearForm);
    connect(ui->btnExport, &QPushButton::clicked, this, &MaterielPage::exportCsv);
    connect(ui->leSearch, &QLineEdit::textChanged, this, &MaterielPage::refreshMateriels);
    connect(ui->cbSort, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MaterielPage::refreshMateriels);

    refresh();
}

MaterielPage::~MaterielPage()
{
    delete ui;
}

// ---------- Rafraîchissement ----------

void MaterielPage::refresh()
{
    refreshMateriels();
    refreshStats();
}

void MaterielPage::refreshMateriels()
{
    const QList<Materiel> items = Database::materiels(ui->leSearch->text().trimmed(), "etat");

    ui->table->setRowCount(0);
    ui->table->setRowCount(items.size());

    auto text = [&](int r, int c, const QString &s, bool bold = false) {
        auto *it = new QTableWidgetItem(s);
        it->setFlags(Qt::ItemIsEnabled);
        if (bold) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
        ui->table->setItem(r, c, it);
    };

    for (int r = 0; r < items.size(); ++r) {
        const Materiel &m = items[r];
        text(r, 0, QString("M%1").arg(m.id, 3, 10, QChar('0')), true);
        text(r, 1, m.reference, true);
        text(r, 2, m.designation);
        text(r, 3, m.categorie);
        if (m.critique())
            ui->table->setCellWidget(r, 4, badgeCell(QString::number(m.quantite), "crit"));
        else
            text(r, 4, QString::number(m.quantite));
        text(r, 5, QString::number(m.seuil));
        ui->table->setCellWidget(r, 6, badgeCell(m.etat, m.etat == "Bon" ? "ok" : "warn"));
        ui->table->setCellWidget(r, 7, badgeCell(m.disponibilite, m.disponibilite == "Disponible" ? "ok" : "warn"));
        text(r, 8, m.dateAcquisition.toString(kDateFmt));
        text(r, 9, m.emplacement);

        auto *cell = new QWidget;
        cell->setObjectName("cell");
        cell->setAttribute(Qt::WA_StyledBackground, true);
        auto *cl = new QHBoxLayout(cell);
        cl->setContentsMargins(4, 0, 0, 0);
        cl->setSpacing(2);
        // Boutons-emoji : Afficher / Modifier / Supprimer
        const struct { const char *emoji, *tip, *kind; } actions[] = {
                        {"👁️", "Afficher", "view"}, {"✏️", "Modifier", "edit"}, {"🗑️","Supprimer", "del"}};
        for (const auto &a : actions) {
            auto *btn = new QToolButton;
            btn->setText(a.emoji);
            btn->setToolTip(a.tip);
            btn->setCursor(Qt::PointingHandCursor);
            const QString link = QString("%1:%2").arg(a.kind).arg(m.id);
            connect(btn, &QToolButton::clicked, this, [this, link] { handleAction(link); });
            cl->addWidget(btn);
        }
        cl->addStretch();
        ui->table->setCellWidget(r, 10, cell);
    }
    ui->table->setFixedHeight(44 + qMax(items.size(), 2) * kRowH + 2);
}

void MaterielPage::refreshStats()
{
    const QList<Materiel> all = Database::materiels("", "reference");

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

    // Bandeau + alerte magasinier
    if (crit > 0) {
        ui->banner->setText(QString("Stock critique : %1 matériel%2 sous le seuil minimal. "
                                    "Un email a été envoyé au magasinier.")
                                .arg(crit).arg(crit > 1 ? "s" : ""));
        ui->banner->show();
        QSet<int> ids;
        for (const Materiel &m : critical) ids.insert(m.id);
        if (ids != lastCritical) {   // n'alerter que si la liste des matériels critiques change
            lastCritical = ids;
            sendAlertEmail(critical);
        }
    } else {
        ui->banner->hide();
        lastCritical.clear();
    }
}

// ---------- Actions ----------

void MaterielPage::submitForm()
{
    Materiel m;
    m.id = editingId;
    m.reference = ui->leRef->text().trimmed().toUpper();
    m.designation = ui->leDesignation->text().trimmed();
    m.categorie = ui->cbCategorie->currentText();
    m.etat = ui->cbEtat->currentText();
    m.disponibilite = ui->cbDispo->currentText();
    m.emplacement = ui->leEmplacement->text().trimmed();

    QStringList errors;
    if (m.reference.isEmpty())
        errors << "La référence est obligatoire.";
    else if (Database::referenceExists(m.reference, editingId))
        errors << "Cette référence existe déjà.";
    if (m.designation.isEmpty())
        errors << "La désignation est obligatoire.";

    bool okQ = false, okS = false;
    m.quantite = ui->leQuantite->text().toInt(&okQ);
    m.seuil = ui->leSeuil->text().toInt(&okS);
    if (!okQ) errors << "La quantité en stock est obligatoire (nombre entier ≥ 0).";
    if (!okS) errors << "Le seuil minimal est obligatoire (nombre entier ≥ 0).";

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
    const bool ok = editingId ? Database::updateMateriel(m, &err) : Database::addMateriel(m, &err);
    if (!ok) {
        QMessageBox::critical(this, "Erreur", err);
        return;
    }
    clearForm();
    refresh();
}

void MaterielPage::clearForm()
{
    for (QLineEdit *e : {ui->leRef, ui->leDesignation, ui->leQuantite, ui->leSeuil, ui->leDate, ui->leEmplacement}) e->clear();
    ui->cbCategorie->setCurrentIndex(0);
    ui->cbEtat->setCurrentIndex(0);
    ui->cbDispo->setCurrentIndex(0);
    editingId = 0;
    ui->btnAdd->setText("Ajouter");
    ui->formTitle->setText("Ajouter");
}

void MaterielPage::fillForm(const Materiel &m)
{
    ui->leRef->setText(m.reference);
    ui->leDesignation->setText(m.designation);
    ui->cbCategorie->setCurrentIndex(qMax(0, ui->cbCategorie->findText(m.categorie)));
    ui->leQuantite->setText(QString::number(m.quantite));
    ui->leSeuil->setText(QString::number(m.seuil));
    ui->cbEtat->setCurrentIndex(qMax(0, ui->cbEtat->findText(m.etat)));
    ui->cbDispo->setCurrentIndex(qMax(0, ui->cbDispo->findText(m.disponibilite)));
    ui->leDate->setText(m.dateAcquisition.toString(kDateFmt));
    ui->leEmplacement->setText(m.emplacement);
    editingId = m.id;
    ui->btnAdd->setText("Mettre à jour");
    ui->formTitle->setText("Modifier");
}

void MaterielPage::handleAction(const QString &link)
{
    const QString kind = link.section(':', 0, 0);
    const int id = link.section(':', 1).toInt();
    const Materiel m = Database::materiel(id);
    if (m.id == 0) return;

    if (kind == "view") {
        QMessageBox::information(
            this, "Détails du matériel",
            QString("<b>%1 – %2</b><br><br>Catégorie : %3<br>Quantité : %4 (seuil minimal : %5)<br>"
                    "État : %6<br>Disponibilité : %7<br>Date d'acquisition : %8<br>Emplacement : %9")
                .arg(m.reference, m.designation, m.categorie).arg(m.quantite).arg(m.seuil)
                .arg(m.etat, m.disponibilite, m.dateAcquisition.toString(kDateFmt), m.emplacement));
    } else if (kind == "edit") {
        fillForm(m);
    } else if (kind == "del") {
        const auto rep = QMessageBox::question(
            this, "Supprimer",
            QString("Supprimer « %1 » ?").arg(m.designation));
        if (rep != QMessageBox::Yes) return;
        QString err;
        if (!Database::deleteMateriel(id, &err)) {
            QMessageBox::critical(this, "Erreur", err);
            return;
        }
        if (editingId == id) clearForm();
        refresh();
    }
}

void MaterielPage::exportCsv()
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
    for (const Materiel &m : Database::materiels("", "reference")) {
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

void MaterielPage::sendAlertEmail(const QList<Materiel> &critical)
{
    // TODO : brancher un vrai envoi d'email (SMTP, API mail...). Pour l'instant on journalise seulement.
    QStringList lines;
    for (const Materiel &m : critical)
        lines << QString("  - %1 %2 : %3 en stock (seuil %4)").arg(m.reference, m.designation).arg(m.quantite).arg(m.seuil);
    qInfo().noquote() << "[ALERTE STOCK] Email à envoyer au magasinier :\n" + lines.join("\n");
}

