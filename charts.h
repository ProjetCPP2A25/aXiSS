#pragma once
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

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
