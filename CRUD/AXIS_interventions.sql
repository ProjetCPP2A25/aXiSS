-- A.X.I.S - Table INTERVENTION (Oracle)
-- A executer une fois avec l'utilisateur AXIS (SQL Developer / SQL*Plus).
-- Les caracteres accentues sont ecrits avec UNISTR pour ne pas dependre de l'encodage du script :
--   Elevee = UNISTR('\00C9lev\00E9e')   Terminee = UNISTR('Termin\00E9e')

CREATE TABLE INTERVENTION (
    ID                 NUMBER(10)     NOT NULL,
    TYPE_INTERVENTION  VARCHAR2(30)   NOT NULL,
    LIEU               VARCHAR2(100)  NOT NULL,
    GRAVITE            VARCHAR2(20)   NOT NULL,
    ETAT               VARCHAR2(20)   NOT NULL,
    LATITUDE           NUMBER(9,6)    NOT NULL,
    LONGITUDE          NUMBER(9,6)    NOT NULL,
    DATE_RECEPTION     DATE           NOT NULL,
    DATE_FIN           DATE,
    CONSTRAINT PK_INTERVENTION PRIMARY KEY (ID),
    CONSTRAINT CK_INTERVENTION_ID CHECK (ID > 0),
    CONSTRAINT CK_INTERVENTION_TYPE CHECK (TYPE_INTERVENTION IN ('Incendie', 'Accident', 'Inondation')),
    CONSTRAINT CK_INTERVENTION_LIEU CHECK (LENGTH(TRIM(LIEU)) > 0),
    CONSTRAINT CK_INTERVENTION_GRAVITE CHECK (GRAVITE IN (UNISTR('\00C9lev\00E9e'), 'Moyenne', 'Faible')),
    CONSTRAINT CK_INTERVENTION_ETAT CHECK (ETAT IN ('En cours', UNISTR('Termin\00E9e'))),
    CONSTRAINT CK_INTERVENTION_GPS CHECK (LATITUDE BETWEEN -90 AND 90 AND LONGITUDE BETWEEN -180 AND 180),
    -- Une intervention terminee a une date de fin posterieure a la reception ; une intervention en cours n'en a pas.
    CONSTRAINT CK_INTERVENTION_FIN CHECK (
        (ETAT = 'En cours' AND DATE_FIN IS NULL)
        OR (ETAT = UNISTR('Termin\00E9e') AND DATE_FIN IS NOT NULL AND DATE_FIN >= DATE_RECEPTION))
);

-- Les 8 interventions d'exemple de la version precedente
INSERT INTO INTERVENTION VALUES (1, 'Incendie',   'Ben Arous',    UNISTR('\00C9lev\00E9e'), 'En cours',              36.7534, 10.2231, TO_DATE('2026-09-30 14:32', 'YYYY-MM-DD HH24:MI'), NULL);
INSERT INTO INTERVENTION VALUES (2, 'Accident',   'Tunis centre', 'Moyenne',                UNISTR('Termin\00E9e'),  36.8065, 10.1815, TO_DATE('2026-09-30 11:05', 'YYYY-MM-DD HH24:MI'), TO_DATE('2026-09-30 11:35', 'YYYY-MM-DD HH24:MI'));
INSERT INTO INTERVENTION VALUES (3, 'Inondation', 'Ariana',       'Faible',                 UNISTR('Termin\00E9e'),  36.8625, 10.1956, TO_DATE('2026-09-30 09:40', 'YYYY-MM-DD HH24:MI'), TO_DATE('2026-09-30 12:50', 'YYYY-MM-DD HH24:MI'));
INSERT INTO INTERVENTION VALUES (4, 'Incendie',   'Ben Arous',    UNISTR('\00C9lev\00E9e'), UNISTR('Termin\00E9e'),  36.7480, 10.2290, TO_DATE('2026-09-12 18:20', 'YYYY-MM-DD HH24:MI'), TO_DATE('2026-09-12 19:55', 'YYYY-MM-DD HH24:MI'));
INSERT INTO INTERVENTION VALUES (5, 'Accident',   'Ben Arous',    'Moyenne',                UNISTR('Termin\00E9e'),  36.7560, 10.2180, TO_DATE('2026-08-27 07:45', 'YYYY-MM-DD HH24:MI'), TO_DATE('2026-08-27 08:25', 'YYYY-MM-DD HH24:MI'));
INSERT INTO INTERVENTION VALUES (6, 'Inondation', 'Ariana',       'Moyenne',                UNISTR('Termin\00E9e'),  36.8601, 10.1932, TO_DATE('2026-09-18 06:30', 'YYYY-MM-DD HH24:MI'), TO_DATE('2026-09-18 10:50', 'YYYY-MM-DD HH24:MI'));
INSERT INTO INTERVENTION VALUES (7, 'Accident',   'Carthage',     'Faible',                 UNISTR('Termin\00E9e'),  36.8528, 10.3233, TO_DATE('2026-09-22 20:05', 'YYYY-MM-DD HH24:MI'), TO_DATE('2026-09-22 20:40', 'YYYY-MM-DD HH24:MI'));
INSERT INTO INTERVENTION VALUES (8, 'Incendie',   'Tunis centre', UNISTR('\00C9lev\00E9e'), 'En cours',              36.8100, 10.1850, TO_DATE('2026-10-01 16:40', 'YYYY-MM-DD HH24:MI'), NULL);

COMMIT;
