#ifndef INTERVENTIONPAGE_H
#define INTERVENTIONPAGE_H

#include <QWidget>
#include <QString>

class InterventionForm;
class InterventionTable;
class MapView;
class RiskZones;
class StatsPanel;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class InterventionPage;
}
QT_END_NAMESPACE

class InterventionPage : public QWidget
{
    Q_OBJECT

public:
    explicit InterventionPage(QWidget *parent = nullptr);
    ~InterventionPage() override;

signals:
    void menuRequested();       // bouton « Menu principal »
    void logoutRequested();     // bouton « Déconnexion »

private slots:
    void onSave();
    void showDetails(int id);
    void startEdit(int id);
    void deleteIntervention(int id);
    void onExport();

private:
    void refreshAll(bool userAction = false);
    void reportError(const QString &error, bool userAction);

    Ui::InterventionPage *ui;

    InterventionForm *m_form;
    InterventionTable *m_table;
    StatsPanel *m_stats;
    RiskZones *m_zones;
    MapView *m_map;
    QTimer *m_timer;

    bool m_lastRefreshFailed = false;
};

#endif // INTERVENTIONPAGE_H