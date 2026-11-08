#pragma once
#include <QMainWindow>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

#include "materiel.h"

// Histogramme simple (barres rouges + valeur au-dessus + libellés dessous)
class BarChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget *parent = nullptr);
    void setData(const QVector<QPair<QString, int>> &data);
    QSize minimumSizeHint() const override { return {300, 230}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<QPair<QString, int>> m_data;
};

// Anneau critique / suffisant avec le total au centre
class DonutWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DonutWidget(QWidget *parent = nullptr);
    void setData(int critical, int sufficient);
    QSize minimumSizeHint() const override { return {130, 130}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_critical = 0;
    int m_sufficient = 0;
};

namespace Ui { class gaxis; }

// Fenêtre principale AXIS : navigation + page « Matériels » (interface définie dans gaxis.ui)
class gaxis : public QMainWindow
{
    Q_OBJECT
public:
    explicit gaxis(QWidget *parent = nullptr);
    ~gaxis() override;

public slots:
    void refresh();

private slots:
    void submitForm();
    void clearForm();
    void exportCsv();
    void handleAction(const QString &link);

private:
    void initMateriel();
    void fillForm(const Materiel &m);
    void refreshMateriels();
    void refreshStats();
    void sendAlertEmail(const QList<Materiel> &critical);

    Ui::gaxis *ui;
    int editingId = 0;
    QSet<int> lastCritical;
};
