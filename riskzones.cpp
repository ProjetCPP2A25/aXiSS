#include "riskzones.h"
#include "ui_interventionpage.h"
#include "helpers.h"

#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

RiskZones::RiskZones(Ui::InterventionPage *ui_, QObject *parent)
    : QObject(parent)
    , ui(ui_)
    , m_net(new QNetworkAccessManager(this))
{
}

QString RiskZones::refresh()
{
    QString error;
    m_zones = Intervention::zonesARisque(QDateTime::currentDateTime().addMonths(-6), &error);
    if (!error.isEmpty())
        return error;
    for (const Intervention::ZoneAgregee &z : std::as_const(m_zones))
        fetchWeather(z);
    render();
    return QString();
}

// Prévisions sur 7 jours (rafales de vent et cumul de pluie) pour une zone.
void RiskZones::fetchWeather(const Intervention::ZoneAgregee &z)
{
    const auto cached = m_weather.constFind(z.cle);
    if (m_pending.contains(z.cle)
        || (cached != m_weather.constEnd() && cached->fetched.secsTo(QDateTime::currentDateTime()) < 30 * 60))
        return;
    m_pending.insert(z.cle);

    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(z.lat, 'f', 4));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(z.lon, 'f', 4));
    q.addQueryItem(QStringLiteral("daily"), QStringLiteral("wind_gusts_10m_max,precipitation_sum"));
    q.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("7"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_net->get(req);
    const QString key = z.cle;
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        Weather w;
        w.fetched = QDateTime::currentDateTime();
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject daily = QJsonDocument::fromJson(reply->readAll()).object()["daily"].toObject();
            const QJsonArray gusts = daily["wind_gusts_10m_max"].toArray();
            const QJsonArray rain = daily["precipitation_sum"].toArray();
            if (!gusts.isEmpty() || !rain.isEmpty()) {
                w.ok = true;
                for (const QJsonValue &v : gusts)
                    w.gust = std::max(w.gust, v.toDouble());
                for (const QJsonValue &v : rain)
                    w.rain += v.toDouble();
            }
        }
        reply->deleteLater();
        m_pending.remove(key);
        m_weather.insert(key, w);
        render();
    });
}

void RiskZones::render()
{
    for (QWidget *w : std::as_const(m_widgets))
        w->deleteLater();
    m_widgets.clear();

    struct Scored { Intervention::ZoneAgregee zone; int total; QString note; };
    QVector<Scored> scored;
    for (const Intervention::ZoneAgregee &z : std::as_const(m_zones)) {
        int bonus = 0;
        QStringList notes;
        if (m_weather.contains(z.cle)) {
            const Weather w = m_weather.value(z.cle);
            if (!w.ok) {
                notes << QStringLiteral("météo indisponible");
            } else {
                if (w.gust >= 60)      { bonus += 4; notes << QStringLiteral("vent fort prévu"); }
                else if (w.gust >= 40) { bonus += 2; notes << QStringLiteral("vent modéré prévu"); }
                if (w.rain >= 40)      { bonus += 4; notes << QStringLiteral("pluies fortes prévues cette semaine"); }
                else if (w.rain >= 15) { bonus += 2; notes << QStringLiteral("pluies modérées prévues"); }
                if (notes.isEmpty())
                    notes << QStringLiteral("météo stable");
            }
        } else {
            notes << QStringLiteral("météo en cours de chargement…");
        }
        Scored s;
        s.zone = z;
        s.total = z.poids + bonus;
        s.note = QStringLiteral("%1 intervention%2 en 6 mois, %3")
                     .arg(z.nombre).arg(z.nombre > 1 ? "s" : "").arg(notes.join(QStringLiteral(", ")));
        scored.push_back(s);
    }
    std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
        return a.total != b.total ? a.total > b.total : a.zone.nombre > b.zone.nombre;
    });

    QFrame *host = ui->frameZonesRisque;
    if (scored.isEmpty()) {
        auto *l = new QLabel(QStringLiteral("Aucune zone à risque détectée."), host);
        l->setStyleSheet(QStringLiteral("color: #AAB0B6; font-size: 13px; background: transparent; border: none;"));
        l->setGeometry(16, 56, 400, 24);
        l->show();
        m_widgets.push_back(l);
        return;
    }

    const int rowWidth = host->width() - 32;
    const int rowsShown = std::min<int>(3, scored.size());
    for (int r = 0; r < rowsShown; ++r) {
        const Scored &s = scored[r];
        auto *row = new QFrame(host);
        row->setGeometry(16, 50 + r * 68, rowWidth, 58);
        row->setStyleSheet(QStringLiteral("background-color: #202428; border: 1px solid #343A40; border-radius: 6px;"));

        auto *name = new QLabel(s.zone.nom, row);
        name->setGeometry(10, 6, 300, 20);
        name->setStyleSheet(QStringLiteral("color: white; font-size: 14px; font-weight: bold;"
                                           "background: transparent; border: none;"));
        auto *info = new QLabel(s.note, row);
        info->setGeometry(10, 28, rowWidth - 130, 24);
        info->setToolTip(s.note);
        info->setStyleSheet(QStringLiteral("color: #AAB0B6; font-size: 12px; background: transparent; border: none;"));

        QString level;
        Helpers::PillColors c;
        if (s.total >= 9)      { level = QStringLiteral("Risque élevé");  c = {"#F7D6D6", "#A61B1B"}; }
        else if (s.total >= 5) { level = QStringLiteral("Risque moyen");  c = {"#FCE8CC", "#9A5A00"}; }
        else                   { level = QStringLiteral("Risque faible"); c = {"#DDF3E4", "#1B7A3E"}; }
        auto *pill = Helpers::makePillLabel(level, c, row);
        pill->setGeometry(rowWidth - 115, 16, 100, 24);

        for (QWidget *w : {static_cast<QWidget *>(row), static_cast<QWidget *>(name),
                           static_cast<QWidget *>(info), static_cast<QWidget *>(pill)})
            w->show();
        m_widgets.push_back(row);
    }
}
