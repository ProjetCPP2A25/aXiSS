#include "interventionform.h"
#include "ui_interventionpage.h"
#include "helpers.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>

#include <algorithm>

InterventionForm::InterventionForm(Ui::InterventionPage *ui_, QObject *parent)
    : QObject(parent)
    , ui(ui_)
{
    ui->dateReception->setDisplayFormat(QLatin1String(Helpers::kDateFormat));
    ui->dateFin->setDisplayFormat(QLatin1String(Helpers::kDateFormat));
    ui->dateReception->setCalendarPopup(true);
    ui->dateFin->setCalendarPopup(true);
    ui->dateFin->setStyleSheet(ui->dateFin->styleSheet()
                               + QStringLiteral("\nQDateTimeEdit:disabled { color: #5E656C; }"));

    connect(ui->btnAjouter, &QPushButton::clicked, this, &InterventionForm::saveRequested);
    connect(ui->btnVider, &QPushButton::clicked, this, &InterventionForm::onClearOrCancel);
    connect(ui->comboEtat, &QComboBox::currentTextChanged, this, &InterventionForm::onEtatChanged);
    connect(ui->dateReception, &QDateTimeEdit::dateTimeChanged, this, &InterventionForm::updateDuree);
    connect(ui->dateFin, &QDateTimeEdit::dateTimeChanged, this, &InterventionForm::updateDuree);
    for (QLineEdit *e : {ui->editLieu, ui->editGPS})
        connect(e, &QLineEdit::returnPressed, this, &InterventionForm::saveRequested);

    clear();
}

void InterventionForm::setEnabled(bool enabled)
{
    ui->frameAjouter->setEnabled(enabled);
}

// La date de fin n'est saisissable que pour une intervention terminée.
void InterventionForm::onEtatChanged()
{
    ui->dateFin->setEnabled(ui->comboEtat->currentText() == Etat::Terminee);
    updateDuree();
}

// La durée n'est jamais saisie : fin - réception (terminée) ou temps écoulé depuis la réception (en cours).
void InterventionForm::updateDuree()
{
    Intervention it;
    it.reception = ui->dateReception->dateTime();
    if (ui->comboEtat->currentText() == Etat::Terminee)
        it.fin = ui->dateFin->dateTime();
    const bool coherent = !it.fin.isValid() || it.fin >= it.reception;
    ui->editDuree->setText(coherent ? it.dureeTexte() : QString());
}

void InterventionForm::clear()
{
    ui->comboType->setCurrentIndex(0);
    ui->comboGravite->setCurrentIndex(0);
    ui->comboEtat->setCurrentIndex(0);
    ui->editLieu->clear();
    ui->editGPS->clear();
    const QDateTime now = QDateTime::currentDateTime();
    ui->dateReception->setDateTime(now);
    ui->dateFin->setDateTime(now);
    onEtatChanged();
}

bool InterventionForm::read(Intervention &out, QString &error) const
{
    out.type = ui->comboType->currentText();
    out.gravite = ui->comboGravite->currentText();
    out.etat = ui->comboEtat->currentText();
    out.lieu = ui->editLieu->text().trimmed();

    if (out.lieu.isEmpty()) {
        error = QStringLiteral("Veuillez saisir le lieu de l'intervention.");
        return false;
    }
    if (!Helpers::parseGps(ui->editGPS->text(), out.lat, out.lon)) {
        error = QStringLiteral("Coordonnées GPS invalides.\nFormat attendu : latitude, longitude (ex. 36.8065, 10.1815).");
        return false;
    }

    out.reception = ui->dateReception->dateTime();
    out.fin = out.etat == Etat::Terminee ? ui->dateFin->dateTime() : QDateTime();

    // Contrôle final (champs obligatoires, cohérence des dates) porté par la classe Intervention
    return out.valider(&error);
}

void InterventionForm::startEdit(const Intervention &it)
{
    m_editId = it.id;

    ui->comboType->setCurrentIndex(std::max(0, ui->comboType->findText(it.type)));
    ui->comboGravite->setCurrentIndex(std::max(0, ui->comboGravite->findText(it.gravite)));
    ui->comboEtat->setCurrentIndex(std::max(0, ui->comboEtat->findText(it.etat)));
    ui->editLieu->setText(it.lieu);
    ui->editGPS->setText(it.gps());
    ui->dateReception->setDateTime(it.reception);
    ui->dateFin->setDateTime(it.fin.isValid() ? it.fin : QDateTime::currentDateTime());
    onEtatChanged();

    ui->labelAjouter->setGeometry(ui->labelAjouter->x(), ui->labelAjouter->y(), 218, ui->labelAjouter->height());
    ui->labelAjouter->setText(QStringLiteral("Modifier %1").arg(it.idText()));
    ui->btnAjouter->setText(QStringLiteral("Modifier"));
    ui->btnVider->setText(QStringLiteral("Annuler"));
    ui->scrollArea->verticalScrollBar()->setValue(0);
    ui->editLieu->setFocus();
}

void InterventionForm::endEdit()
{
    m_editId = -1;
    ui->labelAjouter->setText(QStringLiteral("Ajouter"));
    ui->btnAjouter->setText(QStringLiteral("Ajouter"));
    ui->btnVider->setText(QStringLiteral("Vider"));
    clear();
}

void InterventionForm::onClearOrCancel()
{
    if (isEditing()) {
        endEdit();
        emit editCancelled();
    } else {
        clear();
    }
}
