/* global React */
const { useState, useMemo } = React;

/* ============================================================
   VARIANT B v2 — Algorithm Evaluation Workbench
   Purpose: evaluate & adopt new image-processing algorithms.
   Key shifts from v1:
   - Two algorithm "lanes" (Baseline vs Candidate) per viewport
   - Live quantitative metrics (PSNR, SSIM, CNR, noise σ, runtime)
   - Study queue with batch evaluation status
   - Verdict capture (Pass / Fail / Defer) per fixture
   - Run-set summary with pass rate, regressions, evidence export
   ============================================================ */

const C = {
  bg: "#08090c",
  surface: "rgba(20, 23, 30, 0.85)",
  surfaceSolid: "#14171e",
  surfaceAlt: "rgba(28, 32, 41, 0.9)",
  hairline: "rgba(255, 255, 255, 0.06)",
  hairlineStrong: "rgba(255, 255, 255, 0.12)",
  text: "#f0f2f5",
  textDim: "#8a909c",
  textMute: "#5a5f6a",
  accent: "#7dd3fc",
  accentBg: "rgba(125, 211, 252, 0.12)",
  green: "#86efac",
  amber: "#fcd34d",
  red: "#fca5a5",
  violet: "#c4b5fd",
};

const STUDIES = [
  { id: "WRIST_LAT_001", name: "wrist_lat_3072",     part: "Wrist LAT",  status: "pass",  delta: "+12.4%" },
  { id: "CHEST_PA_004",  name: "chest_pa_synth",     part: "Chest PA",   status: "pass",  delta: "+8.1%"  },
  { id: "ABDOMEN_002",   name: "abdomen_001",        part: "Abdomen",    status: "active",delta: "—"      },
  { id: "SKULL_AP_007",  name: "skull_ap_phantom",   part: "Skull AP",   status: "fail",  delta: "−3.2%"  },
  { id: "PELVIS_AP_003", name: "pelvis_ap_001",      part: "Pelvis AP",  status: "queued",delta: "—"      },
  { id: "WRIST_PA_009",  name: "wrist_pa_low_dose",  part: "Wrist PA",   status: "queued",delta: "—"      },
  { id: "CHEST_LAT_011", name: "chest_lat_calib",    part: "Chest LAT",  status: "defer", delta: "+1.0%"  },
  { id: "HAND_PA_014",   name: "hand_pa_synth",      part: "Hand PA",    status: "queued",delta: "—"      },
];

/* The algorithms below are compiled into this build's worktree.
   Adding/improving algorithms is a code change in the same project,
   not something the GUI fetches or tracks at runtime. */
const ALGORITHMS = [
  "Baseline v1.0",
  "Production v1.2",
  "Candidate v1.4",
  "Candidate v1.5-rc",
];

function VariantC() {
  const [view, setView] = useState("split");
  const [overlay, setOverlay] = useState(0.5);
  const [zoom, setZoom] = useState(100);
  const [activeStudy, setActiveStudy] = useState("ABDOMEN_002");
  const [leftAlgo, setLeftAlgo] = useState("Baseline v1.0");
  const [rightAlgo, setRightAlgo] = useState("Candidate v1.4");
  const [verdict, setVerdict] = useState(null); // pass | fail | defer
  const [tab, setTab] = useState("metrics"); // metrics | parameters | runset | log
  const [focusMode, setFocusMode] = useState(false);
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);
  const showLeft = focusMode ? leftOpen : true;
  const showRight = focusMode ? rightOpen : true;

  const study = STUDIES.find(s => s.id === activeStudy);
  const passed = STUDIES.filter(s => s.status === "pass").length;
  const failed = STUDIES.filter(s => s.status === "fail").length;
  const deferred = STUDIES.filter(s => s.status === "defer").length;

  return (
    <div style={{
      width: 1560, height: 920, background: C.bg, color: C.text,
      fontFamily: "'Inter', -apple-system, system-ui, sans-serif",
      fontSize: 13, position: "relative", overflow: "hidden",
      display: "flex", flexDirection: "column",
    }}>
      {/* Top bar */}
      <div style={{
        height: 56, padding: "0 24px",
        background: C.surfaceSolid, borderBottom: `1px solid ${C.hairline}`,
        display: "flex", alignItems: "center", gap: 20,
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
          <div style={{
            width: 28, height: 28, borderRadius: 8,
            background: "linear-gradient(135deg, #7dd3fc 0%, #3b82f6 100%)",
            display: "flex", alignItems: "center", justifyContent: "center",
            fontSize: 13, fontWeight: 800, color: "#0c1220",
          }}>X</div>
          <div>
            <div style={{ fontSize: 13, fontWeight: 600 }}>XPE Evaluation Workbench</div>
            <div style={{ fontSize: 10, color: C.textMute, letterSpacing: 0.4 }}>ImageProcTest GUI · Run #2026-04-28-A</div>
          </div>
        </div>

        <div style={{ width: 1, height: 24, background: C.hairline, margin: "0 8px" }} />

        {/* Run-set summary */}
        <RunSetBadge passed={passed} failed={failed} deferred={deferred} total={STUDIES.length} />

        <div style={{ flex: 1 }} />

        <PrimaryBtn label="Run on all queued" icon="▶▶" />
        <PrimaryBtn label="Export evidence bundle" icon="📦" filled />

        <div style={{ width: 1, height: 24, background: C.hairline }} />
        <button onClick={() => setFocusMode(!focusMode)} style={{
          height: 36, padding: "0 14px", borderRadius: 8,
          background: focusMode ? C.accent : "transparent",
          color: focusMode ? "#0c1220" : C.text,
          border: `1px solid ${focusMode ? C.accent : C.hairlineStrong}`,
          fontSize: 12, fontWeight: 600, cursor: "pointer",
          display: "flex", alignItems: "center", gap: 8,
        }}>
          <span>{focusMode ? "◱" : "◰"}</span>
          <span>{focusMode ? "Focus mode" : "Focus mode"}</span>
          <span style={{ fontSize: 9, opacity: 0.7, letterSpacing: 0.6 }}>F</span>
        </button>
        <NavIcon icon="⌕" />
        <NavIcon icon="⚙" />
        <NavIcon icon="?" />
      </div>

      {/* Main */}
      <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
        {/* LEFT — study queue (or rail) */}
        {showLeft ? (
          <div style={{ position: "relative", display: "flex" }}>
            <StudyQueue
              studies={STUDIES} active={activeStudy} setActive={setActiveStudy}
              passed={passed} failed={failed} deferred={deferred}
            />
            {focusMode && (
              <button onClick={() => setLeftOpen(false)} title="Collapse"
                style={collapseBtn("left")}>‹</button>
            )}
          </div>
        ) : (
          <EdgeRail side="left" label="Studies" count={STUDIES.length} onClick={() => setLeftOpen(true)} />
        )}

        {/* CENTER — viewport with two algorithm lanes */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0, position: "relative" }}>
          {/* Algorithm bar */}
          <div style={{
            height: 56, background: C.surfaceSolid,
            borderBottom: `1px solid ${C.hairline}`,
            display: "flex", alignItems: "stretch",
          }}>
            <AlgoLane label="LANE A — REFERENCE" color={C.accent}
              algo={leftAlgo} setAlgo={setLeftAlgo} options={ALGORITHMS} />
            <div style={{ width: 1, background: C.hairline }} />
            <AlgoLane label="LANE B — CANDIDATE" color={C.violet}
              algo={rightAlgo} setAlgo={setRightAlgo} options={ALGORITHMS} />
          </div>

          {/* Floating view toolbar */}
          <div style={{
            position: "absolute", top: 72, left: "50%", transform: "translateX(-50%)",
            zIndex: 10,
            background: C.surface, backdropFilter: "blur(20px)",
            border: `1px solid ${C.hairlineStrong}`,
            borderRadius: 12, padding: 6,
            display: "flex", alignItems: "center", gap: 4,
            boxShadow: "0 8px 24px rgba(0,0,0,0.4)",
          }}>
            <ViewBtn active={view === "swipe"} onClick={() => setView("swipe")}>Swipe</ViewBtn>
            <ViewBtn active={view === "split"} onClick={() => setView("split")}>Split</ViewBtn>
            <ViewBtn active={view === "overlay"} onClick={() => setView("overlay")}>Overlay</ViewBtn>
            <ViewBtn active={view === "diff"} onClick={() => setView("diff")}>Difference</ViewBtn>
            <div style={{ width: 1, height: 22, background: C.hairline, margin: "0 4px" }} />
            <ViewBtn>Histogram</ViewBtn>
            <ViewBtn>ROI</ViewBtn>
          </div>

          {/* Floating zoom dock */}
          <div style={{
            position: "absolute", bottom: 24, right: 24, zIndex: 10,
            background: C.surface, backdropFilter: "blur(20px)",
            border: `1px solid ${C.hairlineStrong}`,
            borderRadius: 10, padding: 4,
            display: "flex", alignItems: "center", gap: 2,
            boxShadow: "0 8px 24px rgba(0,0,0,0.4)",
          }}>
            <IconBtn>−</IconBtn>
            <div style={{ fontSize: 11, color: C.text, padding: "0 10px", minWidth: 50, textAlign: "center", fontVariantNumeric: "tabular-nums" }}>{zoom}%</div>
            <IconBtn>+</IconBtn>
            <div style={{ width: 1, height: 18, background: C.hairline, margin: "0 2px" }} />
            <IconBtn>⛶</IconBtn>
            <IconBtn>1:1</IconBtn>
          </div>

          {/* Image area */}
          <div style={{ flex: 1, position: "relative", background: "#000", overflow: "hidden" }}>
            <ComparisonView view={view} overlay={overlay} />

            {/* Top-left: study */}
            <div style={{
              position: "absolute", top: 16, left: 16, zIndex: 5,
              fontSize: 11, color: C.text, lineHeight: 1.6,
              fontFamily: "'JetBrains Mono', Consolas, monospace",
              textShadow: "0 1px 4px rgba(0,0,0,0.9)",
            }}>
              <div style={{ fontSize: 10, color: C.textDim, letterSpacing: 1, marginBottom: 4 }}>STUDY</div>
              <div>{study?.id}</div>
              <div style={{ color: C.textDim }}>{study?.name}.raw</div>
              <div style={{ color: C.textDim }}>1024 × 1024 · UInt16LE · {study?.part}</div>
            </div>

            {/* Top-right: window */}
            <div style={{
              position: "absolute", top: 16, right: 16, zIndex: 5,
              fontSize: 11, color: C.text, lineHeight: 1.6, textAlign: "right",
              fontFamily: "'JetBrains Mono', Consolas, monospace",
              textShadow: "0 1px 4px rgba(0,0,0,0.9)",
            }}>
              <div style={{ fontSize: 10, color: C.textDim, letterSpacing: 1, marginBottom: 4 }}>WINDOW</div>
              <div>C 2048 · W 4096</div>
              <div style={{ color: C.textDim }}>VOI: Linear</div>
              <div style={{ color: C.textDim }}>Calib: 4/7 stages</div>
            </div>

            {/* Lane labels */}
            {view !== "diff" && (
              <>
                <Tag style={{ left: 16, bottom: 16, color: C.accent, borderColor: C.accent }}>A · {leftAlgo}</Tag>
                <Tag style={{ right: 16, bottom: 16, color: C.violet, borderColor: C.violet }}>B · {rightAlgo}</Tag>
              </>
            )}

            {view === "overlay" && (
              <div style={{
                position: "absolute", bottom: 24, left: "50%", transform: "translateX(-50%)",
                zIndex: 5, background: C.surface, backdropFilter: "blur(20px)",
                border: `1px solid ${C.hairlineStrong}`, borderRadius: 10,
                padding: "10px 16px", display: "flex", alignItems: "center", gap: 12,
                width: 320,
              }}>
                <span style={{ fontSize: 11, color: C.textDim, minWidth: 60 }}>Lane B opacity</span>
                <input type="range" min={0} max={1} step={0.01}
                  value={overlay} onChange={e => setOverlay(+e.target.value)}
                  style={{ flex: 1, accentColor: C.violet }} />
                <span style={{ fontSize: 11, fontVariantNumeric: "tabular-nums", minWidth: 32, textAlign: "right" }}>{Math.round(overlay * 100)}%</span>
              </div>
            )}
          </div>

          {/* Verdict bar */}
          <VerdictBar verdict={verdict} setVerdict={setVerdict} />
        </div>

        {/* RIGHT — analysis panel */}
        {showRight ? (
          <div style={{ position: "relative", display: "flex" }}>
            {focusMode && (
              <button onClick={() => setRightOpen(false)} title="Collapse"
                style={collapseBtn("right")}>›</button>
            )}
            <AnalysisPanel tab={tab} setTab={setTab} leftAlgo={leftAlgo} rightAlgo={rightAlgo} />
          </div>
        ) : (
          <EdgeRail side="right" label="Analysis" hint={tab} onClick={() => setRightOpen(true)} />
        )}
      </div>
    </div>
  );
}

/* ===== Edge rail (collapsed panel handle) ===== */
function EdgeRail({ side, label, hint, count, onClick }) {
  const [hover, setHover] = useState(false);
  return (
    <button onClick={onClick}
      onMouseEnter={() => setHover(true)} onMouseLeave={() => setHover(false)}
      style={{
        width: 36, background: hover ? "rgba(255,255,255,0.04)" : C.surfaceSolid,
        border: "none",
        borderLeft: side === "right" ? `1px solid ${C.hairline}` : "none",
        borderRight: side === "left" ? `1px solid ${C.hairline}` : "none",
        cursor: "pointer", color: C.textDim,
        display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", gap: 12,
        padding: "16px 0",
      }}>
      <span style={{ fontSize: 14 }}>{side === "left" ? "›" : "‹"}</span>
      <div style={{
        writingMode: "vertical-rl", transform: "rotate(180deg)",
        fontSize: 11, fontWeight: 600, letterSpacing: 1.4, color: C.text,
      }}>
        {label}
      </div>
      {count != null && (
        <span style={{
          padding: "2px 6px", background: "rgba(255,255,255,0.06)",
          borderRadius: 4, fontSize: 9, color: C.textDim, fontWeight: 700,
        }}>{count}</span>
      )}
      {hint && (
        <span style={{
          writingMode: "vertical-rl", transform: "rotate(180deg)",
          fontSize: 9, color: C.textMute, letterSpacing: 0.8, textTransform: "uppercase",
        }}>{hint}</span>
      )}
    </button>
  );
}

const collapseBtn = (side) => ({
  position: "absolute",
  [side === "left" ? "right" : "left"]: -10,
  top: 14, zIndex: 20,
  width: 20, height: 20, borderRadius: "50%",
  background: C.surfaceSolid, color: C.textDim,
  border: `1px solid ${C.hairlineStrong}`,
  cursor: "pointer", fontSize: 11,
  display: "flex", alignItems: "center", justifyContent: "center",
  boxShadow: "0 2px 6px rgba(0,0,0,0.4)",
});

/* ===== Run-set badge ===== */
function RunSetBadge({ passed, failed, deferred, total }) {
  const remaining = total - passed - failed - deferred;
  const pct = ((passed + failed + deferred) / total) * 100;
  return (
    <div style={{
      display: "flex", alignItems: "center", gap: 14,
      padding: "8px 16px",
      background: "rgba(255,255,255,0.04)",
      border: `1px solid ${C.hairline}`, borderRadius: 10,
    }}>
      <div>
        <div style={{ fontSize: 9, letterSpacing: 1.2, color: C.textMute, marginBottom: 2 }}>RUN-SET</div>
        <div style={{ fontSize: 12, fontWeight: 600 }}>{total - remaining} of {total} evaluated</div>
      </div>
      <div style={{ width: 120, height: 4, background: "rgba(255,255,255,0.08)", borderRadius: 2, overflow: "hidden", display: "flex" }}>
        <div style={{ width: `${(passed/total)*100}%`, background: C.green }} />
        <div style={{ width: `${(failed/total)*100}%`, background: C.red }} />
        <div style={{ width: `${(deferred/total)*100}%`, background: C.amber }} />
      </div>
      <div style={{ display: "flex", gap: 10, fontSize: 11 }}>
        <span style={{ color: C.green }}>{passed} pass</span>
        <span style={{ color: C.red }}>{failed} fail</span>
        <span style={{ color: C.amber }}>{deferred} defer</span>
      </div>
    </div>
  );
}

/* ===== Algorithm lane selector =====
   Lists algorithms compiled into this build. The app does not track or detect
   external changes — if a new algorithm is added in the worktree, it shows up
   here on the next rebuild. Nothing more. */
function AlgoLane({ label, color, algo, setAlgo, options }) {
  return (
    <div style={{
      flex: 1, display: "flex", alignItems: "center",
      padding: "0 20px", gap: 14,
    }}>
      <div style={{
        width: 6, height: 22, borderRadius: 3, background: color,
      }} />
      <div style={{ minWidth: 0, flex: 1 }}>
        <div style={{ fontSize: 9, letterSpacing: 1.4, color: C.textMute, marginBottom: 2 }}>{label}</div>
        <select value={algo} onChange={e => setAlgo(e.target.value)} style={{
          background: "transparent", border: "none", color: C.text,
          fontSize: 14, fontWeight: 600, cursor: "pointer", outline: "none",
          paddingRight: 18, appearance: "none",
          backgroundImage: `linear-gradient(45deg, transparent 50%, ${C.textDim} 50%), linear-gradient(135deg, ${C.textDim} 50%, transparent 50%)`,
          backgroundPosition: "calc(100% - 8px) center, calc(100% - 4px) center",
          backgroundSize: "4px 4px, 4px 4px",
          backgroundRepeat: "no-repeat",
        }}>
          {options.map(o => (
            <option key={o} value={o} style={{ background: C.surfaceSolid }}>{o}</option>
          ))}
        </select>
      </div>
    </div>
  );
}

/* ===== Study queue ===== */
function StudyQueue({ studies, active, setActive, passed, failed, deferred }) {
  return (
    <div style={{
      width: 280, background: C.surfaceSolid,
      borderRight: `1px solid ${C.hairline}`,
      display: "flex", flexDirection: "column", minHeight: 0,
    }}>
      <div style={{ padding: "16px 16px 12px", borderBottom: `1px solid ${C.hairline}` }}>
        <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 10 }}>
          <div style={{ fontSize: 11, fontWeight: 600, flex: 1 }}>Study queue</div>
          <span style={{
            padding: "2px 7px", background: "rgba(255,255,255,0.06)",
            borderRadius: 6, fontSize: 10, color: C.textDim,
          }}>{studies.length}</span>
          <button style={{
            width: 22, height: 22, borderRadius: 5,
            background: "rgba(255,255,255,0.06)", color: C.text,
            border: "none", cursor: "pointer", fontSize: 13,
          }}>+</button>
        </div>
        <div style={{
          display: "flex", padding: 2,
          background: "rgba(0,0,0,0.3)", borderRadius: 6,
          fontSize: 10,
        }}>
          {[["all","All"],["queued","Queued"],["done","Done"]].map(([k, l], i) => (
            <button key={k} style={{
              flex: 1, padding: "5px", borderRadius: 4,
              background: i === 0 ? C.accentBg : "transparent",
              color: i === 0 ? C.accent : C.textDim,
              border: "none", cursor: "pointer", fontWeight: 600,
            }}>{l}</button>
          ))}
        </div>
      </div>

      <div style={{ flex: 1, overflowY: "auto", padding: 8 }}>
        {studies.map(s => (
          <StudyRow key={s.id} study={s} active={s.id === active}
            onClick={() => setActive(s.id)} />
        ))}
      </div>

      <div style={{
        padding: "12px 16px", borderTop: `1px solid ${C.hairline}`,
        fontSize: 10, color: C.textDim, lineHeight: 1.6,
      }}>
        <div style={{ color: C.textMute, letterSpacing: 1, fontSize: 9, marginBottom: 6 }}>FIXTURE SET</div>
        <div style={{ color: C.text }}>gui-s0/golden-26.manifest</div>
        <div>{studies.length} studies loaded</div>
      </div>
    </div>
  );
}

function StudyRow({ study, active, onClick }) {
  const [hover, setHover] = useState(false);
  const statusMap = {
    pass:   { color: C.green,  label: "✓", bg: "rgba(134,239,172,0.1)" },
    fail:   { color: C.red,    label: "✗", bg: "rgba(252,165,165,0.1)" },
    defer:  { color: C.amber,  label: "⏸", bg: "rgba(252,211,77,0.1)" },
    active: { color: C.accent, label: "●", bg: "rgba(125,211,252,0.1)" },
    queued: { color: C.textMute, label: "○", bg: "transparent" },
  };
  const s = statusMap[study.status];
  return (
    <div onClick={onClick}
      onMouseEnter={() => setHover(true)} onMouseLeave={() => setHover(false)}
      style={{
        padding: "10px 12px", marginBottom: 4, borderRadius: 7, cursor: "pointer",
        background: active ? C.accentBg : (hover ? "rgba(255,255,255,0.04)" : "transparent"),
        border: `1px solid ${active ? C.hairlineStrong : "transparent"}`,
        display: "flex", alignItems: "center", gap: 10,
      }}>
      <div style={{
        width: 36, height: 36, borderRadius: 6,
        background: "#000", flexShrink: 0,
        display: "flex", alignItems: "center", justifyContent: "center",
        overflow: "hidden", border: `1px solid ${C.hairline}`,
      }}>
        <img src="assets/xray-placeholder.svg" alt=""
          style={{ width: "100%", height: "100%", objectFit: "cover", opacity: 0.7 }} />
      </div>
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{
          fontSize: 11, fontWeight: 600, color: active ? C.accent : C.text,
          overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap",
        }}>{study.id}</div>
        <div style={{ fontSize: 10, color: C.textDim, marginTop: 1 }}>{study.part}</div>
      </div>
      <div style={{ textAlign: "right" }}>
        <div style={{ color: s.color, fontSize: 13, lineHeight: 1 }}>{s.label}</div>
        <div style={{ fontSize: 9, color: C.textDim, marginTop: 3, fontFamily: "'JetBrains Mono', monospace" }}>{study.delta}</div>
      </div>
    </div>
  );
}

/* ===== Verdict bar ===== */
function VerdictBar({ verdict, setVerdict }) {
  return (
    <div style={{
      height: 64, background: C.surfaceSolid,
      borderTop: `1px solid ${C.hairline}`,
      padding: "0 24px",
      display: "flex", alignItems: "center", gap: 16,
    }}>
      <div style={{ flex: 1, display: "flex", alignItems: "center", gap: 14 }}>
        <div style={{ fontSize: 11, color: C.textDim, letterSpacing: 0.6 }}>
          <div style={{ fontSize: 9, color: C.textMute, letterSpacing: 1.4, marginBottom: 3 }}>VERDICT</div>
          <div>Record evaluation result for ABDOMEN_002</div>
        </div>
      </div>
      <input placeholder="Notes (e.g. ROI artifacts in lower-left, ringing on edges...)"
        style={{
          flex: 2, padding: "9px 12px",
          background: "rgba(0,0,0,0.3)",
          border: `1px solid ${C.hairline}`, borderRadius: 7,
          color: C.text, fontSize: 12, outline: "none",
        }} />
      <VerdictBtn active={verdict === "pass"} color={C.green} onClick={() => setVerdict("pass")}>✓ Pass</VerdictBtn>
      <VerdictBtn active={verdict === "defer"} color={C.amber} onClick={() => setVerdict("defer")}>⏸ Defer</VerdictBtn>
      <VerdictBtn active={verdict === "fail"} color={C.red} onClick={() => setVerdict("fail")}>✗ Fail</VerdictBtn>
      <button style={{
        padding: "9px 18px", borderRadius: 7,
        background: C.accent, color: "#0c1220",
        border: "none", cursor: "pointer", fontWeight: 600, fontSize: 12,
      }}>Save & next →</button>
    </div>
  );
}

function VerdictBtn({ active, color, onClick, children }) {
  return (
    <button onClick={onClick} style={{
      padding: "9px 14px", borderRadius: 7,
      background: active ? `${color}25` : "rgba(255,255,255,0.04)",
      color: active ? color : C.textDim,
      border: `1px solid ${active ? color : C.hairline}`,
      cursor: "pointer", fontWeight: 600, fontSize: 12,
    }}>{children}</button>
  );
}

/* ===== Analysis panel (right) ===== */
function AnalysisPanel({ tab, setTab, leftAlgo, rightAlgo }) {
  return (
    <div style={{
      width: 380, background: C.surfaceSolid,
      borderLeft: `1px solid ${C.hairline}`,
      display: "flex", flexDirection: "column", minHeight: 0,
    }}>
      <div style={{ display: "flex", borderBottom: `1px solid ${C.hairline}` }}>
        {[
          ["metrics", "Metrics"],
          ["parameters", "Parameters"],
          ["runset", "Run-set"],
          ["log", "Log"],
        ].map(([k, l]) => (
          <button key={k} onClick={() => setTab(k)} style={{
            flex: 1, padding: "14px 6px", background: "transparent",
            color: tab === k ? C.text : C.textDim,
            border: "none", cursor: "pointer", fontSize: 11, fontWeight: 600,
            borderBottom: tab === k ? `2px solid ${C.accent}` : "2px solid transparent",
            marginBottom: -1,
          }}>{l}</button>
        ))}
      </div>

      <div style={{ flex: 1, overflowY: "auto", padding: 18 }}>
        {tab === "metrics" && <MetricsTab leftAlgo={leftAlgo} rightAlgo={rightAlgo} />}
        {tab === "parameters" && <ParametersTab />}
        {tab === "runset" && <RunSetTab />}
        {tab === "log" && <LogTab />}
      </div>
    </div>
  );
}

/* ===== Metrics tab — the heart of evaluation ===== */
function MetricsTab({ leftAlgo, rightAlgo }) {
  // Reference values for Lane A vs Lane B
  const metrics = [
    { name: "PSNR",       unit: "dB",     a: 38.42,  b: 41.87,  better: "higher", target: ">= 38" },
    { name: "SSIM",       unit: "",       a: 0.942,  b: 0.971,  better: "higher", target: ">= 0.95" },
    { name: "CNR",        unit: "",       a: 4.21,   b: 5.83,   better: "higher", target: ">= 5.0" },
    { name: "Noise σ",    unit: "DN",     a: 142.6,  b: 89.4,   better: "lower",  target: "<= 100" },
    { name: "Edge sharp.",unit: "px",     a: 1.84,   b: 1.42,   better: "lower",  target: "<= 1.5" },
    { name: "Uniformity", unit: "%",      a: 96.1,   b: 98.7,   better: "higher", target: ">= 97" },
  ];
  const timing = [
    { stage: "Preprocess",  a: 148, b: 156 },
    { stage: "VOI LUT",     a: 89,  b: 91 },
    { stage: "Presentation",a: 175, b: 198 },
    { stage: "Total",       a: 412, b: 445 },
  ];

  return (
    <div>
      <SectionTitle>Quantitative quality</SectionTitle>
      <div style={{
        background: "rgba(0,0,0,0.3)", borderRadius: 8,
        border: `1px solid ${C.hairline}`, overflow: "hidden", marginBottom: 24,
      }}>
        <div style={{
          display: "grid", gridTemplateColumns: "1.4fr 1fr 1fr 1fr",
          padding: "10px 14px", background: "rgba(255,255,255,0.03)",
          borderBottom: `1px solid ${C.hairline}`,
          fontSize: 10, letterSpacing: 1, color: C.textMute, fontWeight: 600,
        }}>
          <span>METRIC</span>
          <span style={{ color: C.accent }}>LANE A</span>
          <span style={{ color: C.violet }}>LANE B</span>
          <span style={{ textAlign: "right" }}>Δ</span>
        </div>
        {metrics.map((m, i) => {
          const delta = m.b - m.a;
          const goodSign = m.better === "higher" ? 1 : -1;
          const isBetter = Math.sign(delta) === goodSign;
          const dColor = isBetter ? C.green : C.red;
          const dPct = ((delta / m.a) * 100).toFixed(1);
          return (
            <div key={m.name} style={{
              display: "grid", gridTemplateColumns: "1.4fr 1fr 1fr 1fr",
              padding: "11px 14px",
              borderBottom: i < metrics.length - 1 ? `1px solid ${C.hairline}` : "none",
              fontSize: 12, alignItems: "center",
            }}>
              <div>
                <div>{m.name} {m.unit && <span style={{ color: C.textMute, fontSize: 10 }}>({m.unit})</span>}</div>
                <div style={{ fontSize: 10, color: C.textMute, marginTop: 2 }}>{m.target}</div>
              </div>
              <span style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: 12 }}>{m.a}</span>
              <span style={{ fontFamily: "'JetBrains Mono', monospace", fontWeight: 600, fontSize: 12 }}>{m.b}</span>
              <div style={{ textAlign: "right" }}>
                <div style={{ color: dColor, fontSize: 11, fontFamily: "'JetBrains Mono', monospace", fontWeight: 600 }}>
                  {delta > 0 ? "+" : ""}{delta.toFixed(2)}
                </div>
                <div style={{ color: dColor, fontSize: 9, opacity: 0.8 }}>
                  ({dPct > 0 ? "+" : ""}{dPct}%)
                </div>
              </div>
            </div>
          );
        })}
      </div>

      <SectionTitle>Stage timing</SectionTitle>
      <div style={{ marginBottom: 24 }}>
        {timing.map(t => {
          const max = Math.max(t.a, t.b);
          return (
            <div key={t.stage} style={{ marginBottom: 10 }}>
              <div style={{ display: "flex", justifyContent: "space-between", fontSize: 11, marginBottom: 5 }}>
                <span style={{ color: t.stage === "Total" ? C.text : C.textDim, fontWeight: t.stage === "Total" ? 600 : 400 }}>{t.stage}</span>
                <span style={{ fontFamily: "'JetBrains Mono', monospace", color: C.textDim, fontSize: 10 }}>
                  <span style={{ color: C.accent }}>{t.a}</span> / <span style={{ color: C.violet }}>{t.b}</span> ms
                </span>
              </div>
              <div style={{ display: "flex", gap: 4, height: 8 }}>
                <div style={{
                  flex: t.a, height: "100%", background: C.accent,
                  borderRadius: 2, opacity: 0.6,
                }} />
                <div style={{
                  flex: t.b, height: "100%", background: C.violet,
                  borderRadius: 2, opacity: 0.6,
                }} />
              </div>
            </div>
          );
        })}
      </div>

      <SectionTitle>Histogram</SectionTitle>
      <Histogram />

      <SectionTitle>ROI measurement</SectionTitle>
      <div style={{
        padding: 12, background: "rgba(0,0,0,0.3)",
        border: `1px solid ${C.hairline}`, borderRadius: 8,
        fontSize: 11, color: C.textDim,
        display: "flex", alignItems: "center", justifyContent: "center",
        height: 70, gap: 10,
      }}>
        <span style={{ fontSize: 16, color: C.textMute }}>⊟</span>
        <span>Drag to draw ROI on viewport for local CNR</span>
      </div>
    </div>
  );
}

function Histogram() {
  // synthetic histogram bars — more weight on low/mid for x-ray
  const bars = useMemo(() => Array.from({ length: 48 }, (_, i) => {
    const x = i / 48;
    const a = Math.exp(-Math.pow((x - 0.3) * 3, 2)) + 0.4 * Math.exp(-Math.pow((x - 0.55) * 5, 2));
    const b = Math.exp(-Math.pow((x - 0.35) * 2.5, 2)) + 0.3 * Math.exp(-Math.pow((x - 0.65) * 4, 2));
    return { a, b };
  }), []);

  return (
    <div style={{
      background: "rgba(0,0,0,0.3)", borderRadius: 8,
      border: `1px solid ${C.hairline}`, padding: 14, marginBottom: 24,
    }}>
      <div style={{
        height: 90, display: "flex", alignItems: "flex-end", gap: 1,
      }}>
        {bars.map((b, i) => (
          <div key={i} style={{ flex: 1, height: "100%", display: "flex", flexDirection: "column", justifyContent: "flex-end", position: "relative" }}>
            <div style={{
              width: "100%",
              height: `${b.a * 70}%`,
              background: C.accent, opacity: 0.45,
              borderRadius: "1px 1px 0 0",
              position: "absolute", bottom: 0, left: 0,
            }} />
            <div style={{
              width: "100%",
              height: `${b.b * 70}%`,
              background: C.violet, opacity: 0.55,
              borderRadius: "1px 1px 0 0",
              position: "absolute", bottom: 0, left: 0,
              mixBlendMode: "screen",
            }} />
          </div>
        ))}
      </div>
      <div style={{
        display: "flex", justifyContent: "space-between",
        fontSize: 9, color: C.textMute, marginTop: 6,
        fontFamily: "'JetBrains Mono', monospace",
      }}>
        <span>0</span><span>16k</span><span>32k</span><span>48k</span><span>65k</span>
      </div>
      <div style={{ display: "flex", gap: 14, fontSize: 10, marginTop: 8 }}>
        <span style={{ color: C.accent }}>■ Lane A</span>
        <span style={{ color: C.violet }}>■ Lane B</span>
      </div>
    </div>
  );
}

/* ===== Parameters tab ===== */
function ParametersTab() {
  return (
    <div>
      <SectionTitle>Calibration stages</SectionTitle>
      <div style={{ marginBottom: 24 }}>
        {["Offset", "Gain", "Defect", "Ghost", "Temperature", "Nonlinearity", "Binning"].map((s, i) => (
          <div key={s} style={{
            display: "flex", alignItems: "center", gap: 12,
            padding: "9px 0", borderBottom: i < 6 ? `1px solid ${C.hairline}` : "none",
          }}>
            <div style={{ flex: 1, fontSize: 12 }}>{s}</div>
            <Seg value={i < 4 ? "Auto" : "Off"} options={["Auto", "On", "Off"]} />
          </div>
        ))}
      </div>

      <SectionTitle>VOI LUT</SectionTitle>
      <FieldB label="Mode">
        <Seg value="Linear" options={["Linear", "LinearExact", "Sigmoid"]} />
      </FieldB>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8, marginBottom: 16 }}>
        <FieldB label="Center"><InputB value="2048" mono /></FieldB>
        <FieldB label="Width"><InputB value="4096" mono /></FieldB>
      </div>

      <SectionTitle>Lane B parameter overrides</SectionTitle>
      <div style={{
        padding: 14, background: `${C.violet}10`, border: `1px solid ${C.violet}30`,
        borderRadius: 8, fontSize: 11, color: C.textDim, lineHeight: 1.6, marginBottom: 12,
      }}>
        Only Lane B parameters can be tuned during evaluation. Lane A is locked to the production reference for fair comparison.
      </div>
      <FieldB label="Sharpening σ"><InputB value="0.85" mono /></FieldB>
      <FieldB label="Denoise strength"><InputB value="0.42" mono /></FieldB>
      <button style={btnB}>Reset Lane B to defaults</button>
    </div>
  );
}

/* ===== Run-set tab ===== */
function RunSetTab() {
  const summary = [
    { label: "PSNR mean Δ",   v: "+3.45 dB", c: C.green },
    { label: "SSIM mean Δ",   v: "+0.029",   c: C.green },
    { label: "Runtime Δ",     v: "+8.0%",    c: C.amber },
    { label: "Regressions",   v: "1 / 8",    c: C.red },
  ];
  return (
    <div>
      <SectionTitle>Aggregated results — 8 studies</SectionTitle>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8, marginBottom: 18 }}>
        {summary.map(s => (
          <div key={s.label} style={{
            padding: "12px 14px", background: "rgba(0,0,0,0.3)",
            border: `1px solid ${C.hairline}`, borderRadius: 8,
          }}>
            <div style={{ fontSize: 10, letterSpacing: 0.6, color: C.textMute, marginBottom: 4 }}>{s.label}</div>
            <div style={{ fontSize: 16, fontWeight: 700, color: s.c, fontFamily: "'JetBrains Mono', monospace" }}>{s.v}</div>
          </div>
        ))}
      </div>

      <SectionTitle>Per-study delta</SectionTitle>
      {STUDIES.map(s => {
        const delta = parseFloat(s.delta) || 0;
        const c = s.status === "fail" ? C.red : s.status === "defer" ? C.amber : delta > 0 ? C.green : C.textDim;
        return (
          <div key={s.id} style={{
            display: "flex", alignItems: "center", gap: 10,
            padding: "9px 0", borderBottom: `1px solid ${C.hairline}`, fontSize: 11,
          }}>
            <span style={{ flex: 1, color: C.text }}>{s.id}</span>
            <span style={{ color: C.textDim, fontFamily: "'JetBrains Mono', monospace", minWidth: 60, textAlign: "right" }}>{s.delta}</span>
            <div style={{ width: 70, height: 6, background: "rgba(255,255,255,0.06)", borderRadius: 3, overflow: "hidden", position: "relative" }}>
              <div style={{
                position: "absolute", left: "50%", top: 0, height: "100%",
                width: `${Math.min(Math.abs(delta) * 3, 50)}%`,
                background: c,
                transform: delta < 0 ? "translateX(-100%)" : "none",
              }} />
              <div style={{ position: "absolute", left: "50%", top: 0, bottom: 0, width: 1, background: C.hairlineStrong }} />
            </div>
          </div>
        );
      })}
      <button style={{ ...btnB, marginTop: 18, background: C.accent, color: "#0c1220", borderColor: C.accent }}>
        Export evidence bundle (.zip)
      </button>
    </div>
  );
}

/* ===== Log tab ===== */
function LogTab() {
  const events = [
    ["08:42:01", "INFO", "Workbench initialized · Run #2026-04-28-A"],
    ["08:42:02", "INFO", "Loaded fixture set: gui-s0/golden-26.manifest"],
    ["08:42:14", "INFO", "Lane A: Baseline v1.0 (build a3f2c91) loaded"],
    ["08:42:14", "INFO", "Lane B: Candidate v1.4 (build 7b8e2f0) loaded"],
    ["08:42:18", "INFO", "WRIST_LAT_001 evaluated · ΔPSNR +3.42 dB"],
    ["08:42:21", "WARN", "SKULL_AP_007 ΔSSIM −0.018 (threshold breach)"],
    ["08:42:23", "INFO", "Verdict captured: SKULL_AP_007 → FAIL"],
    ["08:42:28", "INFO", "ABDOMEN_002 evaluation in progress…"],
  ];
  return (
    <div>
      {events.map(([t, l, m], i) => {
        const c = l === "WARN" ? C.amber : l === "ERR" ? C.red : C.accent;
        return (
          <div key={i} style={{
            padding: "10px 0", borderBottom: i < events.length - 1 ? `1px solid ${C.hairline}` : "none",
            fontSize: 11, fontFamily: "'JetBrains Mono', Consolas, monospace",
          }}>
            <div style={{ display: "flex", gap: 10, alignItems: "baseline" }}>
              <span style={{ color: C.textMute, fontSize: 10 }}>{t}</span>
              <span style={{ color: c, fontWeight: 700, fontSize: 10, minWidth: 38 }}>{l}</span>
            </div>
            <div style={{ marginTop: 4, color: C.text, lineHeight: 1.5 }}>{m}</div>
          </div>
        );
      })}
    </div>
  );
}

/* ===== shared atoms ===== */
function PrimaryBtn({ label, icon, filled }) {
  const [h, setH] = useState(false);
  return (
    <button onMouseEnter={() => setH(true)} onMouseLeave={() => setH(false)}
      style={{
        height: 36, padding: "0 14px", borderRadius: 8,
        background: filled ? C.accent : (h ? C.accentBg : "transparent"),
        color: filled ? "#0c1220" : C.text,
        border: `1px solid ${filled ? C.accent : C.hairlineStrong}`,
        fontSize: 12, fontWeight: 600, cursor: "pointer",
        display: "flex", alignItems: "center", gap: 8,
      }}>
      <span>{icon}</span><span>{label}</span>
    </button>
  );
}

function NavIcon({ icon }) {
  return (
    <button style={{
      width: 36, height: 36, borderRadius: 8,
      background: "transparent", color: C.textDim,
      border: "none", cursor: "pointer", fontSize: 16,
    }}>{icon}</button>
  );
}

function ViewBtn({ active, onClick, children }) {
  return (
    <button onClick={onClick} style={{
      padding: "8px 14px", borderRadius: 7,
      background: active ? C.accent : "transparent",
      color: active ? "#0c1220" : C.text,
      border: "none", cursor: "pointer", fontSize: 11, fontWeight: 600,
    }}>{children}</button>
  );
}

function IconBtn({ children }) {
  const [h, setH] = useState(false);
  return (
    <button onMouseEnter={() => setH(true)} onMouseLeave={() => setH(false)}
      style={{
        width: 28, height: 28, borderRadius: 6,
        background: h ? "rgba(255,255,255,0.08)" : "transparent",
        color: C.text, border: "none", cursor: "pointer",
        fontSize: 12, fontWeight: 500,
      }}>{children}</button>
  );
}

function ComparisonView({ view, overlay }) {
  if (view === "swipe") {
    return (
      <div style={{ position: "absolute", inset: 0 }}>
        <img src="assets/xray-placeholder.svg" alt=""
          style={{ position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain", filter: "contrast(1.15) brightness(1.05)" }} />
        <div style={{ position: "absolute", inset: 0, width: "50%", overflow: "hidden", borderRight: `2px solid ${C.violet}` }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{ width: "200%", height: "100%", objectFit: "contain", objectPosition: "left", filter: "contrast(0.85) brightness(0.85)" }} />
        </div>
      </div>
    );
  }
  if (view === "split") {
    return (
      <div style={{ display: "flex", height: "100%", gap: 2, background: C.hairlineStrong }}>
        <div style={{ flex: 1, background: "#000" }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{ width: "100%", height: "100%", objectFit: "contain", filter: "contrast(0.85) brightness(0.85)" }} />
        </div>
        <div style={{ flex: 1, background: "#000" }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{ width: "100%", height: "100%", objectFit: "contain", filter: "contrast(1.15) brightness(1.05)" }} />
        </div>
      </div>
    );
  }
  if (view === "overlay") {
    return (
      <div style={{ position: "absolute", inset: 0 }}>
        <img src="assets/xray-placeholder.svg" alt=""
          style={{ position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain", filter: "contrast(0.85) brightness(0.85)" }} />
        <img src="assets/xray-placeholder.svg" alt=""
          style={{ position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain", filter: "contrast(1.15) brightness(1.05)", opacity: overlay }} />
      </div>
    );
  }
  return (
    <div style={{ position: "absolute", inset: 0 }}>
      <img src="assets/xray-placeholder.svg" alt=""
        style={{ position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain", filter: "contrast(0.85) brightness(0.85)" }} />
      <img src="assets/xray-placeholder.svg" alt=""
        style={{ position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain", filter: "contrast(1.15) brightness(1.05) hue-rotate(180deg)", mixBlendMode: "difference" }} />
    </div>
  );
}

function Tag({ style, children }) {
  return (
    <div style={{
      position: "absolute", padding: "5px 12px",
      background: "rgba(0,0,0,0.6)", backdropFilter: "blur(8px)",
      border: `1px solid`, borderColor: "currentColor",
      borderRadius: 6, fontSize: 10, fontWeight: 700, letterSpacing: 1.2,
      ...style,
    }}>{children}</div>
  );
}

function SectionTitle({ children }) {
  return (
    <div style={{
      fontSize: 10, fontWeight: 700, letterSpacing: 1.4,
      color: C.textMute, marginBottom: 12, textTransform: "uppercase",
    }}>{children}</div>
  );
}

function Seg({ value, options, onChange }) {
  const [v, setV] = useState(value);
  return (
    <div style={{
      display: "flex", padding: 2,
      background: "rgba(0,0,0,0.3)", borderRadius: 6,
      border: `1px solid ${C.hairline}`,
    }}>
      {options.map(o => (
        <button key={o} onClick={() => { setV(o); onChange?.(o); }} style={{
          padding: "5px 10px", borderRadius: 4,
          background: v === o
            ? (o === "On" ? "rgba(134,239,172,0.18)" : o === "Off" ? "rgba(252,165,165,0.18)" : C.accentBg)
            : "transparent",
          color: v === o
            ? (o === "On" ? C.green : o === "Off" ? C.red : C.accent)
            : C.textDim,
          border: "none", cursor: "pointer", fontSize: 11, fontWeight: 600,
        }}>{o}</button>
      ))}
    </div>
  );
}

function FieldB({ label, children }) {
  return (
    <div style={{ marginBottom: 12 }}>
      <div style={{ fontSize: 11, color: C.textDim, marginBottom: 6 }}>{label}</div>
      {children}
    </div>
  );
}

function InputB({ value, mono }) {
  return (
    <input defaultValue={value} style={{
      width: "100%", padding: "8px 10px",
      background: "rgba(0,0,0,0.3)",
      border: `1px solid ${C.hairline}`, borderRadius: 6,
      color: C.text, fontSize: 12,
      fontFamily: mono ? "'JetBrains Mono', Consolas, monospace" : "inherit",
      boxSizing: "border-box",
    }} />
  );
}

const btnB = {
  width: "100%", padding: "10px 14px", marginTop: 6,
  background: "rgba(255,255,255,0.06)",
  color: C.text, border: `1px solid ${C.hairlineStrong}`,
  borderRadius: 8, fontSize: 12, fontWeight: 600, cursor: "pointer",
};

window.VariantC = VariantC;
