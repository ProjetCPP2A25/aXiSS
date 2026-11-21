INTEGRATION DU MODULE VEHICULES - A.X.I.S (nouvelle interface)
================================================================

Projet unique : ouvrir AXIS.pro dans Qt Creator (Qt 6, qmake, kit MinGW 64 bits avec QODBC).

Ce qui a change
- L'ANCIENNE interface Vehicules (ancienne page PVehicule de axis.ui + code de axis.cpp + vehiculedonut.h/.cpp) a ete supprimee.
- La NOUVELLE interface (ex-projet AXIS_Vehicules, fenetre GAxis) est integree DIRECTEMENT dans axis.ui :
  la page PVehicule contient maintenant ses widgets (il n'y a qu'UN SEUL fichier .ui : axis.ui).
  Tous les widgets de cette page portent le suffixe « Veh » (ex. lePlaqueVeh, tableVehiculesVeh, btnSubmitVeh).
  L'index 2 du SWAxis et le bouton « Vehicules » du menu principal fonctionnent comme avant.
- La logique (ex-gaxis.cpp) est dans axis.cpp, section « MODULE VEHICULES » (methodes de la classe AXIS).
- Fonctions de la nouvelle page : Ajouter / Afficher / Modifier / Supprimer, controle de saisie, recherche, tri,
  bandeau « maintenance requise », 2 statistiques (etat / disponibilite, anneaux), export Excel (CSV),
  bouton Entretiens (barre laterale), bouton « Envoyer un SMS ».
- Navigation : « Menu principal » -> menu de l'application ; « Deconnexion » -> ferme l'application
  (comme les autres modules). Branchement dans AXIS::initialiserModuleVehicules() (axis.cpp).
- La page se rafraichit a chaque ouverture du module (AXIS::allerAuModule(2) -> rafraichirListeVehicules()).
- Le style de la nouvelle page est dans la propriete styleSheet de PVehicule (axis.ui) et limite a cette page.

Inchange
- vehicule.*, entretien.*, entretiendialog.* : classes metier identiques a celles du projet AXIS_Vehicules, avec la
  connexion commune Connexion::oracle() (DSN AXIS_COMMUN). Il n'y a toujours qu'UNE seule connexion Oracle.
- Les scripts SQL sql/oracle_vehicule.sql et sql/oracle_vehicule_exemples.sql (identiques a oracle/01_schema.sql
  et oracle/02_donnees_exemple.sql du projet AXIS_Vehicules).
- Export PDF : l'ancienne page l'avait, la nouvelle interface ne le propose pas (bouton absent) : il n'a donc pas ete reporte.

Base Oracle (a faire UNE fois, dans SQL Developer, avec l'utilisateur AXIS_COMMUN)
1) sql/oracle_vehicule.sql           : cree VEHICULE, ENTRETIEN_VEHICULE, SEQ_ENTRETIEN_VEHICULE, index.
2) sql/oracle_vehicule_exemples.sql  : (facultatif) 3 vehicules de demonstration.

Alerte SMS (facultatif) : variables d'environnement TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN, TWILIO_FROM_NUMBER.
Ne jamais mettre de mots de passe / cles dans le code ni sur GitHub.
