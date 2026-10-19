#pragma once
#include <QString>

// Paramètres du serveur SMTP
struct SmtpConfig {
    QString hote;
    int port = 465;
    QString user, pass;      // identifiant + mot de passe / clé SMTP
};

// Un email à envoyer
struct EmailMessage {
    QString from;            // adresse d'envoi (doit être validée chez le fournisseur)
    QString nomFrom;         // nom affiché de l'expéditeur (ex. « Karim Majri (Chef de caserne) »)
    QString to, cc, replyTo;
    QString sujet, corps;
};

// Envoi via SMTP sécurisé : port 465 = SSL direct, autres ports (587, 2525) = STARTTLS.
bool envoyerEmail(const SmtpConfig &cfg, const EmailMessage &m, QString &erreur);
