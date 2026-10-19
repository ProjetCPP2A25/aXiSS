#include "smtp.h"
#include <QSslSocket>
#include <QDateTime>

static QString messageErreur(int code, const QString &reponse)
{
    switch (code) {
    case 535:
    case 534:
    case 530:
        return QString("Authentification refusée (%1). Vérifiez l'email expéditeur et le MOT DE PASSE D'APPLICATION Google "
                       "(pas votre mot de passe Gmail habituel).").arg(code);
    case 550:
    case 553:
    case 501:
        return QString("Adresse email refusée par le serveur (%1) : %2").arg(code).arg(reponse);
    default:
        return QString("Réponse inattendue du serveur (%1) : %2").arg(code).arg(reponse);
    }
}

bool envoyerEmail(const SmtpConfig &cfg, const EmailMessage &m, QString &erreur)
{
    const QString hote = cfg.hote.trimmed();
    const int port = cfg.port;
    const QString user = cfg.user.trimmed();
    const QString to = m.to.trimmed();
    const QString cc = (m.cc.trimmed().toLower() == to.toLower()) ? QString() : m.cc.trimmed();
    QString from = m.from.trimmed();
    if (from.isEmpty() && user.contains('@')) from = user;   // par défaut : l'identifiant s'il s'agit d'un email
    QString pass = cfg.pass;
    pass.remove(' ').remove('\t').remove('\n');   // Google affiche le code avec des espaces : on les retire

    if (hote.isEmpty() || user.isEmpty() || pass.isEmpty() || to.isEmpty() || from.isEmpty()) {
        erreur = "Paramètres email incomplets (identifiant, mot de passe, adresse d'envoi et destinataire sont obligatoires).";
        return false;
    }
    if (!QSslSocket::supportsSsl()) {
        erreur = "Le chiffrement SSL n'est pas disponible dans cette installation de Qt.";
        return false;
    }

    const bool local = (hote == "localhost" || hote == "127.0.0.1");   // tests locaux uniquement
    QSslSocket s;
    QString derniere;   // dernière réponse du serveur

    auto lire = [&](int &code) -> bool {
        QString ligne;
        while (true) {
            // Attente par petits pas : certaines données déjà reçues avec la poignée de main TLS
            // ne déclenchent pas de nouveau « readyRead », donc on re-vérifie régulièrement.
            int attente = 0;
            while (!s.canReadLine()) {
                s.waitForReadyRead(200);
                if (s.canReadLine()) break;
                attente += 200;
                if (attente >= 15000 || s.state() != QAbstractSocket::ConnectedState) {
                    if (s.canReadLine()) break;
                    return false;
                }
            }
            ligne = QString::fromUtf8(s.readLine()).trimmed();
            derniere = ligne;
            if (ligne.size() < 4 || ligne[3] != '-') break;
        }
        code = ligne.left(3).toInt();
        return true;
    };
    auto cmd = [&](const QByteArray &c, int attendu) -> bool {
        if (!c.isEmpty()) {
            s.write(c + "\r\n");
            s.waitForBytesWritten(15000);
        }
        int code = 0;
        if (!lire(code)) { erreur = "Le serveur n'a pas répondu (délai dépassé)."; return false; }
        if (code != attendu) { erreur = messageErreur(code, derniere); return false; }
        return true;
    };
    auto base64 = [](const QByteArray &data) {
        const QByteArray b = data.toBase64();
        QByteArray out;
        for (int i = 0; i < b.size(); i += 76) out += b.mid(i, 76) + "\r\n";
        return out;
    };
    const QString cible = QString("%1:%2").arg(hote).arg(port);

    if (port == 465) {                       // SSL direct
        s.connectToHostEncrypted(hote, quint16(port));
        if (local) s.ignoreSslErrors();
        if (!s.waitForEncrypted(15000)) {
            erreur = "Connexion impossible à " + cible + " : " + s.errorString() +
                     ". Vérifiez la connexion Internet et le pare-feu.";
            return false;
        }
        if (!cmd("", 220) || !cmd("EHLO axis", 250)) return false;
    } else {                                 // STARTTLS (port 587)
        s.connectToHost(hote, quint16(port));
        if (!s.waitForConnected(15000)) {
            erreur = "Connexion impossible à " + cible + " : " + s.errorString() +
                     ". Vérifiez la connexion Internet et le pare-feu.";
            return false;
        }
        if (!cmd("", 220) || !cmd("EHLO axis", 250) || !cmd("STARTTLS", 220)) return false;
        if (local) s.ignoreSslErrors();
        s.startClientEncryption();
        if (!s.waitForEncrypted(15000)) {
            erreur = "Échec du chiffrement TLS : " + s.errorString();
            return false;
        }
        if (!cmd("EHLO axis", 250)) return false;
    }

    if (!cmd("AUTH LOGIN", 334) || !cmd(user.toUtf8().toBase64(), 334) || !cmd(pass.toUtf8().toBase64(), 235) ||
        !cmd("MAIL FROM:<" + from.toUtf8() + ">", 250) || !cmd("RCPT TO:<" + to.toUtf8() + ">", 250))
        return false;
    if (!cc.isEmpty() && !cmd("RCPT TO:<" + cc.toUtf8() + ">", 250)) return false;
    if (!cmd("DATA", 354)) return false;

    QByteArray msg;
    const QString nom = m.nomFrom.trimmed().isEmpty() ? QString("A.X.I.S") : m.nomFrom.trimmed();
    msg += "From: =?UTF-8?B?" + nom.toUtf8().toBase64() + "?= <" + from.toUtf8() + ">\r\n";
    msg += "To: " + to.toUtf8() + "\r\n";
    if (!cc.isEmpty()) msg += "Cc: " + cc.toUtf8() + "\r\n";
    if (!m.replyTo.trimmed().isEmpty()) msg += "Reply-To: " + m.replyTo.trimmed().toUtf8() + "\r\n";
    msg += "Date: " + QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    msg += "Subject: =?UTF-8?B?" + m.sujet.toUtf8().toBase64() + "?=\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: base64\r\n\r\n";
    msg += base64(m.corps.toUtf8());
    msg += ".\r\n";
    s.write(msg);
    s.waitForBytesWritten(15000);
    int code = 0;
    if (!lire(code)) { erreur = "Le serveur n'a pas confirmé la réception du message."; return false; }
    if (code != 250) { erreur = messageErreur(code, derniere); return false; }

    s.write("QUIT\r\n");
    s.waitForBytesWritten(3000);
    s.disconnectFromHost();
    return true;
}
