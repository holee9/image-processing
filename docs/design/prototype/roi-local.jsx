/* global React */
const { useState, useMemo } = React;

/* ============================================================
   ROI LOCAL MEASUREMENT
   - Drawing tools (rect / circle / freeform / profile line)
   - Signal/Background ROI pairs → automatic CNR
   - Multi-ROI table with Lane A/B values
   - Profile-line plot (intensity along line, A vs B)
   - ROI library: save → reuse on next fixture (regression)
   ============================================================ */

const R = {
  bg: "#08090c",
  surface: "rgba(20, 23, 30, 0.85)",
  surfaceSolid: "#14171e",
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
  signal: "#fcd34d",     // signal ROI = amber
  bg2:    "#7dd3fc",     // background ROI = cyan
  profile:"#c4b5fd",     // profile line = violet
};

// Pre-drawn ROI definitions (positions in % of viewport)
const ROIS = [
  { id: "S1", kind: "signal",  shape: "rect",   x: 32, y: 28, w: 12, h: 8,  pair: "B1", label: "Lesion edge",       laneA: 14820, laneB: 15042 },
  { id: "B1", kind: "bg",      shape: "rect",   x: 18, y: 56, w: 12, h: 9,  pair: "S1", label: "Soft tissue",       laneA:  9820, laneB: 10018 },
  { id: "S2", kind: "signal",  shape: "circle", x: 62, y: 38, w: 9,  h: 9,  pair: "B2", label: "Bone cortex",       laneA: 28140, laneB: 29320 },
  { id: "B2", kind: "bg",      shape: "circle", x: 70, y: 62, w: 8,  h: 8,  pair: "S2", label: "Marrow",            laneA: 18450, laneB: 19010 },
  { id: "L1", kind: "profile", shape: "line",   x1: 22, y1: 20, x2: 78, y2: 70, label: "Diagonal across joint" },
];

// CNR rows derived from ROI pairs
const CNR_ROWS = [
  { pair: "S1 / B1", region: "Lesion vs soft tissue", a: 4.21, b: 5.83, target: ">= 5.0", clinicalWeight: "high" },
  { pair: "S2 / B2", region: "Cortex vs marrow",      a: 6.84, b: 7.42, target: ">= 6.5", clinicalWeight: "med"  },
];

function ROILocal() {
  const [tool, setTool] = useState("rect"); // rect | circle | freeform | profile | move | erase
  const [showOverlay, setShowOverlay] = useState(true);
  const [activeROI, setActiveROI] = useState("S1");
  const [autoSync, setAutoSync] = useState(true);
  const [focusMode, setFocusMode] = useState(false);
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);
  const [plotOpen, setPlotOpen] = useState(true);
  const showLeft = focusMode ? leftOpen : true;
  const showRight = focusMode ? rightOpen : true;
  const showPlot = focusMode ? plotOpen : true;

  return (
    <div style={{
      width: 1560, height: 920, background: R.bg, color: R.text,
      fontFamily: "'Inter', -apple-system, system-ui, sans-serif",
      fontSize: 13, position: "relative", overflow: "hidden",
      display: "flex", flexDirection: "column",
    }}>
      {/* Top bar (compact, contextual) */}
      <div style={{
        height: 48, padding: "0 20px",
        background: R.surfaceSolid, borderBottom: `1px solid ${R.hairline}`,
        display: "flex", alignItems: "center", gap: 16,
      }}>
        <button style={{
          height: 28, padding: "0 12px", borderRadius: 7,
          background: "transparent", color: R.textDim,
          border: `1px solid ${R.hairline}`, cursor: "pointer",
          fontSize: 11, fontWeight: 600,
          display: "flex", alignItems: "center", gap: 6,
        }}>
          <span>←</span><span>Back to workbench</span>
        </button>
        <div style={{ width: 1, height: 22, background: R.hairline }} />
        <div>
          <div style={{ fontSize: 9, letterSpacing: 1.2, color: R.textMute }}>ROI MEASUREMENT</div>
          <div style={{ fontSize: 12, fontWeight: 600 }}>ABDOMEN_002 · abdomen_001.raw</div>
        </div>
        <div style={{ flex: 1 }} />

        <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
          <span style={{ fontSize: 11, color: R.textDim }}>Auto-sync ROI Lane A ↔ B</span>
          <Toggle value={autoSync} onChange={setAutoSync} />
        </div>
        <div style={{ width: 1, height: 22, background: R.hairline }} />

        <button style={btnGhost}>Reset all</button>
        <button style={{ ...btnGhost, color: R.accent, borderColor: `${R.accent}55` }}>
          <span>＋</span> Save to ROI library
        </button>
        <div style={{ width: 1, height: 22, background: R.hairline }} />
        <button onClick={() => setFocusMode(!focusMode)} style={{
          height: 28, padding: "0 12px", borderRadius: 7,
          background: focusMode ? R.accent : "transparent",
          color: focusMode ? "#0c1220" : R.text,
          border: `1px solid ${focusMode ? R.accent : R.hairlineStrong}`,
          cursor: "pointer", fontSize: 11, fontWeight: 600,
          display: "flex", alignItems: "center", gap: 6,
        }}>
          <span>{focusMode ? "◱" : "◰"}</span>
          <span>Focus mode</span>
          <span style={{ fontSize: 9, opacity: 0.7 }}>F</span>
        </button>
      </div>

      {/* Main */}
      <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
        {/* LEFT — tools + ROI list (or rail) */}
        {showLeft ? (
          <div style={{ position: "relative", display: "flex" }}>
            <ROILeftPanel
              tool={tool} setTool={setTool}
              activeROI={activeROI} setActiveROI={setActiveROI}
            />
            {focusMode && (
              <button onClick={() => setLeftOpen(false)} title="Collapse"
                style={roiCollapseBtn("left")}>‹</button>
            )}
          </div>
        ) : (
          <ROIEdgeRail side="left" label="Tools & ROIs" count={ROIS.length} onClick={() => setLeftOpen(true)} />
        )}

        {/* CENTER — viewport */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0, position: "relative" }}>
          {/* Lane labels */}
          <div style={{
            height: 38, background: R.surfaceSolid,
            borderBottom: `1px solid ${R.hairline}`,
            display: "flex", alignItems: "stretch",
          }}>
            <LaneTag color={R.accent} label="LANE A" name="Baseline v1.0" />
            <div style={{ width: 1, background: R.hairline }} />
            <LaneTag color={R.violet} label="LANE B" name="Candidate v1.4" />
          </div>

          {/* Viewports with ROI overlays */}
          <div style={{ flex: 1, display: "flex", gap: 2, background: R.hairlineStrong, position: "relative" }}>
            <ViewportWithROI side="A" rois={ROIS} activeROI={activeROI} setActiveROI={setActiveROI} showOverlay={showOverlay} contrastBoost={0.85} />
            <ViewportWithROI side="B" rois={ROIS} activeROI={activeROI} setActiveROI={setActiveROI} showOverlay={showOverlay} contrastBoost={1.15} />

            {/* Floating overlay toggle */}
            <div style={{
              position: "absolute", top: 12, right: 12, zIndex: 10,
              background: R.surface, backdropFilter: "blur(20px)",
              border: `1px solid ${R.hairlineStrong}`,
              borderRadius: 8, padding: "5px 10px",
              display: "flex", alignItems: "center", gap: 8,
              fontSize: 11,
            }}>
              <span style={{ color: R.textDim }}>ROI overlay</span>
              <Toggle value={showOverlay} onChange={setShowOverlay} small />
            </div>

            {/* Cursor crosshair value tooltip */}
            <div style={{
              position: "absolute", left: "50%", bottom: 16,
              transform: "translateX(-50%)", zIndex: 10,
              background: "rgba(0,0,0,0.7)", backdropFilter: "blur(10px)",
              border: `1px solid ${R.hairlineStrong}`,
              borderRadius: 8, padding: "8px 14px",
              fontFamily: "'JetBrains Mono', Consolas, monospace",
              fontSize: 11,
              display: "flex", gap: 16, alignItems: "center",
            }}>
              <span style={{ color: R.textMute, fontSize: 10 }}>x: <span style={{ color: R.text }}>624</span></span>
              <span style={{ color: R.textMute, fontSize: 10 }}>y: <span style={{ color: R.text }}>418</span></span>
              <span style={{ width: 1, height: 12, background: R.hairline }} />
              <span style={{ color: R.accent }}>A: 14,820</span>
              <span style={{ color: R.violet }}>B: 15,042</span>
              <span style={{ color: R.green, fontSize: 10 }}>Δ +222</span>
            </div>
          </div>

          {/* Profile-line plot dock */}
          {showPlot ? (
            <div style={{ position: "relative" }}>
              {focusMode && (
                <button onClick={() => setPlotOpen(false)} title="Collapse plot"
                  style={{
                    position: "absolute", top: -10, right: 14, zIndex: 20,
                    width: 20, height: 20, borderRadius: "50%",
                    background: R.surfaceSolid, color: R.textDim,
                    border: `1px solid ${R.hairlineStrong}`,
                    cursor: "pointer", fontSize: 11,
                    display: "flex", alignItems: "center", justifyContent: "center",
                    boxShadow: "0 2px 6px rgba(0,0,0,0.4)",
                  }}>↓</button>
              )}
              <ProfilePlot />
            </div>
          ) : (
            <button onClick={() => setPlotOpen(true)} style={{
              height: 28, background: R.surfaceSolid,
              borderTop: `1px solid ${R.hairline}`, border: "none",
              color: R.textDim, fontSize: 10, fontWeight: 700,
              letterSpacing: 1.4, cursor: "pointer",
              display: "flex", alignItems: "center", justifyContent: "center", gap: 8,
            }}>
              <span>↑</span><span>PROFILE PLOT — L1 diagonal across joint</span>
            </button>
          )}
        </div>

        {/* RIGHT — measurements */}
        {showRight ? (
          <div style={{ position: "relative", display: "flex" }}>
            {focusMode && (
              <button onClick={() => setRightOpen(false)} title="Collapse"
                style={roiCollapseBtn("right")}>›</button>
            )}
            <ROIRightPanel activeROI={activeROI} setActiveROI={setActiveROI} />
          </div>
        ) : (
          <ROIEdgeRail side="right" label="Measurements" hint="CNR · Δ" onClick={() => setRightOpen(true)} />
        )}
      </div>
    </div>
  );
}

/* ===== Left panel (extracted) ===== */
function ROILeftPanel({ tool, setTool, activeROI, setActiveROI }) {
  return (
    <div style={{
      width: 260, background: R.surfaceSolid,
      borderRight: `1px solid ${R.hairline}`,
      display: "flex", flexDirection: "column", minHeight: 0,
    }}>
      <div style={{ padding: 14, borderBottom: `1px solid ${R.hairline}` }}>
        <SectionHdr>Drawing tools</SectionHdr>
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 6 }}>
          <ToolBtn icon="▭" label="Rectangle" hint="R" active={tool === "rect"} onClick={() => setTool("rect")} />
          <ToolBtn icon="◯" label="Ellipse"   hint="E" active={tool === "circle"} onClick={() => setTool("circle")} />
          <ToolBtn icon="◌" label="Freeform"  hint="F" active={tool === "freeform"} onClick={() => setTool("freeform")} />
          <ToolBtn icon="╱" label="Profile"   hint="P" active={tool === "profile"} onClick={() => setTool("profile")} />
          <ToolBtn icon="✥" label="Move"      hint="V" active={tool === "move"} onClick={() => setTool("move")} />
          <ToolBtn icon="⌫" label="Erase"     hint="X" active={tool === "erase"} onClick={() => setTool("erase")} />
        </div>
      </div>
      <div style={{ padding: 14, borderBottom: `1px solid ${R.hairline}` }}>
        <SectionHdr>Next ROI role</SectionHdr>
        <div style={{
          display: "flex", padding: 2,
          background: "rgba(0,0,0,0.3)", borderRadius: 6,
        }}>
          {[
            { k: "signal", l: "Signal", c: R.signal },
            { k: "bg", l: "Background", c: R.bg2 },
            { k: "free", l: "Free", c: R.textDim },
          ].map((r, i) => (
            <button key={r.k} style={{
              flex: 1, padding: "6px",
              background: i === 0 ? `${r.c}25` : "transparent",
              color: i === 0 ? r.c : R.textDim,
              border: "none", borderRadius: 4, cursor: "pointer",
              fontSize: 10, fontWeight: 700, letterSpacing: 0.4,
            }}>{r.l}</button>
          ))}
        </div>
        <div style={{ fontSize: 10, color: R.textMute, marginTop: 8, lineHeight: 1.5 }}>
          Drawing a Signal ROI auto-prompts you to draw its Background pair → CNR computed instantly.
        </div>
      </div>
      <div style={{ flex: 1, overflowY: "auto" }}>
        <div style={{ padding: "14px 14px 8px" }}>
          <SectionHdr>ROIs on this fixture</SectionHdr>
        </div>
        {ROIS.map(r => (
          <ROIListItem key={r.id} roi={r} active={r.id === activeROI} onClick={() => setActiveROI(r.id)} />
        ))}
      </div>
      <div style={{
        padding: "10px 14px", borderTop: `1px solid ${R.hairline}`,
        display: "flex", justifyContent: "space-between", fontSize: 10, color: R.textDim,
      }}>
        <span>5 ROIs · 2 pairs</span>
        <span>1 saved to library</span>
      </div>
    </div>
  );
}

/* ===== Right panel (extracted) ===== */
function ROIRightPanel({ activeROI, setActiveROI }) {
  return (
    <div style={{
      width: 380, background: R.surfaceSolid,
      borderLeft: `1px solid ${R.hairline}`,
      display: "flex", flexDirection: "column", minHeight: 0,
    }}>
          <div style={{ flex: 1, overflowY: "auto", padding: 18 }}>
            {/* CNR pairs */}
            <SectionHdr>Local CNR — pairs</SectionHdr>
            {CNR_ROWS.map((r, i) => <CNRCard key={i} row={r} />)}

            {/* All ROIs table */}
            <SectionHdr style={{ marginTop: 24 }}>ROI raw values</SectionHdr>
            <div style={{
              background: "rgba(0,0,0,0.3)", borderRadius: 8,
              border: `1px solid ${R.hairline}`, overflow: "hidden",
            }}>
              <div style={{
                display: "grid", gridTemplateColumns: "44px 1fr 76px 76px 50px",
                padding: "8px 12px", background: "rgba(255,255,255,0.03)",
                borderBottom: `1px solid ${R.hairline}`,
                fontSize: 9, letterSpacing: 1, color: R.textMute, fontWeight: 700,
              }}>
                <span>ID</span>
                <span>REGION</span>
                <span style={{ color: R.accent, textAlign: "right" }}>μ A</span>
                <span style={{ color: R.violet, textAlign: "right" }}>μ B</span>
                <span style={{ textAlign: "right" }}>Δ%</span>
              </div>
              {ROIS.filter(r => r.kind !== "profile").map((r, i) => {
                const dPct = (((r.laneB - r.laneA) / r.laneA) * 100).toFixed(2);
                const dC = dPct > 0 ? R.green : R.red;
                const tagColor = r.kind === "signal" ? R.signal : R.bg2;
                return (
                  <div key={r.id}
                    onClick={() => setActiveROI(r.id)}
                    style={{
                      display: "grid", gridTemplateColumns: "44px 1fr 76px 76px 50px",
                      padding: "10px 12px",
                      borderBottom: i < 3 ? `1px solid ${R.hairline}` : "none",
                      fontSize: 11, alignItems: "center", cursor: "pointer",
                      background: r.id === activeROI ? R.accentBg : "transparent",
                    }}>
                    <span style={{
                      fontSize: 10, fontWeight: 700, color: tagColor,
                      fontFamily: "'JetBrains Mono', Consolas, monospace",
                    }}>{r.id}</span>
                    <span>{r.label}</span>
                    <span style={{ textAlign: "right", fontFamily: "'JetBrains Mono', Consolas, monospace", color: R.textDim, fontSize: 10 }}>{r.laneA.toLocaleString()}</span>
                    <span style={{ textAlign: "right", fontFamily: "'JetBrains Mono', Consolas, monospace", fontSize: 10 }}>{r.laneB.toLocaleString()}</span>
                    <span style={{ textAlign: "right", fontFamily: "'JetBrains Mono', Consolas, monospace", fontSize: 10, color: dC, fontWeight: 600 }}>
                      {dPct > 0 ? "+" : ""}{dPct}
                    </span>
                  </div>
                );
              })}
            </div>

            {/* Selected ROI inspector */}
            <SectionHdr style={{ marginTop: 24 }}>Selected: S1 — Lesion edge</SectionHdr>
            <ROIInspector />

            {/* ROI library */}
            <SectionHdr style={{ marginTop: 24 }}>ROI library</SectionHdr>
            <div style={{
              padding: 14, background: `${R.accent}10`,
              border: `1px solid ${R.accent}30`, borderRadius: 8,
              fontSize: 11, color: R.textDim, lineHeight: 1.5,
            }}>
              <div style={{ color: R.accent, fontSize: 10, fontWeight: 700, letterSpacing: 0.6, marginBottom: 6 }}>
                REGRESSION READY
              </div>
              These ROIs will be replayed automatically against every new candidate algorithm in this fixture set, so reviewers see consistent measurements across runs.
            </div>
          </div>
    </div>
  );
}

/* ===== Edge rail / collapse helpers ===== */
function ROIEdgeRail({ side, label, hint, count, onClick }) {
  const [hover, setHover] = useState(false);
  return (
    <button onClick={onClick}
      onMouseEnter={() => setHover(true)} onMouseLeave={() => setHover(false)}
      style={{
        width: 36, background: hover ? "rgba(255,255,255,0.04)" : R.surfaceSolid,
        border: "none",
        borderLeft: side === "right" ? `1px solid ${R.hairline}` : "none",
        borderRight: side === "left" ? `1px solid ${R.hairline}` : "none",
        cursor: "pointer", color: R.textDim,
        display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", gap: 12,
        padding: "16px 0",
      }}>
      <span style={{ fontSize: 14 }}>{side === "left" ? "›" : "‹"}</span>
      <div style={{
        writingMode: "vertical-rl", transform: "rotate(180deg)",
        fontSize: 11, fontWeight: 600, letterSpacing: 1.4, color: R.text,
      }}>{label}</div>
      {count != null && (
        <span style={{
          padding: "2px 6px", background: "rgba(255,255,255,0.06)",
          borderRadius: 4, fontSize: 9, color: R.textDim, fontWeight: 700,
        }}>{count}</span>
      )}
      {hint && (
        <span style={{
          writingMode: "vertical-rl", transform: "rotate(180deg)",
          fontSize: 9, color: R.textMute, letterSpacing: 0.8, textTransform: "uppercase",
        }}>{hint}</span>
      )}
    </button>
  );
}

const roiCollapseBtn = (side) => ({
  position: "absolute",
  [side === "left" ? "right" : "left"]: -10,
  top: 14, zIndex: 20,
  width: 20, height: 20, borderRadius: "50%",
  background: R.surfaceSolid, color: R.textDim,
  border: `1px solid ${R.hairlineStrong}`,
  cursor: "pointer", fontSize: 11,
  display: "flex", alignItems: "center", justifyContent: "center",
  boxShadow: "0 2px 6px rgba(0,0,0,0.4)",
});

/* ===== ViewportWithROI ===== */
function ViewportWithROI({ side, rois, activeROI, setActiveROI, showOverlay, contrastBoost }) {
  return (
    <div style={{ flex: 1, position: "relative", background: "#000", overflow: "hidden" }}>
      <img src="assets/xray-placeholder.svg" alt=""
        style={{
          position: "absolute", inset: 0,
          width: "100%", height: "100%", objectFit: "contain",
          filter: `contrast(${contrastBoost}) brightness(${contrastBoost === 0.85 ? 0.85 : 1.05})`,
        }} />

      {/* Side label */}
      <div style={{
        position: "absolute", top: 10, left: 12, zIndex: 5,
        fontSize: 10, fontWeight: 700, letterSpacing: 1.2,
        color: side === "A" ? R.accent : R.violet,
        textShadow: "0 1px 4px rgba(0,0,0,0.9)",
      }}>{side}</div>

      {/* ROI overlay layer */}
      {showOverlay && (
        <svg viewBox="0 0 100 100" preserveAspectRatio="none"
          style={{ position: "absolute", inset: 0, width: "100%", height: "100%", pointerEvents: "none" }}>
          {rois.map(r => {
            if (r.shape === "rect") {
              const color = r.kind === "signal" ? R.signal : R.bg2;
              const isActive = r.id === activeROI;
              return (
                <g key={r.id}>
                  <rect x={r.x} y={r.y} width={r.w} height={r.h}
                    fill={`${color}1a`} stroke={color}
                    strokeWidth={isActive ? 0.4 : 0.25}
                    strokeDasharray={isActive ? "" : "1 0.5"} vectorEffect="non-scaling-stroke" />
                  <rect x={r.x} y={r.y - 2.6} width={r.id.length * 1.6 + 1.5} height={2.4}
                    fill={color} />
                  <text x={r.x + 0.6} y={r.y - 0.8} fill="#0c1220"
                    style={{ fontSize: 1.6, fontWeight: 700, fontFamily: "monospace" }}>
                    {r.id}
                  </text>
                </g>
              );
            }
            if (r.shape === "circle") {
              const color = r.kind === "signal" ? R.signal : R.bg2;
              const isActive = r.id === activeROI;
              const cx = r.x + r.w / 2, cy = r.y + r.h / 2;
              return (
                <g key={r.id}>
                  <ellipse cx={cx} cy={cy} rx={r.w / 2} ry={r.h / 2}
                    fill={`${color}1a`} stroke={color}
                    strokeWidth={isActive ? 0.4 : 0.25}
                    strokeDasharray={isActive ? "" : "1 0.5"} vectorEffect="non-scaling-stroke" />
                  <rect x={r.x} y={r.y - 2.6} width={r.id.length * 1.6 + 1.5} height={2.4} fill={color} />
                  <text x={r.x + 0.6} y={r.y - 0.8} fill="#0c1220"
                    style={{ fontSize: 1.6, fontWeight: 700, fontFamily: "monospace" }}>
                    {r.id}
                  </text>
                </g>
              );
            }
            if (r.shape === "line") {
              return (
                <g key={r.id}>
                  <line x1={r.x1} y1={r.y1} x2={r.x2} y2={r.y2}
                    stroke={R.profile} strokeWidth={0.4} vectorEffect="non-scaling-stroke" />
                  <circle cx={r.x1} cy={r.y1} r={0.6} fill={R.profile} />
                  <circle cx={r.x2} cy={r.y2} r={0.6} fill={R.profile} />
                  <rect x={r.x1} y={r.y1 - 2.6} width={3} height={2.4} fill={R.profile} />
                  <text x={r.x1 + 0.6} y={r.y1 - 0.8} fill="#0c1220"
                    style={{ fontSize: 1.6, fontWeight: 700, fontFamily: "monospace" }}>
                    {r.id}
                  </text>
                </g>
              );
            }
            return null;
          })}
          {/* Pair connectors */}
          {rois.filter(r => r.kind === "signal" && r.pair).map(r => {
            const partner = rois.find(o => o.id === r.pair);
            if (!partner) return null;
            const cx1 = r.x + r.w / 2, cy1 = r.y + r.h / 2;
            const cx2 = partner.x + partner.w / 2, cy2 = partner.y + partner.h / 2;
            return (
              <line key={`pair-${r.id}`} x1={cx1} y1={cy1} x2={cx2} y2={cy2}
                stroke={R.textDim} strokeWidth={0.15} strokeDasharray="0.5 0.5"
                vectorEffect="non-scaling-stroke" opacity={0.5} />
            );
          })}
        </svg>
      )}

      {/* Active measurement chip on the active ROI */}
      {showOverlay && side === "B" && (() => {
        const a = rois.find(r => r.id === activeROI);
        if (!a || a.shape === "line") return null;
        return (
          <div style={{
            position: "absolute",
            left: `${a.x + a.w + 1}%`, top: `${a.y}%`,
            background: "rgba(0,0,0,0.85)", backdropFilter: "blur(8px)",
            border: `1px solid ${R.signal}55`, borderRadius: 6,
            padding: "6px 10px", fontSize: 10,
            fontFamily: "'JetBrains Mono', Consolas, monospace",
            zIndex: 6, lineHeight: 1.5,
          }}>
            <div style={{ color: R.signal, fontWeight: 700, fontSize: 9, letterSpacing: 0.6 }}>{a.id} · SIGNAL</div>
            <div style={{ color: R.text }}>μ <span style={{ color: R.violet }}>15,042</span></div>
            <div style={{ color: R.textDim }}>σ 142.6</div>
            <div style={{ color: R.textDim }}>n 9,216</div>
          </div>
        );
      })()}
    </div>
  );
}

/* ===== Profile plot (intensity along profile line) ===== */
function ProfilePlot() {
  // Synthetic profile: edge with sharper transition for B (candidate)
  const points = useMemo(() => {
    const pts = [];
    for (let i = 0; i <= 100; i++) {
      const x = i / 100;
      const baseEdge = 1 / (1 + Math.exp(-(x - 0.5) * 8));   // soft edge for A
      const sharpEdge = 1 / (1 + Math.exp(-(x - 0.5) * 16)); // sharper for B
      const noiseA = Math.sin(i * 1.7) * 0.04 + Math.cos(i * 0.9) * 0.03;
      const noiseB = Math.sin(i * 2.3) * 0.015;
      pts.push({
        x: i,
        a: 0.35 + 0.5 * baseEdge + noiseA,
        b: 0.35 + 0.55 * sharpEdge + noiseB,
      });
    }
    return pts;
  }, []);

  const W = 100, H = 100;
  const toPath = key => points.map((p, i) =>
    `${i === 0 ? "M" : "L"} ${p.x} ${(1 - p[key]) * H}`
  ).join(" ");

  return (
    <div style={{
      height: 168, background: R.surfaceSolid,
      borderTop: `1px solid ${R.hairline}`,
      padding: "12px 18px", display: "flex", gap: 16,
    }}>
      <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 12, marginBottom: 8 }}>
          <SectionHdr style={{ marginBottom: 0 }}>Profile L1 — diagonal across joint</SectionHdr>
          <div style={{ flex: 1 }} />
          <span style={{ fontSize: 10, color: R.textMute }}>length 612 px</span>
          <span style={{ fontSize: 10, color: R.profile }}>● profile line</span>
        </div>
        <div style={{ flex: 1, position: "relative", background: "rgba(0,0,0,0.3)", border: `1px solid ${R.hairline}`, borderRadius: 6 }}>
          <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none"
            style={{ position: "absolute", inset: 0, width: "100%", height: "100%" }}>
            {/* gridlines */}
            {[0.25, 0.5, 0.75].map(y => (
              <line key={y} x1={0} x2={W} y1={y * H} y2={y * H}
                stroke={R.hairline} strokeWidth={0.2} vectorEffect="non-scaling-stroke" />
            ))}
            {/* lane A */}
            <path d={toPath("a")} stroke={R.accent} strokeWidth={0.4} fill="none"
              vectorEffect="non-scaling-stroke" opacity={0.85} />
            {/* lane B */}
            <path d={toPath("b")} stroke={R.violet} strokeWidth={0.5} fill="none"
              vectorEffect="non-scaling-stroke" />
            {/* edge marker (transition zone) */}
            <line x1={50} x2={50} y1={0} y2={H}
              stroke={R.signal} strokeWidth={0.25} strokeDasharray="0.8 0.8"
              vectorEffect="non-scaling-stroke" opacity={0.6} />
          </svg>
          <div style={{
            position: "absolute", left: 8, top: 6, fontSize: 9,
            color: R.textMute, fontFamily: "'JetBrains Mono', Consolas, monospace",
          }}>65,535</div>
          <div style={{
            position: "absolute", left: 8, bottom: 4, fontSize: 9,
            color: R.textMute, fontFamily: "'JetBrains Mono', Consolas, monospace",
          }}>0</div>
          <div style={{
            position: "absolute", right: 8, bottom: 4, fontSize: 9,
            color: R.signal, fontFamily: "'JetBrains Mono', Consolas, monospace",
          }}>edge transition</div>
        </div>
      </div>

      {/* Plot legend / measured */}
      <div style={{ width: 220, display: "flex", flexDirection: "column", gap: 8 }}>
        <SectionHdr style={{ marginBottom: 0 }}>Edge response</SectionHdr>
        <ProfileMetric label="Lane A · 10–90% rise" value="1.84 px" color={R.accent} />
        <ProfileMetric label="Lane B · 10–90% rise" value="1.42 px" color={R.violet} highlight />
        <ProfileMetric label="Δ Sharpness" value="−0.42 px better" color={R.green} />
        <ProfileMetric label="Overshoot" value="A 1.2% · B 0.4%" color={R.textDim} />
      </div>
    </div>
  );
}

function ProfileMetric({ label, value, color, highlight }) {
  return (
    <div style={{
      padding: "6px 10px",
      background: highlight ? `${color}15` : "rgba(0,0,0,0.3)",
      border: `1px solid ${highlight ? `${color}40` : R.hairline}`,
      borderRadius: 6,
      display: "flex", justifyContent: "space-between", alignItems: "center",
    }}>
      <span style={{ fontSize: 10, color: R.textDim }}>{label}</span>
      <span style={{ fontSize: 11, color, fontFamily: "'JetBrains Mono', Consolas, monospace", fontWeight: 600 }}>{value}</span>
    </div>
  );
}

/* ===== CNR pair card ===== */
function CNRCard({ row }) {
  const dPct = (((row.b - row.a) / row.a) * 100).toFixed(1);
  const meets = row.b >= parseFloat(row.target.replace(">=", "").trim());
  return (
    <div style={{
      background: "rgba(0,0,0,0.3)", borderRadius: 8,
      border: `1px solid ${R.hairline}`, padding: 14, marginBottom: 10,
    }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 4 }}>
        <span style={{
          padding: "2px 7px", borderRadius: 4,
          background: `${R.signal}20`, color: R.signal,
          fontSize: 9, fontWeight: 700, letterSpacing: 0.6,
          fontFamily: "'JetBrains Mono', Consolas, monospace",
        }}>{row.pair}</span>
        <span style={{
          fontSize: 9, color: meets ? R.green : R.red,
          fontWeight: 700, letterSpacing: 0.6,
        }}>
          {meets ? "✓ MEETS TARGET" : "✗ MISS"}
        </span>
      </div>
      <div style={{ fontSize: 11, color: R.text, marginBottom: 10 }}>{row.region}</div>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 8 }}>
        <Stat label="LANE A" value={row.a.toFixed(2)} color={R.accent} />
        <Stat label="LANE B" value={row.b.toFixed(2)} color={R.violet} />
        <Stat label="Δ" value={`${dPct > 0 ? "+" : ""}${dPct}%`} color={dPct > 0 ? R.green : R.red} />
      </div>
      <div style={{ fontSize: 9, color: R.textMute, marginTop: 8, letterSpacing: 0.4 }}>
        Target {row.target} · clinical weight {row.clinicalWeight}
      </div>
    </div>
  );
}

function Stat({ label, value, color }) {
  return (
    <div style={{
      padding: "8px 10px", background: "rgba(255,255,255,0.03)",
      borderRadius: 6, border: `1px solid ${R.hairline}`,
    }}>
      <div style={{ fontSize: 9, color: R.textMute, letterSpacing: 0.6, marginBottom: 3 }}>{label}</div>
      <div style={{ fontSize: 14, fontWeight: 700, color, fontFamily: "'JetBrains Mono', Consolas, monospace" }}>{value}</div>
    </div>
  );
}

/* ===== ROI list item ===== */
function ROIListItem({ roi, active, onClick }) {
  const tagColor = roi.kind === "signal" ? R.signal : roi.kind === "bg" ? R.bg2 : R.profile;
  const shapeIcon = roi.shape === "rect" ? "▭" : roi.shape === "circle" ? "◯" : "╱";
  return (
    <div onClick={onClick} style={{
      padding: "9px 14px", cursor: "pointer",
      background: active ? R.accentBg : "transparent",
      borderLeft: `2px solid ${active ? R.accent : "transparent"}`,
      display: "flex", alignItems: "center", gap: 10,
    }}>
      <div style={{
        width: 22, height: 22, borderRadius: 5, background: `${tagColor}20`,
        color: tagColor, fontSize: 12,
        display: "flex", alignItems: "center", justifyContent: "center",
      }}>{shapeIcon}</div>
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontSize: 11, fontWeight: 600, color: active ? R.accent : R.text,
          overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
          {roi.id} · {roi.label}
        </div>
        <div style={{ fontSize: 9, color: R.textMute, marginTop: 1, letterSpacing: 0.4 }}>
          {roi.kind === "profile" ? "PROFILE LINE" : roi.kind === "signal" ? `SIGNAL → pair ${roi.pair}` : `BACKGROUND → pair ${roi.pair}`}
        </div>
      </div>
    </div>
  );
}

/* ===== ROI inspector ===== */
function ROIInspector() {
  return (
    <div style={{
      background: "rgba(0,0,0,0.3)", borderRadius: 8,
      border: `1px solid ${R.hairline}`, padding: 14,
    }}>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 12 }}>
        <Field label="Origin (px)" value="624, 418" />
        <Field label="Size (px)" value="184 × 122" />
        <Field label="Pixels (n)" value="22,448" />
        <Field label="Shape" value="Rectangle" />
      </div>
      <div style={{ height: 1, background: R.hairline, margin: "10px 0" }} />
      <div style={{
        display: "grid", gridTemplateColumns: "70px 1fr 1fr 1fr",
        fontSize: 10, color: R.textMute, letterSpacing: 0.6, fontWeight: 700,
        marginBottom: 6,
      }}>
        <span></span><span>μ</span><span>σ</span><span>median</span>
      </div>
      {[
        { l: "Lane A", c: R.accent, mu: "14,820", sd: "210.4", med: "14,802" },
        { l: "Lane B", c: R.violet, mu: "15,042", sd: "142.6", med: "15,030" },
      ].map(r => (
        <div key={r.l} style={{
          display: "grid", gridTemplateColumns: "70px 1fr 1fr 1fr",
          padding: "6px 0", fontSize: 11, alignItems: "center",
          fontFamily: "'JetBrains Mono', Consolas, monospace",
        }}>
          <span style={{ color: r.c, fontFamily: "'Inter', sans-serif", fontWeight: 600, fontSize: 11 }}>{r.l}</span>
          <span style={{ color: r.c }}>{r.mu}</span>
          <span style={{ color: R.textDim }}>{r.sd}</span>
          <span style={{ color: R.textDim }}>{r.med}</span>
        </div>
      ))}
    </div>
  );
}

function Field({ label, value }) {
  return (
    <div>
      <div style={{ fontSize: 9, color: R.textMute, letterSpacing: 0.6, marginBottom: 3 }}>{label}</div>
      <div style={{ fontSize: 12, color: R.text, fontFamily: "'JetBrains Mono', Consolas, monospace" }}>{value}</div>
    </div>
  );
}

/* ===== atoms ===== */
function ToolBtn({ icon, label, hint, active, onClick }) {
  return (
    <button onClick={onClick} style={{
      padding: "9px 8px", borderRadius: 6,
      background: active ? R.accentBg : "rgba(255,255,255,0.03)",
      color: active ? R.accent : R.text,
      border: `1px solid ${active ? `${R.accent}55` : R.hairline}`,
      cursor: "pointer", fontSize: 10,
      display: "flex", flexDirection: "column", alignItems: "center", gap: 4,
    }}>
      <span style={{ fontSize: 14 }}>{icon}</span>
      <span style={{ fontWeight: 600 }}>{label}</span>
      <span style={{ fontSize: 8, color: R.textMute, letterSpacing: 0.4 }}>{hint}</span>
    </button>
  );
}

function LaneTag({ color, label, name }) {
  return (
    <div style={{
      flex: 1, padding: "0 16px",
      display: "flex", alignItems: "center", gap: 10,
    }}>
      <div style={{ width: 4, height: 16, borderRadius: 2, background: color }} />
      <span style={{ fontSize: 9, color: R.textMute, letterSpacing: 1.4, fontWeight: 700 }}>{label}</span>
      <span style={{ fontSize: 12, fontWeight: 600, color }}>{name}</span>
    </div>
  );
}

function Toggle({ value, onChange, small }) {
  const w = small ? 28 : 34, h = small ? 16 : 18;
  return (
    <button onClick={() => onChange(!value)} style={{
      width: w, height: h, borderRadius: h / 2,
      background: value ? R.accent : "rgba(255,255,255,0.1)",
      border: "none", cursor: "pointer", padding: 0, position: "relative",
      transition: "background 0.15s",
    }}>
      <div style={{
        position: "absolute", top: 2, left: value ? w - h + 2 : 2,
        width: h - 4, height: h - 4, borderRadius: "50%",
        background: value ? "#0c1220" : R.text, transition: "left 0.15s",
      }} />
    </button>
  );
}

function SectionHdr({ children, style }) {
  return (
    <div style={{
      fontSize: 10, fontWeight: 700, letterSpacing: 1.4,
      color: R.textMute, marginBottom: 10, textTransform: "uppercase",
      ...style,
    }}>{children}</div>
  );
}

const btnGhost = {
  height: 28, padding: "0 12px", borderRadius: 7,
  background: "transparent", color: R.text,
  border: `1px solid ${R.hairlineStrong}`, cursor: "pointer",
  fontSize: 11, fontWeight: 600,
  display: "flex", alignItems: "center", gap: 6,
};

window.ROILocal = ROILocal;
