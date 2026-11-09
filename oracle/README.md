# Scripts Oracle — AXIS Véhicules

## Ordre d'exécution

1. Ouvrir SQL Developer et se connecter avec le compte Oracle de l'application.
2. Ouvrir et exécuter `01_schema.sql` une seule fois. Il crée les tables `VEHICULE` et `ENTRETIEN_VEHICULE`, leurs contraintes, index et la séquence des entretiens.
3. Facultatif : exécuter `02_donnees_exemple.sql` pour insérer trois véhicules de démonstration. Les insertions sont protégées contre les doublons d'ID.
4. Configurer dans l'application le DSN ODBC, l'utilisateur et le mot de passe Oracle correspondant à cette même connexion.

## Remarques

- Exécuter `01_schema.sql` dans le schéma qui possède les tables. L'application utilise les noms sans préfixe de schéma.
- `ID_VEHICULE` est attribué actuellement par le code C++ en prenant le plus grand ID existant puis en ajoutant 1. Les insertions simultanées depuis plusieurs postes pourraient nécessiter une séquence dédiée et une adaptation du code.
- La séquence `SEQ_ENTRETIEN_VEHICULE` est appelée par le module de gestion des entretiens.
- Les scripts ne créent ni utilisateur Oracle, ni DSN ODBC, ni identifiants de connexion.
- Si les tables existent déjà, ne relancez pas `01_schema.sql` : Oracle retournera une erreur d'objet déjà existant.
