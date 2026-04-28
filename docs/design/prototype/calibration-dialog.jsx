/* global React */
const { useState } = React;

/* ============================================================
   CALIBRATION SETTINGS DIALOG (On/Off only)
   - Shown as a modal over the workbench
   - Stage list (left) + per-stage detail (right)
   - Each stage: On/Off, lookup file, fallback, threshold
   - Profile management at top (load / save / duplicate)
   - Per-detector overrides
   - Validation chip when files are missing/expired
   ============================================================ */

const K = {
  bg: "#08090c",
  surface: "rgba(20, 23, 30, 0.95)",
  surfaceSolid: "#14171e",
  surfaceAlt: "#1a1d25",
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

const STAGES = [
  {
    id: "offset", name: "Offset", order: 1, on: true,
    desc: "Subtract dark frame to remove pedestal",
    file: "calib/offset_2026-04-12.cal",
    fileStatus: "ok", fileAge: "16d",
    fallback: "skip",
    threshold: { label: "Max DN", value: "120" },
  },
  {
    id: "gain", name: "Gain", order: 2, on: true,
    desc: "Multiplicative correction for pixel-to-pixel response",
    file: "calib/gain_flat_4kV.cal",
    fileStatus: "ok", fileAge: "9d",
    fallback: "skip",
    threshold: { label: "Min ratio", value: "0.85" },
  },
  {
    id: "defect", name: "Defect", order: 3, on: true,
    desc: "Replace bad pixels via 3×3 median",
    file: "calib/defect_map.cal",
    fileStatus: "ok", fileAge: "21d",
    fallback: "neighbor-mean",
    threshold: { label: "σ threshold", value: "5" },
  },
  {
    id: "ghost", name: "Ghost", order: 4, on: true,
    desc: "Compensate residual signal from prior exposure",
    file: "calib/ghost_lut.cal",
    fileStatus: "warn", fileAge: "62d",
    fallback: "skip",
    threshold: { label: "Decay τ (ms)", value: "180" },
  },
  {
    id: "temp", name: "Temperature", order: 5, on: false,
    desc: "Compensate sensor temperature drift",
    file: "calib/temp_curve.cal",
    fileStatus: "missing", fileAge: "—",
    fallback: "skip",
    threshold: { label: "Drift gain", value: "1.00" },
  },
  {
    id: "nonlin", name: "Nonlinearity", order: 6, on: false,
    desc: "Linearize sensor response near saturation",
    file: "calib/nonlin_lut.cal",
    fileStatus: "ok", fileAge: "31d",
    fallback: "linear",
    threshold: { label: "Sat. limit", value: "62000" },
  },
  {
    id: "binning", name: "Binning", order: 7, on: false,
    desc: "2×2 hardware binning post-processing",
    file: "—",
    fileStatus: "na", fileAge: "—",
    fallback: "skip",
    threshold: { label: "Block size", value: "2" },
  },
];

const PROFILES = [
  "Production v1.2",
  "Lab tuning · low dose",
  "Worst-case stress",
  "Untitled draft",
];

const DETECTORS = [
  { id: "DET-A", name: "Detector A · floor unit",  override: false },
  { id: "DET-B", name: "Detector B · table top",   override: true  },
  { id: "DET-C", name: "Detector C · phantom rig", override: false },
];

function CalibrationDialog() {
  const [active, setActive] = useState("ghost");
  const [stages, setStages] = useState(STAGES);
  const [profile, setProfile] = useState("Production v1.2");
  const [activeDet, setActiveDet] = useState("DET-A");
  const [dirty, setDirty] = useState(true);

  const stage = stages.find(s => s.id === active);

  const toggleStage = (id) => {
    setStages(prev => prev.map(s => s.id === id ? { ...s, on: !s.on } : s));
    setDirty(true);
  };

  const onCount = stages.filter(s => s.on).length;
  const warnCount = stages.filter(s => s.on && s.fileStatus !== "ok").length;

  return (
    <div style={{
      width: 1560, height: 920, background: K.bg,
      fontFamily: "'Inter', -apple-system, system-ui, sans-serif",
      color: K.text, fontSize: 13, position: "relative",
      overflow: "hidden",
    }}>
      {/* dimmed workbench background */}
      <BackdropMock />

      {/* dialog */}
      <div style={{
        position: "absolute", inset: 0,
        background: "rgba(0,0,0,0.55)", backdropFilter: "blur(2px)",
        display: "flex", alignItems: "center", justifyContent: "center",
      }}>
        <div style={{
          width: 1180, height: 760,
          background: K.surfaceSolid, borderRadius: 14,
          border: `1px solid ${K.hairlineStrong}`,
          boxShadow: "0 24px 80px rgba(0,0,0,0.6)",
          display: "flex", flexDirection: "column", overflow: "hidden",
        }}>
          {/* dialog header */}
          <div style={{
            height: 60, padding: "0 24px",
            borderBottom: `1px solid ${K.hairline}`,
            display: "flex", alignItems: "center", gap: 16,
          }}>
            <div>
              <div style={{ fontSize: 14, fontWeight: 600 }}>Calibration settings</div>
              <div style={{ fontSize: 10, color: K.textMute, letterSpacing: 0.6, marginTop: 2 }}>
                Settings apply to all studies in run-set #2026-04-28-A
              </div>
            </div>
            <div style={{ flex: 1 }} />
            {warnCount > 0 && (
              <div style={{
                padding: "5px 10px", background: `${K.amber}15`,
                border: `1px solid ${K.amber}40`, borderRadius: 6,
                color: K.amber, fontSize: 10, fontWeight: 600,
                display: "flex", alignItems: "center", gap: 6,
              }}>
                <span>⚠</span>
                <span>{warnCount} stage{warnCount !== 1 ? "s" : ""} need attention</span>
              </div>
            )}
            <button style={btnGhostK}>?<span style={{ marginLeft: 4 }}>Help</span></button>
            <button style={{
              width: 28, height: 28, borderRadius: 6,
              background: "transparent", color: K.textDim,
              border: `1px solid ${K.hairline}`, cursor: "pointer", fontSize: 14,
            }}>✕</button>
          </div>

          {/* profile bar */}
          <div style={{
            padding: "12px 24px",
            background: K.surfaceAlt, borderBottom: `1px solid ${K.hairline}`,
            display: "flex", alignItems: "center", gap: 14,
          }}>
            <span style={{ fontSize: 10, color: K.textMute, letterSpacing: 1.2, fontWeight: 700 }}>PROFILE</span>
            <select value={profile} onChange={e => setProfile(e.target.value)} style={{
              background: K.surfaceSolid, border: `1px solid ${K.hairline}`,
              color: K.text, fontSize: 12, fontWeight: 600,
              padding: "7px 12px", borderRadius: 6, minWidth: 220, outline: "none",
            }}>
              {PROFILES.map(p => <option key={p} style={{ background: K.surfaceSolid }}>{p}</option>)}
            </select>
            {dirty && (
              <span style={{
                fontSize: 10, color: K.amber, fontWeight: 600,
              }}>● Unsaved changes</span>
            )}
            <div style={{ flex: 1 }} />
            <button style={btnGhostK}>Duplicate</button>
            <button style={btnGhostK}>Rename</button>
            <button style={btnGhostK}>Delete</button>
            <div style={{ width: 1, height: 22, background: K.hairline }} />
            <span style={{ fontSize: 10, color: K.textDim }}>
              <span style={{ color: K.green }}>{onCount}</span>
              <span> / {stages.length} stages on</span>
            </span>
          </div>

          {/* main */}
          <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
            {/* stage list */}
            <div style={{
              width: 340, borderRight: `1px solid ${K.hairline}`,
              display: "flex", flexDirection: "column", minHeight: 0,
            }}>
              <div style={{
                padding: "10px 18px", fontSize: 10, fontWeight: 700,
                letterSpacing: 1.4, color: K.textMute,
                borderBottom: `1px solid ${K.hairline}`,
              }}>STAGES · APPLIED IN ORDER</div>
              <div style={{ flex: 1, overflowY: "auto" }}>
                {stages.map(s => (
                  <StageRow key={s.id} stage={s}
                    active={s.id === active}
                    onClick={() => setActive(s.id)}
                    onToggle={() => toggleStage(s.id)} />
                ))}
              </div>
              <div style={{
                padding: "10px 18px", borderTop: `1px solid ${K.hairline}`,
                display: "flex", alignItems: "center", gap: 10,
                fontSize: 10, color: K.textDim,
              }}>
                <span>↕ drag to reorder</span>
                <span style={{ flex: 1 }} />
                <button style={btnGhostMini}>Add stage</button>
              </div>
            </div>

            {/* detail pane */}
            <div style={{ flex: 1, display: "flex", flexDirection: "column", minHeight: 0 }}>
              <StageDetail stage={stage} onToggle={() => toggleStage(stage.id)} />

              {/* per-detector overrides */}
              <div style={{
                borderTop: `1px solid ${K.hairline}`,
                padding: "16px 24px",
                background: K.surfaceAlt,
              }}>
                <div style={{
                  fontSize: 10, fontWeight: 700, letterSpacing: 1.4,
                  color: K.textMute, marginBottom: 10,
                }}>PER-DETECTOR OVERRIDES</div>
                <div style={{ display: "flex", gap: 8 }}>
                  {DETECTORS.map(d => (
                    <button key={d.id} onClick={() => setActiveDet(d.id)} style={{
                      flex: 1, textAlign: "left",
                      padding: "10px 12px", borderRadius: 7,
                      background: d.id === activeDet ? K.accentBg : "rgba(255,255,255,0.03)",
                      border: `1px solid ${d.id === activeDet ? `${K.accent}55` : K.hairline}`,
                      color: K.text, cursor: "pointer",
                    }}>
                      <div style={{ display: "flex", justifyContent: "space-between", marginBottom: 3 }}>
                        <span style={{
                          fontSize: 10, fontWeight: 700, letterSpacing: 0.6,
                          color: d.id === activeDet ? K.accent : K.text,
                        }}>{d.id}</span>
                        {d.override && (
                          <span style={{
                            padding: "1px 6px", fontSize: 8, fontWeight: 700,
                            background: `${K.violet}20`, color: K.violet,
                            borderRadius: 3, letterSpacing: 0.6,
                          }}>OVERRIDE</span>
                        )}
                      </div>
                      <div style={{ fontSize: 10, color: K.textDim }}>{d.name.split("·")[1]?.trim()}</div>
                    </button>
                  ))}
                </div>
                <div style={{ fontSize: 10, color: K.textMute, marginTop: 10, lineHeight: 1.5 }}>
                  Detector B has a custom Ghost calibration file. All other detectors use the values in the active profile.
                </div>
              </div>
            </div>
          </div>

          {/* footer */}
          <div style={{
            height: 64, padding: "0 24px",
            borderTop: `1px solid ${K.hairline}`,
            display: "flex", alignItems: "center", gap: 12,
          }}>
            <span style={{ fontSize: 11, color: K.textDim }}>
              Profile audit: <span style={{ color: K.text }}>edited 2 min ago</span> by <span style={{ color: K.text }}>j.lee</span>
            </span>
            <div style={{ flex: 1 }} />
            <button style={btnGhostK}>Validate all stages</button>
            <button style={btnGhostK}>Cancel</button>
            <button style={{
              padding: "9px 16px", borderRadius: 7,
              background: K.accent, color: "#0c1220",
              border: "none", cursor: "pointer", fontWeight: 600, fontSize: 12,
            }}>Save & apply to run-set</button>
          </div>
        </div>
      </div>
    </div>
  );
}

/* ===== Stage row in left list ===== */
function StageRow({ stage, active, onClick, onToggle }) {
  const [hover, setHover] = useState(false);
  const statusDot = {
    ok: K.green, warn: K.amber, missing: K.red, na: K.textMute,
  }[stage.fileStatus];
  return (
    <div onClick={onClick}
      onMouseEnter={() => setHover(true)} onMouseLeave={() => setHover(false)}
      style={{
        padding: "12px 18px", cursor: "pointer",
        background: active ? K.accentBg : (hover ? "rgba(255,255,255,0.03)" : "transparent"),
        borderLeft: `2px solid ${active ? K.accent : "transparent"}`,
        display: "flex", alignItems: "center", gap: 12,
      }}>
      <span style={{
        fontSize: 10, color: K.textMute, fontFamily: "'JetBrains Mono', Consolas, monospace",
        width: 14, textAlign: "right",
      }}>{stage.order}</span>
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 2 }}>
          <span style={{
            fontSize: 13, fontWeight: 600,
            color: active ? K.accent : (stage.on ? K.text : K.textDim),
          }}>{stage.name}</span>
          {stage.on && stage.fileStatus !== "ok" && stage.fileStatus !== "na" && (
            <span style={{
              width: 6, height: 6, borderRadius: "50%", background: statusDot,
            }} />
          )}
        </div>
        <div style={{
          fontSize: 10, color: stage.on ? K.textDim : K.textMute,
          overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap",
        }}>{stage.desc}</div>
      </div>
      <Toggle on={stage.on} onChange={onToggle} />
    </div>
  );
}

/* ===== Stage detail pane ===== */
function StageDetail({ stage, onToggle }) {
  const fileStatusMap = {
    ok: { color: K.green, label: "Verified" },
    warn: { color: K.amber, label: "Older than 30d" },
    missing: { color: K.red, label: "File missing" },
    na: { color: K.textMute, label: "No file required" },
  };
  const fs = fileStatusMap[stage.fileStatus];

  return (
    <div style={{ flex: 1, padding: 24, overflowY: "auto" }}>
      <div style={{ display: "flex", alignItems: "flex-start", gap: 16 }}>
        <div style={{ flex: 1 }}>
          <div style={{
            display: "flex", alignItems: "center", gap: 10, marginBottom: 4,
          }}>
            <h2 style={{ margin: 0, fontSize: 22, fontWeight: 700 }}>{stage.name}</h2>
            <span style={{
              padding: "2px 8px", borderRadius: 4,
              background: stage.on ? `${K.green}20` : "rgba(255,255,255,0.06)",
              color: stage.on ? K.green : K.textDim,
              fontSize: 9, fontWeight: 700, letterSpacing: 0.6,
            }}>
              {stage.on ? "ON" : "OFF"}
            </span>
            <span style={{ fontSize: 10, color: K.textMute }}>· stage {stage.order} of 7</span>
          </div>
          <p style={{ margin: 0, color: K.textDim, fontSize: 12, lineHeight: 1.6 }}>{stage.desc}.</p>
        </div>
        <Toggle on={stage.on} onChange={onToggle} large />
      </div>

      <div style={{ marginTop: 24, display: "grid", gridTemplateColumns: "1fr 1fr", gap: 16 }}>
        {/* Lookup file */}
        <Card title="LOOKUP FILE">
          <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 8 }}>
            <span style={{
              width: 8, height: 8, borderRadius: "50%", background: fs.color,
            }} />
            <span style={{ fontSize: 11, color: fs.color, fontWeight: 600 }}>{fs.label}</span>
            {stage.fileAge !== "—" && (
              <span style={{ fontSize: 10, color: K.textMute }}>· {stage.fileAge} ago</span>
            )}
          </div>
          <div style={{
            padding: "8px 10px", background: K.surfaceSolid,
            border: `1px solid ${K.hairline}`, borderRadius: 6,
            fontFamily: "'JetBrains Mono', Consolas, monospace",
            fontSize: 11, color: K.text, marginBottom: 8,
            overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap",
          }}>{stage.file}</div>
          <div style={{ display: "flex", gap: 6 }}>
            <button style={btnGhostMini}>Browse…</button>
            <button style={btnGhostMini}>Re-validate</button>
          </div>
        </Card>

        {/* Threshold */}
        <Card title="PARAMETER">
          <div style={{ fontSize: 11, color: K.textDim, marginBottom: 8 }}>{stage.threshold.label}</div>
          <input defaultValue={stage.threshold.value} style={{
            width: "100%", padding: "8px 10px",
            background: K.surfaceSolid,
            border: `1px solid ${K.hairline}`, borderRadius: 6,
            color: K.text, fontSize: 12,
            fontFamily: "'JetBrains Mono', Consolas, monospace",
            boxSizing: "border-box", outline: "none",
          }} />
          <div style={{ fontSize: 10, color: K.textMute, marginTop: 8, lineHeight: 1.5 }}>
            Locked when stage is OFF.
          </div>
        </Card>

        {/* Fallback */}
        <Card title="FALLBACK BEHAVIOR" full>
          <div style={{ fontSize: 11, color: K.textDim, marginBottom: 8, lineHeight: 1.5 }}>
            What to do if this stage fails (file missing or threshold breach):
          </div>
          <div style={{ display: "flex", gap: 6 }}>
            {["skip", "neighbor-mean", "linear", "abort"].map(f => (
              <button key={f} style={{
                flex: 1, padding: "9px",
                background: f === stage.fallback ? K.accentBg : "rgba(255,255,255,0.04)",
                color: f === stage.fallback ? K.accent : K.textDim,
                border: `1px solid ${f === stage.fallback ? `${K.accent}55` : K.hairline}`,
                borderRadius: 6, cursor: "pointer", fontSize: 11, fontWeight: 600,
              }}>{f}</button>
            ))}
          </div>
        </Card>

        {/* Before / after preview */}
        <Card title="PREVIEW · BEFORE → AFTER" full>
          <div style={{ display: "flex", gap: 10 }}>
            <PreviewTile label="OFF" filter="contrast(0.9) brightness(0.85)" />
            <div style={{
              alignSelf: "center", color: K.textMute, fontSize: 16,
            }}>→</div>
            <PreviewTile label="ON" filter="contrast(1.15) brightness(1.05)" highlight />
          </div>
          <div style={{ fontSize: 10, color: K.textMute, marginTop: 8, lineHeight: 1.5 }}>
            Generated from latest sample · re-runs automatically when settings change.
          </div>
        </Card>
      </div>
    </div>
  );
}

/* ===== atoms ===== */
function Card({ title, children, full }) {
  return (
    <div style={{
      gridColumn: full ? "1 / -1" : "auto",
      background: K.surfaceAlt, border: `1px solid ${K.hairline}`,
      borderRadius: 8, padding: 14,
    }}>
      <div style={{
        fontSize: 9, fontWeight: 700, letterSpacing: 1.4,
        color: K.textMute, marginBottom: 10,
      }}>{title}</div>
      {children}
    </div>
  );
}

function Toggle({ on, onChange, large }) {
  const w = large ? 44 : 32, h = large ? 24 : 18;
  return (
    <button onClick={(e) => { e.stopPropagation(); onChange(); }} style={{
      width: w, height: h, borderRadius: h / 2,
      background: on ? K.accent : "rgba(255,255,255,0.1)",
      border: "none", cursor: "pointer", padding: 0, position: "relative",
      transition: "background 0.15s", flexShrink: 0,
    }}>
      <div style={{
        position: "absolute", top: 2, left: on ? w - h + 2 : 2,
        width: h - 4, height: h - 4, borderRadius: "50%",
        background: on ? "#0c1220" : K.text, transition: "left 0.15s",
      }} />
    </button>
  );
}

function PreviewTile({ label, filter, highlight }) {
  return (
    <div style={{ flex: 1, position: "relative" }}>
      <div style={{
        aspectRatio: "16/9",
        background: "#000", borderRadius: 6, overflow: "hidden",
        border: `1px solid ${highlight ? K.accent : K.hairline}`,
      }}>
        <img src="assets/xray-placeholder.svg" alt=""
          style={{ width: "100%", height: "100%", objectFit: "cover", filter }} />
      </div>
      <div style={{
        position: "absolute", top: 6, left: 8,
        padding: "2px 6px", fontSize: 9, fontWeight: 700, letterSpacing: 0.6,
        background: highlight ? K.accent : "rgba(0,0,0,0.6)",
        color: highlight ? "#0c1220" : K.text, borderRadius: 3,
      }}>{label}</div>
    </div>
  );
}

/* dim mock workbench peeking through behind the dialog */
function BackdropMock() {
  return (
    <div style={{ position: "absolute", inset: 0, opacity: 0.7 }}>
      <div style={{
        height: 56, padding: "0 24px",
        background: K.surfaceSolid, borderBottom: `1px solid ${K.hairline}`,
        display: "flex", alignItems: "center", gap: 12,
      }}>
        <div style={{ width: 28, height: 28, borderRadius: 8, background: "linear-gradient(135deg, #7dd3fc, #3b82f6)" }} />
        <div style={{ width: 200, height: 14, background: "rgba(255,255,255,0.08)", borderRadius: 4 }} />
        <div style={{ flex: 1 }} />
        <div style={{ width: 100, height: 28, background: "rgba(255,255,255,0.05)", borderRadius: 6 }} />
        <div style={{ width: 140, height: 28, background: "rgba(255,255,255,0.05)", borderRadius: 6 }} />
      </div>
      <div style={{ display: "flex", height: "calc(100% - 56px)" }}>
        <div style={{ width: 280, background: K.surfaceSolid, borderRight: `1px solid ${K.hairline}` }} />
        <div style={{ flex: 1, background: "#000" }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{ width: "100%", height: "100%", objectFit: "contain", opacity: 0.4 }} />
        </div>
        <div style={{ width: 380, background: K.surfaceSolid, borderLeft: `1px solid ${K.hairline}` }} />
      </div>
    </div>
  );
}

const btnGhostK = {
  height: 32, padding: "0 12px", borderRadius: 7,
  background: "transparent", color: K.text,
  border: `1px solid ${K.hairlineStrong}`, cursor: "pointer",
  fontSize: 11, fontWeight: 600,
};

const btnGhostMini = {
  height: 26, padding: "0 10px", borderRadius: 5,
  background: "rgba(255,255,255,0.04)", color: K.text,
  border: `1px solid ${K.hairline}`, cursor: "pointer",
  fontSize: 10, fontWeight: 600,
};

window.CalibrationDialog = CalibrationDialog;
