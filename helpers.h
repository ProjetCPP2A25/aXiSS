#ifndef HELPERS_H
#define HELPERS_H

// Fonctions utilitaires partagées : formats, validation GPS, pastilles colorées, style des boîtes de dialogue.

#include <QString>

#include "intervention.h"

class QLabel;
class QToolButton;
class QWidget;

namespace Helpers {

inline const char *const kDateFormat = "dd/MM/yyyy HH:mm";

inline const QString kDialogStyle = QStringLiteral(
    "QDialog, QMessageBox { background-color: #202428; }"
    "QLabel { color: #E5E7EB; background: transparent; font-size: 13px; border: none; }"
    "QPushButton { background-color: #181B1F; color: white; border: 1px solid #343A40;"
    "  border-radius: 5px; padding: 7px 18px; min-width: 70px; font-size: 13px; }"
    "QPushButton:hover { background-color: #30353A; }"
    "QPushButton:default { background-color: #FF1717; border: none; font-weight: bold; }"
    "QPushButton:default:hover { background-color: #E31313; }");

struct PillColors {
    const char *bg;
    const char *fg;
};

// "45 min", "1 h 15", "2 h"
QString formatDuration(qint64 minutes);

// "36.8065, 10.1815" (virgule ou point-virgule comme séparateur)
bool parseGps(const QString &text, double &lat, double &lon);

// Retire les espaces pour comparer des coordonnées saisies à la main
QString normalizeGps(const QString &s);

QString formatDate(const QDateTime &d);          // "-" si la date est invalide
QString formatDateTableau(const QDateTime &d);   // "30/09/26" puis "14:32" sur deux lignes (tableau compact)

PillColors graviteColors(const QString &gravite);
PillColors etatColors(const QString &etat);
QString dotColor(const QString &gravite);        // couleur du point sur la carte

QLabel *makePillLabel(const QString &text, const PillColors &c, QWidget *parent = nullptr);
QWidget *makePillCell(const QString &text, const PillColors &c);   // pastille centrée dans une cellule
QToolButton *makeActionButton(const QString &glyph, const QString &tip, const QString &hover);

} // namespace Helpers

#endif // HELPERS_H
