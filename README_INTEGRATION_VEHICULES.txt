INTEGRATION DU MODULE VEHICULES - A.X.I.S
==========================================

Projet unique : ouvrir AXIS.pro dans Qt Creator (Qt 6, qmake, kit MinGW 64 bits avec QODBC).

Ce qui a ete fait
- vehicule.h/.cpp, entretien.h/.cpp, entretiendialog.h/.cpp ajoutes (module Vehicules + suivi des entretiens).
- Ces classes utilisent maintenant la connexion commune Connexion::oracle() (connexion nommee "oracle"),
  comme Employe, Intervention et Materiel. Il n'y a plus qu'UNE seule connexion : DSN AXIS_ORACLE, utilisateur
  AXIS_COMMUN, mot de passe dans la variable d'environnement AXIS_ORACLE_PASSWORD (voir README_INTEGRATION_MATERIEL.txt).
- La page PVehicule de axis.ui est branchee (axis.cpp, section MODULE VEHICULES / axis.h) :
  Ajouter, Afficher, Modifier, Supprimer, controle de saisie, recherche, tri, bandeau "maintenance requise",
  2 statistiques (etat / disponibilite), export Excel (CSV), export PDF, bouton Entretiens, bouton Alerte SMS.
  axis.ui n'a pas ete modifie : la recherche et les boutons Entretiens / Alerte SMS sont ajoutes par code.
- AXIS.pro : ajout des 3 classes + des 2 scripts SQL. Le module "network" etait deja present.

Base Oracle (a faire UNE fois, dans SQL Developer, avec l'utilisateur AXIS_COMMUN)
1) sql/oracle_vehicule.sql           : cree VEHICULE, ENTRETIEN_VEHICULE, SEQ_ENTRETIEN_VEHICULE, index.
2) sql/oracle_vehicule_exemples.sql  : (facultatif) 3 vehicules de demonstration.
   Si la table VEHICULE existe deja dans AXIS_COMMUN, ne relancez pas le script : verifiez seulement que les colonnes
   sont ID_VEHICULE, IMMATRICULATION, TYPE, KILOMETRAGE, ETAT, DISPONIBILITE, DATE_DERNIERE_MAINTENANCE,
   DATE_PROCHAINE_MAINTENANCE, DATE_EXPIRATION_ASSURANCE, DATE_VISITE_TECHNIQUE.

Alerte SMS (facultatif) : variables d'environnement TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN, TWILIO_FROM_NUMBER.
Ne jamais mettre de mots de passe / cles dans le code ni sur GitHub.

Ancien projet AXIS_Vehicules (fenetre gaxis) : n'est plus necessaire, tout est dans AXIS.pro.
