#pragma once
#include <QDate>
#include <QString>
#include <QVector>

// Véhicule de la caserne + accès à la table « vehicules » (même base SQLite que les autres modules).
struct Vehicule
{
    QString id, plate, type;
    int km = 0;
    QString etat, dispo;
    QDate lastMaint, nextMaint, insurance, tech;

    // Crée la table si besoin et insère les véhicules d'exemple quand elle est vide.
    static bool initialiser(QString *erreur = nullptr);
    static QVector<Vehicule> tous();
    static bool ajouter(const Vehicule &v, QString *erreur = nullptr);
    static bool modifier(const Vehicule &v, QString *erreur = nullptr);
    static bool supprimer(const QString &id, QString *erreur = nullptr);
};
