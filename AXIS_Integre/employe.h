#pragma once
#include <QString>
#include <QVector>
#include <QFrame>
#include <QMap>
#include <QStringList>
#include <QDateTime>
#include <QDialog>

class QLabel;
class QLineEdit;

// Utilisateur connecté (résultat de Employe::authentifier)
struct SessionEmploye
{
    QString id, nomComplet, role;      // role = POSTE de l'employé : Administrateur, Chef de caserne, Opérateur de dispatch, Magasinier
};

// Une ligne du mini-journal d'activité
struct EntreeJournal
{
    QDateTime date;
    QString utilisateur, action;
};

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

    // ------------------------------------------------------------------
    // Métiers innovants (gestion des employés)
    // ------------------------------------------------------------------
    // 1) Recherche : employés dont le NOM contient le texte saisi (vide = tous)
    static QVector<Employe> rechercher(const QString &nom);

    // 2) Tri par grade (Caporal < Sergent < Lieutenant < Capitaine), avec filtre de recherche facultatif
    static QVector<Employe> trierParGrade(bool croissant = true, const QString &nom = QString());

    // 3) Export : attestation de présence de cet employé, au format Excel (.xlsx)
    bool exporterAttestationExcel(const QString &chemin) const;

    // 4) Statistiques : effectif par disponibilité et par grade (clé = libellé, valeur = nombre d'employés)
    static QMap<QString, int> statistiquesParDisponibilite();
    static QMap<QString, int> statistiquesParGrade();
    static QStringList disponibilites();                    // dans l'ordre d'affichage
    static QStringList grades();                            // du plus bas au plus élevé

    // ------------------------------------------------------------------
    // Authentification, mot de passe oublié et droits d'accès
    // ------------------------------------------------------------------
    static bool authentifier(const QString &email, const QString &motDePasse, SessionEmploye &session);
    // « Mot de passe oublié » : l'identité est vérifiée (email + ID de l'employé), puis le nouveau mot de passe est enregistré
    static bool reinitialiserMotDePasse(const QString &email, const QString &idEmploye, const QString &nouveau);
    static bool changerMotDePasse(const QString &id, const QString &nouveau);
    // Droits d'accès du cahier de spécification, selon le poste (module : 0 Employés, 1 Interventions, 2 Véhicules, 3 Matériels, 4 Gardes)
    static bool moduleAutorise(const QString &poste, int module);

    // ------------------------------------------------------------------
    // Mini-journal d'activité (gardé dans les paramètres de l'application : aucune table Oracle)
    // ------------------------------------------------------------------
    static bool journaliser(const QString &utilisateur, const QString &action);   // ex. « Karim Majri », « a ajouté l'employé Rim Trabelsi »
    static QVector<EntreeJournal> dernieresActivites(int nombre = 8);             // du plus récent au plus ancien

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

// ======================================================================
// ======================================================================
// Page de connexion : email + mot de passe (vérifiés dans Oracle), « Mot de passe oublié ? », Quitter.
class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    SessionEmploye session() const { return session_; }     // valable après exec() == Accepted

private:
    void seConnecter();
    void motDePasseOublie();

    QLineEdit *leEmail_ = nullptr;
    QLineEdit *leMdp_ = nullptr;
    SessionEmploye session_;
    int echecs_ = 0;
};
