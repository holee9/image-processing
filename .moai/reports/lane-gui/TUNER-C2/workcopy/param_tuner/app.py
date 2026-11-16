"""Post 파라미터 자동 튜닝 UI (PyQt5).

실행: python param_tuner/app.py   (옵션: --bit-depth 16)
 - Source(pre) / Target(post) / Output(run_pipeline 방식 post) 3패널 (줌/팬 동기화)
 - [실행]: default 파라미터로 처리 → target 과 비교하며 파라미터 자동 탐색 → 결과 저장
"""
import json
import os
import sys
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from param_tuner import bootstrap  # noqa: E402
BIT = bootstrap.init()

import numpy as np  # noqa: E402
from PyQt5 import QtCore, QtGui, QtWidgets  # noqa: E402
from PyQt5.QtCore import Qt, pyqtSignal  # noqa: E402

import run_pipeline as rp  # noqa: E402
from pe_post.common import PIX_MAX  # noqa: E402
from param_tuner.metrics import LossWeights  # noqa: E402
from param_tuner.optimizer import OptConfig  # noqa: E402
from param_tuner.param_space import (CHEST_ONLY_POST, GROUP_ORDER, GROUP_TITLES,  # noqa: E402
                                     NON_CHEST_ONLY_POST, UNUSED_POST,
                                     apply_object_type_mask, build_param_defs,
                                     default_params, params_from_dict)
from param_tuner.param_tips import metric_tooltip_html, tooltip_html  # noqa: E402
from param_tuner.rawsize import KNOWN_SIZES, candidate_sizes  # noqa: E402
from param_tuner.protocols import (DEFAULT_PROTOCOLS, PROTOCOLS_PATH, engine_object_type,  # noqa: E402
                                   load_protocols, save_protocols, validate as validate_protocols)
from param_tuner.theme import C, GROUP_COLORS, apply_theme  # noqa: E402
from param_tuner.tuner_core import JobConfig, TuneJob, load_raw_image  # noqa: E402
from param_tuner.widgets import ImagePanel, LossPlot, MetricCard, StageStepper  # noqa: E402

# loss 비교 해상도: 전체 해상도 고정 (파이프라인도 항상 전체 해상도로 실행하며, 비교도 축소하지 않음)
EVAL_SCALE = 1.0

STATUS_COLORS = {"idle": C["gray"], "run": C["accent"], "done": C["good"], "stop": C["warn"], "err": C["bad"]}


# ======================================================================
# 촬영 프로토콜 관리 대화상자
# ======================================================================
class ProtocolDialog(QtWidgets.QDialog):
    """촬영 프로토콜(이름 + 프로토콜 번호 + 선택적 엔진 ObjectType) 추가/수정/삭제. 저장 시 protocols.json 에 기록."""

    def __init__(self, protocols, parent=None):
        super().__init__(parent)
        self.setWindowTitle("촬영 프로토콜 관리")
        self.resize(680, 580)
        self.protocols = None                      # 확인(저장) 후 결과 목록
        lay = QtWidgets.QVBoxLayout(self)
        info = QtWidgets.QLabel(
            "프로토콜 이름과 프로토콜 번호(1001~4001)를 등록합니다. 셀을 더블클릭해 수정할 수 있습니다.\n"
            "'엔진 ObjectType'(선택)은 파이프라인에 전달할 오버라이드 값입니다. 비우면 프로토콜 번호를 그대로 전달하며,\n"
            "파이프라인이 프로토콜 번호(1001~4001)에 맞는 촬영부위(Chest, Abdomen, Spine 등) 알고리즘을 자동 적용합니다.")
        info.setWordWrap(True)
        info.setStyleSheet(f"color:{C['muted']};")
        lay.addWidget(info)

        self.table = QtWidgets.QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["프로토콜 이름 (예: ABDOMEN_ERECT)", "프로토콜 번호", "엔진 ObjectType (선택)"])
        self.table.horizontalHeaderItem(2).setToolTip("비우면 프로토콜 번호(예: 1107)를 그대로 ObjectType 으로 전달합니다 (C++ TObjectType 4자리 코드와 동일).\n"
                                                      "다른 ObjectType 코드를 강제 지정할 때만 입력하세요.")
        self.table.horizontalHeader().setSectionResizeMode(0, QtWidgets.QHeaderView.Stretch)
        self.table.setColumnWidth(1, 110)
        self.table.setColumnWidth(2, 170)
        self.table.verticalHeader().setVisible(False)
        self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        for p in protocols:
            self._add_row(p["name"], p["code"], p.get("object_type", ""))
        lay.addWidget(self.table, 1)

        row = QtWidgets.QHBoxLayout()
        for text, tip, slot in (
                ("+ 추가", "새 프로토콜 행을 추가합니다.", lambda: self._add_row("", "", edit=True)),
                ("선택 삭제", "선택한 행을 삭제합니다.", self._delete_selected),
                ("기본 목록으로", "기본 프로토콜 목록으로 되돌립니다. (확인을 누르기 전에는 저장되지 않음)", self._restore_defaults),
                ("파일 위치 열기", f"저장 파일 위치를 엽니다.\n{PROTOCOLS_PATH}", self._open_folder)):
            b = QtWidgets.QPushButton(text)
            b.setToolTip(tip)
            b.clicked.connect(slot)
            row.addWidget(b)
        row.addStretch(1)
        lay.addLayout(row)

        bb = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Save | QtWidgets.QDialogButtonBox.Cancel)
        bb.button(QtWidgets.QDialogButtonBox.Save).setText("저장")
        bb.button(QtWidgets.QDialogButtonBox.Cancel).setText("취소")
        bb.accepted.connect(self._on_save)
        bb.rejected.connect(self.reject)
        lay.addWidget(bb)

    def _add_row(self, name, code, object_type="", edit=False):
        r = self.table.rowCount()
        self.table.insertRow(r)
        it_name = QtWidgets.QTableWidgetItem(str(name))
        it_code = QtWidgets.QTableWidgetItem(str(code))
        it_ot = QtWidgets.QTableWidgetItem("" if object_type in (None, "") else str(object_type))
        for it in (it_code, it_ot):
            it.setTextAlignment(Qt.AlignCenter)
        self.table.setItem(r, 0, it_name)
        self.table.setItem(r, 1, it_code)
        self.table.setItem(r, 2, it_ot)
        if edit:
            self.table.setCurrentCell(r, 0)
            self.table.editItem(it_name)

    def _delete_selected(self):
        for r in sorted({i.row() for i in self.table.selectedIndexes()}, reverse=True):
            self.table.removeRow(r)

    def _restore_defaults(self):
        self.table.setRowCount(0)
        for p in DEFAULT_PROTOCOLS:
            self._add_row(p["name"], p["code"], p.get("object_type", ""))

    @staticmethod
    def _open_folder():
        os.startfile(os.path.dirname(PROTOCOLS_PATH))

    def _collect(self):
        out = []
        for r in range(self.table.rowCount()):
            name = self.table.item(r, 0).text().strip()
            code = self.table.item(r, 1).text().strip()
            ot = self.table.item(r, 2).text().strip()
            item = {"name": name, "code": code}
            if ot:
                item["object_type"] = ot
            out.append(item)
        return out

    def _on_save(self):
        items = self._collect()
        msg = validate_protocols(items)
        if msg:
            QtWidgets.QMessageBox.warning(self, "입력 확인", msg)
            return
        items = [{"name": p["name"], "code": int(p["code"]),
                  **({"object_type": int(p["object_type"])} if "object_type" in p else {})} for p in items]
        try:
            save_protocols(items)
        except OSError as ex:
            QtWidgets.QMessageBox.critical(
                self, "저장 실패", f"protocols.json 을 저장할 수 없습니다.\n쓰기 권한이 있는 위치에서 실행하세요.\n\n{ex}")
            return
        self.protocols = items
        self.accept()


def default_target_path() -> str:
    cand = rp.RAW_PATH.replace(".dcm.raw", ".dcm.post.target.raw")
    return cand if os.path.exists(cand) else ""


def fmt_time(sec: float) -> str:
    sec = int(sec)
    return f"{sec // 60:02d}:{sec % 60:02d}"


def rgba(hex_color, a):
    c = QtGui.QColor(hex_color)
    return f"rgba({c.red()},{c.green()},{c.blue()},{a})"


# ======================================================================
# 워커 스레드
# ======================================================================
class Worker(QtCore.QThread):
    log = pyqtSignal(str)
    stage = pyqtSignal(str)
    evaled = pyqtSignal(object)
    done = pyqtSignal(object)
    failed = pyqtSignal(str)

    def __init__(self, cfg, init, defs, single=False):
        super().__init__()
        self.cfg, self.init, self.defs, self.single = cfg, init, defs, single
        self._stop = False

    def stop(self):
        self._stop = True

    def run(self):
        try:
            job = TuneJob(self.cfg, self.log.emit, self.evaled.emit, self.stage.emit,
                          lambda: self._stop)
            if self.single:
                out, loss, detail = job.run_single(self.init)
                self.evaled.emit({"n": 1, "loss": loss, "detail": detail, "params": self.init,
                                  "is_best": True, "out": out, "best_loss": loss})
                self.done.emit({"single": True, "best": self.init, "out": out, "detail": detail})
            else:
                self.done.emit(job.run(self.init, self.defs))
        except Exception:
            self.failed.emit(traceback.format_exc())


# ======================================================================
# 메인 윈도우
# ======================================================================
class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(f"Post Param Tuner  (bit depth {BIT})")
        self.resize(1760, 1020)
        self.src = self.tgt = self.out = None
        self.worker = None
        self.last_out_dir = None
        self.t_start = 0.0
        self.n_evals = 0
        self.n_improved = 0
        self.base_detail = None
        self.single_mode = False
        self.stop_requested = False
        self.opt_cfg = OptConfig()
        self.activity = ""
        self.known_sizes = [tuple(s) for s in KNOWN_SIZES]       # 크기 자동 추정에 쓰는 알려진 (가로, 세로)
        self.base = default_params()
        self.defs = build_param_defs(self.base)
        self._build_ui()
        self._load_settings()
        self.refresh_inputs(quiet=True)
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self._tick)

    # ---------------------------------------------------------------- UI
    @staticmethod
    def _spin(v, lo, hi, tip=""):
        s = QtWidgets.QSpinBox()
        s.setRange(lo, hi)
        s.setValue(v)
        s.setToolTip(tip)
        return s

    @staticmethod
    def _label(text, tip=""):
        l = QtWidgets.QLabel(text)
        l.setProperty("role", "muted")
        l.setStyleSheet(f"color:{C['muted']};")
        l.setToolTip(tip)
        return l

    def _build_ui(self):
        central = QtWidgets.QWidget()
        central.setObjectName("root")
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)
        root.setContentsMargins(14, 10, 14, 6)
        root.setSpacing(8)

        # ---- 헤더
        head = QtWidgets.QHBoxLayout()
        t = QtWidgets.QLabel("Post Param Tuner")
        t.setObjectName("title")
        t.setToolTip("target post 와 가장 유사한 결과를 만드는 파라미터를 자동 탐색합니다.")
        chip = QtWidgets.QLabel(f"{BIT}-bit")
        chip.setObjectName("chip")
        self.badge = QtWidgets.QLabel()
        self.btn_settings = QtWidgets.QToolButton()
        self.btn_settings.setText("설정 (영상/최적화/Loss)")
        self.btn_settings.setCheckable(True)
        self.btn_settings.setChecked(False)   # 기본 접힘: 영상/그래프 공간 확보
        self.btn_settings.toggled.connect(lambda v: self.settings_box.setVisible(v))
        head.addWidget(t)
        head.addSpacing(8)
        head.addWidget(chip)
        head.addSpacing(18)
        self.head, self._btn_slot = head, head.count()   # 버튼은 아래에서 이 위치에 삽입
        head.addStretch(1)
        head.addWidget(self.badge)
        head.addWidget(self.btn_settings)
        root.addLayout(head)
        self._set_status("idle", "대기 중")

        # ---- 입력 파일
        pc = QtWidgets.QFrame()
        pc.setObjectName("card")
        pg = QtWidgets.QGridLayout(pc)
        pg.setContentsMargins(12, 8, 12, 8)
        self.ed_src = QtWidgets.QLineEdit()          # 경로는 설정 파일에 저장하지 않음 (드래그 앤 드롭/찾아보기로 지정)
        self.ed_tgt = QtWidgets.QLineEdit()
        self.ed_src.setPlaceholderText("Source raw 파일을 영상 패널로 드래그하거나 [찾아보기]")
        self.ed_tgt.setPlaceholderText("Target raw 파일을 영상 패널로 드래그하거나 [찾아보기]")
        for r, (lbl, ed, tip) in enumerate((
                ("Source (pre)", self.ed_src, "처리할 입력(pre) raw 영상. 14bit: int16 / 16bit: uint16 little-endian"),
                ("Target (post)", self.ed_tgt, "닮아야 할 기준(target post) raw 영상. Source 와 같은 크기/비트 심도"))):
            b = QtWidgets.QPushButton("찾아보기")
            b.clicked.connect(lambda _=False, e=ed: self._browse(e))
            ed.editingFinished.connect(lambda e=ed: self._load_path(e, e.text()))   # 직접 입력한 경로도 크기 추정 후 로드
            ed.setToolTip(tip)
            pg.addWidget(self._label(lbl, tip), r, 0)
            pg.addWidget(ed, r, 1)
            pg.addWidget(b, r, 2)
        pg.setColumnStretch(1, 1)
        root.addWidget(pc)

        # ---- 설정 (접기 가능)
        self.settings_box = QtWidgets.QWidget()
        sl = QtWidgets.QHBoxLayout(self.settings_box)
        sl.setContentsMargins(0, 0, 0, 0)
        sl.setSpacing(8)

        gi = QtWidgets.QGroupBox("영상 / 장비")
        g = QtWidgets.QGridLayout(gi)
        self.sp_w = self._spin(rp.WIDTH, 16, 20000, "영상 가로 픽셀 수")
        self.sp_h = self._spin(rp.HEIGHT, 16, 20000, "영상 세로 픽셀 수")
        for sp in (self.sp_w, self.sp_h):
            sp.setToolTip(sp.toolTip() + "\n값을 바꾸면 지정된 영상을 새 크기로 다시 불러옵니다.\n"
                          "영상을 끌어다 놓으면 파일 크기로 가로×세로를 자동 설정합니다.")
            sp.setKeyboardTracking(False)          # 입력 도중(예: 3,30,303)이 아니라 확정(Enter/포커스 이동) 시에만 반영
            sp.valueChanged.connect(self._on_size_changed)
        self.cb_bit = QtWidgets.QComboBox()
        self.cb_bit.addItems(["14", "16"])
        self.cb_bit.setCurrentText(str(BIT))
        self.cb_bit.setToolTip("처리 비트 심도. 변경 시 앱 재시작이 필요합니다 (pe_post 가 import 시점에 고정).")
        self.cb_bit.activated.connect(self._on_bit_changed)
        self.protocols = load_protocols()
        self.cb_body = QtWidgets.QComboBox()
        self.cb_body.setToolTip("촬영 프로토콜 (예: CHEST_PA, ABDOMEN_ERECT). 괄호 안은 프로토콜 번호(1001~4001)입니다.\n"
                                "프로토콜 번호에 따라 엔진 촬영부위(ObjectType) 및 전용 보정(Chest, Abdomen, Spine 등)이 자동 적용됩니다.")
        self._fill_protocols(select_name="CHEST_PA", default_code=1107)                 # 최초 기본 선택
        self.cb_body.currentIndexChanged.connect(lambda _=0: self._sync_protocol_param_defs())
        self.btn_proto = QtWidgets.QPushButton("관리")
        self.btn_proto.setToolTip("촬영 프로토콜을 추가·수정·삭제합니다.\n"
                                  "목록은 protocols.json 에 저장되어 재빌드 없이 바로 반영됩니다.")
        self.btn_proto.clicked.connect(self._on_manage_protocols)
        self.ed_shutter = QtWidgets.QLineEdit(",".join(str(int(v)) for v in rp.AUTO_SHUTTER_POINTS.flatten()))
        self.ed_shutter.setToolTip("Auto-Shutter(조사야) 좌표 4점: x1,y1(좌상), x2,y2(좌하), x3,y3(우하), x4,y4(우상)")
        for k, (lbl, w) in enumerate((("가로 W", self.sp_w), ("세로 H", self.sp_h), ("Bit", self.cb_bit))):
            g.addWidget(self._label(lbl), 0, k * 2)
            g.addWidget(w, 0, k * 2 + 1)
        g.addWidget(self._label("촬영 프로토콜"), 1, 0)
        g.addWidget(self.cb_body, 1, 1, 1, 4)
        g.addWidget(self.btn_proto, 1, 5)
        g.addWidget(self._label("Shutter"), 2, 0)
        g.addWidget(self.ed_shutter, 2, 1, 1, 5)
        sl.addWidget(gi, 5)

        go = QtWidgets.QGroupBox("최적화")
        g = QtWidgets.QGridLayout(go)
        self.sp_evals = self._spin(800, 1, 100000, "최대 평가(파이프라인 실행) 횟수(전체 Iteration 합산). 도달하면 종료합니다.")
        self.sp_min = self._spin(30, 1, 1440, "최대 소요 시간(분). 도달하면 종료합니다.")
        self.sp_rounds = self._spin(
            1, 1, 10,
            "전체 그룹(계조/톤 → Manual LUT) 탐색을 처음부터 끝까지 반복(Iteration)하는 횟수.\n"
            "2 이상이면 이전 Iteration 의 최적 파라미터를 바탕으로 앞 그룹부터 다시 미세 조정합니다."
        )
        self.sp_rounds.valueChanged.connect(self._on_rounds_changed)
        for k, (lbl, w) in enumerate((("최대 평가수", self.sp_evals), ("최대 시간(분)", self.sp_min),
                                      ("Iteration (반복)", self.sp_rounds))):
            g.addWidget(self._label(lbl), k // 2, (k % 2) * 2)
            g.addWidget(w, k // 2, (k % 2) * 2 + 1)
        sl.addWidget(go, 3)

        gw = QtWidgets.QGroupBox("Loss 가중치")
        g = QtWidgets.QGridLayout(gw)
        self.wsp = {}
        for k, (key, lbl, tip) in enumerate((
                ("pix", "픽셀 오차", "L1(L1 Norm, 평균 절대 오차). 전체 밝기/톤 일치에 가장 민감합니다."),
                ("ssim", "SSIM(Structural Similarity Index Measure)", "1 - SSIM(Structural Similarity Index Measure) (구조 유사도). 형태/질감의 일치를 반영합니다."),
                ("edge", "엣지", "경사(gradient) 크기 차이. 선예도/디테일 파라미터에 민감합니다."),
                ("hist", "히스토그램", "분위수 차이. 대비/밝기 분포 일치를 반영합니다."))):
            sp = QtWidgets.QDoubleSpinBox()
            sp.setRange(0, 100)
            sp.setSingleStep(0.1)
            sp.setValue(1.0)
            sp.setToolTip(tip)
            self.wsp[key] = sp
            g.addWidget(self._label(lbl, tip), k, 0)      # 긴 이름(SSIM 풀네임) 때문에 한 열로 배치
            g.addWidget(sp, k, 1)
        sl.addWidget(gw, 4)
        self.settings_box.setVisible(False)
        root.addWidget(self.settings_box)

        # ---- 실행 버튼 + 진행 상황
        self.btn_run = QtWidgets.QPushButton("▶  실행 (자동 튜닝)")
        self.btn_run.setObjectName("primary")
        self.btn_stop = QtWidgets.QPushButton("■  중지")
        self.btn_stop.setObjectName("danger")
        self.btn_single = QtWidgets.QPushButton("1회 처리")
        self.btn_load = QtWidgets.QPushButton("파라미터 불러오기")
        self.btn_open = QtWidgets.QPushButton("결과 폴더")
        self.btn_run.setToolTip("default 파라미터로 처리한 뒤 target 과 비교하며 파라미터를 자동 탐색합니다.")
        self.btn_stop.setToolTip("현재 평가가 끝나는 즉시 중단하고, 지금까지의 best 결과를 저장합니다.")
        self.btn_single.setToolTip("탐색 없이 테이블의 '초기' 값으로 한 번만 처리하고 loss 를 표시합니다.")
        self.btn_load.setToolTip("저장된 best_params.json 을 읽어 '초기' 값으로 채웁니다.")
        self.btn_open.setToolTip("마지막 결과가 저장된 폴더를 엽니다.")
        self.btn_stop.setEnabled(False)
        self.btn_open.setEnabled(False)
        self.btn_run.clicked.connect(lambda: self.on_run(False))
        self.btn_stop.clicked.connect(self.on_stop)
        self.btn_single.clicked.connect(lambda: self.on_run(True))
        self.btn_load.clicked.connect(self.on_load_params)
        self.btn_open.clicked.connect(lambda: self.last_out_dir and os.startfile(self.last_out_dir))

        prog = QtWidgets.QFrame()
        prog.setObjectName("card")
        pv = QtWidgets.QVBoxLayout(prog)
        pv.setContentsMargins(12, 10, 12, 10)
        pv.setSpacing(8)
        for k, b in enumerate((self.btn_run, self.btn_stop, self.btn_single, self.btn_load, self.btn_open)):
            self.head.insertWidget(self._btn_slot + k, b)
        self.stepper = StageStepper()
        self.stepper.setToolTip("탐색은 그룹 순서대로 진행됩니다. 맥동하는 칸이 현재 단계입니다.\nManual LUT(Look-Up Table) 단계는 마지막에 톤 곡선(제어점)을 미세 조정합니다.")
        pv.addWidget(self.stepper)
        self.lbl_activity = QtWidgets.QLabel("Source/Target 을 지정하고 [실행]을 누르세요.")
        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 1000)
        self.progress.setTextVisible(False)
        self.progress.setFixedHeight(12)
        self.progress.setToolTip("진행률 = max(평가수/최대 평가수, 경과 시간/최대 시간). 수렴하면 그 전에 종료될 수 있습니다.")
        pv.addWidget(self.lbl_activity)
        pv.addWidget(self.progress)
        cards = QtWidgets.QHBoxLayout()
        cards.setSpacing(8)
        self.card_loss = MetricCard("BEST LOSS", metric_tooltip_html("loss"))
        self.card_psnr = MetricCard("PSNR(Peak Signal-to-Noise Ratio)", metric_tooltip_html("psnr"))
        self.card_ssim = MetricCard("SSIM(Structural Similarity Index Measure)", metric_tooltip_html("ssim"))
        self.card_mae = MetricCard("MAE(Mean Absolute Error)", metric_tooltip_html("mae"))
        self.card_eval = MetricCard("평가 / 시간", metric_tooltip_html("evals"))
        for c in (self.card_loss, self.card_psnr, self.card_ssim, self.card_mae, self.card_eval):
            cards.addWidget(c, 1)
        pv.addLayout(cards)
        root.addWidget(prog)

        # ---- 영상 + 파라미터
        split = QtWidgets.QSplitter(Qt.Horizontal)
        left = QtWidgets.QWidget()
        lv = QtWidgets.QVBoxLayout(left)
        lv.setContentsMargins(0, 0, 0, 0)
        lv.setSpacing(6)

        self.panels = [ImagePanel("Source (pre)", C["gray"], PIX_MAX, "raw 파일을 여기로 드래그하세요", "Source 로 불러오기"),
                       ImagePanel("Target (post)", C["good"], PIX_MAX, "raw 파일을 여기로 드래그하세요", "Target 으로 불러오기"),
                       ImagePanel("Output", C["accent"], PIX_MAX, "[실행] 후 표시됩니다")]
        self.panels[0].fileDropped.connect(lambda path: self._on_file_dropped(self.ed_src, "Source", path))
        self.panels[1].fileDropped.connect(lambda path: self._on_file_dropped(self.ed_tgt, "Target", path))
        ph = QtWidgets.QHBoxLayout()
        ph.setSpacing(8)
        for p in self.panels:
            ph.addWidget(p, 1)
            p.viewChanged.connect(self._sync_view)
            p.hovered.connect(self._on_hover)
        for i, p in enumerate(self.panels):
            p.windowDragged.connect(lambda lo, hi, i=i: self._on_wl_dragged(i, lo, hi))
            p.autoWindowRequested.connect(lambda x0, y0, x1, y1, i=i: self._on_auto_window_region(i, x0, y0, x1, y1))
        lv.addLayout(ph, 5)

        wl = QtWidgets.QHBoxLayout()
        self.sp_lo = self._spin(0, 0, PIX_MAX, "Target/Output 표시 하한 (Window lo)")
        self.sp_hi = self._spin(PIX_MAX, 1, PIX_MAX, "Target/Output 표시 상한 (Window hi)")
        self.sp_lo.valueChanged.connect(self._apply_window)
        self.sp_hi.valueChanged.connect(self._apply_window)
        btn_auto = QtWidgets.QPushButton("Auto W/L(Window/Level)")
        btn_auto.setToolTip("Target 의 0.5%~99.5% 분위수로 표시 범위를 자동 설정합니다.")
        btn_auto.clicked.connect(self._auto_window)
        btn_view = QtWidgets.QPushButton("뷰 리셋")
        btn_view.setToolTip("줌/이동을 처음 상태(전체 보기)로 되돌립니다. (영상 위에서 Ctrl+더블클릭도 동일)")
        btn_view.clicked.connect(self._reset_views)
        self.chk_diff = QtWidgets.QCheckBox("Output 을 차이 영상으로 표시")
        self.chk_diff.setToolTip("|Output - Target| 을 표시합니다. 차이가 큰 위치가 밝게 보입니다 (표시 범위의 25% 차이 = 흰색).")
        self.chk_diff.toggled.connect(self._refresh_output_panel)
        wl.addWidget(self._label("Window"))
        wl.addWidget(self.sp_lo)
        wl.addWidget(self._label("~"))
        wl.addWidget(self.sp_hi)
        wl.addWidget(btn_auto)
        wl.addWidget(btn_view)
        wl.addSpacing(10)
        wl.addWidget(self.chk_diff)
        wl.addStretch(1)
        for p in self.panels:
            p.setToolTip("휠: 줌  /  왼쪽 드래그: 이동  (줌·이동은 3패널 동기화)\n"
                         "오른쪽 드래그: W/L(Window/Level) 조정 (좌우 = Window 폭, 위아래 = Level, 위로 = 밝게)\n"
                         "더블클릭: 화면에 보이는 영역 기준 자동 W/L  /  Ctrl+더블클릭: 뷰 리셋\n"
                         "Target·Output 은 W/L 을 공유하고 Source 는 따로 조정됩니다.\n"
                         "마우스 위치의 화소값은 하단 상태바에 표시됩니다.")
        lv.addLayout(wl)

        bottom = QtWidgets.QSplitter(Qt.Horizontal)
        pw = QtWidgets.QWidget()
        pl = QtWidgets.QVBoxLayout(pw)
        pl.setContentsMargins(0, 0, 0, 0)
        pl.setSpacing(4)
        mode_row = QtWidgets.QHBoxLayout()
        mode_row.addStretch(1)
        mode_row.addWidget(self._label("Y 축"))
        self.btn_zoom = QtWidgets.QPushButton("확대")
        self.btn_full = QtWidgets.QPushButton("전체")
        for b, m in ((self.btn_zoom, "zoom"), (self.btn_full, "full")):
            b.setCheckable(True)
            b.setProperty("seg", True)
            b.clicked.connect(lambda _=False, mm=m: self._set_plot_mode(mm))
            mode_row.addWidget(b)
        self.btn_zoom.setToolTip("최근 평가값 범위에 맞춰 확대 (후반부 미세 개선 확인용)")
        self.btn_full.setToolTip("초기값부터 전체 범위를 표시")
        self.btn_zoom.setChecked(True)
        self.plot = LossPlot()
        pl.addLayout(mode_row)
        pl.addWidget(self.plot, 1)
        self.logbox = QtWidgets.QPlainTextEdit()
        self.logbox.setReadOnly(True)
        self.logbox.setMaximumBlockCount(3000)
        bottom.addWidget(pw)
        bottom.addWidget(self.logbox)
        bottom.setStretchFactor(0, 3)
        bottom.setStretchFactor(1, 2)
        lv.addWidget(bottom, 4)
        split.addWidget(left)

        right = QtWidgets.QWidget()
        rv = QtWidgets.QVBoxLayout(right)
        rv.setContentsMargins(0, 0, 0, 0)
        rv.addWidget(self._label("파라미터  (이름에 마우스를 올리면 설명이 표시됩니다)"))
        self.table = QtWidgets.QTableWidget(len(self.defs), 7)
        self.table.setHorizontalHeaderLabels(["탐색", "", "이름", "초기", "현재 best", "최소", "최대"])
        self.table.verticalHeader().setVisible(False)
        self.table.verticalHeader().setDefaultSectionSize(26)
        self.table.setAlternatingRowColors(True)
        self.table.setShowGrid(False)
        self._fill_table()
        rv.addWidget(self.table, 1)
        split.addWidget(right)
        split.setStretchFactor(0, 5)
        split.setStretchFactor(1, 3)
        split.setSizes([1000, 560])
        split.setChildrenCollapsible(False)
        root.addWidget(split, 1)
        self.statusBar().showMessage("준비됨")

    def _fill_table(self):
        self.table.blockSignals(True)
        heads = ["이 파라미터를 탐색에 포함할지 선택", "파라미터 그룹 (탐색 순서대로 진행)",
                 "파라미터 이름 [PostParam 인덱스]", "탐색 시작값 (run_pipeline 기본값). 편집 가능",
                 "탐색 중 발견한 현재 최적값 (변경된 값은 강조 표시)", "탐색 최소값 (편집 가능)", "탐색 최대값 (편집 가능)"]
        for c, tip in enumerate(heads):
            self.table.horizontalHeaderItem(c).setToolTip(tip)
        for r, d in enumerate(self.defs):
            tip = tooltip_html(d)
            chk = QtWidgets.QTableWidgetItem()
            chk.setFlags(Qt.ItemIsUserCheckable | Qt.ItemIsEnabled)
            chk.setCheckState(Qt.Checked if d.enabled else Qt.Unchecked)
            grp = QtWidgets.QTableWidgetItem("●")      # 그룹 색상 점 (그룹명은 툴팁)
            grp.setForeground(QtGui.QBrush(QtGui.QColor(GROUP_COLORS[d.group])))
            grp.setTextAlignment(Qt.AlignCenter)
            idx = f" [{d.index}]" if d.kind == "post" else ""
            cells = [chk, grp, QtWidgets.QTableWidgetItem(f"{d.name}{idx}"),
                     QtWidgets.QTableWidgetItem(str(d.default)), QtWidgets.QTableWidgetItem(""),
                     QtWidgets.QTableWidgetItem(str(d.lo)), QtWidgets.QTableWidgetItem(str(d.hi))]
            for c, it in enumerate(cells):
                if c in (1, 2, 4):
                    it.setFlags(Qt.ItemIsEnabled)
                if c in (3, 5, 6):
                    it.setTextAlignment(Qt.AlignCenter)
                it.setToolTip(tip)
                self.table.setItem(r, c, it)
        for c, wd in ((0, 44), (1, 26), (2, 168), (3, 52), (4, 92), (5, 48), (6, 48)):
            self.table.setColumnWidth(c, wd)
        self.table.blockSignals(False)

    # ---------------------------------------------------------------- 상태 표시
    def _set_status(self, kind, text):
        col = STATUS_COLORS[kind]
        self.badge.setText(f"●  {text}")
        self.badge.setStyleSheet(f"background:{rgba(col, 36)}; color:{col}; border:1px solid {col};"
                                 f"border-radius:12px; padding:4px 16px; font-weight:700;")

    def _set_plot_mode(self, mode):
        self.btn_zoom.setChecked(mode == "zoom")
        self.btn_full.setChecked(mode == "full")
        self.plot.set_mode(mode)

    def _tick(self):
        self.stepper.tick()
        el = time.time() - self.t_start
        if self.single_mode:
            self.lbl_activity.setText(f"<span style='color:{C['accent']}'>●</span> 1회 처리 중… "
                                      f"<span style='color:{C['muted']}'>경과 {fmt_time(el)}</span>")
        else:
            frac = max(self.n_evals / max(self.opt_cfg.max_evals, 1), el / max(self.opt_cfg.max_seconds, 1))
            self.progress.setValue(int(min(frac, 1.0) * 1000))
            dot_col = C["accent"] if int(el * 2) % 2 == 0 else C["muted"]
            self.lbl_activity.setText(
                f"<span style='color:{dot_col}'>●</span> <b>{self.activity}</b> "
                f"<span style='color:{C['muted']}'>· 평가 {self.n_evals}/{self.opt_cfg.max_evals} · "
                f"경과 {fmt_time(el)} / {fmt_time(self.opt_cfg.max_seconds)} · 진행률 {frac * 100:.0f}% (최대 평가수/시간 기준) "
                f"(평가 1회에 수 초~40초 소요될 수 있음)</span>")
        self.card_eval.set(f"{self.n_evals}", f"{fmt_time(el)} 경과 · 개선 {self.n_improved}회")

    # ---------------------------------------------------------------- 촬영 프로토콜
    def _fill_protocols(self, select_name=None, select_code=None, default_code=None):
        """콤보박스를 self.protocols 로 채우고 (이름 → 코드 → default_code 순으로) 선택을 복원."""
        self.cb_body.blockSignals(True)
        self.cb_body.clear()
        for p in self.protocols:
            ot = engine_object_type(p)
            label = f"{p['name']} ({p['code']})" if ot == p["code"] else f"{p['name']} ({p['code']} → ObjectType {ot})"
            self.cb_body.addItem(label, p["code"])
            self.cb_body.setItemData(self.cb_body.count() - 1, p["name"], Qt.UserRole + 1)
            self.cb_body.setItemData(self.cb_body.count() - 1, ot, Qt.UserRole + 2)       # 파이프라인에 전달할 값
        self.cb_body.blockSignals(False)
        self._select_protocol(select_name, select_code if select_code is not None else default_code)
        self._sync_protocol_param_defs()

    def _select_protocol(self, name=None, code=None):
        def _norm_name(s):
            return str(s).strip().upper().replace("W/O", "WO").replace("W/", "W").replace("-", "_").replace(" ", "_")

        idx = -1
        if name:
            idx = next((i for i in range(self.cb_body.count())
                        if self.cb_body.itemData(i, Qt.UserRole + 1) == name), -1)
            if idx < 0:
                target_n = _norm_name(name)
                idx = next((i for i in range(self.cb_body.count())
                            if _norm_name(self.cb_body.itemData(i, Qt.UserRole + 1) or "") == target_n), -1)
        if idx < 0 and code is not None:
            idx = self.cb_body.findData(code)
            if idx < 0:
                idx = next((i for i in range(self.cb_body.count())
                            if self.cb_body.itemData(i, Qt.UserRole + 2) == int(code)), -1)
        if idx >= 0:
            self.cb_body.setCurrentIndex(idx)
        elif self.cb_body.currentIndex() < 0 and self.cb_body.count():
            self.cb_body.setCurrentIndex(0)

    def _current_protocol_name(self):
        return self.cb_body.itemData(self.cb_body.currentIndex(), Qt.UserRole + 1) or ""

    def _current_object_type(self):
        """선택된 프로토콜의 엔진 ObjectType (미지정이면 프로토콜 번호)."""
        v = self.cb_body.itemData(self.cb_body.currentIndex(), Qt.UserRole + 2)
        return int(v) if v is not None else rp.OBJECT_TYPE

    def _sync_protocol_param_defs(self):
        """선택된 촬영 프로토콜(Chest vs 비-Chest)에 맞춰 부위 전용/미사용 파라미터 체크박스를 자동 전환."""
        ot = self._current_object_type()
        apply_object_type_mask(self.defs, ot)
        if not hasattr(self, "table"):
            return
        cond_indices = UNUSED_POST | CHEST_ONLY_POST | NON_CHEST_ONLY_POST
        self.table.blockSignals(True)
        for r, d in enumerate(self.defs):
            if d.kind == "post" and d.index in cond_indices:
                it = self.table.item(r, 0)
                if it is not None:
                    it.setCheckState(Qt.Checked if d.enabled else Qt.Unchecked)
        self.table.blockSignals(False)

    def _on_manage_protocols(self):
        if self.worker and self.worker.isRunning():
            self.statusBar().showMessage("실행 중에는 프로토콜을 편집할 수 없습니다. 중지 후 다시 시도하세요.", 5000)
            return
        dlg = ProtocolDialog(self.protocols, self)
        if dlg.exec_() == QtWidgets.QDialog.Accepted and dlg.protocols:
            keep_name, keep_code = self._current_protocol_name(), self.cb_body.currentData()
            self.protocols = dlg.protocols
            self._fill_protocols(select_name=keep_name, select_code=keep_code)
            self.statusBar().showMessage(f"프로토콜 {len(self.protocols)}개 저장됨: {PROTOCOLS_PATH}", 6000)
            self.logbox.appendPlainText(f"[프로토콜] 목록 저장 ({len(self.protocols)}개): {PROTOCOLS_PATH}")

    # ---------------------------------------------------------------- 설정 저장/복원
    def _on_rounds_changed(self, rounds: int):
        """Iteration 횟수를 늘릴 때 전체 최대 평가수가 부족하면 자동으로 상향 조정."""
        min_needed = 800 if rounds <= 1 else 600 + (rounds - 1) * 500
        if self.sp_evals.value() < min_needed:
            self.sp_evals.setValue(800 if rounds <= 1 else rounds * 700)

    def _load_settings(self):
        try:
            with open(bootstrap.SETTINGS_PATH, encoding="utf-8") as f:
                s = json.load(f)
        except Exception:
            return
        self.sp_rounds.blockSignals(True)
        for k, w in (("width", self.sp_w), ("height", self.sp_h),
                     ("max_evals", self.sp_evals), ("max_minutes", self.sp_min), ("rounds", self.sp_rounds)):
            if k in s:
                w.setValue(int(s[k]))
        self.sp_rounds.blockSignals(False)
        self._on_rounds_changed(self.sp_rounds.value())
        if "shutter" in s:
            self.ed_shutter.setText(s["shutter"])
        self._select_protocol(s.get("protocol"), s.get("body"))
        for sz in s.get("known_sizes", []):
            try:
                self._remember_size(int(sz[0]), int(sz[1]))
            except (TypeError, ValueError, IndexError):
                pass
        for k, w in self.wsp.items():
            if k in s.get("weights", {}):
                w.setValue(s["weights"][k])

    def _save_settings(self, bit=None):
        s = {"width": self.sp_w.value(), "height": self.sp_h.value(), "bit_depth": bit or BIT,
             "protocol": self._current_protocol_name(), "body": self.cb_body.currentData(),
             "shutter": self.ed_shutter.text(), "known_sizes": [list(sz) for sz in self.known_sizes[-30:]],
             "max_evals": self.sp_evals.value(), "max_minutes": self.sp_min.value(),
             "rounds": self.sp_rounds.value(), "weights": {k: w.value() for k, w in self.wsp.items()}}
        try:
            with open(bootstrap.SETTINGS_PATH, "w", encoding="utf-8") as f:
                json.dump(s, f, indent=2, ensure_ascii=False)
        except Exception:
            pass

    def closeEvent(self, e):
        if self.worker and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait(60000)
        self._save_settings()
        super().closeEvent(e)

    def _on_bit_changed(self):
        new = int(self.cb_bit.currentText())
        if new == BIT:
            return
        r = QtWidgets.QMessageBox.question(self, "비트 심도 변경",
                                           f"비트 심도를 {new}bit 로 바꾸려면 재시작이 필요합니다. 지금 재시작할까요?")
        if r == QtWidgets.QMessageBox.Yes:
            self._save_settings(bit=new)
            args = [a for i, a in enumerate(sys.argv)
                    if a != "--bit-depth" and not (i > 0 and sys.argv[i - 1] == "--bit-depth")]
            if bootstrap.FROZEN:      # exe 는 sys.argv[0] 이 자기 자신이므로 인자만 전달
                args = args[1:]
            os.execv(sys.executable, [sys.executable] + args)
        self.cb_bit.setCurrentText(str(BIT))

    # ---------------------------------------------------------------- 입력/표시
    def _browse(self, ed):
        path, _ = QtWidgets.QFileDialog.getOpenFileName(self, "raw 영상 선택", os.path.dirname(ed.text()) or "",
                                                        "Raw (*.raw);;All (*)")
        if path:
            self._load_path(ed, path)

    # -- 영상 크기: 파일 크기로 추정해 입력 크기를 자동 갱신
    def _set_size(self, w, h):
        for sp, v in ((self.sp_w, w), (self.sp_h, h)):
            sp.blockSignals(True)                  # valueChanged 로 인한 중복 재로드 방지
            sp.setValue(v)
            sp.blockSignals(False)

    def _remember_size(self, w, h):
        if (w, h) not in self.known_sizes:
            self.known_sizes.append((w, h))

    def _ensure_size_for(self, path):
        """path 파일이 현재 가로×세로와 맞지 않으면 파일 크기로 크기를 추정해 입력값을 갱신한다.
        반환: 계속 진행해도 되면 True (사용자가 크기 선택을 취소하거나 추정 불가이면 False)."""
        try:
            nbytes = os.path.getsize(path)
        except OSError as ex:
            QtWidgets.QMessageBox.warning(self, "불러오기 실패", str(ex))
            return False
        w, h = self.sp_w.value(), self.sp_h.value()
        if nbytes == w * h * 2:
            return True
        cands = candidate_sizes(nbytes, self.known_sizes)
        if not cands:
            QtWidgets.QMessageBox.warning(
                self, "영상 크기를 알 수 없음",
                f"파일 크기 {nbytes:,} 바이트로 가능한 영상 크기를 찾지 못했습니다.\n"
                f"(화소당 2바이트 가정, 가로 × 세로 × 2 = 파일 크기)\n"
                f"가로/세로를 직접 입력해 주세요. 현재 {w} × {h} → {w * h * 2:,} 바이트")
            return False
        known = [c for c in cands if c[2]]
        if len(known) == 1 or len(cands) == 1:
            nw, nh, _ = (known or cands)[0]
        else:
            items = [f"{cw} × {ch}" + ("   (알려진 크기)" if ck else "") for cw, ch, ck in cands]
            item, ok = QtWidgets.QInputDialog.getItem(
                self, "영상 크기 선택",
                f"파일 크기 {nbytes:,} 바이트 (화소당 2바이트)로 가능한 크기입니다.\n가로 × 세로를 선택하세요:",
                items, 0, False)
            if not ok:
                return False
            nw, nh, _ = cands[items.index(item)]
        self._set_size(nw, nh)
        self._remember_size(nw, nh)
        self.statusBar().showMessage(f"영상 크기를 {nw} × {nh} 로 자동 설정했습니다 (파일 크기 {nbytes:,} 바이트 ÷ 2).", 8000)
        self.logbox.appendPlainText(f"[크기 자동 설정] {os.path.basename(path)}: {nw} × {nh}")
        return True

    def _load_path(self, edit, path):
        """드롭/찾아보기/직접 입력으로 지정된 raw 파일을 불러온다 (필요하면 입력 크기를 먼저 갱신)."""
        if self.worker and self.worker.isRunning():
            self.statusBar().showMessage("실행 중에는 파일을 바꿀 수 없습니다. 중지 후 다시 시도하세요.", 5000)
            return
        if not path.strip():
            self.refresh_inputs(quiet=True)
            return
        path = os.path.normpath(path.strip())        # Qt 는 'D:/..' 형태로 전달 → 윈도우 경로로 정규화
        if not os.path.isfile(path):
            edit.setText(path)
            self.refresh_inputs(quiet=True)
            return
        if not self._ensure_size_for(path):
            return
        edit.setText(path)
        self.refresh_inputs(quiet=False, primary=0 if edit is self.ed_src else 1)
        name = "Source" if edit is self.ed_src else "Target"
        self.statusBar().showMessage(f"{name} 로드: {path}", 5000)
        self.logbox.appendPlainText(f"[{name}] {path}")

    def _clear_image(self, i, attr):
        setattr(self, attr, None)
        self.panels[i].clear()

    def _reset_output(self):
        self.out = None
        self.panels[2].clear()

    def _on_size_changed(self):
        """가로/세로 입력값을 바꾸면 현재 지정된 영상을 새 크기로 다시 불러온다."""
        if self.worker and self.worker.isRunning():
            return
        self.refresh_inputs(quiet=True)

    def refresh_inputs(self, quiet=True, primary=None):
        """Source/Target 을 현재 가로×세로로 (다시) 불러온다.
        불러오지 못한 영상은 이전 영상이 남지 않도록 비운다. primary: 실패 시 대화상자로 알릴 영상(0/1)."""
        w, h = self.sp_w.value(), self.sp_h.value()
        for i, ed, attr, name in ((0, self.ed_src, "src", "Source"), (1, self.ed_tgt, "tgt", "Target")):
            path = ed.text().strip()
            if not path:
                continue
            if not os.path.isfile(path):
                self._clear_image(i, attr)
                continue
            try:
                img = load_raw_image(path, w, h)
            except Exception as ex:
                self._clear_image(i, attr)
                self.logbox.appendPlainText(f"[{name} 불러오기 실패] {ex}")
                self.statusBar().showMessage(f"{name} 를 {w} × {h} 로 불러올 수 없습니다 - {ex}", 10000)
                if not quiet and (primary is None or primary == i):
                    QtWidgets.QMessageBox.warning(self, "불러오기 실패", str(ex))
                continue
            self._remember_size(w, h)
            setattr(self, attr, img)
            lo, hi = np.percentile(img, [0.5, 99.5])
            self.panels[i].subtitle = f"{w}×{h}"
            if i == 0:
                self.panels[0].set_image(img, lo, hi)
            else:
                self.sp_lo.blockSignals(True)
                self.sp_hi.blockSignals(True)
                self.sp_lo.setValue(int(lo))
                self.sp_hi.setValue(int(hi))
                self.sp_lo.blockSignals(False)
                self.sp_hi.blockSignals(False)
                self.panels[1].set_image(img, lo, hi)
        if self.out is not None and self.out.shape != (h, w):      # 이전 크기의 Output 은 더 이상 유효하지 않음
            self._reset_output()
        self._refresh_output_panel()

    def _on_file_dropped(self, edit, name, path):
        self._load_path(edit, path)

    def _auto_window(self):
        ref = self.tgt if self.tgt is not None else self.out
        if ref is None:
            return
        lo, hi = np.percentile(ref, [0.5, 99.5])
        self.sp_lo.setValue(int(lo))
        self.sp_hi.setValue(int(hi))

    def _apply_window(self):
        lo, hi = self.sp_lo.value(), max(self.sp_hi.value(), self.sp_lo.value() + 1)
        self.panels[1].set_window(lo, hi)
        self._refresh_output_panel()

    def _set_shared_window(self, lo, hi):
        """Target/Output 공유 W/L 설정 (스핀박스 갱신 + 한 번만 렌더링)."""
        lo = int(round(max(0, min(PIX_MAX - 1, lo))))
        hi = int(round(min(PIX_MAX, max(lo + 1, hi))))
        for sp, v in ((self.sp_lo, lo), (self.sp_hi, hi)):
            sp.blockSignals(True)
            sp.setValue(v)
            sp.blockSignals(False)
        self._apply_window()
        return lo, hi

    def _in_diff_mode(self):
        return self.chk_diff.isChecked() and self.out is not None

    def _on_wl_dragged(self, idx, lo, hi):
        """영상 위 오른쪽 드래그로 W/L 조정. Source 는 개별, Target/Output 은 공유."""
        if idx == 0:
            lo = max(0.0, min(PIX_MAX - 1.0, lo))
            hi = min(float(PIX_MAX), max(lo + 1.0, hi))
            self.panels[0].set_window(lo, hi)
        elif idx == 2 and self._in_diff_mode():
            self.statusBar().showMessage("차이 영상 표시 중에는 Target 패널에서 W/L(Window/Level)을 조정하세요.", 3000)
            return
        else:
            lo, hi = self._set_shared_window(lo, hi)
        self.statusBar().showMessage(f"W/L(Window/Level)  {int(lo)} ~ {int(hi)}   (W {int(hi - lo)}, L {int((lo + hi) / 2)})")

    def _on_auto_window_region(self, idx, x0, y0, x1, y1):
        """더블클릭: 화면에 보이는 영역의 0.5%~99.5% 분위수로 자동 W/L."""
        arr = (self.src, self.tgt, self.tgt if self._in_diff_mode() else self.out)[idx]
        if arr is None or x1 <= x0 or y1 <= y0:
            return
        region = arr[y0:y1, x0:x1]
        step = max(1, int(np.sqrt(region.size / 400000.0)))      # 큰 영역은 간격 샘플링
        lo, hi = np.percentile(region[::step, ::step], [0.5, 99.5])
        if hi <= lo:
            hi = lo + 1
        if idx == 0:
            self.panels[0].set_window(lo, hi)
        else:
            lo, hi = self._set_shared_window(lo, hi)
        self.statusBar().showMessage(f"화면 기준 자동 W/L(Window/Level): {int(lo)} ~ {int(hi)}   "
                                     f"(영역 x {x0}-{x1}, y {y0}-{y1})", 5000)

    def _reset_views(self):
        for p in self.panels:
            if p._buf is not None:
                h, w = p._buf.shape
                p.set_view(1.0, w / 2, h / 2)

    def _refresh_output_panel(self):
        if self.out is None:
            return
        lo, hi = self.sp_lo.value(), max(self.sp_hi.value(), self.sp_lo.value() + 1)
        pn = self.panels[2]
        if self.chk_diff.isChecked() and self.tgt is not None and self.tgt.shape == self.out.shape:
            d = np.abs(self.out.astype(np.float32) - self.tgt.astype(np.float32))
            pn.set_image(d, 0, (hi - lo) * 0.25)
            pn.title, pn.dot = "|Output - Target|", C["bad"]
        else:
            pn.set_image(self.out, lo, hi)
            pn.title, pn.dot = "Output", C["accent"]

    def _sync_view(self, zoom, cx, cy):
        for p in self.panels:
            if p is not self.sender():
                p.set_view(zoom, cx, cy)

    def _on_hover(self, x, y):
        vals = []
        for name, a in (("Source", self.src), ("Target", self.tgt), ("Output", self.out)):
            if a is not None and 0 <= y < a.shape[0] and 0 <= x < a.shape[1]:
                vals.append(f"{name} {int(a[y, x])}")
        self.statusBar().showMessage(f"({x}, {y})    " + "    ".join(vals))

    # ---------------------------------------------------------------- 파라미터 <-> 테이블
    def _read_table(self):
        """Validate the complete table before changing definitions or starting work."""
        init = default_params()
        pending = []
        for r, d in enumerate(self.defs):
            values = []
            for c in (3, 5, 6):
                try:
                    values.append(int(self.table.item(r, c).text()))
                except ValueError as ex:
                    raise ValueError(f"{d.name}: 초기값/최소/최대는 정수로 입력하세요.") from ex
            default, lo, hi = values
            if lo > hi:
                raise ValueError(f"{d.name}: 최소값은 최대값보다 클 수 없습니다.")
            # Preserve the existing explicit initial-value range expansion policy.
            lo, hi = min(lo, default), max(hi, default)
            enabled = self.table.item(r, 0).checkState() == Qt.Checked
            pending.append((d, default, lo, hi, enabled))
            if d.kind == "post":
                init.post[d.index] = default
            else:
                init.lut[d.index] = default
        if not init.lut_valid():
            raise ValueError("LUT X 좌표는 0~100 안에서 엄격하게 증가해야 합니다.")
        for d, default, lo, hi, enabled in pending:
            d.default, d.lo, d.hi, d.enabled = default, lo, hi, enabled
        return init, self.defs

    def _show_params(self, p, col):
        self.table.blockSignals(True)
        for r, d in enumerate(self.defs):
            v = p.get(d)
            it = self.table.item(r, col)
            diff = v - d.default
            changed = col == 4 and diff != 0
            it.setText(f"{v}   ({diff:+d})" if changed else str(v))
            it.setForeground(QtGui.QBrush(QtGui.QColor(C["warn"] if changed else C["text"])))
            f = it.font()
            f.setBold(changed)
            it.setFont(f)
            it.setTextAlignment(Qt.AlignCenter)
        self.table.blockSignals(False)

    def on_load_params(self):
        start = os.path.join(bootstrap.APP_DIR, "results")
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "best_params.json 선택", start if os.path.isdir(start) else bootstrap.APP_DIR, "JSON (*.json)")
        if not path:
            return
        try:
            with open(path, encoding="utf-8") as f:
                p = params_from_dict(json.load(f))
        except Exception as ex:
            QtWidgets.QMessageBox.warning(self, "불러오기 실패", str(ex))
            return
        for r, d in enumerate(self.defs):
            self.table.item(r, 3).setText(str(p.get(d)))
            self.table.item(r, 5).setText(str(min(d.lo, p.get(d))))
            self.table.item(r, 6).setText(str(max(d.hi, p.get(d))))
        self.logbox.appendPlainText(f"초기값으로 불러옴: {path}")

    # ---------------------------------------------------------------- 실행
    def _make_cfg(self):
        try:
            sh = [int(v) for v in self.ed_shutter.text().replace(" ", "").split(",")]
            assert len(sh) == 8
        except Exception:
            raise ValueError("Shutter 는 정수 8개(x1,y1,...,x4,y4)로 입력하세요.")
        for name, path in (("Source", self.ed_src.text()), ("Target", self.ed_tgt.text())):
            if not os.path.isfile(path):
                raise ValueError(f"{name} 파일이 없습니다: {path}")
        cfg = JobConfig(self.ed_src.text(), self.ed_tgt.text(), self.sp_w.value(), self.sp_h.value(),
                        self._current_object_type(), sh,
                        rp.COLLIMATOR_EDGE_RADIUS, EVAL_SCALE,
                        LossWeights(**{k: w.value() for k, w in self.wsp.items()}))
        cfg.opt = OptConfig(max_evals=self.sp_evals.value(), max_seconds=self.sp_min.value() * 60.0,
                            rounds=self.sp_rounds.value())
        cfg.protocol_name = self._current_protocol_name()
        cfg.protocol_code = self.cb_body.currentData()
        return cfg

    def on_run(self, single=False):
        if self.worker and self.worker.isRunning():
            return
        try:
            cfg = self._make_cfg()
        except ValueError as ex:
            QtWidgets.QMessageBox.warning(self, "입력 확인", str(ex))
            return
        self.refresh_inputs(quiet=False)
        if self.src is None or self.tgt is None:
            return
        try:
            init, defs = self._read_table()
        except ValueError as ex:
            QtWidgets.QMessageBox.warning(self, "입력 확인", str(ex))
            return
        self._show_params(init, 4)
        self._save_settings()
        self.plot.reset()
        self.out = None
        self.panels[2].arr = self.panels[2].qimg = None
        self.panels[2].subtitle = ""
        self.n_evals = self.n_improved = 0
        self.base_detail = None
        self.single_mode, self.stop_requested = single, False
        self.opt_cfg = cfg.opt
        self.activity = "준비 중 (1~13단계 전처리)"
        self.t_start = time.time()
        for c in (self.card_loss, self.card_psnr, self.card_ssim, self.card_mae, self.card_eval):
            c.set("-")
        skip = [g for g in GROUP_ORDER if not any(d.enabled and d.group == g for d in defs)]
        self.stepper.reset(skip)
        self.progress.setRange(0, 0) if single else self.progress.setRange(0, 1000)
        self.progress.setValue(0)
        self.logbox.appendPlainText("=" * 56 + ("\n[1회 처리]" if single else "\n[자동 튜닝 시작]"))
        self._set_status("run", "1회 처리 중" if single else "튜닝 실행 중")
        self.worker = Worker(cfg, init, defs, single)
        self.worker.log.connect(self.logbox.appendPlainText)
        self.worker.stage.connect(self._on_stage)
        self.worker.evaled.connect(self._on_eval)
        self.worker.done.connect(self._on_done)
        self.worker.failed.connect(self._on_failed)
        self._set_running(True)
        self.timer.start(150)
        self.worker.start()

    def on_stop(self):
        if self.worker:
            self.worker.stop()
            self.stop_requested = True
            self.activity = "중지 요청됨 — 현재 평가가 끝나면 종료하고 best 를 저장합니다"
            self._set_status("stop", "중지 중")
            self.btn_stop.setEnabled(False)

    def _set_running(self, running):
        self.btn_run.setEnabled(not running)
        self.btn_single.setEnabled(not running)
        self.btn_load.setEnabled(not running)
        self.btn_stop.setEnabled(running and not self.single_mode)

    def _on_stage(self, text):
        self.logbox.appendPlainText(f"▶ {text}")
        if "사용자 중지" in text:
            return
        if text.startswith("초기"):
            idx = 0
        elif text.startswith("최종"):
            idx = len(StageStepper.LABELS) - 1
        else:
            idx = next((gi + 1 for gi, g in enumerate(GROUP_ORDER) if GROUP_TITLES[g] in text), None)
        if idx is None:
            return
        if not self.stop_requested:
            self.activity = text
        self.stepper.set_active(idx)
        if 1 <= idx <= len(GROUP_ORDER):
            g = GROUP_ORDER[idx - 1]
            self.plot.add_marker(GROUP_TITLES[g], GROUP_COLORS[g])

    def _update_cards(self, d):
        b = self.base_detail or d

        def delta(cur, base, fmt, higher_better, unit=""):
            diff = cur - base
            if abs(diff) < 1e-12:
                return "초기값 (변화 없음)", C["muted"]
            good = (diff > 0) == higher_better
            arrow = "▲" if diff > 0 else "▼"
            return f"{arrow} {format(abs(diff), fmt)}{unit} vs 초기", (C["good"] if good else C["bad"])

        loss, base_l = d["loss"], b["loss"]
        pct = (loss - base_l) / base_l * 100 if base_l else 0
        sub = f"{'▼' if pct < 0 else '▲'} {abs(pct):.1f}% (초기 {base_l:.4f})" if abs(pct) > 1e-9 else "초기값 (변화 없음)"
        self.card_loss.set(f"{loss:.4f}", sub, C["accent"])
        self.card_loss.set_sub(sub, C["good"] if pct < 0 else C["muted"])
        for card, key, fmt, hb, unit, val in (
                (self.card_psnr, "psnr", ".2f", True, " dB", f"{d['psnr']:.2f} dB"),
                (self.card_ssim, "ssim", ".4f", True, "", f"{d['ssim']:.4f}"),
                (self.card_mae, "mae", ".1f", False, "", f"{d['mae']:.1f}")):
            s, col = delta(d[key], b[key], fmt, hb, unit)
            card.set(val)
            card.set_sub(s, col)

    def _on_eval(self, info):
        self.n_evals = info["n"] if not self.single_mode else 1
        self.plot.add(info["loss"], info["best_loss"], info["is_best"])
        if self.base_detail is None:
            self.base_detail = dict(info["detail"])
        if info["is_best"]:
            self.n_improved += 1
            self.out = info["out"]
            self._refresh_output_panel()
            self.panels[2].subtitle = f"best #{info['n']} · loss {info['loss']:.4f}"
            self._show_params(info["params"], 4)
            self._update_cards(info["detail"])
            self.logbox.appendPlainText(f"  * [{info['n']:4d}] best loss = {info['loss']:.5f}")

    def _on_done(self, res):
        self.timer.stop()
        self._tick()
        self._set_running(False)
        self.out = res["out"]
        self._refresh_output_panel()
        self._show_params(res["best"], 4)
        d = res["detail"]
        self.progress.setRange(0, 1000)
        self.progress.setValue(1000)
        if res.get("single"):
            self.stepper.reset()
            self.lbl_activity.setText("1회 처리 완료")
            self._set_status("done", "1회 처리 완료")
            return
        self.last_out_dir = res["out_dir"]
        self.btn_open.setEnabled(True)
        if self.stop_requested:
            self.stepper.halt()
            self._set_status("stop", "중지됨 (best 저장)")
        else:
            self.stepper.finish()
            self._set_status("done", "튜닝 완료")
        self._update_cards(d)
        self.card_eval.set(f"{d['evals']}", f"{d['seconds']}s 소요 · 개선 {self.n_improved}회")
        note = " (전체 해상도 재평가)" if d.get("note") else ""
        self.lbl_activity.setText(f"결과 저장됨{note}: <span style='color:{C['accent']}'>{res['out_dir']}</span>")
        self.logbox.appendPlainText(f"저장 완료: {res['out_dir']}")

    def _on_failed(self, tb):
        self.timer.stop()
        self._set_running(False)
        self.stepper.halt()
        self.progress.setRange(0, 1000)
        self._set_status("err", "오류 발생")
        self.lbl_activity.setText(f"<span style='color:{C['bad']}'>오류가 발생했습니다. 로그를 확인하세요.</span>")
        self.logbox.appendPlainText(tb)
        QtWidgets.QMessageBox.critical(self, "오류", tb.splitlines()[-1])


def selftest():
    """빌드 검증용: 창 없이 default 파라미터로 1회 처리하고 결과를 selftest.txt 에 기록."""
    out = os.path.join(bootstrap.APP_DIR, "selftest.txt")
    try:
        cfg = JobConfig(rp.RAW_PATH, default_target_path(), rp.WIDTH, rp.HEIGHT,
                        rp.OBJECT_TYPE, [int(v) for v in rp.AUTO_SHUTTER_POINTS.flatten()],
                        rp.COLLIMATOR_EDGE_RADIUS, EVAL_SCALE)
        _, loss, d = TuneJob(cfg, lambda m: None).run_single(default_params())
        msg = f"OK loss={loss:.5f} psnr={d['psnr']:.2f} ssim={d['ssim']:.4f} bit={BIT}"
    except Exception:
        msg = "FAIL\n" + traceback.format_exc()
    with open(out, "w", encoding="utf-8") as f:
        f.write(msg)


def main():
    if "--selftest" in sys.argv:
        selftest()
        return None
    # Spyder 등에서는 이전 실행의 QApplication 이 커널에 남아 있을 수 있으므로 재사용하되,
    # 이벤트 루프는 항상 직접 돌린다 (돌리지 않으면 창이 '응답 없음'이 됨).
    try:    # 작업표시줄에 python.exe 가 아닌 앱 아이콘이 표시되도록 고유 ID 지정
        import ctypes
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("PE.PostParamTuner")
    except Exception:
        pass
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv)
    apply_theme(app)
    if os.path.isfile(bootstrap.ICON_PATH):
        app.setWindowIcon(QtGui.QIcon(bootstrap.ICON_PATH))
    w = MainWindow()
    w.show()
    w.raise_()
    w.activateWindow()
    app.exec_()
    return w


if __name__ == "__main__":
    _w = main()
