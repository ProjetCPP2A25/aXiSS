#include "interventionpage.h"
#include "ui_interventionpage.h"

#include "interventionform.h"
#include "interventiontable.h"
#include "statspanel.h"
#include "riskzones.h"
#include "mapview.h"
#include "intervention.h"
#include "interventiondialogs.h"
#include "helpers.h"
#include "xlsxwriter.h"

#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>

namespace {
constexpr int kActualisationMs = 5 * 60 * 1000;     // vérification automatique toutes les 5 minutes
}

InterventionPage::InterventionPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InterventionPage)
{
    ui->setupUi(this);

    m_form = new InterventionForm(ui, this);
    m_table = new InterventionTable(ui, this);
    m_stats = new StatsPanel(ui);
    m_zones = new RiskZones(ui, this);
    m_map = new MapView(ui, this);

    // Formulaire
    connect(m_form, &InterventionForm::saveRequested,
            this, &InterventionPage::onSave);

    connect(m_form, &InterventionForm::editCancelled,
            this,
            [this] { ui->statusbar->showMessage(QStringLiteral("Modification annulée."), 3000); });

    connect(m_table, &InterventionTable::noMatch,
            this,
            [this] {
                ui->statusbar->showMessage(
                    QStringLiteral("Aucune intervention ne correspond à ces coordonnées GPS."), 3000);
            });

    // Tableau
    connect(m_table, &InterventionTable::viewRequested,
            this, &InterventionPage::showDetails);

    connect(m_table, &InterventionTable::editRequested,
            this, &InterventionPage::startEdit);

    connect(m_table, &InterventionTable::deleteRequested,
            this, &InterventionPage::deleteIntervention);

    // Carte
    connect(m_map, &MapView::interventionClicked,
            this, &InterventionPage::showDetails);

    // Export
    connect(ui->btnExporterInterventions,
            &QPushButton::clicked,
            this,
            &InterventionPage::onExport);

    // Barre latérale
    connect(ui->btnMenuPrincipal, &QPushButton::clicked, this, &InterventionPage::menuRequested);
    connect(ui->btnDeconnexion, &QPushButton::clicked, this, &InterventionPage::logoutRequested);

    // Actualisation automatique
    m_timer = new QTimer(this);

    connect(m_timer, &QTimer::timeout,
            this,
            [this] {
                refreshAll(false);
            });

    m_timer->start(kActualisationMs);

    refreshAll(true);
}

InterventionPage::~InterventionPage()
{
    delete ui;
}

void InterventionPage::refreshAll(bool userAction)
{
    QString error = m_table->refresh();
    if (error.isEmpty())
        error = m_stats->refresh();
    if (error.isEmpty())
        error = m_zones->refresh();
    if (error.isEmpty())
        error = m_map->refresh();
    reportError(error, userAction);
}

// Une erreur de la base est affichée dans une boîte de dialogue pour une action de l'utilisateur ;
// pendant l'actualisation automatique, elle n'apparaît qu'une fois (pas de rafale de messages).
void InterventionPage::reportError(const QString &error, bool userAction)
{
    if (error.isEmpty()) {
        m_lastRefreshFailed = false;
        return;
    }
    if (userAction || !m_lastRefreshFailed)
        Dialogs::showError(this, QStringLiteral("Erreur de la base de données"), error);
    else
        ui->statusbar->showMessage(QStringLiteral("Base de données indisponible : actualisation impossible."), 4000);
    m_lastRefreshFailed = true;
}

void InterventionPage::onSave()
{
    Intervention it;
    QString error;
    if (!m_form->read(it, error)) {
        Dialogs::showWarning(this, QStringLiteral("Informations incomplètes"), error);
        return;
    }

    if (m_form->isEditing()) {
        it.id = m_form->editId();
        if (!it.modifier(&error)) {
            Dialogs::showError(this, QStringLiteral("Modification impossible"), error);
            return;
        }
        ui->statusbar->showMessage(QStringLiteral("Intervention %1 modifiée.").arg(it.idText()), 4000);
        m_form->endEdit();
    } else {
        if (!it.ajouter(&error)) {
            Dialogs::showError(this, QStringLiteral("Ajout impossible"), error);
            return;
        }
        ui->statusbar->showMessage(QStringLiteral("Intervention %1 ajoutée.").arg(it.idText()), 4000);
        m_form->clear();
    }
    refreshAll(true);
}

void InterventionPage::showDetails(int id)
{
    Intervention it;
    QString error;
    if (!Intervention::afficher(id, it, &error)) {
        Dialogs::showError(this, QStringLiteral("Affichage impossible"), error);
        refreshAll(true);
        return;
    }
    DetailsDialog dlg(it, this);
    if (dlg.exec() == QDialog::Accepted)
        startEdit(id);
}

void InterventionPage::startEdit(int id)
{
    Intervention it;
    QString error;
    if (!Intervention::afficher(id, it, &error)) {
        Dialogs::showError(this, QStringLiteral("Modification impossible"), error);
        refreshAll(true);
        return;
    }
    m_form->startEdit(it);
}

void InterventionPage::deleteIntervention(int id)
{
    Intervention it;
    QString error;
    if (!Intervention::afficher(id, it, &error)) {
        Dialogs::showError(this, QStringLiteral("Suppression impossible"), error);
        refreshAll(true);
        return;
    }
    // Confirmation obligatoire avant toute suppression
    if (!Dialogs::confirmDelete(this, it))
        return;

    if (!Intervention::supprimer(id, &error)) {
        Dialogs::showError(this, QStringLiteral("Suppression impossible"), error);
        return;
    }
    if (m_form->isEditing() && m_form->editId() == id)
        m_form->endEdit();
    refreshAll(true);
    ui->statusbar->showMessage(QStringLiteral("Intervention %1 supprimée.").arg(it.idText()), 4000);
}

void InterventionPage::onExport()
{
    // Toutes les interventions correspondant à la recherche et au tri (pas seulement la page affichée)
    QString error;
    const QVector<Intervention> rows = Intervention::lister(m_table->filtre(), 0, 0, &error);
    if (!error.isEmpty()) {
        Dialogs::showError(this, QStringLiteral("Export impossible"), error);
        return;
    }
    if (rows.isEmpty()) {
        Dialogs::showInfo(this, QStringLiteral("Exporter"), QStringLiteral("Aucune intervention à exporter."));
        return;
    }

    const QString defaultName = QDir::homePath() + QStringLiteral("/interventions_")
                                + QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))
                                + QStringLiteral(".xlsx");
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Exporter vers Excel"), defaultName,
                                                QStringLiteral("Classeur Excel (*.xlsx)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive))
        path += QStringLiteral(".xlsx");

    const QStringList header = {
        QStringLiteral("ID"), QStringLiteral("Type"), QStringLiteral("Lieu"), QStringLiteral("Gravité"),
        QStringLiteral("Coordonnées GPS"), QStringLiteral("Réception alerte"), QStringLiteral("Fin"),
        QStringLiteral("Durée"), QStringLiteral("État")};

    QVector<QStringList> data;
    for (const Intervention &i : rows) {
        data.push_back({i.idText(), i.type, i.lieu, i.gravite, i.gps(),
                        Helpers::formatDate(i.reception), Helpers::formatDate(i.fin),
                        i.dureeTexte(), i.etat});
    }

    if (!Xlsx::write(path, QStringLiteral("Interventions"), header, data,
                     {8, 14, 20, 12, 22, 20, 20, 12, 12}, &error)) {
        Dialogs::showError(this, QStringLiteral("Export impossible"),
                           QStringLiteral("Le fichier n'a pas pu être écrit :\n%1")
                               .arg(error.isEmpty() ? QStringLiteral("Erreur inconnue.") : error));
        return;
    }
    ui->statusbar->showMessage(
        QStringLiteral("%1 intervention(s) exportée(s) : %2").arg(rows.size()).arg(path), 6000);
}