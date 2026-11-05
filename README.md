# A.X.I.S — Gestion des employés

Application desktop Qt/C++ pour la connexion des utilisateurs, la gestion des employés (CRUD), les statistiques et l’alerte visuelle de surcharge horaire.

## Fichiers principaux
- `main.cpp` : démarrage de l’application.
- `axis.ui`, `axis.h`, `axis.cpp` : interface et logique de l’application.
- `employe.h`, `employe.cpp` : classe métier Employe, CRUD, authentification, statistiques et barre latérale (`SidebarFrame`).
- `connexion.h`, `connexion.cpp` : ouverture et initialisation de la base SQLite locale.
- `ressources.qrc`, `style.qss`, `logo.png` : ressources et style.

Les modules Congés et l’envoi d’emails/SMTP ont été retirés. La barre latérale et les fonctions de réinitialisation du mot de passe sont intégrées à `employe.h` et `employe.cpp`. L’adresse email de l’employé reste utilisée comme identifiant de connexion et comme donnée de contact.

## Base de données
Oracle XE via ODBC (`QODBC`), source de données `axis` (Administrateur ODBC 64 bits). Les tables sont créées dans SQL Developer (`sql/script_axis.sql`), pas dans le code.
Table `EMPLOYE` : ID_EMPLOYE (séquence `SEQ_EMPLOYE`), NOM, PRENOM, TELEPHONE, EMAIL, GRADE, POSTE, AGE, HEURES_TRAVAIL_SEMAINE, JOURS_CONGE_RESTANTS, DISPONIBILITE, MOT_DE_PASSE (SHA-256), ALERTE.
Mot de passe Oracle : à saisir dans `connexion.cpp` (ne pas le pousser sur GitHub).

## Compilation
Ouvrir `AXIS.pro` dans Qt Creator, configurer un kit Qt Desktop compatible puis lancer qmake et compiler.

Comptes de démonstration initialisés lors de la première création de la base (mot de passe : `1234`) :
- `karim.majri@email.com` — Chef de caserne
- `admin@axis.com` — Administrateur
- `leila.mansouri@email.com` — Opérateur de dispatch
- `youssef.bensalah@email.com` — Magasinier


La page « Mot de passe oublié » est conservée : elle permet de définir un nouveau mot de passe localement à partir de l’adresse email enregistrée, sans SMTP ni envoi de courriel. Le code de la barre latérale et la méthode de réinitialisation sont intégrés dans `employe.cpp`.
