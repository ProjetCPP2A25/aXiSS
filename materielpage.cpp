#include "materielpage.h"
#include "ui_materielpage.h"   // généré à partir de materielpage.ui
#include "charts.h"
#include "style.h"
#include "smtp.h"
#include "sidebar.h"

#include <QtWidgets>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <memory>

static const QStringList kCategories = {"Lutte incendie", "Protection", "Secours"};
static const char *kDateFmt = "dd/MM/yyyy";
// Destinataire des alertes de stock critique (modifiable via la clé « materiel/alert_to » des réglages)
static const char *kAlertEmail = "ahmed.rebhi@esprit.tn";
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
    // Style en ligne (et non via propriété dynamique) pour que la taille du badge soit correcte dès l'affichage
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
    // Intégration dans AXIS : l'interface du .ui est construite dans un widget interne,
    // placé dans un QScrollArea : la page défile verticalement.
    auto *contenu = new QWidget;
    ui->setupUi(contenu);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(contenu);

    // Barre latérale commune à gauche, page défilante à droite
    auto *sidebar = new SidebarFrame;
    connect(sidebar, &SidebarFrame::menuRequested, this, &MaterielPage::menuRequested);
    connect(sidebar, &SidebarFrame::logoutRequested, this, &MaterielPage::logoutRequested);

    auto *externe = new QHBoxLayout(this);
    externe->setContentsMargins(0, 0, 0, 0);
    externe->setSpacing(0);
    externe->addWidget(sidebar);
    externe->addWidget(scroll, 1);

    // Thème sombre appliqué à cette page uniquement (n'affecte pas le reste de l'application AXIS)
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#17191c"));
    pal.setColor(QPalette::WindowText, QColor("#e6e6e6"));
    pal.setColor(QPalette::Base, QColor("#14161a"));
    pal.setColor(QPalette::AlternateBase, QColor("#1c1f23"));
    pal.setColor(QPalette::Text, QColor("#e6e6e6"));
    pal.setColor(QPalette::Button, QColor("#1c1f23"));
    pal.setColor(QPalette::ButtonText, QColor("#e6e6e6"));
    pal.setColor(QPalette::Highlight, QColor("#ff0000"));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::PlaceholderText, QColor("#5b6068"));
    setPalette(pal);
    setAttribute(Qt::WA_StyledBackground, true);   // nécessaire pour que le fond (QSS) soit dessiné
    setStyleSheet(kStyle);

    ui->leQuantite->setValidator(new QIntValidator(0, 1000000, this));
    ui->leSeuil->setValidator(new QIntValidator(0, 1000000, this));

    // Tableau des matériels : largeurs de colonnes (la désignation prend l'espace restant)
    QHeaderView *h = ui->table->horizontalHeader();
    h->setFixedHeight(44);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::Fixed);
    h->setSectionResizeMode(2, QHeaderView::Stretch);
    const int widths[] = {48, 80, 0, 92, 66, 46, 100, 98, 82, 76, 112};
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
    static const char *cols[] = {"etat", "designation", "categorie", "quantite", "date_acquisition"};
    const QList<Materiel> items = Database::materiels(ui->leSearch->text().trimmed(), cols[ui->cbSort->currentIndex()]);

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
            {"👁️", "Afficher", "view"}, {"✏️", "Modifier", "edit"}, {"🗑️", "Supprimer", "del"}};
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

    // Bandeau + alerte par email : on n'alerte que pour les matériels qui VIENNENT de passer sous le seuil
    QSet<int> ids;
    for (const Materiel &m : critical) ids.insert(m.id);

    if (!alertsPrimed) {
        alertsPrimed = true;            // démarrage : pas d'email, on mémorise l'état actuel
    } else {
        QList<Materiel> nouveaux;
        for (const Materiel &m : critical)
            if (!lastCritical.contains(m.id)) nouveaux << m;
        if (!nouveaux.isEmpty()) sendAlertEmail(nouveaux);
    }
    lastCritical = ids;

    if (crit > 0) {
        QString txt = QString("Stock critique : %1 matériel%2 sous le seuil minimal.")
                          .arg(crit).arg(crit > 1 ? "s" : "");
        if (!mailStatus.isEmpty()) txt += " " + mailStatus;
        ui->banner->setText(txt);
        ui->banner->show();
    } else {
        ui->banner->hide();
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
        errors << "La désignation est obligatoire (nom du matériel).";
    else if (m.designation.size() < 2 || !m.designation.contains(QRegularExpression("\\p{L}")))
        errors << "La désignation doit être un nom de matériel (ex. Extincteur CO2), pas un nombre.";

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
    QStringList lines;
    for (const Materiel &m : critical)
        lines << QString("  - %1 %2 : %3 en stock (seuil minimal %4)").arg(m.reference, m.designation).arg(m.quantite).arg(m.seuil);

    // Mêmes réglages SMTP que le reste de l'application (menu « Paramètres email »)
    QSettings s;
    SmtpConfig cfg;
    cfg.hote = s.value("smtp/host", "smtp.gmail.com").toString();
    cfg.port = s.value("smtp/port", 465).toInt();
    cfg.user = s.value("smtp/user").toString().trimmed();
    cfg.pass = s.value("smtp/pass").toString();
    QString from = s.value("smtp/from").toString().trimmed();
    if (from.isEmpty() && cfg.user.contains('@')) from = cfg.user;
    const QString to = s.value("materiel/alert_to", kAlertEmail).toString().trimmed();

    qInfo().noquote() << "[ALERTE STOCK] Email pour" << to << ":\n" + lines.join("\n");

    if (cfg.user.isEmpty() || cfg.pass.trimmed().isEmpty() || from.isEmpty()) {
        mailStatus = "Email non envoyé : paramètres email non configurés.";
        return;
    }

    EmailMessage msg;
    msg.from = from;
    msg.nomFrom = "A.X.I.S - Gestion des matériels";
    msg.to = to;
    msg.sujet = QString("[A.X.I.S] Stock critique : %1 matériel%2 sous le seuil minimal")
                    .arg(critical.size()).arg(critical.size() > 1 ? "s" : "");
    msg.corps = QString("Bonjour,\n\nLes matériels suivants sont passés sous leur seuil minimal de stock :\n\n%1\n\n"
                        "Merci de procéder au réapprovisionnement.\n\nA.X.I.S - RedFlame Protection")
                    .arg(lines.join("\n"));

    mailStatus = "Envoi de l'email d'alerte en cours...";

    // Envoi dans un thread séparé : l'interface ne se bloque pas
    auto erreur = std::make_shared<QString>();
    auto ok = std::make_shared<bool>(false);
    QThread *th = QThread::create([=] { *ok = envoyerEmail(cfg, msg, *erreur); });
    connect(th, &QThread::finished, this, [this, th, erreur, ok, to] {
        mailStatus = *ok ? QString("Email d'alerte envoyé à %1.").arg(to)
                         : QString("Email non envoyé : %1").arg(*erreur);
        if (!*ok) qWarning().noquote() << "[ALERTE STOCK]" << mailStatus;
        th->deleteLater();
        refreshStats();
    });
    th->start();
}
