#include "interventiondialogs.h"
#include "helpers.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

DetailsDialog::DetailsDialog(const Intervention &it, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Intervention %1").arg(it.idText()));
    setStyleSheet(Helpers::kDialogStyle);
    setMinimumWidth(380);

    auto *root = new QVBoxLayout(this);
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
    addRow(QStringLiteral("Gravité"), Helpers::makePillLabel(it.gravite, Helpers::graviteColors(it.gravite)));
    addRow(QStringLiteral("Coordonnées GPS"), text(it.gps()));
    addRow(QStringLiteral("Réception de l'alerte"), text(Helpers::formatDate(it.reception)));
    addRow(QStringLiteral("Fin"), text(Helpers::formatDate(it.fin)));
    addRow(QStringLiteral("Durée"), text(it.dureeTexte()));
    addRow(QStringLiteral("État"), Helpers::makePillLabel(it.etat, Helpers::etatColors(it.etat)));
    root->addLayout(form);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *bClose = new QPushButton(QStringLiteral("Fermer"));
    auto *bEdit = new QPushButton(QStringLiteral("Modifier"));
    bEdit->setDefault(true);
    buttons->addWidget(bClose);
    buttons->addWidget(bEdit);
    root->addLayout(buttons);
    connect(bClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(bEdit, &QPushButton::clicked, this, &QDialog::accept);
}

namespace Dialogs {

namespace {

void showBox(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text)
{
    QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
    box.setStyleSheet(Helpers::kDialogStyle);
    box.exec();
}

} // namespace

bool confirmDelete(QWidget *parent, const Intervention &it)
{
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Supprimer l'intervention"),
                    QStringLiteral("Supprimer l'intervention %1 (%2 – %3) ?\nCette action est définitive.")
                        .arg(it.idText(), it.type, it.lieu),
                    QMessageBox::NoButton, parent);
    auto *yes = box.addButton(QStringLiteral("Supprimer"), QMessageBox::AcceptRole);
    auto *no = box.addButton(QStringLiteral("Annuler"), QMessageBox::RejectRole);
    box.setDefaultButton(no);
    box.setStyleSheet(Helpers::kDialogStyle);
    box.exec();
    return box.clickedButton() == yes;
}

void showInfo(QWidget *parent, const QString &title, const QString &text)
{
    showBox(parent, QMessageBox::Information, title, text);
}

void showWarning(QWidget *parent, const QString &title, const QString &text)
{
    showBox(parent, QMessageBox::Warning, title, text);
}

void showError(QWidget *parent, const QString &title, const QString &text)
{
    showBox(parent, QMessageBox::Critical, title, text);
}

} // namespace Dialogs
