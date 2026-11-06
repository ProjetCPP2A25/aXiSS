#include "gaxis.h"
#include "./ui_gaxis.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Pages du QStackedWidget (même ordre que la barre latérale)
enum Page { PageEmployes, PageInterventions, PageVehicules, PageMateriels };

constexpr int kActualisationMs = 5 * 60 * 1000;     // vérification automatique toutes les 5 minutes

// Styles des boutons de la barre latérale (module affiché / autres modules)
const QString kNavActive = QStringLiteral(
    "QPushButton { background-color: #FF1717; color: white; border: none; border-radius: 7px;"
    "  text-align: left; padding-left: 12px; font-size: 14px; font-weight: bold; }");
const QString kNavInactive = QStringLiteral(
    "QPushButton { background-color: transparent; color: #E5E7EB; border: none;"
    "  text-align: left; padding-left: 12px; font-size: 14px; }"
    "QPushButton:hover { background-color: #30363C; border-radius: 7px; }");

// Colonnes du tableau
enum Col { ColId, ColType, ColLieu, ColGravite, ColGps, ColReception, ColFin, ColDuree, ColEtat, ColActions,
           ColumnCount };

// ---------------------------------------------------------------------------
// Fonctions utilitaires d'affichage
// ---------------------------------------------------------------------------

const char *const kDateFormat = "dd/MM/yyyy HH:mm";

const QString kDialogStyle = QStringLiteral(
    "QDialog, QMessageBox { background-color: #202428; }"
    "QLabel { color: #E5E7EB; background: transparent; font-size: 13px; border: none; }"
    "QPushButton { background-color: #181B1F; color: white; border: 1px solid #343A40;"
    "  border-radius: 5px; padding: 7px 18px; min-width: 70px; font-size: 13px; }"
    "QPushButton:hover { background-color: #30353A; }"
    "QPushButton:default { background-color: #FF1717; border: none; font-weight: bold; }"
    "QPushButton:default:hover { background-color: #E31313; }");

struct PillColors {
    const char *bg;
    const char *fg;
};

// "36.8065, 10.1815" (virgule ou point-virgule comme séparateur)
bool parseGps(const QString &text, double &lat, double &lon)
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*(-?\\d+(?:\\.\\d+)?)\\s*[,;]\\s*(-?\\d+(?:\\.\\d+)?)\\s*$"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return false;
    lat = m.captured(1).toDouble();
    lon = m.captured(2).toDouble();
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

QString formatDate(const QDateTime &d)          // "-" si la date est invalide
{
    return d.isValid() ? d.toString(QLatin1String(kDateFormat)) : QStringLiteral("-");
}

QString formatDateTableau(const QDateTime &d)   // "30/09/26" puis "14:32" sur deux lignes (tableau compact)
{
    return d.isValid() ? d.toString(QStringLiteral("dd/MM/yy\nHH:mm")) : QStringLiteral("-");
}

PillColors graviteColors(const QString &gravite)
{
    if (gravite == Grav::Elevee)  return {"#F7D6D6", "#A61B1B"};
    if (gravite == Grav::Moyenne) return {"#FCE8CC", "#9A5A00"};
    return {"#DDF3E4", "#1B7A3E"};
}

PillColors etatColors(const QString &etat)
{
    return etat == Etat::EnCours ? PillColors{"#FCE8CC", "#9A5A00"} : PillColors{"#DDF3E4", "#1B7A3E"};
}

QLabel *makePillLabel(const QString &text, const PillColors &c, QWidget *parent = nullptr)
{
    auto *l = new QLabel(text, parent);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(QStringLiteral(
        "background-color: %1; color: %2; border: none; border-radius: 9px;"
        "padding: 2px 8px; font-size: 11px;").arg(QLatin1String(c.bg), QLatin1String(c.fg)));
    return l;
}

QWidget *makePillCell(const QString &text, const PillColors &c)   // pastille centrée dans une cellule
{
    auto *box = new QWidget;
    box->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *lay = new QHBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(makePillLabel(text, c), 0, Qt::AlignLeft | Qt::AlignVCenter);
    return box;
}

QToolButton *makeActionButton(const QString &glyph, const QString &tip, const QString &hover)
{
    auto *b = new QToolButton;
    b->setText(glyph);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(26, 26);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: #181B1F; color: white; border: 1px solid #343A40;"
        "  border-radius: 5px; font-size: 12px; }"
        "QToolButton:hover { background-color: %1; border: 1px solid %1; }").arg(hover));
    return b;
}

// ---------------------------------------------------------------------------
// Boîtes de dialogue
// ---------------------------------------------------------------------------

void showBox(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text)
{
    QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
    box.setStyleSheet(kDialogStyle);
    box.exec();
}

void showWarning(QWidget *parent, const QString &title, const QString &text)
{
    showBox(parent, QMessageBox::Warning, title, text);
}

void showError(QWidget *parent, const QString &title, const QString &text)
{
    showBox(parent, QMessageBox::Critical, title, text);
}

// Confirmation obligatoire avant toute suppression
bool confirmDelete(QWidget *parent, const Intervention &it)
{
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Supprimer l'intervention"),
                    QStringLiteral("Supprimer l'intervention %1 (%2 – %3) ?\nCette action est définitive.")
                        .arg(it.idText(), it.type, it.lieu),
                    QMessageBox::NoButton, parent);
    auto *yes = box.addButton(QStringLiteral("Supprimer"), QMessageBox::AcceptRole);
    auto *no = box.addButton(QStringLiteral("Annuler"), QMessageBox::RejectRole);
    box.setDefaultButton(no);
    box.setStyleSheet(kDialogStyle);
    box.exec();
    return box.clickedButton() == yes;
}

// Détails d'une intervention ; renvoie true si l'utilisateur a cliqué sur "Modifier"
bool showDetailsDialog(QWidget *parent, const Intervention &it)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Intervention %1").arg(it.idText()));
    dlg.setStyleSheet(kDialogStyle);
    dlg.setMinimumWidth(380);

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("Intervention %1").arg(it.idText()));
    title->setStyleSheet(QStringLiteral("color: #FF1717; font-size: 18px; font-weight: bold;"
                                        "background: transparent; border: none;"));
    root->addWidget(title);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(8);
    auto addRow = [form](const QString &k, QWidget *v) {
        auto *key = new QLabel(k);
        key->setStyleSheet(QStringLiteral("color: #AAB0B6; background: transparent; border: none;"));
        form->addRow(key, v);
    };
    auto text = [](const QString &s) {
        auto *l = new QLabel(s);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };
    addRow(QStringLiteral("Type"), text(it.type));
    addRow(QStringLiteral("Lieu"), text(it.lieu));
    addRow(QStringLiteral("Gravité"), makePillLabel(it.gravite, graviteColors(it.gravite)));
    addRow(QStringLiteral("Coordonnées GPS"), text(it.gps()));
    addRow(QStringLiteral("Réception de l'alerte"), text(formatDate(it.reception)));
    addRow(QStringLiteral("Fin"), text(formatDate(it.fin)));
    addRow(QStringLiteral("Durée"), text(it.dureeTexte()));
    addRow(QStringLiteral("État"), makePillLabel(it.etat, etatColors(it.etat)));
    root->addLayout(form);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *bClose = new QPushButton(QStringLiteral("Fermer"));
    auto *bEdit = new QPushButton(QStringLiteral("Modifier"));
    bEdit->setDefault(true);
    buttons->addWidget(bClose);
    buttons->addWidget(bEdit);
    root->addLayout(buttons);
    QObject::connect(bClose, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(bEdit, &QPushButton::clicked, &dlg, &QDialog::accept);

    return dlg.exec() == QDialog::Accepted;
}

} // namespace

GAxis::GAxis(const QString &role, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GAxis)
{
    ui->setupUi(this);

    setupForm();
    setupTable();

    // Barre latérale : un bouton par module
    connect(ui->btnEmployes, &QPushButton::clicked, this, [this] { showModule(PageEmployes); });
    connect(ui->btnInterventions, &QPushButton::clicked, this, [this] { showModule(PageInterventions); });
    connect(ui->btnVehicules, &QPushButton::clicked, this, [this] { showModule(PageVehicules); });
    connect(ui->btnMateriels, &QPushButton::clicked, this, [this] { showModule(PageMateriels); });

    // Contrôle d'accès : seuls l'opérateur de dispatch et le chef de caserne gèrent les interventions
    const bool allowed = role.isEmpty() || Intervention::roleAutorise(role);
    if (!allowed) {
        ui->btnInterventions->setEnabled(false);
        showModule(PageEmployes);
        statusBar()->showMessage(
            QStringLiteral("Accès refusé : le rôle « %1 » ne permet pas de gérer les interventions.").arg(role));
        m_timer = nullptr;
        return;
    }
    showModule(PageInterventions);

    // Actualisation automatique en arrière-plan : durées des interventions en cours
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] { refreshAll(false); });
    m_timer->start(kActualisationMs);

    refreshAll(true);
}

GAxis::~GAxis()
{
    delete ui;
}

void GAxis::showModule(int page)
{
    ui->stackedWidgetInterventions->setCurrentIndex(page);

    // Seul le bouton du module affiché est rouge
    const QList<QPushButton *> buttons = {ui->btnEmployes, ui->btnInterventions, ui->btnVehicules, ui->btnMateriels};
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setStyleSheet(i == page ? kNavActive : kNavInactive);

    if (page == PageInterventions) {
        ui->scrollArea->verticalScrollBar()->setValue(0);
        statusBar()->showMessage(QStringLiteral("Gestion des interventions"), 2500);
    }
}

void GAxis::refreshAll(bool userAction)
{
    reportError(refreshTable(), userAction);
}

// Une erreur de la base est affichée dans une boîte de dialogue pour une action de l'utilisateur ;
// pendant l'actualisation automatique, elle n'apparaît qu'une fois dans la barre d'état (pas de rafale de messages).
void GAxis::reportError(const QString &error, bool userAction)
{
    if (error.isEmpty()) {
        m_lastRefreshFailed = false;
        return;
    }
    if (userAction || !m_lastRefreshFailed)
        showError(this, QStringLiteral("Erreur de la base de données"), error);
    else
        statusBar()->showMessage(QStringLiteral("Base de données indisponible : actualisation impossible."), 4000);
    m_lastRefreshFailed = true;
}

// ---------------------------------------------------------------------------
// CRUD (appels à la classe Intervention)
// ---------------------------------------------------------------------------

void GAxis::onSave()
{
    Intervention it;
    QString error;
    if (!readForm(it, error)) {
        showWarning(this, QStringLiteral("Informations incomplètes"), error);
        return;
    }

    if (isEditing()) {
        it.id = m_editId;
        if (!it.modifier(&error)) {
            showError(this, QStringLiteral("Modification impossible"), error);
            return;
        }
        statusBar()->showMessage(QStringLiteral("Intervention %1 modifiée.").arg(it.idText()), 4000);
        endFormEdit();
    } else {
        if (!it.ajouter(&error)) {
            showError(this, QStringLiteral("Ajout impossible"), error);
            return;
        }
        statusBar()->showMessage(QStringLiteral("Intervention %1 ajoutée.").arg(it.idText()), 4000);
        clearForm();
    }
    refreshAll(true);
}

void GAxis::showDetails(int id)
{
    Intervention it;
    QString error;
    if (!Intervention::afficher(id, it, &error)) {
        showError(this, QStringLiteral("Affichage impossible"), error);
        refreshAll(true);
        return;
    }
    if (showDetailsDialog(this, it))
        startEdit(id);
}

void GAxis::startEdit(int id)
{
    Intervention it;
    QString error;
    if (!Intervention::afficher(id, it, &error)) {
        showError(this, QStringLiteral("Modification impossible"), error);
        refreshAll(true);
        return;
    }
    startFormEdit(it);
}

void GAxis::deleteIntervention(int id)
{
    Intervention it;
    QString error;
    if (!Intervention::afficher(id, it, &error)) {
        showError(this, QStringLiteral("Suppression impossible"), error);
        refreshAll(true);
        return;
    }
    if (!confirmDelete(this, it))
        return;

    if (!Intervention::supprimer(id, &error)) {
        showError(this, QStringLiteral("Suppression impossible"), error);
        return;
    }
    if (isEditing() && m_editId == id)
        endFormEdit();
    refreshAll(true);
    statusBar()->showMessage(QStringLiteral("Intervention %1 supprimée.").arg(it.idText()), 4000);
}

// ---------------------------------------------------------------------------
// Formulaire "Ajouter" / "Modifier"
// ---------------------------------------------------------------------------

void GAxis::setupForm()
{
    ui->dateReceptionIntervention->setDisplayFormat(QLatin1String(kDateFormat));
    ui->dateFinIntervention->setDisplayFormat(QLatin1String(kDateFormat));
    ui->dateReceptionIntervention->setCalendarPopup(true);
    ui->dateFinIntervention->setCalendarPopup(true);
    ui->dateFinIntervention->setStyleSheet(ui->dateFinIntervention->styleSheet()
                               + QStringLiteral("\nQDateTimeEdit:disabled { color: #5E656C; }"));

    connect(ui->btnAjouterIntervention, &QPushButton::clicked, this, &GAxis::onSave);
    connect(ui->btnViderIntervention, &QPushButton::clicked, this, &GAxis::onClearOrCancel);
    connect(ui->comboEtatIntervention, &QComboBox::currentTextChanged, this, &GAxis::onEtatChanged);
    connect(ui->dateReceptionIntervention, &QDateTimeEdit::dateTimeChanged, this, &GAxis::updateDuree);
    connect(ui->dateFinIntervention, &QDateTimeEdit::dateTimeChanged, this, &GAxis::updateDuree);
    for (QLineEdit *e : {ui->editLieuIntervention, ui->editGPSIntervention})
        connect(e, &QLineEdit::returnPressed, this, &GAxis::onSave);

    clearForm();
}

// La date de fin n'est saisissable que pour une intervention terminée.
void GAxis::onEtatChanged()
{
    ui->dateFinIntervention->setEnabled(ui->comboEtatIntervention->currentText() == Etat::Terminee);
    updateDuree();
}

// La durée n'est jamais saisie : fin - réception (terminée) ou temps écoulé depuis la réception (en cours).
void GAxis::updateDuree()
{
    Intervention it;
    it.reception = ui->dateReceptionIntervention->dateTime();
    if (ui->comboEtatIntervention->currentText() == Etat::Terminee)
        it.fin = ui->dateFinIntervention->dateTime();
    const bool coherent = !it.fin.isValid() || it.fin >= it.reception;
    ui->editDureeIntervention->setText(coherent ? it.dureeTexte() : QString());
}

void GAxis::onClearOrCancel()
{
    if (isEditing()) {
        endFormEdit();
        statusBar()->showMessage(QStringLiteral("Modification annulée."), 3000);
    } else {
        clearForm();
    }
}

void GAxis::clearForm()
{
    ui->comboTypeIntervention->setCurrentIndex(0);
    ui->comboGraviteIntervention->setCurrentIndex(0);
    ui->comboEtatIntervention->setCurrentIndex(0);
    ui->editLieuIntervention->clear();
    ui->editGPSIntervention->clear();
    const QDateTime now = QDateTime::currentDateTime();
    ui->dateReceptionIntervention->setDateTime(now);
    ui->dateFinIntervention->setDateTime(now);
    onEtatChanged();
}

bool GAxis::readForm(Intervention &out, QString &error) const
{
    out.type = ui->comboTypeIntervention->currentText();
    out.gravite = ui->comboGraviteIntervention->currentText();
    out.etat = ui->comboEtatIntervention->currentText();
    out.lieu = ui->editLieuIntervention->text().trimmed();

    if (out.lieu.isEmpty()) {
        error = QStringLiteral("Veuillez saisir le lieu de l'intervention.");
        return false;
    }
    if (!parseGps(ui->editGPSIntervention->text(), out.lat, out.lon)) {
        error = QStringLiteral("Coordonnées GPS invalides.\nFormat attendu : latitude, longitude (ex. 36.8065, 10.1815).");
        return false;
    }

    out.reception = ui->dateReceptionIntervention->dateTime();
    out.fin = out.etat == Etat::Terminee ? ui->dateFinIntervention->dateTime() : QDateTime();

    // Contrôle final (champs obligatoires, cohérence des dates) porté par la classe Intervention
    return out.valider(&error);
}

void GAxis::startFormEdit(const Intervention &it)
{
    m_editId = it.id;

    ui->comboTypeIntervention->setCurrentIndex(std::max(0, ui->comboTypeIntervention->findText(it.type)));
    ui->comboGraviteIntervention->setCurrentIndex(std::max(0, ui->comboGraviteIntervention->findText(it.gravite)));
    ui->comboEtatIntervention->setCurrentIndex(std::max(0, ui->comboEtatIntervention->findText(it.etat)));
    ui->editLieuIntervention->setText(it.lieu);
    ui->editGPSIntervention->setText(it.gps());
    ui->dateReceptionIntervention->setDateTime(it.reception);
    ui->dateFinIntervention->setDateTime(it.fin.isValid() ? it.fin : QDateTime::currentDateTime());
    onEtatChanged();

    ui->labelAjouterIntervention->setGeometry(ui->labelAjouterIntervention->x(), ui->labelAjouterIntervention->y(), 218, ui->labelAjouterIntervention->height());
    ui->labelAjouterIntervention->setText(QStringLiteral("Modifier %1").arg(it.idText()));
    ui->btnAjouterIntervention->setText(QStringLiteral("Modifier"));
    ui->btnViderIntervention->setText(QStringLiteral("Annuler"));
    ui->scrollArea->verticalScrollBar()->setValue(0);
    ui->editLieuIntervention->setFocus();
}

void GAxis::endFormEdit()
{
    m_editId = -1;
    ui->labelAjouterIntervention->setText(QStringLiteral("Ajouter"));
    ui->btnAjouterIntervention->setText(QStringLiteral("Ajouter"));
    ui->btnViderIntervention->setText(QStringLiteral("Vider"));
    clearForm();
}

// ---------------------------------------------------------------------------
// Tableau des interventions (liste complète, boutons Afficher / Modifier / Supprimer)
// ---------------------------------------------------------------------------

void GAxis::setupTable()
{
    QTableWidget *t = ui->tableInterventions;
    t->setColumnCount(ColumnCount);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setFocusPolicy(Qt::NoFocus);
    t->setWordWrap(false);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(46);

    // Largeurs prévues pour une fenêtre de 1366 px de large (dates sur deux lignes pour gagner de la place)
    QHeaderView *h = t->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Fixed);
    h->setStretchLastSection(true);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setHighlightSections(false);
    const int widths[ColumnCount - 1] = {46, 80, 84, 74, 124, 78, 78, 64, 78};
    for (int c = 0; c < ColumnCount - 1; ++c)
        t->setColumnWidth(c, widths[c]);

    connect(t, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto *it = ui->tableInterventions->item(row, ColId))
            showDetails(it->data(Qt::UserRole).toInt());
    });

    // Recherche, tri et pagination (métiers innovants) : pas encore actifs
    ui->editRechercheIntervention->setClearButtonEnabled(true);
    ui->comboTriIntervention->clear();
    ui->comboTriIntervention->addItems({
        QStringLiteral("Aucun tri"),
        QStringLiteral("Gravité : élevée → faible"),
        QStringLiteral("Gravité : faible → élevée")});
    ui->btnPagePrecedente->setEnabled(false);
    ui->btnPageSuivante->setEnabled(false);
}

QString GAxis::refreshTable()
{
    QTableWidget *t = ui->tableInterventions;

    QString error;
    const QVector<Intervention> rows = Intervention::lister(&error);
    if (!error.isEmpty())
        return error;
    const int total = int(rows.size());

    ui->labelPaginationIntervention->setText(QStringLiteral("%1 intervention%2").arg(total).arg(total > 1 ? "s" : ""));

    t->clearContents();
    t->setRowCount(rows.size());

    auto makeItem = [](const QString &s, const QString &tip) {
        auto *item = new QTableWidgetItem(s);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setToolTip(tip);
        return item;
    };

    for (int r = 0; r < rows.size(); ++r) {
        const Intervention &i = rows[r];
        t->setRowHeight(r, 46);

        auto *idItem = makeItem(i.idText(), i.idText());
        idItem->setData(Qt::UserRole, i.id);
        t->setItem(r, ColId, idItem);
        t->setItem(r, ColType, makeItem(i.type, i.type));
        t->setItem(r, ColLieu, makeItem(i.lieu, i.lieu));
        t->setItem(r, ColGravite, makeItem(QString(), i.gravite));
        t->setItem(r, ColGps, makeItem(i.gps(), i.gps()));
        t->setItem(r, ColReception, makeItem(formatDateTableau(i.reception), formatDate(i.reception)));
        t->setItem(r, ColFin, makeItem(formatDateTableau(i.fin), formatDate(i.fin)));
        t->setItem(r, ColDuree, makeItem(i.dureeTexte(), i.dureeTexte()));
        t->setItem(r, ColEtat, makeItem(QString(), i.etat));
        t->setItem(r, ColActions, makeItem(QString(), QString()));

        t->setCellWidget(r, ColGravite, makePillCell(i.gravite, graviteColors(i.gravite)));
        t->setCellWidget(r, ColEtat, makePillCell(i.etat, etatColors(i.etat)));

        // Actions : Afficher / Modifier / Supprimer
        auto *actions = new QWidget;
        actions->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto *lay = new QHBoxLayout(actions);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        auto *bView = makeActionButton(QStringLiteral("👁"), QStringLiteral("Afficher"), QStringLiteral("#30353A"));
        auto *bEdit = makeActionButton(QStringLiteral("✎"), QStringLiteral("Modifier"), QStringLiteral("#30353A"));
        auto *bDel = makeActionButton(QStringLiteral("🗑"), QStringLiteral("Supprimer"), QStringLiteral("#A61B1B"));
        lay->addWidget(bView);
        lay->addWidget(bEdit);
        lay->addWidget(bDel);
        lay->addStretch();
        const int id = i.id;
        connect(bView, &QToolButton::clicked, this, [this, id] { showDetails(id); });
        connect(bEdit, &QToolButton::clicked, this, [this, id] { startEdit(id); });
        connect(bDel, &QToolButton::clicked, this, [this, id] { deleteIntervention(id); });
        t->setCellWidget(r, ColActions, actions);
    }
    return QString();
}
