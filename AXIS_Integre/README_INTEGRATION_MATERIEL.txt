INTEGRATION DU MODULE MATERIEL - A.X.I.S

1) Ouvrir AXIS.pro dans Qt Creator (qmake).
2) Vérifier que le DSN ODBC 64 bits s'appelle AXIS_ORACLE et pointe vers la base Oracle.
3) La connexion utilise l'utilisateur AXIS_COMMUN et lit le mot de passe depuis la variable d'environnement AXIS_ORACLE_PASSWORD.
4) Avant de lancer depuis Qt Creator, définir AXIS_ORACLE_PASSWORD dans l'environnement du processus Qt Creator.
   Pour un test temporaire depuis PowerShell :
       $env:AXIS_ORACLE_PASSWORD = "VOTRE_MOT_DE_PASSE"
       & "C:\chemin\vers\QtCreator.exe"
   Ou lancer l'application depuis le même PowerShell après compilation.
5) Le code Materiel utilise les colonnes Oracle suivantes :
   ID_MATERIEL, REFERENCE, DESIGNATION, CATEGORIE, QUANTITE_STOCK,
   SEUIL_MINIMAL, ETAT, DISPONIBILITE, DATE_ACQUISITION, EMPLACEMENT_STOCK.
   Vérifier que ces noms correspondent exactement à la table MATERIEL du schéma AXIS_COMMUN.

Fonctions intégrées dans la page Matériels :
- Ajouter / Afficher / Modifier / Supprimer
- Recherche par référence ou désignation
- Tri par référence, désignation, catégorie, quantité ou date
- Export CSV compatible Excel
- Statistiques par catégorie et détection du stock critique

La page Interventions et le module Employés d'origine sont conservés.
Le projet n'a pas pu être compilé ici car l'environnement Qt/Oracle de votre PC n'est pas disponible.

--- MISE A JOUR : nouvelle interface Matériels ---
Aucun nouveau fichier : la page Matériels reste dans axis.ui / axis.cpp (carte « Suivi des mouvements de stock » supprimée, tri par état,
badges colorés, boutons d'action, export Excel, alerte email de stock critique). Seuls materiel.cpp / materiel.h sont propres au module.
materiel.cpp détecte tout seul les noms de colonnes (SEUIL_MINIMAL_STOCK ou SEUIL_MINIMAL, EMPLACEMENT_STOCKAGE ou EMPLACEMENT_STOCK).
AXIS.pro : seul ajout = "network" (envoi d'email). Email : renseigner SMTP_PASSWORD dans axis.cpp (jamais sur GitHub).

--- MISE A JOUR : nouvelle interface Matériels ---
Aucun nouveau fichier : la page Matériels reste dans axis.ui / axis.cpp (carte « Suivi des mouvements de stock » supprimée, tri par état,
badges colorés, boutons d'action, export Excel, alerte email de stock critique). Seuls materiel.cpp / materiel.h sont propres au module.
materiel.cpp détecte tout seul les noms de colonnes (SEUIL_MINIMAL_STOCK ou SEUIL_MINIMAL, EMPLACEMENT_STOCKAGE ou EMPLACEMENT_STOCK).
AXIS.pro : seul ajout = "network" (envoi d'email). Email : renseigner SMTP_PASSWORD dans axis.cpp (jamais sur GitHub).
