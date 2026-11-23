INTEGRATION DU MODULE VEHICULES - A.X.I.S (interface ProjetFeriel)
==================================================================

Projet unique : ouvrir AXIS.pro dans Qt Creator (Qt 6, qmake, kit MinGW 64 bits avec QODBC).

Ce qui a change (remplacement par la nouvelle version de ProjetFeriel / AXIS_Vehicules)
- vehicule.h / vehicule.cpp : remplaces par la nouvelle version. Elle contient maintenant, dans les memes fichiers,
  la classe Vehicule ET les entretiens (struct Entretien, GestionEntretiens, GestionEntretiensDialog).
  Les anciens fichiers entretien.h/.cpp et entretiendialog.h/.cpp ont donc ete supprimes (et retires de AXIS.pro).
- Toutes les requetes SQL du module passent par la connexion commune Connexion::oracle() (une seule connexion Oracle).
- Nouveaute : un entretien peut etre rattache a un employe (colonne ENTRETIEN_VEHICULE.ID_EMPLOYE -> EMPLOYE.ID_EMPLOYE).
- Alerte SMS : le numero du destinataire est pre-rempli par la variable d'environnement TWILIO_TO_NUMBER (facultatif).
- Page PVehicule de axis.ui : l'interface est identique a la nouvelle interface (verifie widget par widget) ;
  elle est conservee telle quelle. Tous ses widgets portent le suffixe « Veh » (ex. lePlaqueVeh, btnSubmitVeh).
- Logique de la page (ex-gaxis.cpp) : section « MODULE VEHICULES » de axis.cpp (methodes de la classe AXIS).
  Navigation : bouton « Bienvenue Emna ❤️ » -> menu de l'application ; « Deconnexion » -> message « Au revoir Emna ❤️ » puis ferme l'application.

Fonctions de la page : Ajouter / Afficher / Modifier / Supprimer, controle de saisie, recherche, tri,
bandeau « maintenance requise », 2 statistiques (etat / disponibilite), export Excel (CSV),
bouton Entretiens (barre laterale), bouton « Envoyer un SMS ».

Base Oracle
- sql/export.sql : export complet de la base (SQL Developer, schema AXIS) : tables EMPLOYE, VEHICULE, ENTRETIEN_VEHICULE
  (avec ID_EMPLOYE -> EMPLOYE), GARDE, AFFECTE, INTERVENTION, MATERIEL, UTILISATION_*, sequences, contraintes et donnees.
  Il remplace les anciens scripts oracle_vehicule*.sql (supprimes).
- Executer dans SQL Developer sur une base vide (les objets sont prefixes « AXIS. » : si l'application se connecte avec
  un autre utilisateur, par ex. AXIS_COMMUN, remplacer le prefixe ou executer le script sous le bon schema).

Alerte SMS (facultatif) : variables d'environnement TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN, TWILIO_FROM_NUMBER
(et TWILIO_TO_NUMBER pour pre-remplir le destinataire). Ne jamais mettre de mots de passe / cles dans le code ni sur GitHub.
