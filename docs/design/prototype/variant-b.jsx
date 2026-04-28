/* global React */
const { useState } = React;

/* ============================================================
   VARIANT B — Modern Medical Viewer
   Viewport-first. Floating glassy panels. Generous spacing.
   For clinical/mixed users who care about clarity.
   ============================================================ */

const B = {
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
};

function VariantB() {
  const [view, setView] = useState("split");
  const [overlay, setOverlay] = useState(0.5);
  const [zoom, setZoom] = useState(100);
  const [panelOpen, setPanelOpen] = useState("calibration"); // null | runtime | calibration | display | logs
  const [calibration, setCalibration] = useState({
    Offset: "Auto", Gain: "Auto", Defect: "Auto",
    Ghost: "Auto", Temperature: "Auto", Nonlinearity: "Auto", Binning: "Auto"
  });
  const [bodyPart, setBodyPart] = useState("Chest PA");
  const [voiMode, setVoiMode] = useState("Linear");

  const setStage = (k, v) => setCalibration(s => ({ ...s, [k]: v }));
  const activeStages = Object.values(calibration).filter(v => v !== "Off").length;

  return (
    <div style={{
      width: 1560, height: 920, background: B.bg, color: B.text,
      fontFamily: "'Inter', -apple-system, system-ui, sans-serif",
      fontSize: 13, position: "relative", overflow: "hidden",
      display: "flex", flexDirection: "column",
    }}>
      {/* Top bar */}
      <div style={{
        height: 56, padding: "0 24px",
        background: B.surfaceSolid,
        borderBottom: `1px solid ${B.hairline}`,
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
            <div style={{ fontSize: 13, fontWeight: 600 }}>ImageProcTest</div>
            <div style={{ fontSize: 10, color: B.textMute, letterSpacing: 0.4 }}>GUI-S0 · v0.1.0</div>
          </div>
        </div>

        <div style={{ width: 1, height: 24, background: B.hairline, margin: "0 8px" }} />

        {/* primary actions */}
        <PrimaryBtn label="Load image" icon="📂" />
        <PrimaryBtn label="Run pipeline" icon="▶" filled />

        <div style={{ flex: 1 }} />

        {/* status chips */}
        <Chip color={B.green}>Backend connected</Chip>
        <Chip color={B.textDim}>Mock · v0.0.0</Chip>

        <div style={{ width: 1, height: 24, background: B.hairline }} />

        {/* nav icons */}
        <NavIcon icon="⌕" />
        <NavIcon icon="⚙" />
        <NavIcon icon="?" />
      </div>

      {/* Main viewport area */}
      <div style={{ flex: 1, position: "relative", display: "flex", minHeight: 0 }}>
        {/* Left rail — collapsed nav */}
        <div style={{
          width: 64, background: B.surfaceSolid,
          borderRight: `1px solid ${B.hairline}`,
          display: "flex", flexDirection: "column",
          padding: "16px 0", gap: 4,
        }}>
          <RailIcon icon="⊞" label="Runtime" active={panelOpen === "runtime"}
            onClick={() => setPanelOpen(panelOpen === "runtime" ? null : "runtime")} />
          <RailIcon icon="⊡" label="Raw" />
          <RailIcon icon="◈" label="Calibration" active={panelOpen === "calibration"}
            onClick={() => setPanelOpen(panelOpen === "calibration" ? null : "calibration")} />
          <RailIcon icon="◐" label="Display" active={panelOpen === "display"}
            onClick={() => setPanelOpen(panelOpen === "display" ? null : "display")} />
          <div style={{ flex: 1 }} />
          <RailIcon icon="≡" label="Logs" badge="6" active={panelOpen === "logs"}
            onClick={() => setPanelOpen(panelOpen === "logs" ? null : "logs")} />
          <RailIcon icon="!" label="Alerts" badge="3" badgeColor={B.amber}
            active={panelOpen === "alerts"}
            onClick={() => setPanelOpen(panelOpen === "alerts" ? null : "alerts")} />
        </div>

        {/* Viewport */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0, position: "relative" }}>
          {/* Floating top toolbar */}
          <div style={{
            position: "absolute", top: 16, left: "50%", transform: "translateX(-50%)",
            zIndex: 10,
            background: B.surface, backdropFilter: "blur(20px)",
            border: `1px solid ${B.hairlineStrong}`,
            borderRadius: 12, padding: 6,
            display: "flex", alignItems: "center", gap: 4,
            boxShadow: "0 8px 24px rgba(0,0,0,0.4)",
          }}>
            <ViewBtn active={view === "swipe"} onClick={() => setView("swipe")}>Swipe</ViewBtn>
            <ViewBtn active={view === "split"} onClick={() => setView("split")}>Split</ViewBtn>
            <ViewBtn active={view === "overlay"} onClick={() => setView("overlay")}>Overlay</ViewBtn>
            <ViewBtn active={view === "diff"} onClick={() => setView("diff")}>Difference</ViewBtn>
          </div>

          {/* Floating zoom controls bottom-right */}
          <div style={{
            position: "absolute", bottom: 24, right: 24, zIndex: 10,
            display: "flex", flexDirection: "column", gap: 8,
            alignItems: "flex-end",
          }}>
            <div style={{
              background: B.surface, backdropFilter: "blur(20px)",
              border: `1px solid ${B.hairlineStrong}`,
              borderRadius: 10, padding: 4,
              display: "flex", alignItems: "center", gap: 2,
              boxShadow: "0 8px 24px rgba(0,0,0,0.4)",
            }}>
              <IconBtn>−</IconBtn>
              <div style={{
                fontSize: 11, fontVariantNumeric: "tabular-nums",
                color: B.text, padding: "0 10px", minWidth: 50, textAlign: "center",
              }}>{zoom}%</div>
              <IconBtn>+</IconBtn>
              <div style={{ width: 1, height: 18, background: B.hairline, margin: "0 2px" }} />
              <IconBtn>⛶</IconBtn>
              <IconBtn>1:1</IconBtn>
            </div>
          </div>

          {/* Image area */}
          <div style={{
            flex: 1, position: "relative", background: "#000",
            overflow: "hidden",
          }}>
            <ComparisonView view={view} overlay={overlay} />

            {/* Top-left image info */}
            <div style={{
              position: "absolute", top: 16, left: 16, zIndex: 5,
              fontSize: 11, color: B.text, lineHeight: 1.6,
              fontFamily: "'JetBrains Mono', Consolas, monospace",
              textShadow: "0 1px 4px rgba(0,0,0,0.9)",
            }}>
              <div style={{ fontSize: 10, color: B.textDim, letterSpacing: 1, marginBottom: 4 }}>STUDY</div>
              <div>RAW · 1024 × 1024</div>
              <div style={{ color: B.textDim }}>16-bit · UInt16LE</div>
              <div style={{ color: B.textDim }}>{bodyPart}</div>
            </div>

            {/* Top-right image info */}
            <div style={{
              position: "absolute", top: 16, right: 16, zIndex: 5,
              fontSize: 11, color: B.text, lineHeight: 1.6, textAlign: "right",
              fontFamily: "'JetBrains Mono', Consolas, monospace",
              textShadow: "0 1px 4px rgba(0,0,0,0.9)",
            }}>
              <div style={{ fontSize: 10, color: B.textDim, letterSpacing: 1, marginBottom: 4 }}>WINDOW</div>
              <div>C 2048 · W 4096</div>
              <div style={{ color: B.textDim }}>{voiMode}</div>
              <div style={{ color: B.textDim }}>{activeStages}/7 stages</div>
            </div>

            {/* Overlay opacity slider when in overlay mode */}
            {view === "overlay" && (
              <div style={{
                position: "absolute", bottom: 24, left: "50%", transform: "translateX(-50%)",
                zIndex: 5, background: B.surface, backdropFilter: "blur(20px)",
                border: `1px solid ${B.hairlineStrong}`, borderRadius: 10,
                padding: "10px 16px", display: "flex", alignItems: "center", gap: 12,
                width: 320,
              }}>
                <span style={{ fontSize: 11, color: B.textDim, minWidth: 60 }}>Opacity</span>
                <input type="range" min={0} max={1} step={0.01}
                  value={overlay} onChange={e => setOverlay(+e.target.value)}
                  style={{ flex: 1, accentColor: B.accent }} />
                <span style={{
                  fontSize: 11, fontVariantNumeric: "tabular-nums",
                  minWidth: 32, textAlign: "right",
                }}>{Math.round(overlay * 100)}%</span>
              </div>
            )}
          </div>

          {/* Bottom strip — quick stats */}
          <div style={{
            height: 56, background: B.surfaceSolid,
            borderTop: `1px solid ${B.hairline}`,
            padding: "0 24px",
            display: "flex", alignItems: "center", gap: 32,
            fontSize: 12,
          }}>
            <Stat label="Min / Max" value="142 / 65,210" />
            <Stat label="Mean" value="3,847.2" />
            <Stat label="σ" value="1,201.4" />
            <div style={{ width: 1, height: 24, background: B.hairline }} />
            <Stat label="Last run" value="412 ms" accent={B.green} />
            <Stat label="Preprocess" value="148 ms" />
            <Stat label="VOI" value="89 ms" />
            <Stat label="Present" value="175 ms" />
            <div style={{ flex: 1 }} />
            <div style={{ display: "flex", alignItems: "center", gap: 8, color: B.textDim, fontSize: 11 }}>
              <Pulse />
              <span>Mock pipeline · synthetic fixture</span>
            </div>
          </div>
        </div>

        {/* Floating side panel */}
        {panelOpen && (
          <SidePanel
            kind={panelOpen}
            close={() => setPanelOpen(null)}
            calibration={calibration}
            setStage={setStage}
            voiMode={voiMode} setVoiMode={setVoiMode}
            bodyPart={bodyPart} setBodyPart={setBodyPart}
          />
        )}
      </div>
    </div>
  );
}

/* ===== components ===== */

function PrimaryBtn({ label, icon, filled }) {
  const [h, setH] = useState(false);
  return (
    <button onMouseEnter={() => setH(true)} onMouseLeave={() => setH(false)}
      style={{
        height: 36, padding: "0 14px", borderRadius: 8,
        background: filled ? B.accent : (h ? B.accentBg : "transparent"),
        color: filled ? "#0c1220" : B.text,
        border: `1px solid ${filled ? B.accent : B.hairlineStrong}`,
        fontSize: 12, fontWeight: 600, cursor: "pointer",
        display: "flex", alignItems: "center", gap: 8,
      }}>
      <span>{icon}</span>
      <span>{label}</span>
    </button>
  );
}

function Chip({ color, children }) {
  return (
    <div style={{
      display: "flex", alignItems: "center", gap: 8,
      padding: "5px 12px", borderRadius: 14,
      background: "rgba(255,255,255,0.04)",
      border: `1px solid ${B.hairline}`,
      fontSize: 11, color: B.text,
    }}>
      <span style={{ width: 6, height: 6, borderRadius: 3, background: color }} />
      <span>{children}</span>
    </div>
  );
}

function NavIcon({ icon }) {
  return (
    <button style={{
      width: 36, height: 36, borderRadius: 8,
      background: "transparent", color: B.textDim,
      border: "none", cursor: "pointer", fontSize: 16,
    }}>{icon}</button>
  );
}

function RailIcon({ icon, label, active, badge, badgeColor, onClick }) {
  const [h, setH] = useState(false);
  return (
    <div onMouseEnter={() => setH(true)} onMouseLeave={() => setH(false)}
      onClick={onClick}
      style={{ position: "relative", display: "flex", justifyContent: "center" }}>
      <button style={{
        width: 44, height: 44, borderRadius: 10,
        background: active ? B.accentBg : (h ? "rgba(255,255,255,0.05)" : "transparent"),
        color: active ? B.accent : B.textDim,
        border: "none", cursor: "pointer", fontSize: 18,
        display: "flex", alignItems: "center", justifyContent: "center",
        position: "relative",
      }}>
        {icon}
        {badge && (
          <span style={{
            position: "absolute", top: 6, right: 6,
            background: badgeColor || B.accent,
            color: badgeColor === B.amber ? "#000" : "#0c1220",
            fontSize: 9, fontWeight: 700,
            padding: "1px 5px", borderRadius: 6, lineHeight: 1.2,
          }}>{badge}</span>
        )}
      </button>
      {h && !active && (
        <div style={{
          position: "absolute", left: 52, top: 12,
          background: B.surfaceSolid, color: B.text,
          padding: "4px 8px", borderRadius: 4,
          fontSize: 11, whiteSpace: "nowrap", zIndex: 50,
          border: `1px solid ${B.hairline}`,
        }}>{label}</div>
      )}
    </div>
  );
}

function ViewBtn({ active, onClick, children }) {
  return (
    <button onClick={onClick} style={{
      padding: "8px 16px", borderRadius: 7,
      background: active ? B.accent : "transparent",
      color: active ? "#0c1220" : B.text,
      border: "none", cursor: "pointer",
      fontSize: 12, fontWeight: 600,
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
        color: B.text, border: "none", cursor: "pointer",
        fontSize: 12, fontWeight: 500,
      }}>{children}</button>
  );
}

function ComparisonView({ view, overlay }) {
  if (view === "swipe") {
    return (
      <div style={{ position: "absolute", inset: 0 }}>
        <img src="assets/xray-placeholder.svg" alt=""
          style={{
            position: "absolute", inset: 0, width: "100%", height: "100%",
            objectFit: "contain", filter: "contrast(1.15) brightness(1.05)",
          }} />
        <div style={{
          position: "absolute", inset: 0, width: "50%", overflow: "hidden",
          borderRight: `2px solid ${B.accent}`,
        }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{
              width: "200%", height: "100%", objectFit: "contain",
              objectPosition: "left", filter: "contrast(0.85) brightness(0.85)",
            }} />
        </div>
        <Tag style={{ left: 16, bottom: 16, color: B.accent }}>SOURCE</Tag>
        <Tag style={{ right: 16, bottom: 16, color: B.green }}>PROCESSED</Tag>
      </div>
    );
  }
  if (view === "split") {
    return (
      <div style={{ display: "flex", height: "100%", gap: 2, background: B.accent }}>
        <div style={{ flex: 1, position: "relative", background: "#000" }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{ width: "100%", height: "100%", objectFit: "contain", filter: "contrast(0.85) brightness(0.85)" }} />
          <Tag style={{ left: 16, bottom: 16, color: B.accent }}>SOURCE</Tag>
        </div>
        <div style={{ flex: 1, position: "relative", background: "#000" }}>
          <img src="assets/xray-placeholder.svg" alt=""
            style={{ width: "100%", height: "100%", objectFit: "contain", filter: "contrast(1.15) brightness(1.05)" }} />
          <Tag style={{ left: 16, bottom: 16, color: B.green }}>PROCESSED</Tag>
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
          style={{
            position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain",
            filter: "contrast(1.15) brightness(1.05)", opacity: overlay,
          }} />
      </div>
    );
  }
  // diff
  return (
    <div style={{ position: "absolute", inset: 0 }}>
      <img src="assets/xray-placeholder.svg" alt=""
        style={{ position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain", filter: "contrast(0.85) brightness(0.85)" }} />
      <img src="assets/xray-placeholder.svg" alt=""
        style={{
          position: "absolute", inset: 0, width: "100%", height: "100%", objectFit: "contain",
          filter: "contrast(1.15) brightness(1.05) hue-rotate(180deg)",
          mixBlendMode: "difference",
        }} />
      <Tag style={{ left: 16, bottom: 16, color: B.amber }}>DIFFERENCE HEATMAP</Tag>
    </div>
  );
}

function Tag({ style, children }) {
  return (
    <div style={{
      position: "absolute", padding: "4px 10px",
      background: "rgba(0,0,0,0.6)", backdropFilter: "blur(8px)",
      borderRadius: 6, fontSize: 10, fontWeight: 700, letterSpacing: 1.4,
      ...style,
    }}>{children}</div>
  );
}

function Stat({ label, value, accent }) {
  return (
    <div>
      <div style={{ fontSize: 10, color: B.textMute, letterSpacing: 0.6, marginBottom: 2 }}>{label}</div>
      <div style={{ fontSize: 13, fontWeight: 600, color: accent || B.text, fontVariantNumeric: "tabular-nums" }}>{value}</div>
    </div>
  );
}

function Pulse() {
  return (
    <span style={{
      display: "inline-block", width: 6, height: 6, borderRadius: 3,
      background: B.green, boxShadow: `0 0 6px ${B.green}`,
    }} />
  );
}

/* ===== Side Panel ===== */

function SidePanel({ kind, close, calibration, setStage, voiMode, setVoiMode, bodyPart, setBodyPart }) {
  return (
    <div style={{
      position: "absolute", top: 16, right: 16, bottom: 16, width: 360,
      background: B.surface, backdropFilter: "blur(24px)",
      border: `1px solid ${B.hairlineStrong}`,
      borderRadius: 14, zIndex: 20,
      boxShadow: "0 16px 48px rgba(0,0,0,0.5)",
      display: "flex", flexDirection: "column", overflow: "hidden",
    }}>
      <div style={{
        padding: "16px 20px", display: "flex", alignItems: "center", gap: 12,
        borderBottom: `1px solid ${B.hairline}`,
      }}>
        <div style={{ flex: 1 }}>
          <div style={{ fontSize: 14, fontWeight: 600 }}>{panelTitle(kind)}</div>
          <div style={{ fontSize: 11, color: B.textDim, marginTop: 2 }}>{panelSub(kind)}</div>
        </div>
        <button onClick={close} style={{
          width: 28, height: 28, borderRadius: 14,
          background: "rgba(255,255,255,0.05)", color: B.textDim,
          border: "none", cursor: "pointer", fontSize: 14,
        }}>×</button>
      </div>
      <div style={{ flex: 1, overflowY: "auto", padding: 20 }}>
        {kind === "runtime" && <RuntimeContent />}
        {kind === "calibration" && <CalibrationContent calibration={calibration} setStage={setStage} />}
        {kind === "display" && <DisplayContent voiMode={voiMode} setVoiMode={setVoiMode} bodyPart={bodyPart} setBodyPart={setBodyPart} />}
        {kind === "logs" && <LogsContent />}
        {kind === "alerts" && <AlertsContent />}
      </div>
    </div>
  );
}

function panelTitle(k) {
  return { runtime: "Runtime", calibration: "Calibration", display: "Display pipeline", logs: "Activity log", alerts: "Alerts" }[k];
}
function panelSub(k) {
  return {
    runtime: "Backend & native DLL diagnostics",
    calibration: "Off · On · Auto per stage",
    display: "Modality LUT → VOI → Presentation",
    logs: "Last 6 events",
    alerts: "Active warnings",
  }[k];
}

function RuntimeContent() {
  return (
    <div>
      <Section title="Backend">
        <Row label="Mode" value={<Pill>Mock</Pill>} />
        <Row label="Status" value={<Pill color={B.green}>Initialized</Pill>} />
        <Row label="Backend" value="MockXpeBackend" />
        <Row label="Version" value="v0.0.0-mock" mono />
      </Section>
      <Section title="Native runtime">
        <Row label="Supported" value={<Pill color={B.textMute}>No</Pill>} />
        <Row label="DLL detected" value={<Pill color={B.amber}>Missing</Pill>} />
        <Row label="DLL path" value="—" mono dim />
      </Section>
      <Section title="Display backend">
        <Row label="Version" value="v0.0.0-mock" mono />
        <Row label="DLL detected" value={<Pill color={B.amber}>Missing</Pill>} />
      </Section>
      <div style={{
        padding: 12, background: "rgba(252,211,77,0.08)",
        border: `1px solid rgba(252,211,77,0.2)`, borderRadius: 8,
        fontSize: 11, color: B.textDim, lineHeight: 1.5, marginTop: 12,
      }}>
        Native backend activates only when <span style={{ color: B.text, fontFamily: "'JetBrains Mono', monospace" }}>xpe_common.dll</span> and <span style={{ color: B.text, fontFamily: "'JetBrains Mono', monospace" }}>xpe_display.dll</span> ABI matches Phase 1b.
      </div>
    </div>
  );
}

function CalibrationContent({ calibration, setStage }) {
  const stages = Object.keys(calibration);
  return (
    <div>
      <Section title="Calibration paths">
        <PathField label="Offset" />
        <PathField label="Gain" />
        <PathField label="Defect" />
      </Section>
      <Section title="Stage modes">
        <p style={{ fontSize: 11, color: B.textDim, lineHeight: 1.6, margin: "0 0 14px" }}>
          <strong style={{ color: B.text }}>Auto</strong> follows fixture metadata. <strong style={{ color: B.text }}>On</strong> forces a stage for A/B. <strong style={{ color: B.text }}>Off</strong> bypasses for measurement.
        </p>
        {stages.map(s => (
          <div key={s} style={{
            display: "flex", alignItems: "center", gap: 12,
            padding: "10px 0", borderBottom: `1px solid ${B.hairline}`,
          }}>
            <div style={{ flex: 1, fontSize: 12 }}>{s}</div>
            <Seg value={calibration[s]} options={["Auto", "On", "Off"]}
              onChange={v => setStage(s, v)} />
          </div>
        ))}
      </Section>
    </div>
  );
}

function DisplayContent({ voiMode, setVoiMode, bodyPart, setBodyPart }) {
  return (
    <div>
      <Section title="VOI LUT">
        <FieldB label="Mode">
          <Seg value={voiMode} options={["Linear", "LinearExact", "Sigmoid"]} onChange={setVoiMode} />
        </FieldB>
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8 }}>
          <FieldB label="Window center"><InputB value="2048" mono /></FieldB>
          <FieldB label="Window width"><InputB value="4096" mono /></FieldB>
        </div>
      </Section>
      <Section title="Body part preset">
        <FieldB label="Preset">
          <select value={bodyPart} onChange={e => setBodyPart(e.target.value)}
            style={selectStyle}>
            {["Chest PA", "Chest LAT", "Abdomen", "Skull AP", "Wrist LAT", "Pelvis AP"].map(o =>
              <option key={o}>{o}</option>)}
          </select>
        </FieldB>
        <button style={btnB}>Apply preset</button>
      </Section>
      <Section title="Modality LUT">
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8 }}>
          <FieldB label="Slope"><InputB value="1.0" mono /></FieldB>
          <FieldB label="Intercept"><InputB value="0.0" mono /></FieldB>
        </div>
        <label style={{ display: "flex", alignItems: "center", gap: 10, marginTop: 12, fontSize: 12, color: B.textDim, cursor: "pointer" }}>
          <input type="checkbox" style={{ accentColor: B.accent }} />
          <span>GSDF (validation pending)</span>
        </label>
      </Section>
      <button style={{ ...btnB, background: B.accent, color: "#0c1220" }}>Apply display pipeline</button>
    </div>
  );
}

function LogsContent() {
  const logs = [
    ["08:42:01", "INFO", "GUI-S0 initialized."],
    ["08:42:01", "INFO", "MockXpeBackend ready (v0.0.0-mock)"],
    ["08:42:14", "INFO", "Loaded raw 1024×1024 UInt16LE"],
    ["08:42:14", "DEBG", "SHA-256 verified: a3f2…7c91"],
    ["08:42:18", "INFO", "Calibration evaluation: 4/7 stages auto"],
    ["08:42:22", "INFO", "Apply Display Pipeline → 412 ms"],
  ];
  return (
    <div>
      {logs.map(([t, l, m], i) => {
        const c = l === "WARN" ? B.amber : l === "DEBG" ? B.textMute : B.accent;
        return (
          <div key={i} style={{
            padding: "10px 0", borderBottom: i < logs.length - 1 ? `1px solid ${B.hairline}` : "none",
            fontSize: 12, fontFamily: "'JetBrains Mono', Consolas, monospace",
          }}>
            <div style={{ display: "flex", gap: 10, alignItems: "baseline" }}>
              <span style={{ color: B.textMute, fontSize: 10 }}>{t}</span>
              <span style={{ color: c, fontWeight: 600, fontSize: 10, minWidth: 38 }}>{l}</span>
            </div>
            <div style={{ marginTop: 4, color: B.text, lineHeight: 1.5 }}>{m}</div>
          </div>
        );
      })}
    </div>
  );
}

function AlertsContent() {
  const alerts = [
    ["INFO", "GSDF validation pending — DICOM PS3.14 not enforced."],
    ["WARN", "Native DLL not detected. Falling back to Mock backend."],
    ["WARN", "Modality rescale slope=1.0 — defaults applied."],
  ];
  return (
    <div>
      {alerts.map(([l, m], i) => {
        const c = l === "ERROR" ? B.red : l === "WARN" ? B.amber : B.accent;
        return (
          <div key={i} style={{
            padding: 14, marginBottom: 10, borderRadius: 10,
            background: `${c}15`, border: `1px solid ${c}40`,
          }}>
            <div style={{ fontSize: 10, fontWeight: 700, color: c, letterSpacing: 1.2, marginBottom: 6 }}>{l}</div>
            <div style={{ fontSize: 12, lineHeight: 1.5 }}>{m}</div>
          </div>
        );
      })}
    </div>
  );
}

/* small primitives */
function Section({ title, children }) {
  return (
    <div style={{ marginBottom: 24 }}>
      <div style={{
        fontSize: 10, fontWeight: 700, letterSpacing: 1.4,
        color: B.textMute, marginBottom: 12, textTransform: "uppercase",
      }}>{title}</div>
      {children}
    </div>
  );
}

function Row({ label, value, mono, dim }) {
  return (
    <div style={{
      display: "flex", justifyContent: "space-between", alignItems: "center",
      padding: "8px 0", borderBottom: `1px solid ${B.hairline}`,
      fontSize: 12,
    }}>
      <span style={{ color: B.textDim }}>{label}</span>
      <span style={{
        color: dim ? B.textMute : B.text,
        fontFamily: mono ? "'JetBrains Mono', Consolas, monospace" : "inherit",
        fontSize: mono ? 11 : 12,
      }}>{value}</span>
    </div>
  );
}

function Pill({ color, children }) {
  return (
    <span style={{
      display: "inline-flex", alignItems: "center", gap: 6,
      padding: "3px 10px", borderRadius: 10,
      background: color ? `${color}20` : "rgba(255,255,255,0.06)",
      color: color || B.text,
      fontSize: 11, fontWeight: 500,
    }}>
      {color && <span style={{ width: 6, height: 6, borderRadius: 3, background: color }} />}
      {children}
    </span>
  );
}

function PathField({ label }) {
  return (
    <div style={{ marginBottom: 10 }}>
      <div style={{ fontSize: 11, color: B.textDim, marginBottom: 6 }}>{label}</div>
      <div style={{ display: "flex", gap: 6 }}>
        <div style={{
          flex: 1, padding: "8px 10px",
          background: "rgba(0,0,0,0.3)",
          border: `1px solid ${B.hairline}`, borderRadius: 6,
          fontSize: 11, fontFamily: "'JetBrains Mono', Consolas, monospace",
          color: B.textDim,
          overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap",
        }}>./calibration/{label.toLowerCase()}/</div>
        <button style={{
          padding: "0 14px", background: "rgba(255,255,255,0.06)",
          color: B.text, border: `1px solid ${B.hairline}`,
          borderRadius: 6, fontSize: 11, cursor: "pointer",
        }}>Browse</button>
      </div>
    </div>
  );
}

function Seg({ value, options, onChange }) {
  return (
    <div style={{
      display: "flex", padding: 2,
      background: "rgba(0,0,0,0.3)", borderRadius: 6,
      border: `1px solid ${B.hairline}`,
    }}>
      {options.map(o => (
        <button key={o} onClick={() => onChange(o)} style={{
          padding: "5px 11px", borderRadius: 4,
          background: value === o
            ? (o === "On" ? "rgba(134,239,172,0.18)" : o === "Off" ? "rgba(252,165,165,0.18)" : B.accentBg)
            : "transparent",
          color: value === o
            ? (o === "On" ? B.green : o === "Off" ? B.red : B.accent)
            : B.textDim,
          border: "none", cursor: "pointer", fontSize: 11, fontWeight: 600,
        }}>{o}</button>
      ))}
    </div>
  );
}

function FieldB({ label, children }) {
  return (
    <div style={{ marginBottom: 12 }}>
      <div style={{ fontSize: 11, color: B.textDim, marginBottom: 6 }}>{label}</div>
      {children}
    </div>
  );
}

function InputB({ value, mono }) {
  return (
    <input defaultValue={value} style={{
      width: "100%", padding: "8px 10px",
      background: "rgba(0,0,0,0.3)",
      border: `1px solid ${B.hairline}`, borderRadius: 6,
      color: B.text, fontSize: 12,
      fontFamily: mono ? "'JetBrains Mono', Consolas, monospace" : "inherit",
      boxSizing: "border-box",
    }} />
  );
}

const selectStyle = {
  width: "100%", padding: "8px 10px",
  background: "rgba(0,0,0,0.3)",
  border: `1px solid ${B.hairline}`, borderRadius: 6,
  color: B.text, fontSize: 12,
};

const btnB = {
  width: "100%", padding: "10px 14px", marginTop: 6,
  background: "rgba(255,255,255,0.06)",
  color: B.text, border: `1px solid ${B.hairlineStrong}`,
  borderRadius: 8, fontSize: 12, fontWeight: 600, cursor: "pointer",
};

window.VariantB = VariantB;
