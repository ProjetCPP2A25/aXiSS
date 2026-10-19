# A.X.I.S — Employés, Interventions et Véhicules (version intégrée)

Application unique : connexion -> menu principal -> modules.

| Module | Rôles autorisés |
|---|---|
| Gestion des employés | Administrateur, Chef de caserne |
| Gestion des interventions | Opérateur de dispatch, Chef de caserne |
| Gestion des véhicules | Chef de caserne, Administrateur |
| Gestion des matériels | pas encore intégré (bouton masqué) |

Comptes de départ (mot de passe 1234) : admin@axis.com (Administrateur), karim.majri@email.com (Chef de caserne),
leila.mansouri@email.com (Opérateur de dispatch).

## Ouvrir le projet
1. Décompresser l'archive.
2. Ouvrir `AXIS.pro` dans Qt Creator (kit Qt 6 avec Widgets, Network, SQL).
3. Lancer qmake puis « Reconstruire le projet ».

## Ce qui a été intégré pour les véhicules
- Page `VehiclesPage` placée dans `PVehicule` ; bouton « Gestion des véhicules » du menu activé selon le rôle.
- Barre latérale commune (Menu principal / Déconnexion / utilisateur connecté), comme les autres modules.
- Données dans la table SQLite `vehicules` de la même base `caserne.db` (au lieu de `vehicules.json`).
  Au premier lancement : reprise de l'ancien `vehicules.json` s'il existe, sinon 3 véhicules d'exemple.
- Classes renommées pour éviter le conflit avec `DonutWidget` des interventions : `VehicleDonutWidget`, `VehicleSlice`.
- Feuille de style du module limitée à sa page (n'affecte pas les autres modules).
- Ajouts : bouton « Exporter (PDF) », contrôle d'immatriculation en double, bouton d'alerte maintenance par email
  (utilise la configuration SMTP existante) à la place du bouton SMS qui n'était qu'une maquette.

## Correction dans axis.ui
`PInterv` contenait déjà un layout + des textes provisoires, alors que le code ajoutait un 2e layout (Qt l'ignore) :
la page Interventions n'était pas dimensionnée correctement. Les pages `PInterv` et `PVehicule` ont maintenant un layout vide.
