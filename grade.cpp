#include "grade.h"
#include "ui_grade.h"

// Embedded module headers
#include "interventionpage.h"
#include "vehiclespage.h"
#include "materielpage.h"
#include "sidebar.h"
#include "connexion.h"
#include "database.h"
#include "employe.h"

#include <QButtonGroup>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QStyle>
#include <QFile>
#include <QMessageBox>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QScrollArea>

// ============================================================================
// WIDGET GRAPHIQUE CUSTOM : DIAGRAMME EN BARRES (STATISTIQUES & CHARGE)
// ============================================================================
class GardeBarChartWidget : public QWidget
{
public:
    explicit GardeBarChartWidget(QWidget *parent = nullptr)
        : QWidget(parent), m_barColor("#D9363E")
    {
        setMinimumHeight(240);
    }

    void setData(const QVector<QPair<QString, int>> &data, const QColor &color = QColor("#D9363E"))
    {
        m_data = data;
        m_barColor = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (m_data.isEmpty()) {
            p.setPen(QColor("#94A3B8"));
            p.drawText(rect(), Qt::AlignCenter, "Aucune donnée statistique disponible.");
            return;
        }

        const int leftPad = 20, rightPad = 20, topPad = 40, bottomPad = 50;
        const QRectF area(leftPad, topPad, width() - leftPad - rightPad, height() - topPad - bottomPad);

        int maxVal = 1;
        for (const auto &it : m_data) {
            maxVal = qMax(maxVal, it.second);
        }

        const double slotWidth = area.width() / m_data.size();
        const double barWidth = qMin(65.0, slotWidth * 0.55);

        QFont valFont("Segoe UI", 10, QFont::Bold);
        QFont labelFont("Segoe UI", 9);

        // Ligne de base de l'axe X
        p.setPen(QPen(QColor("#E2E8F0"), 1.5));
        p.drawLine(area.bottomLeft(), area.bottomRight());

        for (int i = 0; i < m_data.size(); ++i) {
            const double h = (maxVal > 0) ? (area.height() * m_data[i].second / maxVal) : 0;
            const double x = area.left() + slotWidth * i + (slotWidth - barWidth) / 2.0;
            const QRectF bar(x, area.bottom() - h, barWidth, h);

            // Dessin de la barre arrondie
            p.setPen(Qt::NoPen);
            p.setBrush(m_barColor);
            p.drawRoundedRect(bar, 4, 4);

            // Valeur au-dessus
            p.setPen(QColor("#142238"));
            p.setFont(valFont);
            p.drawText(QRectF(x - 20, bar.top() - 25, barWidth + 40, 22),
                       Qt::AlignHCenter | Qt::AlignVCenter, QString::number(m_data[i].second));

            // Libellé en dessous
            p.setPen(QColor("#64748B"));
            p.setFont(labelFont);
            QRectF lblRect(area.left() + slotWidth * i, area.bottom() + 6, slotWidth, 38);
            p.drawText(lblRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_data[i].first);
        }
    }

private:
    QVector<QPair<QString, int>> m_data;
    QColor m_barColor;
};

// ============================================================================
// MAINWINDOW IMPLEMENTATION
// ============================================================================
Grade::Grade(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Grade)
    , m_navGroup(nullptr)
    , m_interventionPage(nullptr)
    , m_vehiculePage(nullptr)
    , m_materielPage(nullptr)
    , m_tblEmployes(nullptr)
    , m_searchEmployes(nullptr)
{
    ui->setupUi(this);

    applyTheme();
    setupNavigation();
    initDatabase();
    loadGardesFromDatabase();
    setupIntegratedModules();
}

Grade::~Grade()
{
    delete ui;
}

void Grade::applyTheme()
{
    QFile styleFile(":/style.qss");
    if (!styleFile.exists()) {
        styleFile.setFileName("style.qss");
    }
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }
}

void Grade::setupNavigation()
{
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    m_navGroup->addButton(ui->btnMenuPrincipal, 5);
    m_navGroup->addButton(ui->btnGardes, 0);
    m_navGroup->addButton(ui->btnInterventions, 1);
    m_navGroup->addButton(ui->btnVehicules, 2);
    m_navGroup->addButton(ui->btnMateriels, 3);
    m_navGroup->addButton(ui->btnEmployes, 4);

    ui->btnGardes->setChecked(true);
    ui->contentStack->setCurrentIndex(0);

    // Sidebar navigation buttons
    connect(ui->btnMenuPrincipal, &QPushButton::clicked, this, [this]() { navigateToMenu(5); });
    connect(ui->btnGardes, &QPushButton::clicked, this, [this]() { navigateToMenu(0); });
    connect(ui->btnInterventions, &QPushButton::clicked, this, [this]() { navigateToMenu(1); });
    connect(ui->btnVehicules, &QPushButton::clicked, this, [this]() { navigateToMenu(2); });
    connect(ui->btnMateriels, &QPushButton::clicked, this, [this]() { navigateToMenu(3); });
    connect(ui->btnEmployes, &QPushButton::clicked, this, [this]() { navigateToMenu(4); });

    // Hub menu shortcuts
    connect(ui->btnGoGardes, &QPushButton::clicked, this, [this]() { navigateToMenu(0); });
    connect(ui->btnGoInterv, &QPushButton::clicked, this, [this]() { navigateToMenu(1); });
    connect(ui->btnGoVehic, &QPushButton::clicked, this, [this]() { navigateToMenu(2); });
    connect(ui->btnGoMateriel, &QPushButton::clicked, this, [this]() { navigateToMenu(3); });
    connect(ui->btnGoEmploye, &QPushButton::clicked, this, [this]() { navigateToMenu(4); });

    connect(ui->btnDeconnexion, &QPushButton::clicked, this, &Grade::onLogoutClicked);

    // Search bar synchronisation & filtering
    connect(ui->searchBar, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (ui->filterSearch->text() != t) {
            ui->filterSearch->setText(t);
        }
    });
    connect(ui->filterSearch, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (ui->searchBar->text() != t) {
            ui->searchBar->setText(t);
        }
        onFilterChanged();
    });
    connect(ui->filterCategory, &QComboBox::currentIndexChanged, this, &Grade::onFilterChanged);
    connect(ui->filterStatus, &QComboBox::currentIndexChanged, this, &Grade::onFilterChanged);

    // Tri (Sorting)
    if (ui->comboTri) {
        connect(ui->comboTri, &QComboBox::currentIndexChanged, this, &Grade::onSortChanged);
    }

    // Table selection & double-click
    connect(ui->workshopTable, &QTableWidget::itemSelectionChanged, this, &Grade::onGardeSelectionChanged);
    connect(ui->workshopTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        onViewGardeDetailsClicked();
    });

    // Action buttons in Gardes page
    connect(ui->addButton, &QPushButton::clicked, this, &Grade::onAddNewGardeClicked);
    connect(ui->btnConsulter, &QPushButton::clicked, this, &Grade::onViewGardeDetailsClicked);
    connect(ui->btnModifier, &QPushButton::clicked, this, &Grade::onEditGardeClicked);
    connect(ui->detailsButton, &QPushButton::clicked, this, &Grade::onAssignPompiersClicked);
    connect(ui->btnSupprimer, &QPushButton::clicked, this, &Grade::onDeleteGardeClicked);

    // Action Bar Buttons (Statistiques & Export PDF)
    connect(ui->btnStats, &QPushButton::clicked, this, &Grade::onShowStatisticsClicked);
    connect(ui->btnExportPdf, &QPushButton::clicked, this, &Grade::onExportPdfClicked);
}

void Grade::setupIntegratedModules()
{
    // ===== MODULE INTERVENTIONS =====
    while (QLayoutItem *it = ui->layIntervPage->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    ui->layIntervPage->setContentsMargins(0, 0, 0, 0);
    m_interventionPage = new InterventionPage(ui->pageInterventions);
    ui->layIntervPage->addWidget(m_interventionPage);

    connect(m_interventionPage, &InterventionPage::menuRequested, this, [this]() { navigateToMenu(5); });
    connect(m_interventionPage, &InterventionPage::logoutRequested, this, &Grade::onLogoutClicked);

    // ===== MODULE VÉHICULES =====
    while (QLayoutItem *it = ui->layVehiculePage->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    ui->layVehiculePage->setContentsMargins(0, 0, 0, 0);
    m_vehiculePage = new VehiclesPage(ui->pageVehicules);
    if (auto *sb = m_vehiculePage->findChild<SidebarFrame *>()) {
        sb->hide();
    }
    ui->layVehiculePage->addWidget(m_vehiculePage);

    connect(m_vehiculePage, &VehiclesPage::menuRequested, this, [this]() { navigateToMenu(5); });
    connect(m_vehiculePage, &VehiclesPage::logoutRequested, this, &Grade::onLogoutClicked);

    // ===== MODULE MATÉRIELS =====
    while (QLayoutItem *it = ui->layMaterielPage->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    ui->layMaterielPage->setContentsMargins(0, 0, 0, 0);
    m_materielPage = new MaterielPage(ui->pageMateriels);
    if (auto *sb = m_materielPage->findChild<SidebarFrame *>()) {
        sb->hide();
    }
    ui->layMaterielPage->addWidget(m_materielPage);

    connect(m_materielPage, &MaterielPage::menuRequested, this, [this]() { navigateToMenu(5); });
    connect(m_materielPage, &MaterielPage::logoutRequested, this, &Grade::onLogoutClicked);

    // ===== MODULE EMPLOYÉS =====
    setupEmployesPage();
}

void Grade::setupEmployesPage()
{
    while (QLayoutItem *it = ui->layEmployePage->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    ui->layEmployePage->setContentsMargins(30, 24, 30, 24);
    ui->layEmployePage->setSpacing(14);

    auto *topLayout = new QHBoxLayout();
    auto *titleLbl = new QLabel("Module Gestion des Employés & Effectifs", ui->pageEmployes);
    titleLbl->setStyleSheet("color: #142238; font-size: 20pt; font-weight: bold;");
    topLayout->addWidget(titleLbl);
    topLayout->addStretch();

    auto *btnRetour = new QPushButton("← Retour aux Gardes", ui->pageEmployes);
    btnRetour->setObjectName("secondaryButton");
    btnRetour->setCursor(Qt::PointingHandCursor);
    connect(btnRetour, &QPushButton::clicked, this, [this]() { navigateToMenu(0); });
    topLayout->addWidget(btnRetour);
    ui->layEmployePage->addLayout(topLayout);

    auto *subLbl = new QLabel("Effectifs du centre de secours RedFlame : Officiers, Sapeurs, planning de garde et disponibilités.", ui->pageEmployes);
    subLbl->setStyleSheet("color: #64748B; font-size: 10pt;");
    ui->layEmployePage->addWidget(subLbl);

    m_searchEmployes = new QLineEdit(ui->pageEmployes);
    m_searchEmployes->setPlaceholderText("Rechercher un pompier par nom, grade ou poste...");
    m_searchEmployes->setMinimumHeight(38);
    connect(m_searchEmployes, &QLineEdit::textChanged, this, [this](const QString &text) {
        refreshEmployesTable(text);
    });
    ui->layEmployePage->addWidget(m_searchEmployes);

    m_tblEmployes = new QTableWidget(ui->pageEmployes);
    m_tblEmployes->setColumnCount(8);
    m_tblEmployes->setHorizontalHeaderLabels({"ID", "Nom et prénom", "Grade", "Poste", "Téléphone", "Email", "Heures/sem", "Disponibilité"});
    m_tblEmployes->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblEmployes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tblEmployes->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tblEmployes->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_tblEmployes->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_tblEmployes->verticalHeader()->setVisible(false);
    m_tblEmployes->setAlternatingRowColors(true);
    m_tblEmployes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblEmployes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblEmployes->setShowGrid(true);

    ui->layEmployePage->addWidget(m_tblEmployes, 1);

    refreshEmployesTable();
}

void Grade::refreshEmployesTable(const QString &search)
{
    if (!m_tblEmployes) return;

    QVector<Employe> list = Employe::lister(search, 0);
    m_tblEmployes->setRowCount(list.size());

    for (int r = 0; r < list.size(); ++r) {
        const auto &e = list[r];

        auto *item0 = new QTableWidgetItem(e.id);
        item0->setTextAlignment(Qt::AlignCenter);
        m_tblEmployes->setItem(r, 0, item0);

        auto *item1 = new QTableWidgetItem(e.nom);
        m_tblEmployes->setItem(r, 1, item1);

        auto *item2 = new QTableWidgetItem(e.grade);
        item2->setTextAlignment(Qt::AlignCenter);
        m_tblEmployes->setItem(r, 2, item2);

        auto *item3 = new QTableWidgetItem(e.poste);
        m_tblEmployes->setItem(r, 3, item3);

        auto *item4 = new QTableWidgetItem(e.telephone);
        item4->setTextAlignment(Qt::AlignCenter);
        m_tblEmployes->setItem(r, 4, item4);

        auto *item5 = new QTableWidgetItem(e.email);
        m_tblEmployes->setItem(r, 5, item5);

        auto *item6 = new QTableWidgetItem(QString::number(e.heures) + " h");
        item6->setTextAlignment(Qt::AlignCenter);
        m_tblEmployes->setItem(r, 6, item6);

        QWidget *badgeWidget = new QWidget();
        QHBoxLayout *badgeLayout = new QHBoxLayout(badgeWidget);
        badgeLayout->setContentsMargins(6, 4, 6, 4);
        badgeLayout->setAlignment(Qt::AlignCenter);

        QLabel *badgeLabel = new QLabel(QString("● %1").arg(e.disponibilite));
        if (e.disponibilite == "Disponible") {
            badgeLabel->setObjectName("activeStatus");
        } else if (e.disponibilite == "En intervention") {
            badgeLabel->setObjectName("pendingStatus");
        } else {
            badgeLabel->setObjectName("expiredStatus");
        }
        badgeLabel->setAlignment(Qt::AlignCenter);
        badgeLayout->addWidget(badgeLabel);

        m_tblEmployes->setCellWidget(r, 7, badgeWidget);
    }
}

void Grade::navigateToMenu(int stackIndex)
{
    ui->contentStack->setCurrentIndex(stackIndex);

    switch (stackIndex) {
    case 0:
        ui->btnGardes->setChecked(true);
        break;
    case 1:
        ui->btnInterventions->setChecked(true);
        break;
    case 2:
        ui->btnVehicules->setChecked(true);
        if (m_vehiculePage) m_vehiculePage->reload();
        break;
    case 3:
        ui->btnMateriels->setChecked(true);
        if (m_materielPage) m_materielPage->refresh();
        break;
    case 4:
        ui->btnEmployes->setChecked(true);
        refreshEmployesTable();
        break;
    case 5:
        ui->btnMenuPrincipal->setChecked(true);
        break;
    default:
        break;
    }
}

// ============================================================================
// BASE DE DONNÉES & CHARGEMENT DES GARDES
// ============================================================================
void Grade::initDatabase()
{
    QString connErr;
    Connexion::ouvrir(connErr);

    QString dbErr;
    Database::open(&dbErr);

    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.exec("CREATE TABLE IF NOT EXISTS gardes ("
               "id TEXT PRIMARY KEY, "
               "date TEXT NOT NULL, "
               "type TEXT NOT NULL, "
               "heure_debut TEXT NOT NULL, "
               "heure_fin TEXT NOT NULL, "
               "effectif_requis INTEGER NOT NULL, "
               "chef_garde TEXT NOT NULL, "
               "statut TEXT NOT NULL, "
               "vehicules TEXT, "
               "secteur TEXT, "
               "pompiers_affectes TEXT)");

        // Add column pompiers_affectes if missing from earlier runs
        q.exec("ALTER TABLE gardes ADD COLUMN pompiers_affectes TEXT");

        q.exec("SELECT COUNT(*) FROM gardes");
        if (q.next() && q.value(0).toInt() == 0) {
            struct SeedGarde {
                const char* id;
                const char* date;
                const char* type;
                const char* heureDebut;
                const char* heureFin;
                int effectifRequis;
                const char* chefGarde;
                const char* statut;
                const char* vehicules;
                const char* secteur;
                const char* pompiers;
            };
            const SeedGarde items[] = {
                {"GRD-2026-001", "18/10/2026", "Jour", "07:00", "19:00", 4, "Lt. Karim Majri", "En cours", "FPT-01, VSAV-02", "Secteur Centre (Caserne Principale)", "Sami Gharbi, Rim Trabelsi, Youssef Ben Salah, Leila Mansouri"},
                {"GRD-2026-002", "18/10/2026", "Nuit", "19:00", "07:00", 3, "Cap. Admin AXIS", "Planifiée", "FPT-02, CCFM-01", "Secteur Nord / Banlieue", "Rim Trabelsi, Youssef Ben Salah"},
                {"GRD-2026-003", "19/10/2026", "Jour", "07:00", "19:00", 4, "Lt. Karim Majri", "Planifiée", "EPA-01, FPT-01, VSAV-01", "Secteur Industriel SEVESO", "Sami Gharbi, Leila Mansouri, Admin AXIS"},
                {"GRD-2026-004", "19/10/2026", "Nuit", "19:00", "07:00", 3, "Sgt. Sami Gharbi", "Planifiée", "VSAV-02, CCFM-02", "Secteur Ouest", "Rim Trabelsi, Youssef Ben Salah, Admin AXIS"},
                {"GRD-2026-005", "17/10/2026", "Jour", "07:00", "19:00", 4, "Lt. Karim Majri", "Terminée", "FPT-01, VSAV-01", "Secteur Centre", "Sami Gharbi, Rim Trabelsi, Youssef Ben Salah, Leila Mansouri"},
                {"GRD-2026-006", "17/10/2026", "Nuit", "19:00", "07:00", 3, "Cap. Admin AXIS", "Terminée", "VSAV-02, FPT-02", "Secteur Sud", "Rim Trabelsi, Leila Mansouri, Youssef Ben Salah"},
                {"GRD-2026-007", "16/10/2026", "Nuit", "19:00", "07:00", 3, "Lt. Karim Majri", "Annulée", "FPT-03", "Secteur Est (Intempéries)", "Sami Gharbi, Youssef Ben Salah"}
            };
            for (const auto &it : items) {
                QSqlQuery ins(db);
                ins.prepare("INSERT INTO gardes VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
                ins.addBindValue(it.id);
                ins.addBindValue(it.date);
                ins.addBindValue(it.type);
                ins.addBindValue(it.heureDebut);
                ins.addBindValue(it.heureFin);
                ins.addBindValue(it.effectifRequis);
                ins.addBindValue(it.chefGarde);
                ins.addBindValue(it.statut);
                ins.addBindValue(it.vehicules);
                ins.addBindValue(it.secteur);
                ins.addBindValue(it.pompiers);
                ins.exec();
            }
        }
    }
}

void Grade::loadGardesFromDatabase()
{
    m_gardes.clear();

    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q("SELECT id, date, type, heure_debut, heure_fin, effectif_requis, chef_garde, statut, vehicules, secteur, pompiers_affectes FROM gardes ORDER BY id ASC", db);
        while (q.next()) {
            Garde g;
            g.id = q.value(0).toString();
            g.date = q.value(1).toString();
            g.type = q.value(2).toString();
            g.heureDebut = q.value(3).toString();
            g.heureFin = q.value(4).toString();
            g.effectifRequis = q.value(5).toInt();
            g.chefGarde = q.value(6).toString();
            g.statut = q.value(7).toString();
            g.vehicules = q.value(8).toString();
            g.secteur = q.value(9).toString();

            QString rawPompiers = q.value(10).toString();
            if (!rawPompiers.trimmed().isEmpty()) {
                const auto parts = rawPompiers.split(',');
                for (const QString &p : parts) {
                    if (!p.trimmed().isEmpty()) {
                        g.pompiersAffectes.append(p.trimmed());
                    }
                }
            }

            if (g.statut.contains("cours", Qt::CaseInsensitive)) {
                g.statutStyle = "activeStatus";
            } else if (g.statut.contains("planifi", Qt::CaseInsensitive)) {
                g.statutStyle = "pendingStatus";
            } else if (g.statut.contains("termin", Qt::CaseInsensitive)) {
                g.statutStyle = "completedStatus";
            } else {
                g.statutStyle = "expiredStatus";
            }
            m_gardes.append(g);
        }
    }

    renderTable();
    updateMetrics();

    if (!m_gardes.isEmpty()) {
        ui->workshopTable->selectRow(0);
        onGardeSelectionChanged();
    }
}

void Grade::renderTable()
{
    ui->workshopTable->setRowCount(0);

    // 8 Columns configuration
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->workshopTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    ui->workshopTable->setRowCount(m_gardes.size());

    for (int r = 0; r < m_gardes.size(); ++r) {
        const auto &g = m_gardes[r];

        // 0: ID Garde
        auto *item0 = new QTableWidgetItem(g.id);
        item0->setTextAlignment(Qt::AlignCenter);
        ui->workshopTable->setItem(r, 0, item0);

        // 1: Date
        auto *item1 = new QTableWidgetItem(g.date);
        item1->setTextAlignment(Qt::AlignCenter);
        ui->workshopTable->setItem(r, 1, item1);

        // 2: Type
        QString typeDisplay = (g.type == "Jour") ? "☀️  Jour" : "🌙  Nuit";
        auto *item2 = new QTableWidgetItem(typeDisplay);
        item2->setTextAlignment(Qt::AlignCenter);
        ui->workshopTable->setItem(r, 2, item2);

        // 3: Horaires
        auto *item3 = new QTableWidgetItem(QString("%1 - %2").arg(g.heureDebut, g.heureFin));
        item3->setTextAlignment(Qt::AlignCenter);
        ui->workshopTable->setItem(r, 3, item3);

        // 4: Couverture Effectif (Affecté / Requis)
        QString covText = QString("%1 / %2").arg(g.effectifAffecte()).arg(g.effectifRequis);
        auto *item4 = new QTableWidgetItem(covText);
        item4->setTextAlignment(Qt::AlignCenter);
        if (!g.effectifSuffisant() && g.statut != "Annulée") {
            item4->setForeground(QBrush(QColor("#B45309")));
            item4->setToolTip(QString("Sous-effectif : %1 pompiers requis, seulement %2 affectés.").arg(g.effectifRequis).arg(g.effectifAffecte()));
        } else {
            item4->setForeground(QBrush(QColor("#166534")));
        }
        ui->workshopTable->setItem(r, 4, item4);

        // 5: Chef de Garde
        auto *item5 = new QTableWidgetItem(g.chefGarde);
        item5->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        ui->workshopTable->setItem(r, 5, item5);

        // 6: Pompiers Affectés
        QString teamStr = g.pompiersAffectes.join(", ");
        if (teamStr.isEmpty()) teamStr = "(Aucun pompier affecté)";
        auto *item6 = new QTableWidgetItem(teamStr);
        item6->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item6->setToolTip(teamStr);
        ui->workshopTable->setItem(r, 6, item6);

        // 7: Statut Badge Widget
        QWidget *badgeWidget = new QWidget();
        QHBoxLayout *badgeLayout = new QHBoxLayout(badgeWidget);
        badgeLayout->setContentsMargins(6, 4, 6, 4);
        badgeLayout->setAlignment(Qt::AlignCenter);

        QLabel *badgeLabel = new QLabel(QString("● %1").arg(g.statut));
        badgeLabel->setObjectName(g.statutStyle);
        badgeLabel->setAlignment(Qt::AlignCenter);
        badgeLayout->addWidget(badgeLabel);

        ui->workshopTable->setCellWidget(r, 7, badgeWidget);
    }
}

void Grade::updateMetrics()
{
    int totalGardes = m_gardes.size();
    int enCours = 0;
    int planifiees = 0;
    int effectifMobilise = 0;
    int nonAnnulees = 0;

    for (const auto &g : m_gardes) {
        if (g.statut.contains("cours", Qt::CaseInsensitive)) {
            enCours++;
            effectifMobilise += g.effectifAffecte();
            nonAnnulees++;
        } else if (g.statut.contains("planifi", Qt::CaseInsensitive)) {
            planifiees++;
            effectifMobilise += g.effectifAffecte();
            nonAnnulees++;
        } else if (g.statut.contains("termin", Qt::CaseInsensitive)) {
            nonAnnulees++;
        }
    }

    ui->statValue1->setText(QString::number(totalGardes));
    ui->statValue2->setText(QString::number(enCours));
    ui->statValue3->setText(QString::number(planifiees));
    ui->statValue4->setText(QString::number(effectifMobilise));

    int taux = totalGardes > 0 ? (nonAnnulees * 100 / totalGardes) : 100;
    ui->statValueParticipation->setText(QString("%1%").arg(taux));
}

// ============================================================================
// SÉLECTION, RECHERCHE ET TRI
// ============================================================================
void Grade::onGardeSelectionChanged()
{
    int row = ui->workshopTable->currentRow();
    if (row >= 0 && row < m_gardes.size()) {
        const auto &g = m_gardes[row];

        ui->selectedWorkshopTitle->setText(QString("%1 – Garde de %2 (%3)").arg(g.id, g.type, g.date));
        ui->pendingStatus->setText(QString("● %1").arg(g.statut));
        ui->pendingStatus->setObjectName(g.statutStyle);

        ui->pendingStatus->style()->unpolish(ui->pendingStatus);
        ui->pendingStatus->style()->polish(ui->pendingStatus);

        QString detailText = QString(
            "<b>ID Garde :</b> %1<br><br>"
            "<b>Date :</b> %2<br><br>"
            "<b>Tour de veille :</b> %3 (%4 - %5)<br><br>"
            "<b>Couverture Effectif :</b> %6 / %7 sapeurs (%8)<br><br>"
            "<b>Chef de Garde :</b> %9<br><br>"
            "<b>Véhicules Mobilisés :</b> %10<br><br>"
            "<b>Secteur Couvert :</b> %11<br><br>"
            "<b>Composition de l'équipe :</b><br>%12"
        ).arg(g.id, g.date, g.type, g.heureDebut, g.heureFin,
             QString::number(g.effectifAffecte()),
             QString::number(g.effectifRequis),
             g.effectifSuffisant() ? "<span style='color:#16A34A;'>Complet</span>" : "<span style='color:#DC2626;'>Sous-effectif ⚠️</span>",
             g.chefGarde, g.vehicules, g.secteur,
             g.pompiersAffectes.isEmpty() ? "<i>Aucun pompier affecté</i>" : ("• " + g.pompiersAffectes.join("<br>• ")));

        ui->details->setText(detailText);
    }
}

void Grade::onFilterChanged()
{
    const QString searchText = ui->filterSearch->text().trimmed();
    const QString typeFilter = ui->filterCategory->currentText();
    const QString statusFilter = ui->filterStatus->currentText();

    for (int r = 0; r < ui->workshopTable->rowCount(); ++r) {
        if (r >= m_gardes.size()) break;
        const auto &g = m_gardes[r];

        bool matchSearch = true;
        if (!searchText.isEmpty()) {
            matchSearch = g.id.contains(searchText, Qt::CaseInsensitive)
                       || g.date.contains(searchText, Qt::CaseInsensitive)
                       || g.chefGarde.contains(searchText, Qt::CaseInsensitive)
                       || g.vehicules.contains(searchText, Qt::CaseInsensitive)
                       || g.secteur.contains(searchText, Qt::CaseInsensitive)
                       || g.statut.contains(searchText, Qt::CaseInsensitive);

            if (!matchSearch) {
                for (const QString &p : g.pompiersAffectes) {
                    if (p.contains(searchText, Qt::CaseInsensitive)) {
                        matchSearch = true;
                        break;
                    }
                }
            }
        }

        bool matchType = true;
        if (typeFilter.contains("Jour", Qt::CaseInsensitive)) {
            matchType = (g.type == "Jour");
        } else if (typeFilter.contains("Nuit", Qt::CaseInsensitive)) {
            matchType = (g.type == "Nuit");
        }

        bool matchStatus = true;
        if (statusFilter != "Tous les statuts" && ui->filterStatus->currentIndex() > 0) {
            matchStatus = g.statut.contains(statusFilter, Qt::CaseInsensitive);
        }

        ui->workshopTable->setRowHidden(r, !(matchSearch && matchType && matchStatus));
    }
}

void Grade::onSortChanged(int index)
{
    if (m_gardes.isEmpty()) return;

    switch (index) {
    case 0: // Date chronologique (asc)
        std::sort(m_gardes.begin(), m_gardes.end(), [](const Garde &a, const Garde &b) {
            QDate da = QDate::fromString(a.date, "dd/MM/yyyy");
            QDate db = QDate::fromString(b.date, "dd/MM/yyyy");
            return da < db;
        });
        break;
    case 1: // Date plus récente (desc)
        std::sort(m_gardes.begin(), m_gardes.end(), [](const Garde &a, const Garde &b) {
            QDate da = QDate::fromString(a.date, "dd/MM/yyyy");
            QDate db = QDate::fromString(b.date, "dd/MM/yyyy");
            return da > db;
        });
        break;
    case 2: // Type (Jour d'abord, puis Nuit)
        std::sort(m_gardes.begin(), m_gardes.end(), [](const Garde &a, const Garde &b) {
            if (a.type != b.type) return a.type == "Jour";
            return a.date < b.date;
        });
        break;
    case 3: // Statut
        std::sort(m_gardes.begin(), m_gardes.end(), [](const Garde &a, const Garde &b) {
            return a.statut < b.statut;
        });
        break;
    case 4: // Couverture Effectif (Moins bien couvertes d'abord)
        std::sort(m_gardes.begin(), m_gardes.end(), [](const Garde &a, const Garde &b) {
            double ratioA = a.effectifRequis > 0 ? (double)a.effectifAffecte() / a.effectifRequis : 1.0;
            double ratioB = b.effectifRequis > 0 ? (double)b.effectifAffecte() / b.effectifRequis : 1.0;
            return ratioA < ratioB;
        });
        break;
    default:
        break;
    }

    renderTable();
    if (!m_gardes.isEmpty()) {
        ui->workshopTable->selectRow(0);
        onGardeSelectionChanged();
    }
}

// ============================================================================
// CRUD : AJOUTER & MODIFIER UNE GARDE
// ============================================================================
QStringList Grade::obtenirNomsPompiers() const
{
    QStringList noms;
    QVector<Employe> list = Employe::lister(QString(), 0);
    for (const auto &e : list) {
        noms.append(e.nom);
    }
    return noms;
}

void Grade::onAddNewGardeClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle("A.X.I.S – Planifier une Nouvelle Garde");
    dlg.resize(540, 620);

    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    QString newId = QString("GRD-2026-%1").arg(m_gardes.size() + 1, 3, 10, QChar('0'));
    auto *txtId = new QLineEdit(newId, &dlg);
    txtId->setEnabled(false);

    auto *dteDate = new QDateEdit(QDate::currentDate(), &dlg);
    dteDate->setCalendarPopup(true);
    dteDate->setDisplayFormat("dd/MM/yyyy");

    auto *cmbType = new QComboBox(&dlg);
    cmbType->addItems({"Jour", "Nuit"});

    auto *txtDebut = new QLineEdit("07:00", &dlg);
    auto *txtFin = new QLineEdit("19:00", &dlg);

    connect(cmbType, &QComboBox::currentTextChanged, this, [txtDebut, txtFin](const QString &t) {
        if (t == "Jour") {
            txtDebut->setText("07:00");
            txtFin->setText("19:00");
        } else {
            txtDebut->setText("19:00");
            txtFin->setText("07:00");
        }
    });

    auto *spnEffectif = new QSpinBox(&dlg);
    spnEffectif->setRange(1, 20);
    spnEffectif->setValue(4);
    spnEffectif->setSuffix(" pompiers");

    auto *cmbChef = new QComboBox(&dlg);
    cmbChef->setEditable(true);
    QStringList chefs = {"Lt. Karim Majri", "Cap. Admin AXIS", "Sgt. Sami Gharbi", "Cap. Faouzi Trabelsi"};
    cmbChef->addItems(chefs);

    auto *txtVehicules = new QLineEdit("FPT-01, VSAV-02", &dlg);
    auto *txtSecteur = new QLineEdit("Secteur Centre (Caserne Principale)", &dlg);
    auto *cmbStatut = new QComboBox(&dlg);
    cmbStatut->addItems({"Planifiée", "En cours", "Terminée", "Annulée"});

    form->addRow("ID Garde :", txtId);
    form->addRow("Date de garde :", dteDate);
    form->addRow("Type de veille :", cmbType);
    form->addRow("Heure Début :", txtDebut);
    form->addRow("Heure Fin :", txtFin);
    form->addRow("Effectif Requis :", spnEffectif);
    form->addRow("Chef de Garde :", cmbChef);
    form->addRow("Véhicules Mobilisés :", txtVehicules);
    form->addRow("Secteur d'Intervention :", txtSecteur);
    form->addRow("Statut :", cmbStatut);

    layout->addLayout(form);

    // Section Affectation des Pompiers (Checklist)
    auto *lblTeamTitle = new QLabel("<b>Sélection des Pompiers Affectés à la Garde :</b>", &dlg);
    layout->addWidget(lblTeamTitle);

    auto *listPompiers = new QListWidget(&dlg);
    listPompiers->setMaximumHeight(150);

    QStringList allPompiers = obtenirNomsPompiers();
    if (allPompiers.isEmpty()) {
        allPompiers = {"Sami Gharbi", "Rim Trabelsi", "Karim Majri", "Leila Mansouri", "Admin AXIS", "Youssef Ben Salah"};
    }

    for (const QString &pNom : allPompiers) {
        auto *item = new QListWidgetItem(QString("👨‍🚒  %1").arg(pNom), listPompiers);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    layout->addWidget(listPompiers);

    auto *lblCounter = new QLabel("Effectif sélectionné : 0 / 4 requis", &dlg);
    lblCounter->setStyleSheet("font-weight: bold; color: #B45309;");
    layout->addWidget(lblCounter);

    auto updateCounter = [listPompiers, spnEffectif, lblCounter]() {
        int checked = 0;
        for (int i = 0; i < listPompiers->count(); ++i) {
            if (listPompiers->item(i)->checkState() == Qt::Checked) checked++;
        }
        int req = spnEffectif->value();
        lblCounter->setText(QString("Effectif sélectionné : %1 / %2 requis").arg(checked).arg(req));
        if (checked >= req) {
            lblCounter->setStyleSheet("font-weight: bold; color: #166534;");
        } else {
            lblCounter->setStyleSheet("font-weight: bold; color: #B45309;");
        }
    };

    connect(listPompiers, &QListWidget::itemChanged, this, [updateCounter](QListWidgetItem *) {
        updateCounter();
    });
    connect(spnEffectif, &QSpinBox::valueChanged, this, [updateCounter](int) {
        updateCounter();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        QStringList assigned;
        for (int i = 0; i < listPompiers->count(); ++i) {
            if (listPompiers->item(i)->checkState() == Qt::Checked) {
                assigned.append(allPompiers[i]);
            }
        }

        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("INSERT INTO gardes VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            q.addBindValue(txtId->text());
            q.addBindValue(dteDate->date().toString("dd/MM/yyyy"));
            q.addBindValue(cmbType->currentText());
            q.addBindValue(txtDebut->text().trimmed());
            q.addBindValue(txtFin->text().trimmed());
            q.addBindValue(spnEffectif->value());
            q.addBindValue(cmbChef->currentText().trimmed());
            q.addBindValue(cmbStatut->currentText());
            q.addBindValue(txtVehicules->text().trimmed());
            q.addBindValue(txtSecteur->text().trimmed());
            q.addBindValue(assigned.join(", "));
            q.exec();
        }

        loadGardesFromDatabase();

        for (int r = 0; r < m_gardes.size(); ++r) {
            if (m_gardes[r].id == txtId->text()) {
                ui->workshopTable->selectRow(r);
                onGardeSelectionChanged();
                break;
            }
        }

        QMessageBox::information(this, "Garde Planifiée",
            QString("La garde %1 a été enregistrée avec succès avec %2 sapeur(s) affecté(s).")
            .arg(txtId->text()).arg(assigned.size()));
    }
}

void Grade::onEditGardeClicked()
{
    int row = ui->workshopTable->currentRow();
    if (row < 0 || row >= m_gardes.size()) {
        QMessageBox::warning(this, "Sélection", "Veuillez sélectionner une garde à modifier.");
        return;
    }

    Garde &g = m_gardes[row];

    QDialog dlg(this);
    dlg.setWindowTitle(QString("A.X.I.S – Modifier la Garde (%1)").arg(g.id));
    dlg.resize(540, 620);

    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *txtId = new QLineEdit(g.id, &dlg);
    txtId->setEnabled(false);

    auto *dteDate = new QDateEdit(QDate::fromString(g.date, "dd/MM/yyyy"), &dlg);
    dteDate->setCalendarPopup(true);
    dteDate->setDisplayFormat("dd/MM/yyyy");

    auto *cmbType = new QComboBox(&dlg);
    cmbType->addItems({"Jour", "Nuit"});
    cmbType->setCurrentText(g.type);

    auto *txtDebut = new QLineEdit(g.heureDebut, &dlg);
    auto *txtFin = new QLineEdit(g.heureFin, &dlg);

    connect(cmbType, &QComboBox::currentTextChanged, this, [txtDebut, txtFin](const QString &t) {
        if (t == "Jour") {
            txtDebut->setText("07:00");
            txtFin->setText("19:00");
        } else {
            txtDebut->setText("19:00");
            txtFin->setText("07:00");
        }
    });

    auto *spnEffectif = new QSpinBox(&dlg);
    spnEffectif->setRange(1, 20);
    spnEffectif->setValue(g.effectifRequis);
    spnEffectif->setSuffix(" pompiers");

    auto *cmbChef = new QComboBox(&dlg);
    cmbChef->setEditable(true);
    QStringList chefs = {"Lt. Karim Majri", "Cap. Admin AXIS", "Sgt. Sami Gharbi", "Cap. Faouzi Trabelsi"};
    if (!chefs.contains(g.chefGarde)) chefs.prepend(g.chefGarde);
    cmbChef->addItems(chefs);
    cmbChef->setCurrentText(g.chefGarde);

    auto *txtVehicules = new QLineEdit(g.vehicules, &dlg);
    auto *txtSecteur = new QLineEdit(g.secteur, &dlg);

    auto *cmbStatut = new QComboBox(&dlg);
    cmbStatut->addItems({"Planifiée", "En cours", "Terminée", "Annulée"});
    cmbStatut->setCurrentText(g.statut);

    form->addRow("ID Garde :", txtId);
    form->addRow("Date :", dteDate);
    form->addRow("Type de Garde :", cmbType);
    form->addRow("Heure Début :", txtDebut);
    form->addRow("Heure Fin :", txtFin);
    form->addRow("Effectif Requis :", spnEffectif);
    form->addRow("Chef de Garde :", cmbChef);
    form->addRow("Véhicules :", txtVehicules);
    form->addRow("Secteur :", txtSecteur);
    form->addRow("Statut :", cmbStatut);

    layout->addLayout(form);

    auto *lblTeamTitle = new QLabel("<b>Mettre à jour l'équipe (affecter ou remplacer des pompiers) :</b>", &dlg);
    layout->addWidget(lblTeamTitle);

    auto *listPompiers = new QListWidget(&dlg);
    listPompiers->setMaximumHeight(150);

    QStringList allPompiers = obtenirNomsPompiers();
    if (allPompiers.isEmpty()) {
        allPompiers = {"Sami Gharbi", "Rim Trabelsi", "Karim Majri", "Leila Mansouri", "Admin AXIS", "Youssef Ben Salah"};
    }

    for (const QString &pNom : allPompiers) {
        auto *item = new QListWidgetItem(QString("👨‍🚒  %1").arg(pNom), listPompiers);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool isAssigned = g.pompiersAffectes.contains(pNom, Qt::CaseInsensitive);
        item->setCheckState(isAssigned ? Qt::Checked : Qt::Unchecked);
    }
    layout->addWidget(listPompiers);

    auto *lblCounter = new QLabel(&dlg);
    layout->addWidget(lblCounter);

    auto updateCounter = [listPompiers, spnEffectif, lblCounter]() {
        int checked = 0;
        for (int i = 0; i < listPompiers->count(); ++i) {
            if (listPompiers->item(i)->checkState() == Qt::Checked) checked++;
        }
        int req = spnEffectif->value();
        lblCounter->setText(QString("Effectif sélectionné : %1 / %2 requis").arg(checked).arg(req));
        if (checked >= req) {
            lblCounter->setStyleSheet("font-weight: bold; color: #166534;");
        } else {
            lblCounter->setStyleSheet("font-weight: bold; color: #B45309;");
        }
    };
    updateCounter();

    connect(listPompiers, &QListWidget::itemChanged, this, [updateCounter](QListWidgetItem *) {
        updateCounter();
    });
    connect(spnEffectif, &QSpinBox::valueChanged, this, [updateCounter](int) {
        updateCounter();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        QStringList assigned;
        for (int i = 0; i < listPompiers->count(); ++i) {
            if (listPompiers->item(i)->checkState() == Qt::Checked) {
                assigned.append(allPompiers[i]);
            }
        }

        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("UPDATE gardes SET date=?, type=?, heure_debut=?, heure_fin=?, effectif_requis=?, "
                      "chef_garde=?, statut=?, vehicules=?, secteur=?, pompiers_affectes=? WHERE id=?");
            q.addBindValue(dteDate->date().toString("dd/MM/yyyy"));
            q.addBindValue(cmbType->currentText());
            q.addBindValue(txtDebut->text().trimmed());
            q.addBindValue(txtFin->text().trimmed());
            q.addBindValue(spnEffectif->value());
            q.addBindValue(cmbChef->currentText().trimmed());
            q.addBindValue(cmbStatut->currentText());
            q.addBindValue(txtVehicules->text().trimmed());
            q.addBindValue(txtSecteur->text().trimmed());
            q.addBindValue(assigned.join(", "));
            q.addBindValue(g.id);
            q.exec();
        }

        loadGardesFromDatabase();
        ui->workshopTable->selectRow(row);
        onGardeSelectionChanged();

        QMessageBox::information(this, "Modification Effectuée",
            QString("La garde %1 a été mise à jour avec succès.").arg(g.id));
    }
}

void Grade::onAssignPompiersClicked()
{
    onEditGardeClicked();
}

void Grade::onDeleteGardeClicked()
{
    int row = ui->workshopTable->currentRow();
    if (row < 0 || row >= m_gardes.size()) {
        QMessageBox::warning(this, "Sélection", "Veuillez sélectionner une garde à supprimer.");
        return;
    }

    const auto &g = m_gardes[row];

    auto reply = QMessageBox::question(this, "Confirmation de Suppression",
        QString("Êtes-vous sûr de vouloir supprimer définitivement la garde :\n\n"
                "• ID : %1\n"
                "• Date : %2 (%3)\n"
                "• Chef : %4\n"
                "• Statut : %5\n\n"
                "⚠️ Cette action est irréversible.")
        .arg(g.id, g.date, g.type, g.chefGarde, g.statut),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("DELETE FROM gardes WHERE id=?");
            q.addBindValue(g.id);
            q.exec();
        }

        loadGardesFromDatabase();
        QMessageBox::information(this, "Garde Supprimée", "La garde a été supprimée du planning.");
    }
}

// ============================================================================
// AFFICHER UNE GARDE
// ============================================================================
void Grade::onViewGardeDetailsClicked()
{
    int row = ui->workshopTable->currentRow();
    if (row < 0 || row >= m_gardes.size()) return;

    const auto &g = m_gardes[row];

    QDialog dlg(this);
    dlg.setWindowTitle(QString("Détails Opérationnels – %1 (%2)").arg(g.id, g.date));
    dlg.resize(600, 500);

    auto *layout = new QVBoxLayout(&dlg);

    // Title banner
    auto *topBox = new QFrame(&dlg);
    topBox->setStyleSheet("background-color: #111C2E; border-radius: 8px; padding: 14px;");
    auto *topLay = new QHBoxLayout(topBox);

    auto *lblTitle = new QLabel(QString("<b style='font-size:14pt; color:white;'>🛡️ %1 – Garde de %2</b><br>"
                                        "<span style='color:#94A3B8;'>Date : %3 | Horaires : %4 - %5</span>")
                                .arg(g.id, g.type, g.date, g.heureDebut, g.heureFin), topBox);
    topLay->addWidget(lblTitle);

    auto *lblSt = new QLabel(QString("● %1").arg(g.statut), topBox);
    lblSt->setObjectName(g.statutStyle);
    lblSt->setAlignment(Qt::AlignCenter);
    topLay->addWidget(lblSt);
    layout->addWidget(topBox);

    // Coverage Ratio & Status
    auto *covBox = new QFrame(&dlg);
    covBox->setStyleSheet("background-color: white; border: 1px solid #E2E8F0; border-radius: 8px; padding: 12px;");
    auto *covLay = new QVBoxLayout(covBox);

    int pct = g.effectifRequis > 0 ? (g.effectifAffecte() * 100 / g.effectifRequis) : 100;
    auto *lblCov = new QLabel(QString("<b>Couverture des Effectifs :</b> %1 affecté(s) sur %2 requis (%3%)")
                              .arg(g.effectifAffecte()).arg(g.effectifRequis).arg(pct), covBox);
    covLay->addWidget(lblCov);

    auto *bar = new QProgressBar(covBox);
    bar->setRange(0, 100);
    bar->setValue(qMin(100, pct));
    if (pct < 100) {
        bar->setStyleSheet("QProgressBar::chunk { background-color: #F59E0B; }");
    } else {
        bar->setStyleSheet("QProgressBar::chunk { background-color: #10B981; }");
    }
    covLay->addWidget(bar);
    layout->addWidget(covBox);

    // Team members list
    auto *lblEquipe = new QLabel("<b>👨‍🚒 Composition de l'Équipe Mobilisée :</b>", &dlg);
    layout->addWidget(lblEquipe);

    auto *tblTeam = new QTableWidget(&dlg);
    tblTeam->setColumnCount(2);
    tblTeam->setHorizontalHeaderLabels({"Nom du Sapeur", "Grade & Fonction"});
    tblTeam->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tblTeam->verticalHeader()->setVisible(false);
    tblTeam->setRowCount(g.pompiersAffectes.size());

    for (int i = 0; i < g.pompiersAffectes.size(); ++i) {
        QString p = g.pompiersAffectes[i];
        Employe emp = Employe::trouver(p);
        if (emp.nom.isEmpty()) {
            QSqlQuery qEmp;
            qEmp.prepare("SELECT grade, poste FROM employes WHERE nom=?");
            qEmp.addBindValue(p);
            if (qEmp.exec() && qEmp.next()) {
                emp.grade = qEmp.value(0).toString();
                emp.poste = qEmp.value(1).toString();
            }
        }

        tblTeam->setItem(i, 0, new QTableWidgetItem("👨‍🚒 " + p));
        tblTeam->setItem(i, 1, new QTableWidgetItem(emp.grade.isEmpty() ? "Sapeur de 1ère classe" : (emp.grade + " - " + emp.poste)));
    }
    layout->addWidget(tblTeam, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    layout->addWidget(buttons);

    dlg.exec();
}

// ============================================================================
// STATISTIQUES & CHARGE DE TRAVAIL AVEC DIAGRAMMES
// ============================================================================
QMap<QString, int> Grade::compterGardesParPompier() const
{
    QMap<QString, int> charge;
    for (const auto &g : m_gardes) {
        if (g.statut == "Annulée") continue;
        for (const QString &p : g.pompiersAffectes) {
            charge[p]++;
        }
    }
    return charge;
}

void Grade::onShowStatisticsClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle("A.X.I.S – Statistiques des Gardes & Équité de la Charge");
    dlg.resize(780, 680);

    auto *mainLayout = new QVBoxLayout(&dlg);

    auto *scrollArea = new QScrollArea(&dlg);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollContent = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(scrollContent);
    layout->setSpacing(20);

    // Title
    auto *titleLbl = new QLabel("📊 Analyse Opérationnelle des Gardes & Charge de Travail", scrollContent);
    titleLbl->setStyleSheet("font-size: 16pt; font-weight: bold; color: #142238;");
    layout->addWidget(titleLbl);

    auto *subLbl = new QLabel("Répartition équitable des tours de veille et suivi de charge des sapeurs-pompiers.", scrollContent);
    subLbl->setStyleSheet("color: #64748B; font-size: 10pt;");
    layout->addWidget(subLbl);

    // 1. Diagramme 1 : Gardes par Type (Jour vs Nuit)
    auto *boxType = new QFrame(scrollContent);
    boxType->setStyleSheet("background-color: white; border: 1px solid #E2E8F0; border-radius: 10px; padding: 14px;");
    auto *layType = new QVBoxLayout(boxType);
    auto *lblT1 = new QLabel("<b>Diagramme 1 : Répartition des Gardes par Type (Jour ☀️ vs Nuit 🌙)</b>", boxType);
    layType->addWidget(lblT1);

    int nbJour = 0, nbNuit = 0;
    for (const auto &g : m_gardes) {
        if (g.type == "Jour") nbJour++;
        else nbNuit++;
    }

    auto *chartType = new GardeBarChartWidget(boxType);
    chartType->setData({
        {"Garde de Jour (12h)", nbJour},
        {"Garde de Nuit (12h)", nbNuit}
    }, QColor("#F59E0B"));
    layType->addWidget(chartType);
    layout->addWidget(boxType);

    // 2. Diagramme 2 : Gardes par Statut
    auto *boxStatut = new QFrame(scrollContent);
    boxStatut->setStyleSheet("background-color: white; border: 1px solid #E2E8F0; border-radius: 10px; padding: 14px;");
    auto *layStatut = new QVBoxLayout(boxStatut);
    auto *lblT2 = new QLabel("<b>Diagramme 2 : Répartition des Gardes par Statut</b>", boxStatut);
    layStatut->addWidget(lblT2);

    int nbEnCours = 0, nbPlanifiees = 0, nbTerminees = 0, nbAnnulees = 0;
    for (const auto &g : m_gardes) {
        if (g.statut.contains("cours", Qt::CaseInsensitive)) nbEnCours++;
        else if (g.statut.contains("planifi", Qt::CaseInsensitive)) nbPlanifiees++;
        else if (g.statut.contains("termin", Qt::CaseInsensitive)) nbTerminees++;
        else nbAnnulees++;
    }

    auto *chartStatut = new GardeBarChartWidget(boxStatut);
    chartStatut->setData({
        {"En cours", nbEnCours},
        {"Planifiée", nbPlanifiees},
        {"Terminée", nbTerminees},
        {"Annulée", nbAnnulees}
    }, QColor("#2563EB"));
    layStatut->addWidget(chartStatut);
    layout->addWidget(boxStatut);

    // 3. Diagramme 3 : Répartition de la charge par Pompier (Équité)
    auto *boxPompiers = new QFrame(scrollContent);
    boxPompiers->setStyleSheet("background-color: white; border: 1px solid #E2E8F0; border-radius: 10px; padding: 14px;");
    auto *layPompiers = new QVBoxLayout(boxPompiers);
    auto *lblT3 = new QLabel("<b>Diagramme 3 : Nombre de Gardes Affectées par Pompier (Charge de Travail)</b>", boxPompiers);
    layPompiers->addWidget(lblT3);

    QMap<QString, int> charge = compterGardesParPompier();
    QVector<QPair<QString, int>> dataPompiers;
    for (auto it = charge.begin(); it != charge.end(); ++it) {
        dataPompiers.append({it.key(), it.value()});
    }

    auto *chartPompiers = new GardeBarChartWidget(boxPompiers);
    chartPompiers->setData(dataPompiers, QColor("#D9363E"));
    layPompiers->addWidget(chartPompiers);

    // Tableau de synthèse de la charge
    auto *tblSynthese = new QTableWidget(boxPompiers);
    tblSynthese->setColumnCount(4);
    tblSynthese->setHorizontalHeaderLabels({"Sapeur-Pompier", "Nombre de Gardes", "Volume Horaire", "Diagnostic d'Équité"});
    tblSynthese->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tblSynthese->verticalHeader()->setVisible(false);
    tblSynthese->setRowCount(charge.size());

    int r = 0;
    for (auto it = charge.begin(); it != charge.end(); ++it, ++r) {
        tblSynthese->setItem(r, 0, new QTableWidgetItem("👨‍🚒 " + it.key()));
        tblSynthese->setItem(r, 1, new QTableWidgetItem(QString("%1 garde(s)").arg(it.value())));
        tblSynthese->setItem(r, 2, new QTableWidgetItem(QString("%1 h de garde").arg(it.value() * 12)));

        QString diag = (it.value() >= 4) ? "⚠️ Sollicité (>48h)" : "✅ Charge Équilibrée";
        auto *itDiag = new QTableWidgetItem(diag);
        itDiag->setForeground(QBrush(it.value() >= 4 ? QColor("#B45309") : QColor("#166534")));
        tblSynthese->setItem(r, 3, itDiag);
    }
    tblSynthese->setMinimumHeight(160);
    layPompiers->addWidget(tblSynthese);

    layout->addWidget(boxPompiers);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    auto *btnClose = new QPushButton("Fermer", &dlg);
    btnClose->setObjectName("secondaryButton");
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    mainLayout->addWidget(btnClose, 0, Qt::AlignRight);

    dlg.exec();
}

// ============================================================================
// EXPORT PDF
// ============================================================================
void Grade::onExportPdfClicked()
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/planning_gardes_redflame.pdf";
    QString fileName = QFileDialog::getSaveFileName(this, "Exporter le Planning de Garde en PDF", defaultPath, "Documents PDF (*.pdf)");

    if (fileName.isEmpty()) return;

    QPdfWriter pdfWriter(fileName);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Landscape);
    pdfWriter.setResolution(300);

    // Build formal HTML document for the guard schedule
    QString html = R"(
    <html>
    <head>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; color: #1E293B; }
        .header { background-color: #111C2E; color: white; padding: 18px 24px; border-radius: 8px; margin-bottom: 20px; }
        .title { font-size: 20pt; font-weight: bold; color: #FFFFFF; }
        .subtitle { font-size: 11pt; color: #94A3B8; margin-top: 5px; }
        table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        th { background-color: #F8FAFC; color: #334155; border: 1px solid #CBD5E1; padding: 10px 8px; font-size: 10pt; font-weight: bold; text-align: center; }
        td { border: 1px solid #E2E8F0; padding: 8px; font-size: 9.5pt; text-align: left; }
        .center { text-align: center; }
        .badge-active { background-color: #DCFCE7; color: #166534; font-weight: bold; padding: 3px 6px; border-radius: 4px; }
        .badge-pending { background-color: #FEF3C7; color: #92400E; font-weight: bold; padding: 3px 6px; border-radius: 4px; }
        .badge-completed { background-color: #E2E8F0; color: #334155; font-weight: bold; padding: 3px 6px; border-radius: 4px; }
        .badge-cancel { background-color: #FEE2E2; color: #B91C1C; font-weight: bold; padding: 3px 6px; border-radius: 4px; }
        .footer { margin-top: 30px; font-size: 9pt; color: #64748B; border-top: 1px solid #CBD5E1; padding-top: 10px; }
        .sign { float: right; text-align: center; margin-top: 20px; }
    </style>
    </head>
    <body>
        <div class="header">
            <div class="title">CENTRE DE SECOURS PRINCIPAL REDFLAME</div>
            <div class="subtitle">PLANNING OFFICIEL DES TOURS DE GARDE ET DE VEILLE OPÉRATIONNELLE</div>
        </div>
        <p><b>Date d'édition :</b> )" + QDateTime::currentDateTime().toString("dd/MM/yyyy à HH:mm") + R"( | <b>Édité par :</b> Chef de Centre / Responsable des Gardes</p>
        <table>
            <thead>
                <tr>
                    <th style="width: 11%;">ID Garde</th>
                    <th style="width: 10%;">Date</th>
                    <th style="width: 9%;">Type</th>
                    <th style="width: 12%;">Horaires</th>
                    <th style="width: 9%;">Effectif</th>
                    <th style="width: 14%;">Chef de Garde</th>
                    <th style="width: 25%;">Équipe des Sapeurs-Pompiers</th>
                    <th style="width: 10%;">Statut</th>
                </tr>
            </thead>
            <tbody>
    )";

    for (const auto &g : m_gardes) {
        QString stClass = "badge-pending";
        if (g.statut.contains("cours", Qt::CaseInsensitive)) stClass = "badge-active";
        else if (g.statut.contains("termin", Qt::CaseInsensitive)) stClass = "badge-completed";
        else if (g.statut.contains("annul", Qt::CaseInsensitive)) stClass = "badge-cancel";

        QString pompiersList = g.pompiersAffectes.join(", ");
        if (pompiersList.isEmpty()) pompiersList = "-";

        html += QString(
            "<tr>"
            "<td class='center'><b>%1</b></td>"
            "<td class='center'>%2</td>"
            "<td class='center'>%3</td>"
            "<td class='center'>%4 - %5</td>"
            "<td class='center'>%6 / %7</td>"
            "<td>%8</td>"
            "<td>%9</td>"
            "<td class='center'><span class='%10'>%11</span></td>"
            "</tr>"
        ).arg(g.id, g.date, g.type, g.heureDebut, g.heureFin,
             QString::number(g.effectifAffecte()), QString::number(g.effectifRequis),
             g.chefGarde, pompiersList, stClass, g.statut);
    }

    html += R"(
            </tbody>
        </table>
        <div class="footer">
            <div style="float: left; width: 60%;">
                Document officiel consigné auprès du poste de commandement de la caserne.<br>
                Toute indisponibilité imprévue doit être signalée au Chef de Garde 48h à l'avance.
            </div>
            <div class="sign" style="float: right; width: 35%; text-align: center;">
                <b>Le Chef de Caserne &amp; de Centre</b><br><br><br>
                <i>Cachet officiel &amp; Signature</i>
            </div>
        </div>
    </body>
    </html>
    )";

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&pdfWriter);

    auto res = QMessageBox::question(this, "Export PDF Réussi",
        QString("Le planning de garde a été exporté avec succès vers :\n%1\n\nVoulez-vous l'ouvrir maintenant ?").arg(fileName),
        QMessageBox::Yes | QMessageBox::No);

    if (res == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
    }
}

// ============================================================================
// NAVIGATION & DÉCONNEXION
// ============================================================================
void Grade::onLogoutClicked()
{
    auto res = QMessageBox::question(this, "Déconnexion",
        "Voulez-vous fermer la session Responsable des Gardes et retourner au Menu Principal ?",
        QMessageBox::Yes | QMessageBox::No);

    if (res == QMessageBox::Yes) {
        navigateToMenu(5);
    }
}
