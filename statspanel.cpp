#include "statspanel.h"
#include "ui_interventionpage.h"
#include "intervention.h"

#include <QFrame>
#include <QLabel>

#include <algorithm>

QString StatsPanel::refresh()
{
    QString error;
    const QMap<QString, int> counts = Intervention::statistiquesParGravite(&error);
    if (!error.isEmpty())
        return error;

    const int nE = counts.value(Grav::Elevee);
    const int nM = counts.value(Grav::Moyenne);
    const int nF = counts.value(Grav::Faible);

    const int baseline = 185;    // ordonnée de la ligne de base (ligne lineStats)
    const int maxHeight = 100;   // garde le haut du cadre libre pour le titre et le sous-titre
    const int maxCount = std::max({nE, nM, nF, 1});

    auto place = [&](QFrame *bar, QLabel *label, int n) {
        int h = n * maxHeight / maxCount;
        if (n > 0)
            h = std::max(h, 6);
        bar->setGeometry(bar->x(), baseline - h, bar->width(), h);
        bar->setVisible(h > 0);
        label->setText(QString::number(n));
        label->move(label->x(), std::max(62, baseline - h - 32));
    };
    place(ui->barElevee, ui->labelCountElevee, nE);
    place(ui->barMoyenne, ui->labelCountMoyenne, nM);
    place(ui->barFaible, ui->labelCountFaible, nF);
    return QString();
}
