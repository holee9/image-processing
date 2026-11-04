"""다크 테마 (팔레트 + 스타일시트)."""
from PyQt5 import QtGui, QtWidgets

C = dict(bg="#0e1117", panel="#151a23", card="#1a2030", input="#0f131b", border="#2a3245",
         text="#e8ebf2", muted="#8a93a8", faint="#5b6377", accent="#4c8dff", good="#34c38f",
         warn="#f5a623", bad="#f0506e", gray="#7d879c")

GROUP_COLORS = {"tone": "#4c8dff", "freq": "#a78bfa", "suppress": "#f5a623",
                "noise": "#34d399", "lut": "#f472b6"}

QSS = f"""
QMainWindow, QWidget#root {{ background: {C['bg']}; }}
QWidget {{ color: {C['text']}; font-size: 9.5pt; }}
QLabel#title {{ font-size: 16pt; font-weight: 700; }}
QLabel#muted, QLabel[role="muted"] {{ color: {C['muted']}; }}
QLabel#chip {{ background: {C['card']}; border: 1px solid {C['border']}; border-radius: 9px;
              padding: 2px 10px; color: {C['muted']}; }}
QFrame#card {{ background: {C['card']}; border: 1px solid {C['border']}; border-radius: 10px; }}
QGroupBox {{ background: {C['panel']}; border: 1px solid {C['border']}; border-radius: 10px;
            margin-top: 14px; padding: 12px 8px 8px 8px; }}
QGroupBox::title {{ subcontrol-origin: margin; left: 12px; padding: 0 6px; color: {C['muted']};
                   font-weight: 600; }}
QPushButton {{ background: {C['card']}; border: 1px solid {C['border']}; border-radius: 7px;
              padding: 6px 14px; }}
QPushButton:hover {{ border-color: {C['accent']}; }}
QPushButton:disabled {{ color: {C['faint']}; border-color: {C['border']}; background: {C['panel']}; }}
QPushButton#primary {{ background: {C['accent']}; border-color: {C['accent']}; color: white;
                      font-weight: 700; padding: 8px 22px; }}
QPushButton#primary:hover {{ background: #6aa1ff; }}
QPushButton#primary:disabled {{ background: #26314d; border-color: #26314d; color: #6f7a99; }}
QPushButton#danger {{ color: {C['bad']}; border-color: {C['bad']}; font-weight: 600; }}
QPushButton#danger:hover {{ background: rgba(240,80,110,0.14); }}
QPushButton#danger:disabled {{ color: {C['faint']}; border-color: {C['border']}; }}
QPushButton[seg="true"] {{ padding: 2px 10px; border-radius: 5px; }}
QPushButton[seg="true"]:checked {{ background: {C['accent']}; border-color: {C['accent']}; color: white; }}
QToolButton {{ background: {C['card']}; border: 1px solid {C['border']}; border-radius: 7px;
              padding: 5px 12px; }}
QToolButton:checked {{ border-color: {C['accent']}; color: {C['accent']}; }}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {{ background: {C['input']};
        border: 1px solid {C['border']}; border-radius: 6px; padding: 3px 6px; min-height: 20px; }}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {{ border-color: {C['accent']}; }}
QComboBox QAbstractItemView {{ background: {C['card']}; selection-background-color: {C['accent']}; }}
QTableWidget {{ background: {C['panel']}; alternate-background-color: #181e2a;
               border: 1px solid {C['border']}; border-radius: 8px; gridline-color: transparent;
               selection-background-color: #25345a; selection-color: {C['text']}; }}
QTableWidget::item {{ padding: 2px 4px; }}
QHeaderView::section {{ background: {C['card']}; color: {C['muted']}; border: none;
                       border-bottom: 1px solid {C['border']}; padding: 6px 4px; font-weight: 600; }}
QPlainTextEdit {{ background: {C['input']}; border: 1px solid {C['border']}; border-radius: 8px;
                 font-family: Consolas, 'D2Coding', monospace; font-size: 9pt; color: #c3cbe0; }}
QProgressBar {{ background: {C['input']}; border: 1px solid {C['border']}; border-radius: 6px;
               height: 12px; text-align: center; }}
QProgressBar::chunk {{ border-radius: 5px;
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #3a6fe0, stop:1 #6aa1ff); }}
QSplitter::handle {{ background: transparent; }}
QScrollBar:vertical {{ background: transparent; width: 10px; margin: 0; }}
QScrollBar::handle:vertical {{ background: #2d3650; border-radius: 5px; min-height: 30px; }}
QScrollBar:horizontal {{ background: transparent; height: 10px; }}
QScrollBar::handle:horizontal {{ background: #2d3650; border-radius: 5px; min-width: 30px; }}
QScrollBar::add-line, QScrollBar::sub-line {{ width: 0; height: 0; }}
QStatusBar {{ background: {C['panel']}; color: {C['muted']}; }}
QToolTip {{ background: #0b0e14; color: {C['text']}; border: 1px solid {C['accent']};
           padding: 8px; font-size: 9.5pt; }}
QCheckBox::indicator {{ width: 15px; height: 15px; }}
"""


def apply_theme(app: QtWidgets.QApplication):
    app.setStyle("Fusion")
    pal = QtGui.QPalette()
    q = QtGui.QColor
    for role, col in ((QtGui.QPalette.Window, C["bg"]), (QtGui.QPalette.Base, C["input"]),
                      (QtGui.QPalette.AlternateBase, C["panel"]), (QtGui.QPalette.Button, C["card"]),
                      (QtGui.QPalette.ToolTipBase, "#0b0e14"), (QtGui.QPalette.ToolTipText, C["text"]),
                      (QtGui.QPalette.Text, C["text"]), (QtGui.QPalette.WindowText, C["text"]),
                      (QtGui.QPalette.ButtonText, C["text"]), (QtGui.QPalette.BrightText, "#ffffff"),
                      (QtGui.QPalette.Highlight, C["accent"]), (QtGui.QPalette.HighlightedText, "#ffffff")):
        pal.setColor(role, q(col))
    pal.setColor(QtGui.QPalette.Disabled, QtGui.QPalette.Text, q(C["faint"]))
    pal.setColor(QtGui.QPalette.Disabled, QtGui.QPalette.ButtonText, q(C["faint"]))
    app.setPalette(pal)
    f = app.font()
    for fam in ("Segoe UI", "Malgun Gothic"):
        f.setFamily(fam)
        break
    f.setPointSizeF(9.5)
    app.setFont(f)
    app.setStyleSheet(QSS)
