#ifndef SIDEBAR_H
#define SIDEBAR_H

// Barre latérale commune des modules (même apparence que celle de la gestion des interventions) :
// logo A.X.I.S, nom de la société, puis les boutons « Menu principal » et « Déconnexion » à gauche.
// Une ligne d'information facultative (utilisateur connecté) s'affiche sous les boutons.

#include <QFrame>

class QLabel;

class SidebarFrame : public QFrame
{
    Q_OBJECT

public:
    explicit SidebarFrame(QWidget *parent = nullptr);

    void setInfo(const QString &text);      // masquée si le texte est vide

signals:
    void menuRequested();
    void logoutRequested();

private:
    QLabel *m_info;
};

#endif // SIDEBAR_H
