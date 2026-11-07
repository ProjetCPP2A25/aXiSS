#include "vehiculepage.h"
#include "ui_vehiculepage.h"   // généré à partir de vehiculepage.ui
#include "vehiculedonut.h"
#include "style.h"
#include "employe.h"

#include <QtWidgets>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextStream>
#include <algorithm>

namespace {
const char *DATE_FMT = "dd/MM/yyyy";
const QColor GREEN("#1f9d55"), ORANGE("#c2670f"), RED("#c81e1e");
const int kRowH = 58;       // hauteur des lignes du tableau (comme la page Matériels)
const int kHeaderH = 44;    // hauteur de l'en-tête du tableau

// Cellule contenant un badge (kind = ok | crit | warn), identique à celle de la page Matériels
QWidget *badgeCell(const QString &text, const QString &kind)
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

QString badgeKind(const QString &value)
{
    if (value == "Bon" || value == "Disponible") return "ok";
    if (value == "À surveiller" || value == "En maintenance") return "warn";
    return "crit";   // Hors service, En intervention
}
} // namespace

// ------------------------------------------------------------------ utilitaires
QDate VehiculePage::parseDate(const QString &s) { return QDate::fromString(s.trimmed(), DATE_FMT); }
QString VehiculePage::fmt(const QDate &d) { return d.isValid() ? d.toString(DATE_FMT) : QString("—"); }
QString VehiculePage::kmText(int km) const { return QLocale(QLocale::French).toString(km); }

QString VehiculePage::nextId() const
{
    int mx = 0;
    for (const Vehicule &v : Vehicule::tous()) mx = qMax(mx, v.id.mid(1).toInt());
    return QString("V%1").arg(mx + 1, 3, 10, QChar('0'));
}

// ------------------------------------------------------------------ construction UI
VehiculePage::VehiculePage(QWidget *parent) : QWidget(parent), ui(new Ui::VehiculePage)
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

    // Barre latérale à gauche (commune à tous les modules), page défilante à droite
    m_sidebar = new SidebarFrame;
    connect(m_sidebar, &SidebarFrame::menuRequested, this, &VehiculePage::menuRequested);
    connect(m_sidebar, &SidebarFrame::logoutRequested, this, &VehiculePage::logoutRequested);

    auto *externe = new QHBoxLayout(this);
    externe->setContentsMargins(0, 0, 0, 0);
    externe->setSpacing(0);
    externe->addWidget(m_sidebar);
    externe->addWidget(scroll, 1);

    setStyleSheet(kStyle);

    // réglages de l'en-tête du tableau (non disponibles dans le fichier .ui)
    QHeaderView *h = ui->table->horizontalHeader();
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::ResizeToContents);
    h->setFixedHeight(kHeaderH);

    connect(ui->btnAdd, &QPushButton::clicked, this, &VehiculePage::onSubmit);
    connect(ui->btnClear, &QPushButton::clicked, this, &VehiculePage::clearForm);
    connect(ui->sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { refresh(); });
    connect(ui->btnPdf, &QPushButton::clicked, this, &VehiculePage::exportPdf);
    connect(ui->btnExcel, &QPushButton::clicked, this, &VehiculePage::exportExcel);

    QString erreur;
    if (!Vehicule::initialiser(&erreur))
        QMessageBox::critical(this, "Base de données", erreur);
    reload();
}

VehiculePage::~VehiculePage()
{
    delete ui;
}

void VehiculePage::setInfo(const QString &text)
{
    m_sidebar->setInfo(text);
}

void VehiculePage::reload()
{
    m_vehicles = Vehicule::tous();
    refresh();
}

// ------------------------------------------------------------------ rafraîchissement
void VehiculePage::refresh()
{
    // tri
    switch (ui->sortCombo->currentIndex()) {
    case 0: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.km < b.km; }); break;
    case 1: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.id < b.id; }); break;
    case 2: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.plate < b.plate; }); break;
    default: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.nextMaint < b.nextMaint; }); break;
    }

    // tableau
    ui->table->setRowCount(0);
    ui->table->setRowCount(m_vehicles.size());

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

    for (int r = 0; r < m_vehicles.size(); ++r) {
        const Vehicule &v = m_vehicles[r];
        text(r, 0, v.id, true);
        text(r, 1, v.plate, true);
        text(r, 2, v.type);
        text(r, 3, kmText(v.km));
        ui->table->setCellWidget(r, 4, badgeCell(v.etat, badgeKind(v.etat)));
        ui->table->setCellWidget(r, 5, badgeCell(v.dispo, badgeKind(v.dispo)));
        text(r, 6, fmt(v.lastMaint));
        text(r, 7, fmt(v.nextMaint));
        text(r, 8, fmt(v.insurance));
        text(r, 9, fmt(v.tech));

        auto *cell = new QWidget;
        cell->setObjectName("cell");
        cell->setAttribute(Qt::WA_StyledBackground, true);
        auto *cl = new QHBoxLayout(cell);
        cl->setContentsMargins(4, 0, 0, 0);
        cl->setSpacing(2);
        // Boutons-emoji : Afficher / Modifier / Supprimer (comme la page Matériels)
        const struct { const char *emoji, *tip, *kind; } actions[] = {
            {"👁️", "Afficher", "view"}, {"✏️", "Modifier", "edit"}, {"🗑️", "Supprimer", "del"}};
        const QString id = v.id;
        for (const auto &a : actions) {
            auto *btn = new QToolButton;
            btn->setText(a.emoji);
            btn->setToolTip(a.tip);
            btn->setCursor(Qt::PointingHandCursor);
            const QString kind = a.kind;
            connect(btn, &QToolButton::clicked, this, [this, id, kind] {
                if (kind == "view") viewVehicle(id);
                else if (kind == "edit") editVehicle(id);
                else deleteVehicle(id);
            });
            cl->addWidget(btn);
        }
        cl->addStretch();
        ui->table->setCellWidget(r, 10, cell);
    }
    ui->table->setFixedHeight(kHeaderH + qMax(int(m_vehicles.size()), 2) * kRowH + 16);

    // alerte maintenance
    QStringList alerts;
    const QDate today = QDate::currentDate();
    for (const Vehicule &v : m_vehicles)
        if (v.nextMaint.isValid() && v.nextMaint <= today)
            alerts << QString("%1 (état %2, prochaine maintenance le %3)")
                          .arg(v.id, v.etat.toLower(), fmt(v.nextMaint));
    ui->banner->setVisible(!alerts.isEmpty());
    ui->banner->setText("Maintenance requise : " + alerts.join(" ; ") + ".");

    // statistiques
    auto stats = [this](VehiculeDonutWidget *d, QLabel *lg, const QStringList &keys, const QVector<QColor> &cols,
                        bool etat) {
        QVector<VehiculeSlice> slices;
        const int total = m_vehicles.size();
        QString html = "<table cellspacing='4'>";
        for (int i = 0; i < keys.size(); ++i) {
            int n = 0;
            for (const Vehicule &v : m_vehicles) n += ((etat ? v.etat : v.dispo) == keys[i]);
            slices.push_back({keys[i], n, cols[i]});
            const int pct = total ? qRound(100.0 * n / total) : 0;
            html += QString("<tr><td><span style='color:%1'>●</span></td><td>%2 <b>%3</b> (%4%)</td></tr>")
                        .arg(cols[i].name(), keys[i]).arg(n).arg(pct);
        }
        d->setSlices(slices);
        lg->setText(html + "</table>");
    };
    stats(ui->donutEtat, ui->legendEtat, {"Bon", "À surveiller", "Hors service"}, {GREEN, ORANGE, RED}, true);
    stats(ui->donutDispo, ui->legendDispo, {"Disponible", "En intervention", "En maintenance"},
          {GREEN, RED, ORANGE}, false);
}

// ------------------------------------------------------------------ actions
void VehiculePage::clearForm()
{
    m_editId.clear();
    ui->btnAdd->setText("Ajouter");
    ui->formTitle->setText("Ajouter");
    for (QLineEdit *e : {ui->plateEdit, ui->kmEdit, ui->lastMaintEdit, ui->nextMaintEdit, ui->insuranceEdit, ui->techEdit})
        e->clear();
    ui->typeCombo->setCurrentIndex(0);
    ui->etatCombo->setCurrentIndex(0);
    ui->dispoCombo->setCurrentIndex(0);
}

void VehiculePage::onSubmit()
{
    Vehicule v;
    v.plate = ui->plateEdit->text().trimmed();
    if (v.plate.isEmpty()) {
        QMessageBox::warning(this, "Champ manquant", "L'immatriculation est obligatoire.");
        return;
    }
    QString kmTxt = ui->kmEdit->text();
    kmTxt.remove(QRegularExpression("[\\s\\x{202F}\\x{00A0}]"));
    bool ok = false;
    v.km = kmTxt.toInt(&ok);
    if (!ok || v.km < 0) {
        QMessageBox::warning(this, "Kilométrage invalide", "Saisissez un kilométrage numérique (ex. 42 300).");
        return;
    }

    struct F { QLineEdit *e; QDate *d; const char *name; };
    const F fields[] = {{ui->lastMaintEdit, &v.lastMaint, "Dernière maintenance"},
                        {ui->nextMaintEdit, &v.nextMaint, "Prochaine maintenance"},
                        {ui->insuranceEdit, &v.insurance, "Assurance"},
                        {ui->techEdit, &v.tech, "Prochaine visite technique"}};
    for (const F &f : fields) {
        if (f.e->text().trimmed().isEmpty()) continue;
        *f.d = parseDate(f.e->text());
        if (!f.d->isValid()) {
            QMessageBox::warning(this, "Date invalide",
                                 QString("« %1 » : utilisez le format jj/mm/aaaa.").arg(f.name));
            return;
        }
    }
    v.type = ui->typeCombo->currentText();
    v.etat = ui->etatCombo->currentText();
    v.dispo = ui->dispoCombo->currentText();

    QString erreur;
    bool saved = false;
    if (m_editId.isEmpty()) {
        v.id = nextId();
        saved = Vehicule::ajouter(v, &erreur);
    } else {
        v.id = m_editId;
        saved = Vehicule::modifier(v, &erreur);
    }
    if (!saved) {
        QMessageBox::critical(this, "Erreur", erreur);
        return;
    }
    clearForm();
    reload();
}

void VehiculePage::viewVehicle(const QString &id)
{
    for (const Vehicule &v : m_vehicles) {
        if (v.id != id) continue;
        QMessageBox::information(this, "Véhicule " + v.id,
            QString("Immatriculation : %1\nType : %2\nKilométrage : %3 km\nÉtat : %4\nDisponibilité : %5\n"
                    "Dernière maintenance : %6\nProchaine maintenance : %7\nAssurance : %8\n"
                    "Visite technique : %9")
                .arg(v.plate, v.type, kmText(v.km), v.etat, v.dispo, fmt(v.lastMaint), fmt(v.nextMaint),
                     fmt(v.insurance), fmt(v.tech)));
        return;
    }
}

void VehiculePage::editVehicle(const QString &id)
{
    for (const Vehicule &v : m_vehicles) {
        if (v.id != id) continue;
        m_editId = id;
        ui->plateEdit->setText(v.plate);
        ui->typeCombo->setCurrentText(v.type);
        ui->kmEdit->setText(QString::number(v.km));
        ui->etatCombo->setCurrentText(v.etat);
        ui->dispoCombo->setCurrentText(v.dispo);
        ui->lastMaintEdit->setText(v.lastMaint.isValid() ? v.lastMaint.toString(DATE_FMT) : "");
        ui->nextMaintEdit->setText(v.nextMaint.isValid() ? v.nextMaint.toString(DATE_FMT) : "");
        ui->insuranceEdit->setText(v.insurance.isValid() ? v.insurance.toString(DATE_FMT) : "");
        ui->techEdit->setText(v.tech.isValid() ? v.tech.toString(DATE_FMT) : "");
        ui->btnAdd->setText("Enregistrer");
        ui->formTitle->setText("Modifier");
        return;
    }
}

void VehiculePage::deleteVehicle(const QString &id)
{
    if (QMessageBox::question(this, "Supprimer", "Supprimer le véhicule " + id + " ?") != QMessageBox::Yes)
        return;
    QString erreur;
    if (!Vehicule::supprimer(id, &erreur)) {
        QMessageBox::critical(this, "Erreur", erreur);
        return;
    }
    if (m_editId == id) clearForm();
    reload();
}

// ------------------------------------------------------------------ exports
void VehiculePage::exportPdf()
{
    const QString path = QFileDialog::getSaveFileName(this, "Exporter en PDF", "vehicules.pdf", "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QString html = "<h2 style='color:#d90000'>A.X.I.S : Liste des véhicules</h2>"
                   "<table border='1' cellspacing='0' cellpadding='4' width='100%'>"
                   "<tr bgcolor='#eeeeee'><th>ID</th><th>Immatriculation</th><th>Type</th><th>Km</th>"
                   "<th>État</th><th>Disponibilité</th><th>Dern. maint.</th><th>Proch. maint.</th>"
                   "<th>Assurance</th><th>Visite tech.</th></tr>";
    for (const Vehicule &v : m_vehicles)
        html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td>"
                        "<td>%7</td><td>%8</td><td>%9</td><td>%10</td></tr>")
                    .arg(v.id, v.plate, v.type, kmText(v.km), v.etat, v.dispo, fmt(v.lastMaint),
                         fmt(v.nextMaint), fmt(v.insurance), fmt(v.tech));
    html += "</table>";

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);
    QMessageBox::information(this, "Export", "PDF créé :\n" + path);
}

void VehiculePage::exportExcel()
{
    const QString path = QFileDialog::getSaveFileName(this, "Exporter pour Excel", "vehicules.csv",
                                                      "Excel / CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Erreur", "Impossible d'écrire le fichier.");
        return;
    }
    f.write("\xEF\xBB\xBF"); // BOM UTF-8 pour qu'Excel reconnaisse les accents
    QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "ID;Immatriculation;Type;Km;État;Disponibilité;Dernière maint.;Prochaine maint.;Assurance;Visite technique\n";
    for (const Vehicule &v : m_vehicles)
        out << v.id << ';' << v.plate << ';' << v.type << ';' << v.km << ';' << v.etat << ';' << v.dispo
            << ';' << fmt(v.lastMaint) << ';' << fmt(v.nextMaint) << ';' << fmt(v.insurance) << ';'
            << fmt(v.tech) << '\n';
    QMessageBox::information(this, "Export", "Fichier créé (ouvrable dans Excel) :\n" + path);
}
