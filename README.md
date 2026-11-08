# A.X.I.S – RedFlame Protection (Qt / C++)

Projet réduit à : **les interfaces + leur intégration + les CRUD des modules Employés et Interventions**.

## Ce qui fonctionne
- **Gestion des employés** (Oracle via ODBC) : ajouter, modifier, supprimer, afficher / lister, contrôle de saisie.
  Code : `employe.h/.cpp` (classe `Employe` + barre latérale `SidebarFrame`), appelé depuis `axis.cpp`.
- **Navigation** : menu principal → chaque module, et « Menu principal » / « Quitter » dans chaque page.

- **Gestion des interventions** (Oracle via ODBC, table `INTERVENTION`) : ajouter, afficher (détails), modifier, supprimer
  (avec confirmation), vider le formulaire, rafraîchir le tableau, contrôle de saisie.
  Code : `intervention.h/.cpp` (classe métier `Intervention`, toutes les requêtes SQL préparées), interface dans `axis.ui`
  (page `PInterv`) et `axis.h/.cpp` (section « MODULE INTERVENTIONS »).
  Limitation provisoire : la garde (`ID_GARDE`) n'est pas saisissable ; la première garde existante est utilisée
  (l'attribution automatique des gardes viendra plus tard).
  Recherche, tri, pagination, export, statistiques, zones à risque et carte : interface conservée, sans logique.

## Ce qui est affichage seul (boutons non fonctionnels)
- Véhicules (page `PVehicule` de `axis.ui`), Matériels (page `PMateriel` de `axis.ui`) :
  l'interface est intacte, mais aucun bouton n'est relié à une action et il n'y a plus de base de données pour ces modules.
  `AXIS::initialiserModuleVehicules()` et `AXIS::initialiserModuleMateriels()` ne font que préparer les tableaux,
  ajouter la barre latérale et brancher la navigation.
- Garde : pas encore d'interface (le module reste dans son état actuel).

## Fichiers
- Interface unique : `axis.ui` (fenêtre principale + menu + pages Employés, Interventions, Véhicules, Matériels).
  Les pages Véhicules et Matériels sont des `QScrollArea` ; leur feuille de style est dans le `styleSheet` de la page.
  Les widgets de ces pages gardent leurs noms, sauf les 23 noms communs aux deux pages qui sont suffixés `Veh` / `Mat`
  (ex. `tableVeh`, `tableMat`, `bannerVeh`, `bannerMat`).
- Intégration : `main.cpp`, `axis.h`, `axis.cpp`
- Widgets personnalisés utilisés par `axis.ui` : `charts.*` (`BarChartWidget`, `DonutWidget`), `vehiculedonut.*`
- Style : `style.qss`, `ressources.qrc` (logo, flèche)
- Classes métier / base : `employe.*`, `intervention.*`, `connexion.*` (connexion Oracle commune), `sql/oracle_employe.sql`

## Compilation
Qt 6 (modules Widgets et Sql, pilote QODBC), compilateur C++17. Ouvrir `AXIS.pro` dans Qt Creator.

## Oracle
- Exécuter `sql/oracle_employe.sql` dans SQL Developer (connexion `axis`).
- Source ODBC `AXIS_ODBC`, utilisateur et mot de passe dans `connexion.cpp` — ne jamais pousser le vrai mot de passe sur GitHub.
