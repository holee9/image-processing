"""커스텀 위젯: 영상 패널 / loss 그래프 / 지표 카드 / 단계 진행 표시."""
import math

import numpy as np
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import Qt, QPointF, QRectF, pyqtSignal

from param_tuner.param_space import GROUP_ORDER, GROUP_TITLES
from param_tuner.results import to_u8
from param_tuner.theme import C, GROUP_COLORS

Q = QtGui.QColor


def _alpha(col, a):
    c = Q(col)
    c.setAlpha(a)
    return c


# ======================================================================
# 영상 패널
# ======================================================================
class ImagePanel(QtWidgets.QWidget):
    viewChanged = pyqtSignal(float, float, float)   # zoom, cx, cy
    hovered = pyqtSignal(int, int)
    fileDropped = pyqtSignal(str)                    # 드래그 앤 드롭된 파일 경로
    windowDragged = pyqtSignal(float, float)         # 오른쪽 드래그로 제안된 (lo, hi)
    autoWindowRequested = pyqtSignal(int, int, int, int)   # 더블클릭: 화면에 보이는 영상 영역 (x0, y0, x1, y1)

    def __init__(self, title: str, dot_color: str, pix_max: int, empty_hint: str = "",
                 drop_text: str = ""):
        super().__init__()
        self.title, self.dot, self.empty_hint = title, dot_color, empty_hint
        self.drop_text = drop_text          # 비어 있으면 드롭을 받지 않음
        self._drop_hover = False
        self.setAcceptDrops(bool(drop_text))
        self.subtitle = ""
        self.arr = None
        self.qimg = None
        self._buf = None
        self.lo, self.hi = 0.0, float(pix_max)
        self.zoom, self.cx, self.cy = 1.0, None, None
        self._drag = None
        self._wl_drag = None                # (시작 위치, 시작 lo, 시작 hi) - 오른쪽 드래그 W/L 조정 중
        self._wl_text = ""
        self.setMinimumSize(130, 130)
        self.setMouseTracking(True)

    def clear(self):
        """영상을 비우고 보기 상태(줌/위치)를 초기화한다."""
        self.arr = self.qimg = self._buf = None
        self.zoom, self.cx, self.cy = 1.0, None, None
        self.subtitle = ""
        self._wl_drag, self._wl_text = None, ""
        self.update()

    def set_image(self, arr, lo=None, hi=None):
        if self._buf is not None and self._buf.shape != arr.shape:     # 영상 크기가 바뀌면 보기 상태 초기화
            self.zoom, self.cx, self.cy = 1.0, None, None
        self.arr = arr
        if lo is not None:
            self.lo, self.hi = float(lo), float(hi)
        self._rebuild()

    def set_window(self, lo, hi):
        self.lo, self.hi = float(lo), float(hi)
        if self.arr is not None:
            self._rebuild()

    def _rebuild(self):
        self._buf = np.ascontiguousarray(to_u8(self.arr, self.lo, self.hi))
        h, w = self._buf.shape
        self.qimg = QtGui.QImage(self._buf.data, w, h, w, QtGui.QImage.Format_Grayscale8)
        if self.cx is None:
            self.cx, self.cy = w / 2, h / 2
        self.update()

    def set_view(self, zoom, cx, cy):
        self.zoom, self.cx, self.cy = zoom, cx, cy
        self.update()

    def _scale(self):
        h, w = self._buf.shape
        return min(self.width() / w, self.height() / h) * self.zoom

    def _badge(self, p, x, y, text, dot=None, right=False):
        fm = p.fontMetrics()
        w = fm.horizontalAdvance(text) + (30 if dot else 18)
        if right:
            x = x - w
        r = QRectF(x, y, w, 22)
        p.setPen(Qt.NoPen)
        p.setBrush(_alpha("#06080c", 200))
        p.drawRoundedRect(r, 11, 11)
        tx = x + 9
        if dot:
            p.setBrush(Q(dot))
            p.drawEllipse(QPointF(x + 12, y + 11), 4, 4)
            tx = x + 22
        p.setPen(Q(C["text"]))
        p.drawText(QRectF(tx, y, w, 22), Qt.AlignVCenter | Qt.AlignLeft, text)

    def paintEvent(self, _):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing)
        r = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
        path = QtGui.QPainterPath()
        path.addRoundedRect(r, 9, 9)
        p.fillPath(path, Q("#080a0e"))
        p.setClipPath(path)
        if self.qimg is not None:
            s = self._scale()
            p.setRenderHint(QtGui.QPainter.SmoothPixmapTransform, s < 1.5)
            p.translate(self.width() / 2, self.height() / 2)
            p.scale(s, s)
            p.translate(-self.cx, -self.cy)
            p.drawImage(0, 0, self.qimg)
            p.resetTransform()
        else:
            p.setPen(Q(C["faint"]))
            p.drawText(self.rect(), Qt.AlignCenter, "영상 없음\n" + self.empty_hint)
        f = p.font()
        f.setBold(True)
        p.setFont(f)
        self._badge(p, 8, 8, self.title, self.dot)
        if self.subtitle:
            f.setBold(False)
            p.setFont(f)
            self._badge(p, 8, self.height() - 30, self.subtitle)
        if self.qimg is not None:
            f.setBold(False)
            p.setFont(f)
            self._badge(p, self.width() - 8, 8, f"{self._scale() * 100:.0f}%", right=True)
        if self._wl_text:                   # 오른쪽 드래그 중: 현재 W/L 표시
            f.setBold(True)
            p.setFont(f)
            self._badge(p, self.width() - 8, self.height() - 30, self._wl_text, C["warn"], right=True)
        if self._drop_hover:                # 드래그 중: 놓을 위치 강조
            p.fillPath(path, _alpha(C["accent"], 55))
            p.setBrush(Qt.NoBrush)
            p.setPen(QtGui.QPen(Q(C["accent"]), 2, Qt.DashLine))
            p.drawRoundedRect(r.adjusted(5, 5, -5, -5), 8, 8)
            f = p.font()
            f.setPointSizeF(f.pointSizeF() + 2)
            f.setBold(True)
            p.setFont(f)
            p.setPen(Q("#ffffff"))
            p.drawText(self.rect(), Qt.AlignCenter, self.drop_text)
        p.setClipping(False)
        p.setBrush(Qt.NoBrush)
        p.setPen(QtGui.QPen(Q(C["accent"] if self._drop_hover else C["border"]), 2 if self._drop_hover else 1))
        p.drawPath(path)

    # -- 드래그 앤 드롭 (로컬 파일 1개)
    @staticmethod
    def _first_file(event):
        md = event.mimeData()
        if md.hasUrls():
            for u in md.urls():
                if u.isLocalFile():
                    return u.toLocalFile()
        return None

    def dragEnterEvent(self, e):
        if self._first_file(e):
            self._drop_hover = True
            e.acceptProposedAction()
            self.update()
        else:
            e.ignore()

    def dragLeaveEvent(self, e):
        self._drop_hover = False
        self.update()

    def dropEvent(self, e):
        path = self._first_file(e)
        self._drop_hover = False
        self.update()
        if path:
            e.acceptProposedAction()
            self.fileDropped.emit(path)

    def _to_image(self, pos):
        s = self._scale()
        return (self.cx + (pos.x() - self.width() / 2) / s, self.cy + (pos.y() - self.height() / 2) / s)

    def wheelEvent(self, e):
        if self.qimg is None:
            return
        self.zoom = float(np.clip(self.zoom * (1.25 if e.angleDelta().y() > 0 else 0.8), 0.5, 40))
        self.update()
        self.viewChanged.emit(self.zoom, self.cx, self.cy)

    def mousePressEvent(self, e):
        if e.button() == Qt.LeftButton:
            self._drag = e.pos()
        elif e.button() == Qt.RightButton and self.qimg is not None:
            self._wl_drag = (e.pos(), self.lo, self.hi)
            self._wl_text = self._wl_label(self.lo, self.hi)
            self.update()

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.RightButton:
            self._wl_drag, self._wl_text = None, ""
            self.update()
        else:
            self._drag = None

    @staticmethod
    def _wl_label(lo, hi):
        return f"Window {hi - lo:.0f}   Level {(hi + lo) / 2:.0f}"

    def visible_image_rect(self):
        """현재 화면에 보이는 영상 영역 (x0, y0, x1, y1), 영상 경계로 클립."""
        h, w = self._buf.shape
        s = self._scale()
        hw, hh = self.width() / 2 / s, self.height() / 2 / s
        x0, x1 = max(0, int(self.cx - hw)), min(w, int(np.ceil(self.cx + hw)))
        y0, y1 = max(0, int(self.cy - hh)), min(h, int(np.ceil(self.cy + hh)))
        return x0, y0, x1, y1

    def mouseDoubleClickEvent(self, e):
        if self.qimg is None or e.button() != Qt.LeftButton:
            return
        if e.modifiers() & Qt.ControlModifier:      # Ctrl+더블클릭: 뷰 리셋
            h, w = self._buf.shape
            self.zoom, self.cx, self.cy = 1.0, w / 2, h / 2
            self.update()
            self.viewChanged.emit(self.zoom, self.cx, self.cy)
        else:                                       # 더블클릭: 화면에 보이는 영역 기준 자동 W/L
            self.autoWindowRequested.emit(*self.visible_image_rect())

    def mouseMoveEvent(self, e):
        if self.qimg is None:
            return
        if self._wl_drag is not None and (e.buttons() & Qt.RightButton):
            start, lo0, hi0 = self._wl_drag
            w0, c0 = max(hi0 - lo0, 1.0), (lo0 + hi0) / 2
            # 좌우: Window 폭(오른쪽=넓게, 지수 스케일) / 위아래: Level(위로=밝게 → level 감소)
            w = max(1.0, w0 * float(np.exp((e.x() - start.x()) / 200.0)))
            c = c0 + (e.y() - start.y()) * w0 / 300.0
            lo, hi = c - w / 2, c + w / 2
            self._wl_text = self._wl_label(lo, hi)
            self.windowDragged.emit(lo, hi)
            self.update()
        if self._drag is not None:
            s = self._scale()
            self.cx -= (e.x() - self._drag.x()) / s
            self.cy -= (e.y() - self._drag.y()) / s
            self._drag = e.pos()
            self.update()
            self.viewChanged.emit(self.zoom, self.cx, self.cy)
        x, y = self._to_image(e.pos())
        self.hovered.emit(int(x), int(y))


# ======================================================================
# loss 그래프
# ======================================================================
class LossPlot(QtWidgets.QWidget):
    """회색 점: 각 평가의 loss / 파란 계단선: best / 파란 점: 개선 시점 / 주황 링: 마지막 평가."""
    ML, MR, MT, MB = 56, 14, 30, 26

    def __init__(self):
        super().__init__()
        self.setMinimumHeight(130)
        self.setMouseTracking(True)
        self.mode = "zoom"
        self.reset()

    def reset(self):
        self.cur, self.best, self.improved, self.markers = [], [], [], []
        self.update()

    def add(self, loss, best, improved):
        self.cur.append(float(loss))
        self.best.append(float(best))
        if improved:
            self.improved.append(len(self.cur) - 1)
        self.update()

    def add_marker(self, label, color):
        self.markers.append((len(self.cur), label, color))
        self.update()

    def set_mode(self, mode):
        self.mode = mode
        self.update()

    # -- 좌표
    def _plot_rect(self):
        return QRectF(self.ML, self.MT, self.width() - self.ML - self.MR, self.height() - self.MT - self.MB)

    def _yrange(self):
        bmin = min(self.best)
        if self.mode == "full":
            hi = max(self.cur)
        else:
            hi = max(float(np.percentile(self.cur[-80:], 90)), bmin * 1.08, bmin + 1e-6)
        pad = (hi - bmin) * 0.08 + 1e-9
        return bmin - pad, hi + pad

    def _x(self, i, rect):
        n = max(len(self.cur) - 1, 1)
        return rect.left() + rect.width() * (i / n if len(self.cur) > 1 else 0.5)

    def paintEvent(self, _):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing)
        rr = QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
        p.setBrush(Q(C["panel"]))
        p.setPen(QtGui.QPen(Q(C["border"]), 1))
        p.drawRoundedRect(rr, 9, 9)
        f = p.font()
        f.setPointSizeF(8.5)
        p.setFont(f)
        p.setPen(Q(C["muted"]))
        p.drawText(QRectF(12, 6, 200, 18), Qt.AlignVCenter, "Loss 추이")
        if not self.cur:
            p.drawText(self.rect(), Qt.AlignCenter, "평가가 시작되면 표시됩니다")
            return

        rect = self._plot_rect()
        lo, hi = self._yrange()

        def y(v):
            return rect.bottom() - rect.height() * (v - lo) / (hi - lo)

        # 격자 + y 눈금
        p.setPen(QtGui.QPen(_alpha("#ffffff", 22), 1))
        for k in range(5):
            v = lo + (hi - lo) * k / 4
            p.drawLine(QPointF(rect.left(), y(v)), QPointF(rect.right(), y(v)))
            p.setPen(Q(C["muted"]))
            p.drawText(QRectF(2, y(v) - 8, self.ML - 8, 16), Qt.AlignRight | Qt.AlignVCenter, f"{v:.3f}")
            p.setPen(QtGui.QPen(_alpha("#ffffff", 22), 1))
        n = len(self.cur)
        p.setPen(Q(C["muted"]))
        for k in range(5):
            i = int(round((n - 1) * k / 4)) if n > 1 else 0
            p.drawText(QRectF(self._x(i, rect) - 20, rect.bottom() + 4, 40, 16), Qt.AlignHCenter, str(i + 1))

        # 단계 구분선
        for idx, label, col in self.markers:
            if idx >= n:
                continue
            x = self._x(idx, rect)
            p.setPen(QtGui.QPen(_alpha(col, 140), 1, Qt.DashLine))
            p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()))
            tw = p.fontMetrics().horizontalAdvance(label) + 8
            p.setPen(Qt.NoPen)
            p.setBrush(_alpha(C["panel"], 215))
            p.drawRoundedRect(QRectF(x + 2, rect.top() + 1, tw, 14), 4, 4)
            p.setPen(Q(col))
            p.drawText(QRectF(x + 6, rect.top() + 1, tw, 14), Qt.AlignLeft | Qt.AlignVCenter, label)

        p.save()
        p.setClipRect(rect.adjusted(-6, -6, 6, 6))
        # best 영역 채움 + 계단선
        path = QtGui.QPainterPath()
        path.moveTo(self._x(0, rect), y(self.best[0]))
        for i in range(1, n):
            path.lineTo(self._x(i, rect), y(self.best[i - 1]))
            path.lineTo(self._x(i, rect), y(self.best[i]))
        fill = QtGui.QPainterPath(path)
        fill.lineTo(self._x(n - 1, rect), rect.bottom())
        fill.lineTo(self._x(0, rect), rect.bottom())
        fill.closeSubpath()
        grad = QtGui.QLinearGradient(0, rect.top(), 0, rect.bottom())
        grad.setColorAt(0, _alpha(C["accent"], 70))
        grad.setColorAt(1, _alpha(C["accent"], 5))
        p.fillPath(fill, grad)

        # 평가 점 (회색 산점도). 범위 밖(위)은 상단에 ▲ 로 표시
        p.setPen(Qt.NoPen)
        p.setBrush(_alpha("#aab3c8", 150))
        for i, v in enumerate(self.cur):
            if v > hi:
                x = self._x(i, rect)
                p.drawPolygon(QtGui.QPolygonF([QPointF(x, rect.top() + 1), QPointF(x - 3, rect.top() + 7),
                                               QPointF(x + 3, rect.top() + 7)]))
            else:
                p.drawEllipse(QPointF(self._x(i, rect), y(v)), 2.8, 2.8)

        p.setPen(QtGui.QPen(Q(C["accent"]), 2.2))
        p.setBrush(Qt.NoBrush)
        p.drawPath(path)
        # 개선 시점
        p.setPen(QtGui.QPen(Q(C["panel"]), 1.5))
        p.setBrush(Q(C["accent"]))
        for i in self.improved:
            p.drawEllipse(QPointF(self._x(i, rect), y(self.best[i])), 4, 4)
        # 마지막 평가 링
        p.setBrush(Qt.NoBrush)
        p.setPen(QtGui.QPen(Q(C["warn"]), 2))
        last = self.cur[-1]
        p.drawEllipse(QPointF(self._x(n - 1, rect), y(min(max(last, lo), hi))), 5.5, 5.5)
        p.restore()

        # 범례
        lx = max(80, self.width() - self.MR - 4 * 70)
        p.setPen(Qt.NoPen)
        for text, kind, col in (("평가값", "dot", "#aab3c8"), ("best", "line", C["accent"]),
                                ("개선", "dot", C["accent"]), ("마지막", "ring", C["warn"])):
            p.setBrush(Q(col) if kind != "ring" else Qt.NoBrush)
            p.setPen(QtGui.QPen(Q(col), 2) if kind in ("line", "ring") else Qt.NoPen)
            cy = 15
            if kind == "line":
                p.drawLine(QPointF(lx, cy), QPointF(lx + 14, cy))
            else:
                p.drawEllipse(QPointF(lx + 6, cy), 3.5, 3.5)
            p.setPen(Q(C["muted"]))
            p.drawText(QRectF(lx + 18, 6, 60, 18), Qt.AlignVCenter, text)
            lx += 70

    def mouseMoveEvent(self, e):
        if len(self.cur) < 1:
            return
        rect = self._plot_rect()
        n = len(self.cur)
        i = int(round((e.x() - rect.left()) / max(rect.width(), 1) * (n - 1))) if n > 1 else 0
        if 0 <= i < n and rect.adjusted(-8, -8, 8, 8).contains(QPointF(e.pos())):
            QtWidgets.QToolTip.showText(e.globalPos(),
                                        f"평가 #{i + 1}\nloss  {self.cur[i]:.4f}\nbest  {self.best[i]:.4f}", self)


# ======================================================================
# 지표 카드
# ======================================================================
class MetricCard(QtWidgets.QFrame):
    def __init__(self, caption: str, tip: str = ""):
        super().__init__()
        self.setObjectName("card")
        self.setToolTip(tip)
        lay = QtWidgets.QVBoxLayout(self)
        lay.setContentsMargins(14, 6, 14, 6)
        lay.setSpacing(0)
        self.cap = QtWidgets.QLabel(caption)
        self.cap.setWordWrap(True)          # 풀네임이 길어 좁은 창에서는 두 줄로 표시
        self.cap.setStyleSheet(f"color:{C['muted']}; font-size:8.5pt; font-weight:600; border:none;")
        self.val = QtWidgets.QLabel("-")
        self.val.setStyleSheet("font-size:16pt; font-weight:700; border:none;")
        self.sub = QtWidgets.QLabel(" ")
        self.sub.setStyleSheet(f"color:{C['muted']}; font-size:8.5pt; border:none;")
        for w in (self.cap, self.val, self.sub):
            lay.addWidget(w)

    def set(self, value, sub=" ", color=None):
        self.val.setText(value)
        self.val.setStyleSheet(f"font-size:16pt; font-weight:700; border:none; color:{color or C['text']};")
        self.sub.setText(sub)

    def set_sub(self, sub, color=None):
        self.sub.setText(sub)
        self.sub.setStyleSheet(f"color:{color or C['muted']}; font-size:8.5pt; border:none;")


# ======================================================================
# 단계 진행 표시
# ======================================================================
class StageStepper(QtWidgets.QWidget):
    """초기 평가 → 그룹 5개 → 최종 평가. 진행 중 단계는 맥동한다."""
    LABELS = ["초기 평가"] + [GROUP_TITLES[g] for g in GROUP_ORDER] + ["최종 평가"]
    COLORS = [C["gray"]] + [GROUP_COLORS[g] for g in GROUP_ORDER] + [C["good"]]

    def __init__(self):
        super().__init__()
        self.setFixedHeight(32)
        self.reset()
        self._phase = 0.0

    def reset(self, skip=()):
        self.state = ["pending"] * len(self.LABELS)
        self.skip = set(skip)
        self.update()

    def set_active(self, i):
        for j in range(len(self.state)):
            if j < i:
                self.state[j] = "done"
            elif j == i:
                self.state[j] = "active"
            else:
                self.state[j] = "pending"
        self.update()

    def finish(self):
        self.state = ["done"] * len(self.LABELS)
        self.update()

    def halt(self):
        self.state = ["done" if s == "done" else ("stopped" if s == "active" else s) for s in self.state]
        self.update()

    def tick(self):
        self._phase += 0.35
        if "active" in self.state:
            self.update()

    def paintEvent(self, _):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing)
        n, gap = len(self.LABELS), 8
        w = (self.width() - gap * (n - 1)) / n
        pulse = 0.5 + 0.5 * math.sin(self._phase)
        for i, (label, col) in enumerate(zip(self.LABELS, self.COLORS)):
            r = QRectF(i * (w + gap), 3, w, self.height() - 6)
            st = self.state[i]
            skipped = (i - 1) in {GROUP_ORDER.index(g) for g in self.skip if g in GROUP_ORDER}
            if skipped and st == "pending":
                p.setBrush(Qt.NoBrush)
                p.setPen(QtGui.QPen(Q(C["border"]), 1, Qt.DashLine))
                p.drawRoundedRect(r, 8, 8)
                p.setPen(Q(C["faint"]))
                p.drawText(r, Qt.AlignCenter, f"{label} (건너뜀)")
                continue
            if st == "done":
                p.setBrush(_alpha(col, 45))
                p.setPen(QtGui.QPen(_alpha(col, 200), 1))
                text, tcol = label, C["text"]
            elif st == "active":
                p.setBrush(_alpha(col, int(55 + 55 * pulse)))
                p.setPen(QtGui.QPen(Q(col), 2))
                text, tcol = label, "#ffffff"
            elif st == "stopped":
                p.setBrush(_alpha(C["warn"], 40))
                p.setPen(QtGui.QPen(Q(C["warn"]), 1))
                text, tcol = label, C["text"]
            else:
                p.setBrush(Q(C["card"]))
                p.setPen(QtGui.QPen(Q(C["border"]), 1))
                text, tcol = label, C["faint"]
            p.drawRoundedRect(r, 8, 8)
            cy = r.center().y()
            if st == "active":
                p.setPen(Qt.NoPen)
                p.setBrush(_alpha(col, int(120 + 135 * pulse)))
                p.drawEllipse(QPointF(r.left() + 12, cy), 4, 4)
            elif st == "done":      # 체크 표시는 폰트 글리프 대신 직접 그림
                p.setPen(QtGui.QPen(Q(C["good"]), 2, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
                p.setBrush(Qt.NoBrush)
                p.drawPolyline(QtGui.QPolygonF([QPointF(r.left() + 8, cy), QPointF(r.left() + 12, cy + 4),
                                                QPointF(r.left() + 19, cy - 4)]))
            elif st == "stopped":
                p.setPen(Qt.NoPen)
                p.setBrush(Q(C["warn"]))
                p.drawRect(QRectF(r.left() + 8, cy - 4, 8, 8))
            f = p.font()
            f.setBold(st == "active")
            p.setFont(f)
            p.setPen(Q(tcol))
            shift = 16 if st in ("active", "done", "stopped") else 0
            p.drawText(r.adjusted(shift, 0, 0, 0), Qt.AlignCenter, text)
