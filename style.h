#pragma once

static const char kStyle[] = R"QSS(
QMainWindow, #MaterielPage { background: #17191c; }
QScrollArea { border: none; background: #17191c; }
QScrollArea > QWidget > QWidget { background: #17191c; }

#sidebar { background: #1f2226; }
#logo { font-size: 22px; font-weight: 800; color: #ffffff; }
#logoSub { color: #9aa0a6; font-size: 11px; }
#sidebarFooter { color: #9aa0a6; font-size: 11px; border-top: 1px solid #33373d; padding-top: 10px; }
QPushButton[nav="true"] { background: transparent; border: none; border-radius: 6px; text-align: left;
                  padding: 10px 10px; color: #d0d3d6; font-size: 13px; }
QPushButton[nav="true"]:hover { background: #2a2e34; }
QPushButton[nav="true"]:checked { background: #ff0000; color: #ffffff; font-weight: 600; }

#pageTitle { font-size: 20px; font-weight: 700; color: #ffffff; }
QLabel[placeholder="true"] { color: #9aa0a6; font-size: 15px; }
#banner { background: #fbe9d0; color: #8a5a14; border: 1px solid #e0b36a;
               border-radius: 3px; padding: 6px 12px; font-size: 12px; }

QFrame[card="true"] { background: #1c1f23; border: 1px solid #2b2f35; border-radius: 10px; }
QLabel[cardTitle="true"] { color: #d40000; font-weight: 700; font-size: 14px; }
QLabel[fieldLabel="true"] { color: #8d939a; font-size: 11px; }
QLabel[muted="true"] { color: #aeb3b9; font-size: 12px; }

QLineEdit, QComboBox { background: #14161a; border: 1px solid #2b2f35; border-radius: 6px;
                       padding: 6px 9px; color: #e6e6e6; min-height: 18px; }
QLineEdit:focus, QComboBox:focus { border-color: #ff0000; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(:/arrow.png); width: 10px; height: 6px; }
QComboBox QAbstractItemView { background: #14161a; color: #e6e6e6; border: 1px solid #2b2f35;
                              selection-background-color: #ff0000; outline: 0; }

QPushButton { background: #14161a; border: 1px solid #2b2f35; border-radius: 6px;
              padding: 7px 14px; color: #e6e6e6; }
QPushButton:hover { border-color: #5a5f66; }
QPushButton#btnAdd, QPushButton#btnMvSave { background: #ff0000; border: 1px solid #ff0000; color: #ffffff; font-weight: 600; }
QPushButton#btnAdd:hover, QPushButton#btnMvSave:hover { background: #e00000; }

QTableWidget { background: transparent; border: none; color: #e6e6e6; font-size: 11px; }
QTableWidget::item { border-bottom: 1px solid #2b2f35; padding: 2px; }
QHeaderView { background: transparent; }
QHeaderView::section { background: transparent; color: #aeb3b9; border: none;
                       border-bottom: 1px solid #2b2f35; padding: 2px; font-size: 10px; }
QToolButton { background: transparent; border: none; border-radius: 6px; padding: 4px; font-size: 16px; }
QToolButton:hover { background: #2a2e34; }
#cell { border-bottom: 1px solid #2b2f35; }


QMessageBox { background: #1c1f23; }
)QSS";
