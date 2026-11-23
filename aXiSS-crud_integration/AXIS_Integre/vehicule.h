#ifndef VEHICULE_H
#define VEHICULE_H





#include <QDate>
#include <QDialog>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

struct Entretien
{
    QString id;
    QString vehiculeId;
    QString plaque;
    QString type;
    QString description;
    QString garage;
    QString statut;
    QDate datePrevue;
    QDate dateRealisee;
    QDate prochaineDate;
    int kmPrevu = 0;
    int kmRealise = 0;
    int prochainKm = 0;
    int kmVehicule = 0;
    double cout = 0.0;
};

class GestionEntretiens
{
public:
    static bool initialiser(QString *erreur = nullptr);
    static QVector<Entretien> lister(const QString &recherche = QString(), QString *erreur = nullptr);
    static bool vehicules(QVector<QPair<QString, QString>> &resultat, QString *erreur = nullptr);
    static bool ajouter(const Entretien &entretien, QString *erreur = nullptr);
    static bool terminer(const Entretien &entretien, QString *erreur = nullptr);
    static bool supprimer(const QString &id, QString *erreur = nullptr);
};

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

class Vehicule
{
public:
    
    QString id;                 
    QString plaque;             
    QString type;
    int km = 0;
    QString etat;               
    QString dispo;              
    QDate derniereMaint, prochaineMaint, assurance, visiteTech;

    
    static const QStringList &types();
    static const QStringList &etats();
    static const QStringList &disponibilites();

    
    static QString normaliserPlaque(const QString &texte);   
    bool valider(QString *erreur = nullptr) const;

    
    bool ajouter(QString *erreur = nullptr);                  
    static bool afficher(const QString &id, Vehicule &resultat, QString *erreur = nullptr);
    bool modifier(QString *erreur = nullptr) const;
    static bool supprimer(const QString &id, QString *erreur = nullptr);

    
    
    
    
    static QVector<Vehicule> lister(const QString &recherche, int tri, QString *erreur = nullptr);
    static QVector<Vehicule> maintenanceRequise(QString *erreur = nullptr);   
    static QMap<QString, int> statistiques(bool parEtat, QString *erreur = nullptr);  

private:
    static bool plaqueExiste(const QString &plaque, const QString &sauf);
    static QString prochainId();
};

#endif 
