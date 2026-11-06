
#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QString>

class Connection
{
public:
    // Instance unique : Singleton
    static Connection &instance();

    // Interdire la copie
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    // Gestion de la connexion Oracle
    bool ouvrir();
    void fermer();
    bool estOuverte() const;

    QString derniereErreur() const;
    QSqlDatabase database() const;

private:
    Connection() = default;
    ~Connection() = default;

    QString m_erreur;
};

#endif // CONNECTION_H
