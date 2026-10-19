#include "sidebar.h"

#include <QLabel>
#include <QPushButton>

SidebarFrame::SidebarFrame(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("sidebarFrame"));
    setFixedWidth(200);
    setStyleSheet(QStringLiteral("QFrame#sidebarFrame { background-color: #23282D; border: none; }"));

    auto *logo = new QLabel(QStringLiteral("A.X.I.S"), this);
    logo->setGeometry(22, 22, 150, 32);
    logo->setStyleSheet(QStringLiteral(
        "color: white; font-size: 24px; font-weight: bold; background: transparent; border: none;"));

    auto *company = new QLabel(QStringLiteral("RedFlame Protection"), this);
    company->setGeometry(22, 62, 160, 22);
    company->setStyleSheet(QStringLiteral(
        "color: #9AA4AE; font-size: 13px; background: transparent; border: none;"));

    const QString buttonStyle = QStringLiteral(
        "QPushButton { background-color: transparent; color: #E5E7EB; border: none; text-align: left;"
        "  padding-left: 12px; font-size: 14px; }"
        "QPushButton:hover { background-color: #30363C; border-radius: 7px; }");

    auto *menu = new QPushButton(QStringLiteral("Menu principal"), this);
    menu->setGeometry(14, 105, 184, 42);
    menu->setStyleSheet(buttonStyle);
    menu->setCursor(Qt::PointingHandCursor);

    auto *logout = new QPushButton(QStringLiteral("Déconnexion"), this);
    logout->setGeometry(14, 155, 184, 42);
    logout->setStyleSheet(buttonStyle);
    logout->setCursor(Qt::PointingHandCursor);

    m_info = new QLabel(this);
    m_info->setGeometry(22, 210, 166, 60);
    m_info->setWordWrap(true);
    m_info->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_info->setStyleSheet(QStringLiteral(
        "color: #9AA4AE; font-size: 12px; background: transparent; border: none;"));
    m_info->hide();

    connect(menu, &QPushButton::clicked, this, &SidebarFrame::menuRequested);
    connect(logout, &QPushButton::clicked, this, &SidebarFrame::logoutRequested);
}

void SidebarFrame::setInfo(const QString &text)
{
    m_info->setText(text);
    m_info->setVisible(!text.isEmpty());
}
