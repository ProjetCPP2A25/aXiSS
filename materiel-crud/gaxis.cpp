#include "gaxis.h"
#include "ui_gaxis.h"   // généré à partir de gaxis.ui
#include "connection.h"

#include <QtWidgets>
#include <QtMath>
#include <QRegularExpression>

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

// ---------- Graphiques ----------


BarChartWidget::BarChartWidget(QWidget *parent) : QWidget(parent) {}

void BarChartWidget::setData(const QVector<QPair<QString, int>> &data)
{
    m_data = data;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_data.isEmpty()) return;

    const int topPad = 36, bottomPad = 40;
    const QRectF area(10, topPad, width() - 20, height() - topPad - bottomPad);

    int maxV = 1;
    for (const auto &d : m_data) maxV = qMax(maxV, d.second);

    const double slot = area.width() / m_data.size();
    const double barW = qMin(80.0, slot * 0.4);

    QFont valueFont = font();
    valueFont.setPointSize(15);
    valueFont.setBold(true);
    QFont labelFont = font();
    labelFont.setPointSize(11);

    for (int i = 0; i < m_data.size(); ++i) {
        const double h = area.height() * m_data[i].second / maxV;
        const QRectF bar(area.left() + slot * i + (slot - barW) / 2, area.bottom() - h, barW, h);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ff0000"));
        p.drawRoundedRect(bar, 3, 3);

        p.setPen(Qt::white);
        p.setFont(valueFont);
        p.drawText(QRectF(bar.left() - 20, bar.top() - 30, barW + 40, 28),
                   Qt::AlignHCenter | Qt::AlignBottom, QString::number(m_data[i].second));

        p.setPen(QColor("#9aa0a6"));
        p.setFont(labelFont);
        p.drawText(QRectF(area.left() + slot * i, area.bottom() + 8, slot, 28),
                   Qt::AlignHCenter | Qt::AlignTop, m_data[i].first);
    }
    p.setPen(QPen(QColor("#3a3f46"), 1));
    p.drawLine(area.bottomLeft(), area.bottomRight());
}

DonutWidget::DonutWidget(QWidget *parent) : QWidget(parent) {}

void DonutWidget::setData(int critical, int sufficient)
{
    m_critical = critical;
    m_sufficient = sufficient;
    update();
}

void DonutWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int thick = 14;
    const int side = qMin(width(), height()) - thick - 6;
    const QRectF r((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    const int total = m_critical + m_sufficient;

    QPen pen(total > 0 ? QColor("#1e9e4a") : QColor("#3a3f46"), thick);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    p.drawArc(r, 0, 360 * 16);

    if (total > 0 && m_critical > 0) {
        pen.setColor(QColor("#d40000"));
        p.setPen(pen);
        p.drawArc(r, 90 * 16, -int(360.0 * 16 * m_critical / total));
    }

    QFont big = font();
    big.setPointSize(16);
    big.setBold(true);
    p.setFont(big);
    p.setPen(Qt::white);
    p.drawText(QRectF(r.left(), r.center().y() - 22, r.width(), 28), Qt::AlignCenter, QString::number(total));

    QFont small = font();
    small.setPointSize(8);
    p.setFont(small);
    p.setPen(QColor("#9aa0a6"));
    p.drawText(QRectF(r.left(), r.center().y() + 4, r.width(), 16), Qt::AlignCenter, "total");
}

// ---------- Feuille de style (thème sombre) ----------

static const char kStyleRaw[] = R"QSS(
QMainWindow, #materielPage { background: #17191c; }
QScrollArea { border: none; background: #17191c; }
QScrollArea > QWidget > QWidget { background: #17191c; }

#sidebar { background: #1f2226; }
#logo { font-size: 22px; font-weight: 800; color: #ffffff; }
#logoSub { color: #9aa0a6; font-size: 11px; }
#sidebarFooter { color: #9aa0a6; font-size: 11px; border-top: 1px solid #33373d; padding-top: 10px; }
QPushButton[nav="true"] { background: transparent; border: none; border-radius: 6px; text-align: left;
                  padding: 10px 10px; color: #d0d3d6; font-size: 13px; }
QPushButton[nav="true"]:hover { background: #2a2e34; }
QPushButton[nav="true"]:checked { background: #ff0000; color: #ffffff; font-weight: 600; }

#pageTitle { font-size: 20px; font-weight: 700; color: #ffffff; }
QLabel[placeholder="true"] { color: #9aa0a6; font-size: 15px; }
#banner { background: #fbe9d0; color: #8a5a14; border: 1px solid #e0b36a;
               border-radius: 3px; padding: 6px 12px; font-size: 12px; }

QFrame[card="true"] { background: #1c1f23; border: 1px solid #2b2f35; border-radius: 10px; }
QLabel[cardTitle="true"] { color: #d40000; font-weight: 700; font-size: 14px; }
QLabel[fieldLabel="true"] { color: #8d939a; font-size: 11px; }
QLabel[muted="true"] { color: #aeb3b9; font-size: 12px; }

QLineEdit, QComboBox { background: #14161a; border: 1px solid #2b2f35; border-radius: 6px;
                       padding: 6px 9px; color: #e6e6e6; min-height: 18px; }
QLineEdit:focus, QComboBox:focus { border-color: #ff0000; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(%ARROW%); width: 10px; height: 6px; }
QComboBox QAbstractItemView { background: #14161a; color: #e6e6e6; border: 1px solid #2b2f35;
                              selection-background-color: #ff0000; outline: 0; }

QPushButton { background: #14161a; border: 1px solid #2b2f35; border-radius: 6px;
              padding: 7px 14px; color: #e6e6e6; }
QPushButton:hover { border-color: #5a5f66; }
QPushButton#btnAdd, QPushButton#btnMvSave { background: #ff0000; border: 1px solid #ff0000; color: #ffffff; font-weight: 600; }
QPushButton#btnAdd:hover, QPushButton#btnMvSave:hover { background: #e00000; }

QTableWidget { background: transparent; border: none; color: #e6e6e6; font-size: 11px; }
QTableWidget::item { border-bottom: 1px solid #2b2f35; padding: 2px; }
QHeaderView { background: transparent; }
QHeaderView::section { background: transparent; color: #aeb3b9; border: none;
                       border-bottom: 1px solid #2b2f35; padding: 2px; font-size: 10px; }
QToolButton { background: transparent; border: none; border-radius: 6px; padding: 4px; font-size: 16px; }
QToolButton:hover { background: #2a2e34; }
#cell { border-bottom: 1px solid #2b2f35; }


QMessageBox { background: #1c1f23; }
)QSS";

// Petite flèche des listes déroulantes : générée une fois dans le dossier temporaire
static QString styleSheet()
{
    const QString path = QDir::tempPath() + "/axis_arrow.png";
    QPixmap px(10, 6);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#9aa0a6"));
    p.drawPolygon(QPolygon({QPoint(0, 0), QPoint(10, 0), QPoint(5, 6)}));
    p.end();
    px.save(path, "PNG");
    return QString::fromUtf8(kStyleRaw).replace("%ARROW%", path);
}

// ---------- Fenêtre principale ----------

gaxis::gaxis(QWidget *parent) : QMainWindow(parent), ui(new Ui::gaxis)
{
    ui->setupUi(this);

    // Navigation latérale -> pages du QStackedWidget
    connect(ui->btnEmployes, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(0); });
    connect(ui->btnInterventions, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(1); });
    connect(ui->btnVehicules, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(2); });
    connect(ui->btnMateriels, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(3); });

    initMateriel();
}

gaxis::~gaxis()
{
    delete ui;
}

// ---------- Page Matériels : initialisation ----------

void gaxis::initMateriel()
{

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
    ui->materielPage->setPalette(pal);
    ui->materielPage->setAttribute(Qt::WA_StyledBackground, true);   // nécessaire pour que le fond (QSS) soit dessiné

    // Longueurs max = tailles des colonnes Oracle
    ui->leRef->setMaxLength(50);
    ui->leDesignation->setMaxLength(100);
    ui->leEmplacement->setMaxLength(100);
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
    connect(ui->btnAdd, &QPushButton::clicked, this, &gaxis::submitForm);
    connect(ui->btnClear, &QPushButton::clicked, this, &gaxis::clearForm);
    connect(ui->btnExport, &QPushButton::clicked, this, &gaxis::exportCsv);
    connect(ui->leSearch, &QLineEdit::textChanged, this, &gaxis::refreshMateriels);
    connect(ui->cbSort, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &gaxis::refreshMateriels);

    refresh();
}

// ---------- Rafraîchissement ----------

void gaxis::refresh()
{
    refreshMateriels();
    refreshStats();
}

void gaxis::refreshMateriels()
{
    static const char *cols[] = {"etat", "designation", "categorie", "quantite", "date_acquisition"};
    const QList<Materiel> items = Materiel::afficher(ui->leSearch->text().trimmed(), cols[ui->cbSort->currentIndex()]);

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

void gaxis::refreshStats()
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

    // Bandeau + alerte magasinier
    if (crit > 0) {
        ui->banner->setText(QString("Stock critique : %1 matériel%2 sous le seuil minimal.")
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

void gaxis::submitForm()
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
    else if (!QRegularExpression("^[A-Z0-9][A-Z0-9_-]*$").match(m.reference).hasMatch())
        errors << "La référence ne peut contenir que des lettres, chiffres, '-' ou '_' (ex. MAT-0001).";
    else if (Materiel::referenceExiste(m.reference, editingId))
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
    const bool ok = editingId ? m.modifier(&err) : m.ajouter(&err);
    if (!ok) {
        QMessageBox::critical(this, "Erreur", err);
        return;
    }
    clearForm();
    refresh();
}

void gaxis::clearForm()
{
    for (QLineEdit *e : {ui->leRef, ui->leDesignation, ui->leQuantite, ui->leSeuil, ui->leDate, ui->leEmplacement}) e->clear();
    ui->cbCategorie->setCurrentIndex(0);
    ui->cbEtat->setCurrentIndex(0);
    ui->cbDispo->setCurrentIndex(0);
    editingId = 0;
    ui->btnAdd->setText("Ajouter");
    ui->formTitle->setText("Ajouter");
}

void gaxis::fillForm(const Materiel &m)
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

void gaxis::handleAction(const QString &link)
{
    const QString kind = link.section(':', 0, 0);
    const int id = link.section(':', 1).toInt();
    const Materiel m = Materiel::trouver(id);
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
        if (!Materiel::supprimer(id, &err)) {
            QMessageBox::critical(this, "Erreur", err);
            return;
        }
        if (editingId == id) clearForm();
        refresh();
    }
}

void gaxis::exportCsv()
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

void gaxis::sendAlertEmail(const QList<Materiel> &critical)
{
    // TODO : brancher un vrai envoi d'email (SMTP, API mail...). Pour l'instant on journalise seulement.
    QStringList lines;
    for (const Materiel &m : critical)
        lines << QString("  - %1 %2 : %3 en stock (seuil %4)").arg(m.reference, m.designation).arg(m.quantite).arg(m.seuil);
    qInfo().noquote() << "[ALERTE STOCK] Email à envoyer au magasinier :\n" + lines.join("\n");
}

// ---------- Point d'entrée ----------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AXIS");
    app.setOrganizationName("RedFlameProtection");
    app.setStyle("Fusion");

    // Palette sombre (utilisée aussi par les boîtes de dialogue standard)
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
    pal.setColor(QPalette::Link, QColor("#e6e6e6"));
    app.setPalette(pal);
    app.setStyleSheet(styleSheet());

    QString err;
    if (!Connection::ouvrir(&err)) {
        QMessageBox::critical(nullptr, "Base de données",
                              "Impossible de se connecter à Oracle :\n" + err +
                                  "\n\n(Vérifie le DSN, l'utilisateur et le mot de passe dans connection.cpp)");
        return 1;
    }

    gaxis w;
    w.show();
    return app.exec();
}
