#pragma once

static const char kVehicleStyleSheet[] = R"QSS(
QWidget { background:#1c1e21; color:#e6e6e6; font-family:"Montserrat","Segoe UI","Inter",sans-serif; font-size:13px; }
QLabel { background:transparent; }
#sidebar { background:#232528; }
#sidebar QLabel { background:transparent; }
#sidebar QPushButton { text-align:left; padding:9px 10px; border:none; border-radius:6px; background:transparent; color:#e6e6e6; font-size:14px; }
#sidebar QPushButton:hover { background:#2e3033; }
#sidebar QPushButton:checked { background:#ff0000; color:white; font-weight:600; }
#card { background:#232528; border:1px solid #33363a; border-radius:8px; }
#cardTitle { color:#d90000; font-size:15px; font-weight:700; }
#pageTitle { font-size:21px; font-weight:700; }
#muted { color:#b0b3b8; font-size:12px; }
#alert { background:#fdf0dc; color:#b45f06; border-left:4px solid #e8830c; border-radius:4px; padding:8px 12px; }
QLineEdit, QComboBox { background:#1c1e21; border:1px solid #3a3d41; border-radius:5px; padding:6px 8px; color:#e6e6e6; }
QLineEdit:focus, QComboBox:focus { border-color:#ff0000; }
QComboBox::drop-down { border:none; width:24px; }
QComboBox::down-arrow { image:url(:/arrow.png); width:10px; height:6px; }
QComboBox QAbstractItemView { background:#232528; selection-background-color:#ff0000; border:1px solid #3a3d41; }
QPushButton#primary { background:#ff0000; color:white; border:none; border-radius:5px; padding:7px 16px; font-weight:600; }
QPushButton#primary:hover { background:#d90000; }
QPushButton#ghost { background:transparent; border:1px solid #3a3d41; border-radius:5px; padding:7px 14px; }
QPushButton#ghost:hover { background:#2e3033; }
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
