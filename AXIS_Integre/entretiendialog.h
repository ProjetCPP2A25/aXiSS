#ifndef ENTRETIENDIALOG_H
#define ENTRETIENDIALOG_H

#include "entretien.h"

#include <QDialog>

class QLineEdit;
class QTableWidget;
class QLabel;

class GestionEntretiensDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GestionEntretiensDialog(QWidget *parent = nullptr);

private:
    void rafraichir();
    void planifierEntretien();
    void terminerEntretien();
    void supprimerEntretien();

    QLineEdit *m_recherche = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_alerte = nullptr;
    QVector<Entretien> m_entretiens;
};

#endif // ENTRETIENDIALOG_H
