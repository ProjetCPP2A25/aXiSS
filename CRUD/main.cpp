#include "gaxis.h"
#include "connection.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Une seule connexion à la base pour toute l'application (Singleton)
    if (Connection::instance().ouvrir()) {
        qDebug() << "Oracle connection successful!";
    } else {
        qDebug() << "Oracle connection failed:"
                 << Connection::instance().derniereErreur();
    }

    GAxis w;
    w.show();

    const int code = QApplication::exec();
    Connection::instance().fermer();
    return code;
}
