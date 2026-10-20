#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include "axis.h"
#include "connexion.h"
#include "database.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("RedFlame");
    // Nom interne conservé : il détermine l'emplacement de la base SQLite et des réglages email déjà enregistrés
    QCoreApplication::setApplicationName("Caserne");

    // Feuille de style : style.qss (inclus dans ressources.qrc)
    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        a.setStyleSheet(QString::fromUtf8(qss.readAll()));

    QString erreur;
    if (!Connexion::ouvrir(erreur)) {
        QMessageBox::critical(nullptr, "Base de données", erreur);
        return 1;
    }
    QString dbError;
    if (!Database::init(&dbError)) {
        QMessageBox::critical(nullptr, "Base de données", dbError);
        return 1;
    }

    AXIS w;
    w.resize(1500, 850);
    w.show();
    return a.exec();
}
