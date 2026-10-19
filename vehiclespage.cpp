#include "vehiclespage.h"
#include "vehicledonutwidget.h"
#include "vehicletheme.h"
#include "sidebar.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QRegularExpression>
#include <QPdfWriter>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>

namespace {
const char *DATE_FMT = "dd/MM/yyyy";
const QColor GREEN("#1f9d55"), ORANGE("#c2670f"), RED("#c81e1e");

struct BadgeColors { QString bg, fg; };

BadgeColors colorsFor(const QString &value)
{
    if (value == "Bon" || value == "Disponible") return {"#dff5e6", "#1b7f43"};
    if (value == "À surveiller" || value == "En maintenance") return {"#fdebd3", "#b45f06"};
    return {"#fde0e0", "#c0171d"}; // Hors service, En intervention
}

QWidget *makeBadge(const QString &text)
{
    const BadgeColors c = colorsFor(text);
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(4, 0, 4, 0);
    auto *b = new QLabel(text);
    b->setStyleSheet(QString("background:%1;color:%2;border-radius:9px;padding:2px 9px;"
                             "font-size:11px;font-weight:600;")
                         .arg(c.bg, c.fg));
    l->addWidget(b);
    l->addStretch();
    return w;
}

QLabel *fieldLabel(const QString &t)
{
    auto *l = new QLabel(t);
    l->setObjectName("muted");
    return l;
}

QString jd(const QDate &d) { return d.isValid() ? d.toString(Qt::ISODate) : QString(); }
} // namespace

// ------------------------------------------------------------------ utilitaires
QDate VehiclesPage::parseDate(const QString &s) { return QDate::fromString(s.trimmed(), DATE_FMT); }
QString VehiclesPage::fmt(const QDate &d) { return d.isValid() ? d.toString(DATE_FMT) : QString("—"); }
QString VehiclesPage::kmText(int km) const { return QLocale(QLocale::French).toString(km); }

QString VehiclesPage::dataPath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/vehicules.json";
    if (!QFileInfo::exists(path)) {
        // Reprise des véhicules saisis dans l'ancienne version autonome du module (dossier « AXIS »)
        const QString ancien = QDir::cleanPath(dir + "/../../AXIS/vehicules.json");
        if (QFileInfo::exists(ancien))
            QFile::copy(ancien, path);
    }
    return path;
}

QString VehiclesPage::nextId() const
{
    int mx = 0;
    for (const Vehicle &v : m_vehicles) mx = qMax(mx, v.id.mid(1).toInt());
    return QString("V%1").arg(mx + 1, 3, 10, QChar('0'));
}

void VehiclesPage::load()
{
    QFile f(dataPath());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        for (const QJsonValue &val : arr) {
            const QJsonObject o = val.toObject();
            Vehicle v;
            v.id = o["id"].toString();
            v.plate = o["plate"].toString();
            v.type = o["type"].toString();
            v.km = o["km"].toInt();
            v.etat = o["etat"].toString();
            v.dispo = o["dispo"].toString();
            v.lastMaint = QDate::fromString(o["lastMaint"].toString(), Qt::ISODate);
            v.nextMaint = QDate::fromString(o["nextMaint"].toString(), Qt::ISODate);
            v.insurance = QDate::fromString(o["insurance"].toString(), Qt::ISODate);
            v.tech = QDate::fromString(o["tech"].toString(), Qt::ISODate);
            m_vehicles.push_back(v);
        }
        if (!m_vehicles.isEmpty()) return;
    }
    // données d'exemple (identiques à la maquette)
    m_vehicles = {
                   {"V001", "123 TUN 4567", "Camion-citerne", 42300, "Bon", "Disponible",
                    QDate(2026, 9, 15), QDate(2026, 12, 15), QDate(2026, 12, 31), QDate(2026, 11, 15)},
                   {"V002", "145 TUN 8821", "Ambulance", 68950, "À surveiller", "En maintenance",
                    QDate(2026, 8, 2), QDate(2026, 10, 2), QDate(2026, 11, 30), QDate(2026, 12, 10)},
                   {"V003", "98 TUN 3310", "Échelle aérienne", 101400, "Bon", "En intervention",
                    QDate(2026, 7, 20), QDate(2026, 10, 20), QDate(2026, 10, 12), QDate(2027, 1, 5)},
                   };
}

void VehiclesPage::save() const
{
    QJsonArray arr;
    for (const Vehicle &v : m_vehicles) {
        QJsonObject o;
        o["id"] = v.id; o["plate"] = v.plate; o["type"] = v.type; o["km"] = v.km;
        o["etat"] = v.etat; o["dispo"] = v.dispo;
        o["lastMaint"] = jd(v.lastMaint); o["nextMaint"] = jd(v.nextMaint);
        o["insurance"] = jd(v.insurance); o["tech"] = jd(v.tech);
        arr.append(o);
    }
    QFile f(dataPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson());
}

// ------------------------------------------------------------------ construction UI
VehiclesPage::VehiclesPage(QWidget *parent) : QWidget(parent)
{
    // Le style du module n'est appliqué qu'à cette page (le reste de l'application garde le sien)
    setStyleSheet(kVehicleStyleSheet);

    // Barre latérale commune (Menu principal / Déconnexion) à gauche, page défilante à droite
    m_sidebar = new SidebarFrame;
    connect(m_sidebar, &SidebarFrame::menuRequested, this, &VehiclesPage::menuRequested);
    connect(m_sidebar, &SidebarFrame::logoutRequested, this, &VehiclesPage::logoutRequested);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_sidebar);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    outer->addWidget(scroll, 1);

    auto *content = new QWidget;
    scroll->setWidget(content);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    // Titre + alerte maintenance + bouton SMS
    auto *top = new QHBoxLayout;
    top->setSpacing(10);

    // Icône de petite voiture à gauche du titre
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(10);

    auto *carIcon = new QLabel(QString::fromUtf8("🚗"));
    carIcon->setStyleSheet("font-size: 30px; background: transparent;");

    auto *title = new QLabel("Gestion des véhicules");
    title->setObjectName("pageTitle");

    titleRow->addWidget(carIcon, 0, Qt::AlignVCenter);
    titleRow->addWidget(title, 0, Qt::AlignVCenter);

    m_alert = new QLabel;
    m_alert->setObjectName("alert");
    m_alert->setWordWrap(true);
    m_alert->setMaximumWidth(330);

    auto *smsBox = new QWidget;
    smsBox->setObjectName("smsBox");
    auto *smsLayout = new QHBoxLayout(smsBox);
    smsLayout->setContentsMargins(10, 6, 10, 6);
    smsLayout->setSpacing(10);

    auto *smsLabel = new QLabel("✉  Alerte SMS");
    m_smsButton = new QPushButton("Envoyer un SMS");
    m_smsButton->setObjectName("primary");

    smsLayout->addWidget(smsLabel);
    smsLayout->addWidget(m_smsButton);

    top->addLayout(titleRow, 0);
    top->addStretch();
    top->addWidget(m_alert, 0, Qt::AlignVCenter);
    top->addWidget(smsBox, 0, Qt::AlignVCenter);
    root->addLayout(top);

    connect(m_smsButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(
            this,
            "Alerte SMS",
            "Le bouton est prêt à être connecté à un service d'envoi SMS.\n"
            "L'envoi réel nécessite une API ou une passerelle SMS."
            );
    });

    // formulaire + liste
    auto *mid = new QHBoxLayout;
    mid->setSpacing(14);
    mid->addWidget(buildForm(), 0, Qt::AlignTop);
    mid->addWidget(buildList(), 1, Qt::AlignTop);
    root->addLayout(mid);

    // statistiques
    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(14);
    bottom->addWidget(buildStatCard("Statistiques par état du véhicule", "Répartition par état",
                                    m_donutEtat, m_legendEtat), 1);
    bottom->addWidget(buildStatCard("Statistiques par disponibilité", "Répartition par disponibilité",
                                    m_donutDispo, m_legendDispo), 1);
    root->addLayout(bottom);
    root->addStretch();

    load();
    refresh();
}

void VehiclesPage::setUserInfo(const QString &text)
{
    m_sidebar->setInfo(text);
}

QWidget *VehiclesPage::buildForm()
{
    auto *card = new QWidget;
    card->setObjectName("card");
    card->setFixedWidth(290);
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(4);

    auto *t = new QLabel("Ajouter");
    t->setObjectName("cardTitle");
    v->addWidget(t);

    m_plate = new QLineEdit; m_plate->setPlaceholderText("123 TUN 4567");
    m_type = new QComboBox;
    m_type->addItems({"Camion-citerne", "Ambulance", "Échelle aérienne", "Fourgon pompe", "Véhicule léger"});
    m_km = new QLineEdit; m_km->setPlaceholderText("42 300");
    m_etat = new QComboBox; m_etat->addItems({"Bon", "À surveiller", "Hors service"});
    m_dispo = new QComboBox; m_dispo->addItems({"Disponible", "En intervention", "En maintenance"});
    m_last = new QLineEdit; m_next = new QLineEdit; m_ins = new QLineEdit; m_tech = new QLineEdit;
    for (QLineEdit *e : {m_last, m_next, m_ins, m_tech}) e->setPlaceholderText("jj/mm/aaaa");

    auto add = [&](const QString &lab, QWidget *w) { v->addWidget(fieldLabel(lab)); v->addWidget(w); };
    add("Immatriculation", m_plate);
    add("Type", m_type);
    add("Kilométrage", m_km);
    add("État", m_etat);
    add("Disponibilité", m_dispo);
    add("Dernière maintenance", m_last);
    add("Prochaine maintenance", m_next);
    add("Assurance valable jusqu'au", m_ins);
    add("Prochaine visite technique", m_tech);

    auto *btns = new QHBoxLayout;
    m_submit = new QPushButton("Ajouter");
    m_submit->setObjectName("primary");
    auto *clear = new QPushButton("Vider");
    clear->setObjectName("ghost");
    btns->addWidget(m_submit);
    btns->addWidget(clear);
    btns->addStretch();
    v->addSpacing(8);
    v->addLayout(btns);

    connect(m_submit, &QPushButton::clicked, this, &VehiclesPage::onSubmit);
    connect(clear, &QPushButton::clicked, this, &VehiclesPage::clearForm);
    return card;
}

QWidget *VehiclesPage::buildList()
{
    auto *card = new QWidget;
    card->setObjectName("card");
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);

    auto *t = new QLabel("Liste");
    t->setObjectName("cardTitle");
    v->addWidget(t);

    auto *bar = new QHBoxLayout;

    // Recherche instantanée par ID, immatriculation ou type
    auto *search = new QLineEdit;
    search->setPlaceholderText("Rechercher par ID, immatriculation ou type");
    search->setClearButtonEnabled(true);

    // Tri existant conservé tel quel
    m_sort = new QComboBox;
    m_sort->addItems({"Trier par kilométrage", "Trier par ID", "Trier par immatriculation",
                      "Trier par prochaine maintenance"});

    auto *xls = new QPushButton("Exporter (Excel)");
    xls->setObjectName("ghost");

    bar->addWidget(search, 1);
    bar->addWidget(m_sort, 1);
    bar->addWidget(xls);
    v->addLayout(bar);

    m_table = new QTableWidget(0, 11);
    m_table->setHorizontalHeaderLabels({"ID", "Immatriculation", "Type", "Km", "État", "Disponibilité",
                                        "Dernière maint.", "Prochaine maint.", "Assurance",
                                        "Visite technique", "Actions"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setShowGrid(false);
    m_table->setWordWrap(true);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(52);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setFixedHeight(44);
    v->addWidget(m_table);

    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_table->setProperty("searchText", text);
        refresh();
    });
    connect(m_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { refresh(); });
    connect(xls, &QPushButton::clicked, this, &VehiclesPage::exportExcel);
    return card;
}

QWidget *VehiclesPage::buildStatCard(const QString &title, const QString &sub, VehicleDonutWidget *&donut,
                                     QLabel *&legend)
{
    auto *card = new QWidget;
    card->setObjectName("card");
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    auto *t = new QLabel(title);
    t->setObjectName("cardTitle");
    auto *s = new QLabel(sub);
    s->setObjectName("muted");
    v->addWidget(t);
    v->addWidget(s);

    auto *row = new QHBoxLayout;
    row->setSpacing(18);
    donut = new VehicleDonutWidget;
    legend = new QLabel;
    legend->setTextFormat(Qt::RichText);
    row->addWidget(donut);
    row->addWidget(legend);
    row->addStretch();
    v->addLayout(row);
    return card;
}

// ------------------------------------------------------------------ rafraîchissement
void VehiclesPage::refresh()
{
    // tri
    switch (m_sort->currentIndex()) {
    case 0: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.km < b.km; }); break;
    case 1: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.id < b.id; }); break;
    case 2: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.plate < b.plate; }); break;
    default: std::sort(m_vehicles.begin(), m_vehicles.end(), [](auto &a, auto &b) { return a.nextMaint < b.nextMaint; }); break;
    }

    // Filtrage instantané : ID, immatriculation ou type
    const QString searchText = m_table->property("searchText").toString().trimmed();
    QVector<Vehicle> filteredVehicles;
    for (const Vehicle &vehicle : m_vehicles) {
        if (searchText.isEmpty() ||
            vehicle.id.contains(searchText, Qt::CaseInsensitive) ||
            vehicle.plate.contains(searchText, Qt::CaseInsensitive) ||
            vehicle.type.contains(searchText, Qt::CaseInsensitive)) {
            filteredVehicles.push_back(vehicle);
        }
    }

    // tableau
    m_table->setRowCount(filteredVehicles.size());
    for (int r = 0; r < filteredVehicles.size(); ++r) {
        const Vehicle &v = filteredVehicles[r];
        const QStringList cells = {v.id, v.plate, v.type, kmText(v.km)};
        for (int c = 0; c < 4; ++c) m_table->setItem(r, c, new QTableWidgetItem(cells[c]));
        m_table->setCellWidget(r, 4, makeBadge(v.etat));
        m_table->setCellWidget(r, 5, makeBadge(v.dispo));
        m_table->setItem(r, 6, new QTableWidgetItem(fmt(v.lastMaint)));
        m_table->setItem(r, 7, new QTableWidgetItem(fmt(v.nextMaint)));
        m_table->setItem(r, 8, new QTableWidgetItem(fmt(v.insurance)));
        m_table->setItem(r, 9, new QTableWidgetItem(fmt(v.tech)));

        const QString st = "style='color:#e6e6e6;text-decoration:none;'";
        auto *actions = new QLabel(QString("<a href='view' %1>Afficher</a> · <a href='edit' %1>Modifier</a> · "
                                           "<a href='del' %1>Supprimer</a>").arg(st));
        actions->setTextFormat(Qt::RichText);
        actions->setCursor(Qt::PointingHandCursor);
        actions->setContentsMargins(4, 0, 4, 0);
        const QString id = v.id;
        connect(actions, &QLabel::linkActivated, this, [this, id](const QString &l) {
            if (l == "view") viewVehicle(id);
            else if (l == "edit") editVehicle(id);
            else deleteVehicle(id);
        });
        m_table->setCellWidget(r, 10, actions);
    }
    m_table->setFixedHeight(44 + qMax(1, int(filteredVehicles.size())) * 52 + 16);

    // alerte maintenance
    QStringList alerts;
    const QDate today = QDate::currentDate();
    for (const Vehicle &v : m_vehicles)
        if (v.nextMaint.isValid() && v.nextMaint <= today)
            alerts << QString("%1 (état %2, prochaine maintenance le %3)")
                          .arg(v.id, v.etat.toLower(), fmt(v.nextMaint));
    m_alert->setVisible(!alerts.isEmpty());
    m_alert->setText("Maintenance requise : " + alerts.join(" ; ") + ".");

    // statistiques
    auto stats = [this](VehicleDonutWidget *d, QLabel *lg, const QStringList &keys, const QVector<QColor> &cols,
                        bool etat) {
        QVector<VehicleSlice> slices;
        const int total = m_vehicles.size();
        QString html = "<table cellspacing='4'>";
        for (int i = 0; i < keys.size(); ++i) {
            int n = 0;
            for (const Vehicle &v : m_vehicles) n += ((etat ? v.etat : v.dispo) == keys[i]);
            slices.push_back({keys[i], n, cols[i]});
            const int pct = total ? qRound(100.0 * n / total) : 0;
            html += QString("<tr><td><span style='color:%1'>●</span></td><td>%2 <b>%3</b> (%4%)</td></tr>")
                        .arg(cols[i].name(), keys[i]).arg(n).arg(pct);
        }
        d->setSlices(slices);
        lg->setText(html + "</table>");
    };
    stats(m_donutEtat, m_legendEtat, {"Bon", "À surveiller", "Hors service"}, {GREEN, ORANGE, RED}, true);
    stats(m_donutDispo, m_legendDispo, {"Disponible", "En intervention", "En maintenance"},
          {GREEN, RED, ORANGE}, false);
}

// ------------------------------------------------------------------ actions
void VehiclesPage::clearForm()
{
    m_editId.clear();
    m_submit->setText("Ajouter");
    for (QLineEdit *e : {m_plate, m_km, m_last, m_next, m_ins, m_tech}) e->clear();
    m_type->setCurrentIndex(0);
    m_etat->setCurrentIndex(0);
    m_dispo->setCurrentIndex(0);
}

void VehiclesPage::onSubmit()
{
    Vehicle v;
    v.plate = m_plate->text().trimmed();
    if (v.plate.isEmpty()) {
        QMessageBox::warning(this, "Champ manquant", "L'immatriculation est obligatoire.");
        return;
    }
    QString kmTxt = m_km->text();
    kmTxt.remove(QRegularExpression("[\\s\\x{202F}\\x{00A0}]"));
    bool ok = false;
    v.km = kmTxt.toInt(&ok);
    if (!ok || v.km < 0) {
        QMessageBox::warning(this, "Kilométrage invalide", "Saisissez un kilométrage numérique (ex. 42 300).");
        return;
    }

    struct F { QLineEdit *e; QDate *d; const char *name; };
    const F fields[] = {{m_last, &v.lastMaint, "Dernière maintenance"},
                        {m_next, &v.nextMaint, "Prochaine maintenance"},
                        {m_ins, &v.insurance, "Assurance"},
                        {m_tech, &v.tech, "Prochaine visite technique"}};
    for (const F &f : fields) {
        if (f.e->text().trimmed().isEmpty()) continue;
        *f.d = parseDate(f.e->text());
        if (!f.d->isValid()) {
            QMessageBox::warning(this, "Date invalide",
                                 QString("« %1 » : utilisez le format jj/mm/aaaa.").arg(f.name));
            return;
        }
    }
    v.type = m_type->currentText();
    v.etat = m_etat->currentText();
    v.dispo = m_dispo->currentText();

    if (m_editId.isEmpty()) {
        v.id = nextId();
        m_vehicles.push_back(v);
    } else {
        v.id = m_editId;
        for (Vehicle &x : m_vehicles)
            if (x.id == m_editId) x = v;
    }
    save();
    clearForm();
    refresh();
}

void VehiclesPage::viewVehicle(const QString &id)
{
    for (const Vehicle &v : m_vehicles) {
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

void VehiclesPage::editVehicle(const QString &id)
{
    for (const Vehicle &v : m_vehicles) {
        if (v.id != id) continue;
        m_editId = id;
        m_plate->setText(v.plate);
        m_type->setCurrentText(v.type);
        m_km->setText(QString::number(v.km));
        m_etat->setCurrentText(v.etat);
        m_dispo->setCurrentText(v.dispo);
        m_last->setText(v.lastMaint.isValid() ? v.lastMaint.toString(DATE_FMT) : "");
        m_next->setText(v.nextMaint.isValid() ? v.nextMaint.toString(DATE_FMT) : "");
        m_ins->setText(v.insurance.isValid() ? v.insurance.toString(DATE_FMT) : "");
        m_tech->setText(v.tech.isValid() ? v.tech.toString(DATE_FMT) : "");
        m_submit->setText("Enregistrer");
        return;
    }
}

void VehiclesPage::deleteVehicle(const QString &id)
{
    if (QMessageBox::question(this, "Supprimer", "Supprimer le véhicule " + id + " ?") != QMessageBox::Yes)
        return;
    for (int i = 0; i < m_vehicles.size(); ++i)
        if (m_vehicles[i].id == id) { m_vehicles.removeAt(i); break; }
    if (m_editId == id) clearForm();
    save();
    refresh();
}

// ------------------------------------------------------------------ exports
void VehiclesPage::exportPdf()
{
    const QString path = QFileDialog::getSaveFileName(this, "Exporter en PDF", "vehicules.pdf", "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QString html = "<h2 style='color:#d90000'>A.X.I.S : Liste des véhicules</h2>"
                   "<table border='1' cellspacing='0' cellpadding='4' width='100%'>"
                   "<tr bgcolor='#eeeeee'><th>ID</th><th>Immatriculation</th><th>Type</th><th>Km</th>"
                   "<th>État</th><th>Disponibilité</th><th>Dern. maint.</th><th>Proch. maint.</th>"
                   "<th>Assurance</th><th>Visite tech.</th></tr>";
    for (const Vehicle &v : m_vehicles)
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

void VehiclesPage::exportExcel()
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
    for (const Vehicle &v : m_vehicles)
        out << v.id << ';' << v.plate << ';' << v.type << ';' << v.km << ';' << v.etat << ';' << v.dispo
            << ';' << fmt(v.lastMaint) << ';' << fmt(v.nextMaint) << ';' << fmt(v.insurance) << ';'
            << fmt(v.tech) << '\n';
    QMessageBox::information(this, "Export", "Fichier créé (ouvrable dans Excel) :\n" + path);
}
