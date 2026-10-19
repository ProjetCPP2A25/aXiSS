# A.X.I.S – Module « Gestion des matériels » (Qt / C++)

## Prérequis
- Qt 5.15+ ou Qt 6 (modules Widgets et Sql, driver QSQLITE inclus avec Qt)
- Un compilateur C++17

## Compilation
**Qt Creator** : ouvrir `AXIS_Materiels.pro` (qmake), puis Exécuter.

**Ligne de commande (CMake)** :
    cmake -S . -B build && cmake --build build

## Fichiers de l'interface (Qt Designer)
- `mainwindow.ui` : fenêtre principale (barre latérale + pages)
- `materielpage.ui` : écran « Gestion des matériels » (formulaire, liste, statistiques, mouvements)
  Double-clic sur un `.ui` dans Qt Creator pour le modifier visuellement.
  Les graphiques (`BarChartWidget`, `DonutWidget`) sont des widgets promus (en-tête `charts.h`).
  Le style (couleurs) est dans `style.h`.

## Fonctionnalités
- Ajout / modification / suppression / affichage d'un matériel (validation des champs, référence unique)
- Recherche par référence ou désignation, tri (référence, désignation, catégorie, quantité, date)
- Export de l'inventaire en CSV (ouvrable dans Excel)
- Statistiques : quantités par catégorie (histogramme), niveaux de stock (anneau critique / suffisant)
- Suivi des mouvements de stock (Entrée / Sortie) avec mise à jour automatique du stock
- Bandeau « Stock critique » quand quantité < seuil minimal

## Données
Base SQLite `axis.db` créée au premier lancement dans le dossier AppData de l'utilisateur
(avec les données de démonstration de la maquette). Supprimez ce fichier pour repartir de zéro.

## Reste à faire
- `MaterielPage::sendAlertEmail()` journalise seulement l'alerte : brancher un vrai envoi (SMTP...).

## Module « Gestion des véhicules » (intégré)
- `vehiculepage.ui` / `vehiculepage.*` : page Véhicules (formulaire, liste triable, alerte de maintenance,
  statistiques par état et par disponibilité, export PDF et CSV). Même structure que `MaterielPage` :
  barre latérale commune (`SidebarFrame`) + page défilante, signaux `menuRequested` / `logoutRequested`.
- `vehicule.*` : structure `Vehicule` et accès SQLite (table `vehicules`, créée au premier lancement avec
  3 véhicules d'exemple, dans la même base `caserne.db` que les autres modules).
- `vehiculedonut.*` : graphiques en anneau (`VehiculeDonutWidget`, distinct de `DonutWidget` de `charts.h`).
- Accès : rôles « Chef de caserne » et « Administrateur » (`Employe::aAcces`, module 2).
- Le style réutilise `style.h` comme la page Matériels (cartes `card`, `cardTitle`, `fieldLabel`, `muted`,
  bandeau `#banner`, boutons `btnAdd`).
