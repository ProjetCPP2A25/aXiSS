-- =====================================================================
-- Module EMPLOYES : script Oracle (SQL Developer, connexion "axis", F5)
-- A executer UNE SEULE FOIS. Si une ligne donne "name is already used" (ORA-00955, ORA-02264,
-- ORA-01430 ...), l'objet existe deja : ignorez l'erreur et continuez.
-- =====================================================================

-- 1) Sequence de l'identifiant (ID_EMPLOYE genere automatiquement par l'application)
CREATE SEQUENCE SEQ_EMPLOYE START WITH 1 INCREMENT BY 1 NOCACHE;

-- 2) Colonnes utilisees par l'application
ALTER TABLE EMPLOYE ADD (POSTE VARCHAR2(50), ALERTE NUMBER(1) DEFAULT 0 NOT NULL);

-- 3) Contraintes
ALTER TABLE EMPLOYE ADD CONSTRAINT UQ_EMPLOYE_EMAIL UNIQUE (EMAIL);
ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_AGE    CHECK (AGE BETWEEN 18 AND 70);
ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_HEURES CHECK (HEURES_TRAVAIL_SEMAINE BETWEEN 0 AND 100);
ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_CONGES CHECK (JOURS_CONGE_RESTANTS >= 0);
ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_GRADE  CHECK (GRADE IN ('Caporal','Sergent','Lieutenant','Capitaine'));
ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_DISPO  CHECK (DISPONIBILITE IN ('Disponible','En intervention','Indisponible'));
ALTER TABLE EMPLOYE ADD CONSTRAINT CK_EMPLOYE_POSTE  CHECK (POSTE IN
  ('Pompier','Chef d''équipe','Chef de caserne','Opérateur de dispatch','Administrateur','Magasinier'));

-- 4) (Optionnel) employes de depart : ignorez cette partie si la table contient deja vos employes
INSERT INTO EMPLOYE (ID_EMPLOYE,NOM,PRENOM,TELEPHONE,EMAIL,GRADE,AGE,HEURES_TRAVAIL_SEMAINE,JOURS_CONGE_RESTANTS,DISPONIBILITE,POSTE)
VALUES (SEQ_EMPLOYE.NEXTVAL,'Gharbi','Sami','20 123 456','sami.gharbi@email.com','Sergent',34,52,4,'En intervention','Chef d''équipe');
INSERT INTO EMPLOYE (ID_EMPLOYE,NOM,PRENOM,TELEPHONE,EMAIL,GRADE,AGE,HEURES_TRAVAIL_SEMAINE,JOURS_CONGE_RESTANTS,DISPONIBILITE,POSTE)
VALUES (SEQ_EMPLOYE.NEXTVAL,'Trabelsi','Rim','22 456 789','rim.trabelsi@email.com','Caporal',28,32,18,'Disponible','Pompier');
COMMIT;

-- 5) Verification
SELECT ID_EMPLOYE, NOM, PRENOM, EMAIL, POSTE FROM EMPLOYE ORDER BY ID_EMPLOYE;
