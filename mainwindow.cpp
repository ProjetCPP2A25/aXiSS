#include "mainwindow.h"
#include "ui_mainwindow.h"   // généré à partir de mainwindow.ui

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Navigation latérale -> pages du QStackedWidget
    connect(ui->btnEmployes, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(0); });
    connect(ui->btnInterventions, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(1); });
    connect(ui->btnVehicules, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(2); });
    connect(ui->btnMateriels, &QPushButton::clicked, this, [this] { ui->stack->setCurrentIndex(3); });
}

MainWindow::~MainWindow()
{
    delete ui;
}
