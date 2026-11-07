#pragma once
#include <QColor>
#include <QVector>
#include <QWidget>

struct VehiculeSlice {
    QString label;
    int value = 0;
    QColor color;
};

class VehiculeDonutWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VehiculeDonutWidget(QWidget *parent = nullptr);
    void setSlices(const QVector<VehiculeSlice> &slices);
    QSize sizeHint() const override { return {100, 100}; }
    QSize minimumSizeHint() const override { return {80, 80}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<VehiculeSlice> m_slices;
};
