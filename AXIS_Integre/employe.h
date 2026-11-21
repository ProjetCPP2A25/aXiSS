#pragma once
#include <QString>
#include <QVector>
#include <QFrame>

class QLabel;

// Classe Employe : ses données et son CRUD (ajouter, modifier, supprimer, afficher / lister).
class Employe
{
public:
    QString id, nom, telephone, email, grade, poste, disponibilite;
    int age = 30, heures = 40, conges = 18;
    QString prenom;        // nom = nom de famille ; prenom = prénom (deux champs séparés)
    QString nomComplet() const { return (prenom + " " + nom).trimmed(); }

    bool ajouter();
    bool modifier() const;
    static bool supprimer(const QString &id);

    static Employe trouver(const QString &id);
    static QVector<Employe> lister();                       // tous les employés, triés par nom

    // Validations de saisie
    static bool emailValide(const QString &email);          // forme nom@domaine.ext
    static bool telephoneValide(const QString &telephone);  // chiffres uniquement, 8 à 15
    static bool nomValide(const QString &nom);              // lettres, espaces, tirets, apostrophes ; 2 à 40 caractères
    static bool emailExiste(const QString &email, const QString &sauf = QString());

    static QString prochainId();
    static QString derniereErreur();

};

// Barre latérale du module Employés. Déclaration intégrée dans employe.h.
class SidebarFrame : public QFrame
{
    Q_OBJECT
public:
    explicit SidebarFrame(QWidget *parent = nullptr);
    void setInfo(const QString &text);
signals:
    void menuRequested();
    void logoutRequested();
private:
    QLabel *m_info = nullptr;
};
