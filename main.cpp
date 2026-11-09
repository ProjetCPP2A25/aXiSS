#include "gaxis.h"
#include "connexion.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ---- Connexion à Oracle (DSN "axis") ----
    QString erreur;
    if (!Connexion::ouvrir(erreur)) {
        QMessageBox::critical(nullptr, "Connexion", erreur);
        return -1;
    }
    // Message de test : à supprimer quand la connexion est validée
    QMessageBox::information(nullptr, "Connexion", "Connecté à Oracle (axis) !");

    GAxis w;
    w.show();
    return a.exec();
}
