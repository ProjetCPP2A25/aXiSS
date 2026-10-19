#include "interventiontable.h"
#include "ui_interventionpage.h"
#include "helpers.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>

#include <algorithm>

namespace {

enum Col { ColId, ColType, ColLieu, ColGravite, ColGps, ColReception, ColFin, ColDuree, ColEtat, ColActions,
           ColumnCount };

} // namespace

InterventionTable::InterventionTable(Ui::InterventionPage *ui_, QObject *parent)
    : QObject(parent)
    , ui(ui_)
{
    setupTable();
    setupFilters();
}

void InterventionTable::setupTable()
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
            emit viewRequested(it->data(Qt::UserRole).toInt());
    });

    connect(ui->btnPagePrecedente, &QPushButton::clicked, this, [this] {
        if (m_page > 0) {
            --m_page;
            refresh();
        }
    });
    connect(ui->btnPageSuivante, &QPushButton::clicked, this, [this] {
        ++m_page;
        refresh();
    });
}

void InterventionTable::setupFilters()
{
    ui->editRecherche->setClearButtonEnabled(true);

    ui->comboTri->clear();
    ui->comboTri->addItems({
        QStringLiteral("Aucun tri"),
        QStringLiteral("Gravité : élevée → faible"),
        QStringLiteral("Gravité : faible → élevée")});

    connect(ui->editRecherche, &QLineEdit::textChanged, this, [this] { goToFirstPage(); refresh(); });
    connect(ui->comboTri, &QComboBox::currentIndexChanged, this, [this] { goToFirstPage(); refresh(); });
}

void InterventionTable::goToFirstPage()
{
    m_page = 0;
}

Intervention::Filtre InterventionTable::filtre() const
{
    Intervention::Filtre f;
    f.gps = ui->editRecherche->text();
    switch (ui->comboTri->currentIndex()) {
    case 1:  f.tri = Intervention::Tri::GraviteDecroissante; break;
    case 2:  f.tri = Intervention::Tri::GraviteCroissante; break;
    default: f.tri = Intervention::Tri::Aucun; break;
    }
    return f;
}

QString InterventionTable::refresh()
{
    QTableWidget *t = ui->tableInterventions;
    const Intervention::Filtre f = filtre();

    QString error;
    const int total = Intervention::compter(f, &error);
    if (!error.isEmpty())
        return error;

    // La page courante doit rester valide (ex. après une suppression)
    const int pageCount = std::max(1, (total + kLignesParPage - 1) / kLignesParPage);
    m_page = std::clamp(m_page, 0, pageCount - 1);

    const QVector<Intervention> rows = Intervention::lister(f, m_page * kLignesParPage, kLignesParPage, &error);
    if (!error.isEmpty())
        return error;

    ui->labelPagination->setText(QStringLiteral("Page %1 / %2  (%3 intervention%4)")
                                     .arg(m_page + 1).arg(pageCount).arg(total).arg(total > 1 ? "s" : ""));
    ui->btnPagePrecedente->setEnabled(m_page > 0);
    ui->btnPageSuivante->setEnabled(m_page + 1 < pageCount);

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
        t->setItem(r, ColReception, makeItem(Helpers::formatDateTableau(i.reception), Helpers::formatDate(i.reception)));
        t->setItem(r, ColFin, makeItem(Helpers::formatDateTableau(i.fin), Helpers::formatDate(i.fin)));
        t->setItem(r, ColDuree, makeItem(i.dureeTexte(), i.dureeTexte()));
        t->setItem(r, ColEtat, makeItem(QString(), i.etat));
        t->setItem(r, ColActions, makeItem(QString(), QString()));

        t->setCellWidget(r, ColGravite, Helpers::makePillCell(i.gravite, Helpers::graviteColors(i.gravite)));
        t->setCellWidget(r, ColEtat, Helpers::makePillCell(i.etat, Helpers::etatColors(i.etat)));

        // Actions : Afficher / Modifier / Supprimer
        auto *actions = new QWidget;
        actions->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto *lay = new QHBoxLayout(actions);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        auto *bView = Helpers::makeActionButton(QStringLiteral("👁"), QStringLiteral("Afficher"), QStringLiteral("#30353A"));
        auto *bEdit = Helpers::makeActionButton(QStringLiteral("✎"), QStringLiteral("Modifier"), QStringLiteral("#30353A"));
        auto *bDel = Helpers::makeActionButton(QStringLiteral("🗑"), QStringLiteral("Supprimer"), QStringLiteral("#A61B1B"));
        lay->addWidget(bView);
        lay->addWidget(bEdit);
        lay->addWidget(bDel);
        lay->addStretch();
        const int id = i.id;
        connect(bView, &QToolButton::clicked, this, [this, id] { emit viewRequested(id); });
        connect(bEdit, &QToolButton::clicked, this, [this, id] { emit editRequested(id); });
        connect(bDel, &QToolButton::clicked, this, [this, id] { emit deleteRequested(id); });
        t->setCellWidget(r, ColActions, actions);
    }

    if (total == 0 && !ui->editRecherche->text().trimmed().isEmpty())
        emit noMatch();
    return QString();
}
