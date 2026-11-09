#include "gaxis.h"
#include "ui_gaxis.h"
#include "entretiendialog.h"
#include "vehicule.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

namespace {

const char *FORMAT_DATE = "dd/MM/yyyy";
const int HAUTEUR_LIGNE = 52;

// Feuille de style de l'application (les widgets utilisent la propriété "role")
const char *kStyle = R"QSS(
QWidget { background:#1c1e21; color:#e6e6e6; font-family:"Montserrat","Segoe UI","Inter",sans-serif; font-size:13px; }
QLabel { background:transparent; }
#sidebar { background:#232528; }
#sidebar QLabel { background:transparent; }
#sidebar QPushButton { text-align:left; padding:9px 10px; border:none; border-radius:6px; background:transparent; color:#e6e6e6; font-size:14px; }
#sidebar QPushButton:hover { background:#2e3033; }
QFrame[role="card"] { background:#232528; border:1px solid #33363a; border-radius:8px; }
QLabel[role="cardTitle"] { color:#d90000; font-size:15px; font-weight:700; }
QLabel[role="pageTitle"] { font-size:21px; font-weight:700; }
QLabel[role="muted"] { color:#b0b3b8; font-size:12px; }
QLabel[role="alert"] { background:#fdf0dc; color:#b45f06; border-left:4px solid #e8830c; border-radius:4px; padding:8px 12px; }
QLineEdit, QComboBox { background:#1c1e21; border:1px solid #3a3d41; border-radius:5px; padding:6px 8px; color:#e6e6e6; }
QLineEdit:focus, QComboBox:focus { border-color:#ff0000; }
QComboBox QAbstractItemView { background:#232528; selection-background-color:#ff0000; border:1px solid #3a3d41; }
QPushButton[role="primary"] { background:#ff0000; color:white; border:none; border-radius:5px; padding:7px 16px; font-weight:600; }
QPushButton[role="primary"]:hover { background:#d90000; }
QPushButton[role="ghost"] { background:transparent; border:1px solid #3a3d41; border-radius:5px; padding:7px 14px; }
QPushButton[role="ghost"]:hover { background:#2e3033; }
QTableWidget { background:transparent; border:none; gridline-color:transparent; outline:0; }
QTableWidget::item { border-bottom:1px solid #33363a; padding:4px; }
QHeaderView::section { background:transparent; color:#b0b3b8; border:none; padding:6px; font-weight:500; }
QScrollArea { border:none; }
QScrollBar:horizontal { background:#1c1e21; height:12px; }
QScrollBar::handle:horizontal { background:#4a4d52; border-radius:5px; min-width:30px; }
QScrollBar:vertical { background:#1c1e21; width:12px; }
QScrollBar::handle:vertical { background:#4a4d52; border-radius:5px; min-height:30px; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
)QSS";

struct Couleurs { QString fond, texte; };

Couleurs couleursPour(const QString &valeur)
{
    if (valeur == "Bon" || valeur == "Disponible") return {"#dff5e6", "#1b7f43"};
    if (valeur == "À surveiller" || valeur == "En maintenance") return {"#fdebd3", "#b45f06"};
    return {"#fde0e0", "#c0171d"};   // Hors service, En intervention
}

QWidget *creerBadge(const QString &texte)
{
    const Couleurs c = couleursPour(texte);
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(4, 0, 4, 0);
    auto *b = new QLabel(texte);
    b->setStyleSheet(QString("background:%1;color:%2;border-radius:9px;padding:2px 9px;"
                             "font-size:11px;font-weight:600;")
                         .arg(c.fond, c.texte));
    l->addWidget(b);
    l->addStretch();
    return w;
}

QString fmt(const QDate &d) { return d.isValid() ? d.toString(FORMAT_DATE) : QString("—"); }
QString kmTexte(int km) { return QLocale(QLocale::French).toString(km); }

struct Part { int valeur; QColor couleur; };

// Dessine l'anneau de statistiques (100 x 100) dans un QLabel
QPixmap dessinerAnneau(const QVector<Part> &parts, qreal dpr)
{
    const int cote = 100;
    QPixmap pm(QSize(qRound(cote * dpr), qRound(cote * dpr)));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    int total = 0;
    for (const Part &s : parts) total += s.valeur;

    const qreal pw = cote * 0.17;
    const QRectF r(pw / 2, pw / 2, cote - pw, cote - pw);
    QPen pen;
    pen.setWidthF(pw);
    pen.setCapStyle(Qt::FlatCap);

    if (total == 0) {
        pen.setColor(QColor("#3a3d41"));
        p.setPen(pen);
        p.drawEllipse(r);
    } else {
        int debut = 90 * 16;
        for (const Part &s : parts) {
            if (s.valeur <= 0) continue;
            const int angle = -qRound(5760.0 * s.valeur / total);
            pen.setColor(s.couleur);
            p.setPen(pen);
            p.drawArc(r, debut, angle);
            debut += angle;
        }
    }

    p.setPen(QColor("#e6e6e6"));
    QFont f = p.font();
    f.setPointSizeF(15);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(0, 0, cote, cote * 0.58), Qt::AlignHCenter | Qt::AlignBottom, QString::number(total));
    f.setPointSizeF(8);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#b0b3b8"));
    p.drawText(QRectF(0, cote * 0.58, cote, cote * 0.3), Qt::AlignHCenter | Qt::AlignTop, "total");
    return pm;
}

} // namespace

// ------------------------------------------------------------------ construction
GAxis::GAxis(QWidget *parent) : QMainWindow(parent), ui(new Ui::GAxis)
{
    ui->setupUi(this);
    setStyleSheet(kStyle);

    ui->cbType->addItems(Vehicule::types());
    ui->cbEtat->addItems(Vehicule::etats());
    ui->cbDispo->addItems(Vehicule::disponibilites());

    auto *btnEntretiens = new QPushButton("🛠 Entretiens", ui->sidebar);
    btnEntretiens->setCursor(Qt::PointingHandCursor);
    ui->sidebarLayout->insertWidget(5, btnEntretiens);
    connect(btnEntretiens, &QPushButton::clicked, this, [this] {
        GestionEntretiensDialog dialogue(this);
        dialogue.exec();
        rafraichir();
    });

    // Contrôle de saisie à la frappe : le kilométrage n'accepte que des chiffres (et espaces)
    ui->leKm->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9 ]{0,9}"), this));
    ui->lePlaque->setMaxLength(14);

    QTableWidget *t = ui->tableVehicules;
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setShowGrid(false);
    t->setWordWrap(true);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(HAUTEUR_LIGNE);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setFixedHeight(44);

    connect(ui->btnSubmit, &QPushButton::clicked, this, &GAxis::onSubmit);
    connect(ui->btnVider, &QPushButton::clicked, this, &GAxis::viderFormulaire);
    connect(ui->btnExcel, &QPushButton::clicked, this, &GAxis::exporterExcel);
    connect(ui->leRecherche, &QLineEdit::textChanged, this, [this] { rafraichir(); });
    connect(ui->cbTri, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { rafraichir(); });
    m_smsNetwork = new QNetworkAccessManager(this);
    connect(ui->btnSms, &QPushButton::clicked, this, &GAxis::envoyerAlerteSms);

    rafraichir();
}

GAxis::~GAxis()
{
    delete ui;
}

void GAxis::envoyerAlerteSms()
{
    const QString accountSid = qEnvironmentVariable("TWILIO_ACCOUNT_SID").trimmed();
    const QString authToken = qEnvironmentVariable("TWILIO_AUTH_TOKEN").trimmed();
    const QString fromNumber = qEnvironmentVariable("TWILIO_FROM_NUMBER").trimmed();
    if (accountSid.isEmpty() || authToken.isEmpty() || fromNumber.isEmpty()) {
        QMessageBox::warning(
            this, "Configuration SMS",
            "Configurez les variables d'environnement TWILIO_ACCOUNT_SID, "
            "TWILIO_AUTH_TOKEN et TWILIO_FROM_NUMBER, puis relancez l'application.\n\n"
            "Un compte Twilio actif et un numéro expéditeur autorisé sont nécessaires.");
        return;
    }

    QString erreur;
    const QVector<Vehicule> aEntretenir = Vehicule::maintenanceRequise(&erreur);
    if (!erreur.isEmpty()) {
        QMessageBox::critical(this, "Alerte SMS", "Impossible de lire les véhicules :\n" + erreur);
        return;
    }
    if (aEntretenir.isEmpty()) {
        QMessageBox::information(this, "Alerte SMS", "Aucun véhicule n'a une maintenance échue à signaler.");
        return;
    }

    bool ok = false;
    const QString destinataire = QInputDialog::getText(
        this, "Envoyer une alerte SMS", "Numéro du destinataire au format international (ex. +216XXXXXXXX) :",
        QLineEdit::Normal, QString(), &ok).trimmed();
    static const QRegularExpression formatTelephone(R"(^\+[1-9][0-9]{7,14}$)");
    if (!ok) return;
    if (!formatTelephone.match(destinataire).hasMatch()) {
        QMessageBox::warning(this, "Numéro invalide",
                             "Utilisez le format international E.164, par exemple +216XXXXXXXX.");
        return;
    }

    QStringList details;
    const int limite = qMin(aEntretenir.size(), 8);
    for (int i = 0; i < limite; ++i) {
        const Vehicule &vehicule = aEntretenir[i];
        details << QString("%1 : échéance %2").arg(vehicule.plaque, fmt(vehicule.prochaineMaint));
    }
    if (aEntretenir.size() > limite)
        details << QString("et %1 autre(s)").arg(aEntretenir.size() - limite);
    const QString message = QString("AXIS - %1 véhicule(s) nécessitent une maintenance :\n%2")
                                .arg(aEntretenir.size())
                                .arg(details.join("\n"));

    if (QMessageBox::question(this, "Confirmer l'envoi",
                              QString("Envoyer cette alerte à %1 ?\n\n%2").arg(destinataire, message))
        != QMessageBox::Yes) {
        return;
    }

    const QUrl url(QString("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json").arg(accountSid));
    QNetworkRequest request(url);
    const QByteArray identifiants = (accountSid + ":" + authToken).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + identifiants);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery donnees;
    donnees.addQueryItem("From", fromNumber);
    donnees.addQueryItem("To", destinataire);
    donnees.addQueryItem("Body", message);
    QNetworkReply *reply = m_smsNetwork->post(request, donnees.toString(QUrl::FullyEncoded).toUtf8());
    ui->btnSms->setEnabled(false);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray contenu = reply->readAll();
        QString erreur = reply->errorString();
        const QString detailApi = QJsonDocument::fromJson(contenu).object().value("message").toString();
        reply->deleteLater();
        ui->btnSms->setEnabled(true);

        if (status >= 200 && status < 300) {
            QMessageBox::information(this, "Alerte SMS", "La demande d'envoi SMS a été acceptée par Twilio.");
        } else {
            if (!detailApi.isEmpty()) erreur = detailApi;
            QMessageBox::critical(this, "Échec de l'envoi SMS",
                                  QString("Twilio n'a pas accepté le SMS (HTTP %1).\n%2").arg(status).arg(erreur));
        }
    });
}

// ------------------------------------------------------------------ affichage
void GAxis::rafraichir()
{
    QString erreur;
    const QVector<Vehicule> liste =
        Vehicule::lister(ui->leRecherche->text().trimmed(), ui->cbTri->currentIndex(), &erreur);
    if (!erreur.isEmpty()) {
        QMessageBox::critical(this, "Base de données", erreur);
        return;
    }
    remplirTableau(liste);
    afficherAlerte();
    afficherStatistiques();
}

void GAxis::remplirTableau(const QVector<Vehicule> &liste)
{
    QTableWidget *t = ui->tableVehicules;
    t->setRowCount(liste.size());

    for (int r = 0; r < liste.size(); ++r) {
        const Vehicule &v = liste[r];
        const QStringList cellules = {v.id, v.plaque, v.type, kmTexte(v.km)};
        for (int c = 0; c < 4; ++c) t->setItem(r, c, new QTableWidgetItem(cellules[c]));
        t->setCellWidget(r, 4, creerBadge(v.etat));
        t->setCellWidget(r, 5, creerBadge(v.dispo));
        t->setItem(r, 6, new QTableWidgetItem(fmt(v.derniereMaint)));
        t->setItem(r, 7, new QTableWidgetItem(fmt(v.prochaineMaint)));
        t->setItem(r, 8, new QTableWidgetItem(fmt(v.assurance)));
        t->setItem(r, 9, new QTableWidgetItem(fmt(v.visiteTech)));

        const QString st = "style='color:#e6e6e6;text-decoration:none;'";
        auto *actions = new QLabel(QString("<a href='view' %1>Afficher</a> · <a href='edit' %1>Modifier</a> · "
                                           "<a href='del' %1>Supprimer</a>").arg(st));
        actions->setTextFormat(Qt::RichText);
        actions->setCursor(Qt::PointingHandCursor);
        actions->setContentsMargins(4, 0, 4, 0);
        const QString id = v.id;
        connect(actions, &QLabel::linkActivated, this, [this, id](const QString &lien) {
            if (lien == "view") afficherVehicule(id);
            else if (lien == "edit") modifierVehicule(id);
            else supprimerVehicule(id);
        });
        t->setCellWidget(r, 10, actions);
    }
    t->setFixedHeight(44 + qMax(1, int(liste.size())) * HAUTEUR_LIGNE + 16);
}

void GAxis::afficherAlerte()
{
    QString erreur;
    const QVector<Vehicule> liste = Vehicule::maintenanceRequise(&erreur);
    QStringList alertes;
    for (const Vehicule &v : liste)
        alertes << QString("%1 (état %2, prochaine maintenance le %3)")
                       .arg(v.id, v.etat.toLower(), fmt(v.prochaineMaint));
    ui->lblAlerte->setVisible(!alertes.isEmpty());
    ui->lblAlerte->setText("Maintenance requise : " + alertes.join(" ; ") + ".");
}

void GAxis::afficherStatistiques()
{
    const QColor vert("#1f9d55"), orange("#c2670f"), rouge("#c81e1e");

    auto remplir = [this](QLabel *anneau, QLabel *legende, bool parEtat, const QStringList &cles,
                          const QVector<QColor> &couleurs) {
        const QMap<QString, int> stats = Vehicule::statistiques(parEtat);
        int total = 0;
        for (const QString &k : cles) total += stats.value(k);

        QVector<Part> parts;
        QString html = "<table cellspacing='4'>";
        for (int i = 0; i < cles.size(); ++i) {
            const int n = stats.value(cles[i]);
            parts.push_back({n, couleurs[i]});
            const int pct = total ? qRound(100.0 * n / total) : 0;
            html += QString("<tr><td><span style='color:%1'>●</span></td><td>%2 <b>%3</b> (%4%)</td></tr>")
                        .arg(couleurs[i].name(), cles[i]).arg(n).arg(pct);
        }
        anneau->setPixmap(dessinerAnneau(parts, anneau->devicePixelRatioF()));
        legende->setText(html + "</table>");
    };

    remplir(ui->lblDonutEtat, ui->lblLegendeEtat, true, Vehicule::etats(), {vert, orange, rouge});
    remplir(ui->lblDonutDispo, ui->lblLegendeDispo, false, Vehicule::disponibilites(), {vert, rouge, orange});
}

// ------------------------------------------------------------------ actions CRUD
void GAxis::viderFormulaire()
{
    m_idEdition.clear();
    ui->btnSubmit->setText("Ajouter");
    ui->lblFormTitre->setText("Ajouter");
    for (QLineEdit *e : {ui->lePlaque, ui->leKm, ui->leDerniere, ui->leProchaine, ui->leAssurance, ui->leVisite})
        e->clear();
    ui->cbType->setCurrentIndex(0);
    ui->cbEtat->setCurrentIndex(0);
    ui->cbDispo->setCurrentIndex(0);
}

void GAxis::onSubmit()
{
    Vehicule v;
    v.plaque = Vehicule::normaliserPlaque(ui->lePlaque->text());

    QString kmTxt = ui->leKm->text();
    kmTxt.remove(QRegularExpression("[\\s\\x{202F}\\x{00A0}]"));
    bool ok = false;
    v.km = kmTxt.toInt(&ok);
    if (!ok || v.km < 0) {
        QMessageBox::warning(this, "Kilométrage invalide", "Saisissez un kilométrage numérique (ex. 42 300).");
        return;
    }

    struct Champ { QLineEdit *edit; QDate *date; const char *nom; };
    const Champ champs[] = {{ui->leDerniere, &v.derniereMaint, "Dernière maintenance"},
                            {ui->leProchaine, &v.prochaineMaint, "Prochaine maintenance"},
                            {ui->leAssurance, &v.assurance, "Assurance"},
                            {ui->leVisite, &v.visiteTech, "Prochaine visite technique"}};
    for (const Champ &c : champs) {
        if (c.edit->text().trimmed().isEmpty()) continue;      // date facultative
        *c.date = QDate::fromString(c.edit->text().trimmed(), FORMAT_DATE);
        if (!c.date->isValid()) {
            QMessageBox::warning(this, "Date invalide",
                                 QString("« %1 » : utilisez le format jj/mm/aaaa.").arg(c.nom));
            return;
        }
    }
    v.type = ui->cbType->currentText();
    v.etat = ui->cbEtat->currentText();
    v.dispo = ui->cbDispo->currentText();

    QString erreur;
    bool reussi;
    if (m_idEdition.isEmpty()) {
        reussi = v.ajouter(&erreur);
    } else {
        v.id = m_idEdition;
        reussi = v.modifier(&erreur);
    }
    if (!reussi) {
        QMessageBox::warning(this, "Contrôle de saisie", erreur);
        return;
    }
    viderFormulaire();
    rafraichir();
}

void GAxis::afficherVehicule(const QString &id)
{
    Vehicule v;
    QString erreur;
    if (!Vehicule::afficher(id, v, &erreur)) {
        QMessageBox::warning(this, "Erreur", erreur);
        return;
    }
    QMessageBox::information(this, "Véhicule " + v.id,
                             QString("Immatriculation : %1\nType : %2\nKilométrage : %3 km\nÉtat : %4\n"
                                     "Disponibilité : %5\nDernière maintenance : %6\n"
                                     "Prochaine maintenance : %7\nAssurance : %8\nVisite technique : %9")
                                 .arg(v.plaque, v.type, kmTexte(v.km), v.etat, v.dispo, fmt(v.derniereMaint),
                                      fmt(v.prochaineMaint), fmt(v.assurance), fmt(v.visiteTech)));
}

void GAxis::modifierVehicule(const QString &id)
{
    Vehicule v;
    QString erreur;
    if (!Vehicule::afficher(id, v, &erreur)) {
        QMessageBox::warning(this, "Erreur", erreur);
        return;
    }
    m_idEdition = id;
    ui->lePlaque->setText(v.plaque);
    ui->cbType->setCurrentText(v.type);
    ui->leKm->setText(QString::number(v.km));
    ui->cbEtat->setCurrentText(v.etat);
    ui->cbDispo->setCurrentText(v.dispo);
    ui->leDerniere->setText(v.derniereMaint.isValid() ? v.derniereMaint.toString(FORMAT_DATE) : "");
    ui->leProchaine->setText(v.prochaineMaint.isValid() ? v.prochaineMaint.toString(FORMAT_DATE) : "");
    ui->leAssurance->setText(v.assurance.isValid() ? v.assurance.toString(FORMAT_DATE) : "");
    ui->leVisite->setText(v.visiteTech.isValid() ? v.visiteTech.toString(FORMAT_DATE) : "");
    ui->btnSubmit->setText("Enregistrer");
    ui->lblFormTitre->setText("Modifier " + id);
}

void GAxis::supprimerVehicule(const QString &id)
{
    if (QMessageBox::question(this, "Supprimer", "Supprimer le véhicule " + id + " ?") != QMessageBox::Yes)
        return;
    QString erreur;
    if (!Vehicule::supprimer(id, &erreur)) {
        QMessageBox::warning(this, "Erreur", erreur);
        return;
    }
    if (m_idEdition == id) viderFormulaire();
    rafraichir();
}

// ------------------------------------------------------------------ exports
void GAxis::exporterExcel()
{
    QString erreur;
    const QVector<Vehicule> liste = Vehicule::lister(QString(), 1, &erreur);
    if (!erreur.isEmpty()) {
        QMessageBox::critical(this, "Base de données", erreur);
        return;
    }
    const QString chemin = QFileDialog::getSaveFileName(this, "Exporter pour Excel", "vehicules.csv",
                                                        "Excel / CSV (*.csv)");
    if (chemin.isEmpty()) return;

    QFile f(chemin);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Erreur", "Impossible d'écrire le fichier.");
        return;
    }
    f.write("\xEF\xBB\xBF");   // BOM UTF-8 pour qu'Excel reconnaisse les accents
    QTextStream out(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "ID;Immatriculation;Type;Km;État;Disponibilité;Dernière maint.;Prochaine maint.;Assurance;Visite technique\n";
    for (const Vehicule &v : liste)
        out << v.id << ';' << v.plaque << ';' << v.type << ';' << v.km << ';' << v.etat << ';' << v.dispo
            << ';' << fmt(v.derniereMaint) << ';' << fmt(v.prochaineMaint) << ';' << fmt(v.assurance) << ';'
            << fmt(v.visiteTech) << '\n';
    QMessageBox::information(this, "Export", "Fichier créé (ouvrable dans Excel) :\n" + chemin);
}
