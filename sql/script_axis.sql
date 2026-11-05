-- =====================================================================
-- A executer UNE SEULE FOIS dans SQL Developer (connexion "axis"), F5.
-- Ajoute les colonnes necessaires a l'application AXIS et un compte admin.
-- =====================================================================

-- 1) Colonnes manquantes dans la table EMPLOYE
ALTER TABLE EMPLOYE ADD (POSTE VARCHAR2(50), ALERTE NUMBER(1) DEFAULT 0 NOT NULL);

ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_POSTE CHECK (POSTE IN
  ('Pompier','Chef d''équipe','Chef de caserne','Opérateur de dispatch','Administrateur','Magasinier'));

-- 2) Compte administrateur pour se connecter (mot de passe : 1234, stocke en SHA-256)
INSERT INTO EMPLOYE (ID_EMPLOYE, NOM, PRENOM, TELEPHONE, EMAIL, GRADE, AGE,
                     HEURES_TRAVAIL_SEMAINE, JOURS_CONGE_RESTANTS, DISPONIBILITE, MOT_DE_PASSE, POSTE)
VALUES (SEQ_EMPLOYE.NEXTVAL, 'AXIS', 'Admin', '71 000 000', 'admin@axis.com', 'Capitaine', 45,
        40, 20, 'Disponible',
        '03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4', 'Administrateur');
COMMIT;

SELECT ID_EMPLOYE, NOM, PRENOM, EMAIL, POSTE FROM EMPLOYE;
