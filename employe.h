#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QFrame>

class QLabel;

// Classe Employe : gère ses propres données et ses opérations en base.
class Employe
{
public:
    QString id, nom, telephone, email, grade, poste, disponibilite, motDePasse;
    int age = 30, heures = 40, conges = 18;
    bool alerte = false;   // true = email de surcharge déjà envoyé
    QString prenom;        // nom = nom de famille ; prenom = prénom (deux champs séparés)
    QString nomComplet() const { return (prenom + " " + nom).trimmed(); }

    bool ajouter();
    bool modifier() const;
    static bool supprimer(const QString &id);

    static Employe trouver(const QString &id);
    static Employe trouverParEmail(const QString &email);   // id vide = introuvable
    // tri : 0 = grade décroissant, 1 = grade croissant, 2 = par nom (usage interne)
    static QVector<Employe> lister(const QString &recherche, int tri);  // WHERE + ORDER BY
    static QVector<Employe> surcharge(bool seulementNonNotifies);       // plus de 48 h/semaine

    // Validations de saisie
    static bool emailValide(const QString &email);          // forme nom@domaine.ext
    static bool telephoneValide(const QString &telephone);  // chiffres uniquement, 8 à 15
    static bool nomValide(const QString &nom);              // lettres, espaces, tirets, apostrophes ; 2 à 40 caractères
    static bool emailExiste(const QString &email, const QString &sauf = QString());

    static QMap<QString, int> statsDisponibilite();                     // COUNT + GROUP BY
    static QMap<QString, int> statsGrade();

    static Employe authentifier(const QString &email, const QString &motDePasse);  // id vide = échec
    static QString hash(const QString &motDePasse);
    static bool reinitialiserMotDePasse(const QString &email, const QString &nouveauMotDePasse);
    static QString prochainId();
    static QString derniereErreur();

    // Droits d'accès selon le rôle (poste). module : 0 Employés, 1 Interventions, 2 Véhicules, 3 Matériels
    static bool aAcces(const QString &poste, int module);
    static bool estUtilisateur(const QString &poste);   // a accès à au moins un module
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
