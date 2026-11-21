-- =====================================================================
-- Page de connexion et droits d'acces (table EMPLOYE)
-- A executer dans SQL Developer (connexion "axis"). Executez les blocs UN PAR UN (F5).
-- Si une commande dit "nom de colonne en double" : la colonne existe deja, passez a la suite.
-- =====================================================================

-- 1) Colonnes du cahier de specification : Poste et Mot de passe (empreinte, jamais en clair)
ALTER TABLE EMPLOYE ADD (POSTE VARCHAR2(50));
ALTER TABLE EMPLOYE ADD (MOT_DE_PASSE VARCHAR2(64));

-- 2) Donner un poste a chaque employe. Postes qui ont acces a l'application et leurs modules :
--      Administrateur        -> Employes, Vehicules, Gardes
--      Chef de caserne       -> tous les modules
--      Opérateur de dispatch -> Interventions
--      Magasinier            -> Materiels
--    (Pompier et Chef d'equipe n'utilisent pas l'application.)
--    REMPLACEZ les faux emails ci-dessous par les vrais emails de votre table EMPLOYE :
UPDATE EMPLOYE SET POSTE = 'Chef de caserne'        WHERE LOWER(EMAIL) = 'email_du_chef@email.com';
UPDATE EMPLOYE SET POSTE = 'Administrateur'         WHERE LOWER(EMAIL) = 'email_admin@email.com';
UPDATE EMPLOYE SET POSTE = 'Opérateur de dispatch'  WHERE LOWER(EMAIL) = 'email_dispatch@email.com';
-- UPDATE EMPLOYE SET POSTE = 'Magasinier'          WHERE LOWER(EMAIL) = 'email_magasinier@email.com';
COMMIT;
-- (On peut aussi changer le poste avec le bouton "Modifier" du module Employes.)

-- 3) Mot de passe de depart "Axis@2026" pour tous les employes qui ont un poste avec acces.
--    Chacun peut ensuite le changer avec "Mot de passe oublie ?" (email + ID employe).
UPDATE EMPLOYE
   SET MOT_DE_PASSE = LOWER(RAWTOHEX(STANDARD_HASH(ID_EMPLOYE || ':' || 'Axis@2026', 'SHA256')))
 WHERE MOT_DE_PASSE IS NULL
   AND POSTE IN ('Administrateur', 'Chef de caserne', 'Opérateur de dispatch', 'Magasinier');
COMMIT;

-- 3 bis) Compte de Karim Majri : poste Chef de caserne et mot de passe 1234
UPDATE EMPLOYE
   SET POSTE = 'Chef de caserne',
       MOT_DE_PASSE = LOWER(RAWTOHEX(STANDARD_HASH(ID_EMPLOYE || ':' || '1234', 'SHA256')))
 WHERE UPPER(NOM) = 'MAJRI' AND UPPER(PRENOM) = 'KARIM';
COMMIT;

-- 4) Verification
SELECT ID_EMPLOYE, NOM, EMAIL, POSTE, CASE WHEN MOT_DE_PASSE IS NULL THEN 'non' ELSE 'oui' END AS MDP_DEFINI
  FROM EMPLOYE ORDER BY ID_EMPLOYE;
