#include "exportexcel.h"
#include "helpers.h"
#include "interventiondialogs.h"
#include "xlsxwriter.h"

#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QStatusBar>
#include <QtConcurrent>

namespace ExportExcel {

void run(QMainWindow *window, const QVector<Intervention> &rows, QWidget *bouton)
{
    if (rows.isEmpty()) {
        Dialogs::showInfo(window, QStringLiteral("Exporter"), QStringLiteral("Aucune intervention à exporter."));
        return;
    }

    const QString defaultName = QDir::homePath() + QStringLiteral("/interventions_")
                                + QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))
                                + QStringLiteral(".xlsx");
    QString path = QFileDialog::getSaveFileName(window, QStringLiteral("Exporter vers Excel"), defaultName,
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

    if (bouton)
        bouton->setEnabled(false);
    window->statusBar()->showMessage(QStringLiteral("Export Excel en cours..."));

    // Écriture du fichier dans un thread séparé ; le résultat est une chaîne d'erreur (vide si tout va bien).
    const int count = rows.size();
    auto *watcher = new QFutureWatcher<QString>(window);
    QObject::connect(watcher, &QFutureWatcher<QString>::finished, window, [=] {
        const QString error = watcher->result();
        if (bouton)
            bouton->setEnabled(true);
        if (error.isEmpty()) {
            window->statusBar()->showMessage(
                QStringLiteral("%1 intervention(s) exportée(s) : %2").arg(count).arg(path), 6000);
        } else {
            window->statusBar()->clearMessage();
            Dialogs::showError(window, QStringLiteral("Export impossible"),
                               QStringLiteral("Le fichier n'a pas pu être écrit :\n%1").arg(error));
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([=] {
        QString error;
        const bool ok = Xlsx::write(path, QStringLiteral("Interventions"), header, data,
                                    {8, 14, 20, 12, 22, 20, 20, 12, 12}, &error);
        return ok ? QString() : (error.isEmpty() ? QStringLiteral("Erreur inconnue.") : error);
    }));
}

} // namespace ExportExcel
