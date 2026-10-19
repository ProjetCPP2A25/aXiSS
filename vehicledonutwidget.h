#pragma once
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

struct VehicleSlice {
    QString label;
    int value = 0;
    QColor color;
};

class VehicleDonutWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VehicleDonutWidget(QWidget *parent = nullptr);
    void setSlices(const QVector<VehicleSlice> &slices);
    QSize sizeHint() const override { return {100, 100}; }
    QSize minimumSizeHint() const override { return {80, 80}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<VehicleSlice> m_slices;
};
