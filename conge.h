#pragma once
#include <QString>
#include <QVector>

// Classe Conge : une demande de congé d'un employé.
class Conge
{
public:
    int id = 0;
    QString idEmploye, nomEmploye, details, statut;   // statut : En attente / Acceptée / Refusée
    int jours = 0;

    bool ajouter();
    static QVector<Conge> lister(int limite);
    // Accepte ou refuse une demande dans UNE transaction (déduction du solde incluse).
    static bool decider(int id, bool accepter, QString &message);
};
