/* global React */
const { useState, useMemo } = React;

/* ============================================================
   VARIANT A — Classic Pro Workstation
   Dense, structured, Photoshop/Lightroom-like dark UI.
   For QA engineers and technical users who want everything visible.
   ============================================================ */

const A = {
  bg: "#1a1d23",
  panel: "#22262d",
  panelAlt: "#1e2128",
  border: "#2e333c",
  borderStrong: "#3a4150",
  text: "#e6e8eb",
  textDim: "#9ba3ad",
  textMute: "#6b7280",
  accent: "#5b9dff",
  accentDim: "#2d4a7a",
  green: "#4ade80",
  amber: "#fbbf24",
  red: "#f87171",
  divider: "#262a31",
};

function VariantA() {
  const [view, setView] = useState("split"); // swipe | split | overlay | diff
  const [calibration, setCalibration] = useState({
    Offset: "Auto", Gain: "Auto", Defect: "Auto",
    Ghost: "Auto", Temperature: "Auto", Nonlinearity: "Auto", Binning: "Auto"
  });
  const [bodyPart, setBodyPart] = useState("Chest PA");
  const [voiMode, setVoiMode] = useState("Linear");
  const [center, setCenter] = useState(2048);
  const [width, setWidth] = useState(4096);
  const [zoom, setZoom] = useState(100);
  const [overlay, setOverlay] = useState(0.5);
  const [tab, setTab] = useState("calibration");

  const setStage = (k, v) => setCalibration(s => ({ ...s, [k]: v }));

  return (
    <div style={{
      width: 1560, height: 920,
      background: A.bg, color: A.text,
      fontFamily: "'Segoe UI', -apple-system, system-ui, sans-serif",
      fontSize: 13, display: "flex", flexDirection: "column",
      overflow: "hidden",
    }}>
      {/* Title bar */}
      <div style={{
        height: 32, background: "#15171c",
        borderBottom: `1px solid ${A.border}`,
        display: "flex", alignItems: "center", padding: "0 12px",
        fontSize: 12, color: A.textDim,
        WebkitAppRegion: "drag",
      }}>
        <div style={{ display: "flex", gap: 6, marginRight: 12 }}>
          <div style={{ width: 12, height: 12, borderRadius: 6, background: "#ff5f56" }} />
          <div style={{ width: 12, height: 12, borderRadius: 6, background: "#ffbd2e" }} />
          <div style={{ width: 12, height: 12, borderRadius: 6, background: "#27c93f" }} />
        </div>
        <span style={{ color: A.text, fontWeight: 500 }}>ImageProcTest</span>
        <span style={{ margin: "0 8px", color: A.textMute }}>—</span>
        <span>GUI-S0 · Mock Backend · v0.1.0</span>
        <div style={{ flex: 1 }} />
        <span style={{ fontSize: 11, color: A.textMute }}>Ready</span>
      </div>

      {/* Menu bar */}
      <div style={{
        height: 28, background: A.panelAlt,
        borderBottom: `1px solid ${A.border}`,
        display: "flex", alignItems: "center", padding: "0 4px",
        fontSize: 12,
      }}>
        {["File", "Backend", "View", "Pipeline", "Tools", "Help"].map(m => (
          <div key={m} style={{
            padding: "5px 10px", cursor: "pointer", borderRadius: 3,
            color: A.textDim,
          }}
          onMouseEnter={e => e.currentTarget.style.background = A.panel}
          onMouseLeave={e => e.currentTarget.style.background = "transparent"}
          >{m}</div>
        ))}
      </div>

      {/* Toolbar */}
      <div style={{
        height: 44, background: A.panel,
        borderBottom: `1px solid ${A.border}`,
        display: "flex", alignItems: "center", gap: 4, padding: "0 8px",
      }}>
        <ToolBtn icon="⚡" label="Init" primary />
        <ToolBtn icon="◾" label="Shutdown" />
        <Sep />
        <ToolBtn icon="📂" label="Load Raw" />
        <ToolBtn icon="💾" label="Save" />
        <Sep />
        <ToolBtn icon="▶" label="Run Pipeline" accent />
        <Sep />
        <ToolBtn icon="🔍" label="Fit" />
        <ToolBtn icon="1:1" label="100%" />
        <ToolBtn icon="+" label="In" />
        <ToolBtn icon="−" label="Out" />
        <div style={{ flex: 1 }} />
        <div style={{
          display: "flex", alignItems: "center", gap: 8,
          padding: "0 12px", fontSize: 11, color: A.textDim,
        }}>
          <Pulse color={A.green} />
          <span>Backend: MockXpeBackend</span>
          <span style={{ color: A.textMute }}>·</span>
          <span>v0.0.0-mock</span>
        </div>
      </div>

      {/* Main work area */}
      <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
        {/* LEFT — settings rail */}
        <div style={{
          width: 320, background: A.panelAlt,
          borderRight: `1px solid ${A.border}`,
          display: "flex", flexDirection: "column", minHeight: 0,
        }}>
          {/* Tabs */}
          <div style={{
            display: "flex", borderBottom: `1px solid ${A.border}`,
            background: A.bg,
          }}>
            {[
              ["runtime", "Runtime"],
              ["raw", "Raw"],
              ["calibration", "Calibration"],
              ["display", "Display"],
            ].map(([k, label]) => (
              <div key={k} onClick={() => setTab(k)} style={{
                flex: 1, padding: "9px 6px", textAlign: "center",
                fontSize: 11, fontWeight: 500, cursor: "pointer",
                color: tab === k ? A.text : A.textMute,
                borderBottom: tab === k ? `2px solid ${A.accent}` : "2px solid transparent",
                marginBottom: -1,
              }}>{label}</div>
            ))}
          </div>

          <div style={{ flex: 1, overflowY: "auto", padding: 14 }}>
            {tab === "runtime" && <RuntimePanel />}
            {tab === "raw" && <RawPanel />}
            {tab === "calibration" && (
              <CalibrationPanel calibration={calibration} setStage={setStage} />
            )}
            {tab === "display" && (
              <DisplayPanel
                voiMode={voiMode} setVoiMode={setVoiMode}
                center={center} setCenter={setCenter}
                width={width} setWidth={setWidth}
                bodyPart={bodyPart} setBodyPart={setBodyPart}
              />
            )}
          </div>
        </div>

        {/* CENTER — viewport */}
        <div style={{
          flex: 1, display: "flex", flexDirection: "column",
          background: "#0d0f13", minWidth: 0,
        }}>
          {/* Viewport header */}
          <div style={{
            height: 38, padding: "0 14px",
            background: A.panel, borderBottom: `1px solid ${A.border}`,
            display: "flex", alignItems: "center", gap: 12, fontSize: 12,
          }}>
            <span style={{ fontWeight: 600 }}>Source vs Processed</span>
            <span style={{ color: A.textMute }}>·</span>
            <span style={{ color: A.textDim }}>RAW 1024×1024 · UInt16LE</span>
            <div style={{ flex: 1 }} />
            <ViewToggle view={view} setView={setView} />
            <Sep />
            <span style={{ color: A.textDim, fontSize: 11 }}>Zoom</span>
            <input type="range" min={25} max={400} value={zoom} onChange={e => setZoom(+e.target.value)}
              style={{ width: 80 }} />
            <span style={{ color: A.text, fontSize: 11, fontVariantNumeric: "tabular-nums", minWidth: 38 }}>{zoom}%</span>
          </div>

          {/* Image viewport */}
          <div style={{ flex: 1, padding: 14, display: "flex", gap: 10, minHeight: 0 }}>
            <ImagePane label="SOURCE" sublabel="Pre-processing" view={view} side="src" overlay={overlay} />
            <ImagePane label="PROCESSED" sublabel="Display pipeline applied" view={view} side="proc" overlay={overlay} />
          </div>

          {/* Overlay slider when relevant */}
          {view === "overlay" && (
            <div style={{
              height: 40, padding: "0 14px",
              background: A.panel, borderTop: `1px solid ${A.border}`,
              display: "flex", alignItems: "center", gap: 12, fontSize: 11,
            }}>
              <span style={{ color: A.textDim }}>Overlay opacity</span>
              <input type="range" min={0} max={1} step={0.01} value={overlay}
                onChange={e => setOverlay(+e.target.value)} style={{ flex: 1, maxWidth: 300 }} />
              <span style={{ fontVariantNumeric: "tabular-nums", color: A.text }}>
                {(overlay * 100).toFixed(0)}%
              </span>
            </div>
          )}

          {/* Bottom — image summary */}
          <div style={{
            height: 130, background: A.panel,
            borderTop: `1px solid ${A.border}`,
            padding: 14, display: "flex", gap: 24,
          }}>
            <SummaryBlock title="IMAGE">
              <Kv k="Dimensions" v="1024 × 1024" />
              <Kv k="Bit depth" v="16-bit unsigned" />
              <Kv k="Min / Max" v="142 / 65,210" />
              <Kv k="Mean / σ" v="3,847.2 / 1,201.4" />
            </SummaryBlock>
            <SummaryBlock title="CALIBRATION">
              <Kv k="Stages active" v="4 of 7" />
              <Kv k="Offset" v="Auto" />
              <Kv k="Gain" v="Auto" />
              <Kv k="Defect" v="Auto" />
            </SummaryBlock>
            <SummaryBlock title="DISPLAY">
              <Kv k="VOI mode" v="Linear" />
              <Kv k="Window C/W" v="2048 / 4096" />
              <Kv k="Body part" v="Chest PA" />
              <Kv k="GSDF" v="Pending" mute />
            </SummaryBlock>
            <SummaryBlock title="TIMING">
              <Kv k="Last run" v="412 ms" />
              <Kv k="Preprocess" v="148 ms" />
              <Kv k="VOI LUT" v="89 ms" />
              <Kv k="Present" v="175 ms" />
            </SummaryBlock>
          </div>
        </div>

        {/* RIGHT — diagnostics */}
        <div style={{
          width: 360, background: A.panelAlt,
          borderLeft: `1px solid ${A.border}`,
          display: "flex", flexDirection: "column",
        }}>
          <DiagPanel title="Logs" count={6}>
            <LogRow t="08:42:01" lvl="INFO" msg="GUI-S0 initialized." />
            <LogRow t="08:42:01" lvl="INFO" msg="MockXpeBackend ready (v0.0.0-mock)" />
            <LogRow t="08:42:14" lvl="INFO" msg="Loaded raw 1024×1024 UInt16LE" />
            <LogRow t="08:42:14" lvl="DEBG" msg="SHA-256 verified: a3f2…7c91" />
            <LogRow t="08:42:18" lvl="INFO" msg="Calibration evaluation: 4/7 stages auto" />
            <LogRow t="08:42:22" lvl="INFO" msg="Apply Display Pipeline → 412 ms" />
          </DiagPanel>
          <div style={{ height: 1, background: A.border }} />
          <DiagPanel title="Alerts" count={3}>
            <Alert lvl="INFO" msg="GSDF validation pending — DICOM PS3.14 not enforced." />
            <Alert lvl="WARN" msg="Native DLL not detected. Falling back to Mock backend." />
            <Alert lvl="WARN" msg="Modality rescale slope=1.0 — defaults applied." />
          </DiagPanel>
        </div>
      </div>

      {/* Status bar */}
      <div style={{
        height: 24, background: "#15171c",
        borderTop: `1px solid ${A.border}`,
        display: "flex", alignItems: "center", padding: "0 12px",
        fontSize: 11, color: A.textDim, gap: 16,
      }}>
        <span><Pulse color={A.green} /> Ready</span>
        <span style={{ color: A.textMute }}>·</span>
        <span>RAW 1024×1024 UInt16LE</span>
        <span style={{ color: A.textMute }}>·</span>
        <span>4 stages active</span>
        <div style={{ flex: 1 }} />
        <span>Last run 412ms</span>
        <span style={{ color: A.textMute }}>·</span>
        <span>Mem 142 MB</span>
      </div>
    </div>
  );
}

/* ===== sub-components ===== */

function ToolBtn({ icon, label, primary, accent }) {
  const [hover, setHover] = useState(false);
  const bg = primary ? A.accentDim : (accent ? "#1f3a26" : "transparent");
  return (
    <div onMouseEnter={() => setHover(true)} onMouseLeave={() => setHover(false)}
      style={{
        display: "flex", alignItems: "center", gap: 6,
        padding: "5px 10px", borderRadius: 4, fontSize: 12,
        cursor: "pointer", color: A.text,
        background: hover ? (primary ? "#3a5e96" : (accent ? "#2a523a" : A.panelAlt)) : bg,
        border: `1px solid ${primary || accent ? "transparent" : (hover ? A.border : "transparent")}`,
      }}>
      <span style={{ fontSize: 13, opacity: 0.85 }}>{icon}</span>
      <span>{label}</span>
    </div>
  );
}

function Sep() {
  return <div style={{ width: 1, height: 22, background: A.border, margin: "0 4px" }} />;
}

function Pulse({ color }) {
  return (
    <span style={{
      display: "inline-block", width: 8, height: 8, borderRadius: 4,
      background: color, boxShadow: `0 0 6px ${color}`, marginRight: 6,
    }} />
  );
}

function ViewToggle({ view, setView }) {
  const opts = [
    ["swipe", "Swipe"], ["split", "Split"],
    ["overlay", "Overlay"], ["diff", "Diff"],
  ];
  return (
    <div style={{
      display: "flex", background: A.bg, borderRadius: 4,
      border: `1px solid ${A.border}`, padding: 2,
    }}>
      {opts.map(([k, label]) => (
        <div key={k} onClick={() => setView(k)} style={{
          padding: "3px 10px", borderRadius: 3, fontSize: 11, cursor: "pointer",
          background: view === k ? A.accent : "transparent",
          color: view === k ? "#fff" : A.textDim, fontWeight: 500,
        }}>{label}</div>
      ))}
    </div>
  );
}

function ImagePane({ label, sublabel, view, side, overlay }) {
  // overlay: only show one with reduced opacity stacked
  if (view === "overlay" && side === "src") return null;
  const isDiff = view === "diff";
  return (
    <div style={{
      flex: 1, background: "#000",
      border: `1px solid ${A.border}`, borderRadius: 4,
      display: "flex", flexDirection: "column",
      overflow: "hidden", position: "relative",
    }}>
      <div style={{
        position: "absolute", top: 8, left: 10, zIndex: 2,
        fontSize: 10, fontWeight: 700, letterSpacing: 1.2,
        color: side === "proc" ? A.green : A.accent,
        textShadow: "0 1px 4px rgba(0,0,0,0.8)",
      }}>{label}</div>
      <div style={{
        position: "absolute", top: 22, left: 10, zIndex: 2,
        fontSize: 10, color: A.textDim,
        textShadow: "0 1px 4px rgba(0,0,0,0.8)",
      }}>{sublabel}</div>

      <div style={{ flex: 1, position: "relative", overflow: "hidden" }}>
        <img src="assets/xray-placeholder.svg" alt="" style={{
          width: "100%", height: "100%", objectFit: "contain",
          filter: side === "proc" ? "contrast(1.15) brightness(1.05)" : "contrast(0.85) brightness(0.85)",
          mixBlendMode: isDiff ? "difference" : "normal",
        }} />
        {/* corner markers */}
        <div style={{
          position: "absolute", top: 8, right: 10,
          fontSize: 10, color: A.textDim, fontFamily: "Consolas, monospace",
          textShadow: "0 1px 4px rgba(0,0,0,0.8)",
        }}>1024 × 1024</div>
        <div style={{
          position: "absolute", bottom: 8, right: 10,
          fontSize: 10, color: A.textDim, fontFamily: "Consolas, monospace",
          textShadow: "0 1px 4px rgba(0,0,0,0.8)",
        }}>16-bit · UInt16LE</div>
        {/* crosshair when split */}
        {view === "split" && side === "src" && (
          <div style={{
            position: "absolute", top: 0, bottom: 0, right: -1, width: 2,
            background: A.accent, opacity: 0.5,
          }} />
        )}
      </div>
    </div>
  );
}

function RuntimePanel() {
  return (
    <div>
      <SectionTitle>Backend</SectionTitle>
      <Field label="Mode">
        <Select value="Mock" options={["Mock", "Native"]} />
      </Field>
      <Field label="Status">
        <StatusPill color={A.green} text="Initialized" />
      </Field>
      <KvList rows={[
        ["Backend name", "MockXpeBackend"],
        ["Version", "v0.0.0-mock"],
        ["Native runtime", "Not supported"],
        ["Native DLL", "Not detected"],
      ]} />
      <SectionTitle>Display backend</SectionTitle>
      <KvList rows={[
        ["Display version", "v0.0.0-mock"],
        ["Display DLL", "Not detected"],
      ]} />
      <div style={{ marginTop: 14, padding: 10, background: A.bg, borderRadius: 4, fontSize: 11, color: A.textDim, lineHeight: 1.5 }}>
        Native backend activates only when <code style={{ color: A.text }}>xpe_common.dll</code> and <code style={{ color: A.text }}>xpe_display.dll</code> ABI matches Phase 1b.
      </div>
    </div>
  );
}

function RawPanel() {
  return (
    <div>
      <SectionTitle>Raw image format</SectionTitle>
      <Field label="Width"><Input value="1024" /></Field>
      <Field label="Height"><Input value="1024" /></Field>
      <Field label="Pixel format">
        <Select value="UInt16LE" options={["UInt16LE", "UInt16BE", "UInt8", "Float32LE"]} />
      </Field>
      <Field label="Last directory">
        <Input value="D:\\workspace\\image-processing\\..." mono />
      </Field>
    </div>
  );
}

function CalibrationPanel({ calibration, setStage }) {
  return (
    <div>
      <SectionTitle>Calibration paths</SectionTitle>
      {["Offset", "Gain", "Defect"].map(k => (
        <PathRow key={k} label={k} />
      ))}
      <SectionTitle style={{ marginTop: 18 }}>Stage modes</SectionTitle>
      <p style={{ fontSize: 11, color: A.textDim, lineHeight: 1.5, margin: "0 0 10px" }}>
        Auto follows fixture metadata. On forces a stage for A/B. Off bypasses for measurement.
      </p>
      {Object.keys(calibration).map(stage => (
        <div key={stage} style={{
          display: "grid", gridTemplateColumns: "90px 1fr",
          alignItems: "center", marginBottom: 6,
        }}>
          <span style={{ fontSize: 12, color: A.textDim }}>{stage}</span>
          <Segmented value={calibration[stage]} options={["Auto", "On", "Off"]}
            onChange={v => setStage(stage, v)} />
        </div>
      ))}
    </div>
  );
}

function DisplayPanel({ voiMode, setVoiMode, center, setCenter, width, setWidth, bodyPart, setBodyPart }) {
  return (
    <div>
      <SectionTitle>VOI</SectionTitle>
      <Field label="Mode">
        <Select value={voiMode} options={["Linear", "LinearExact", "Sigmoid"]}
          onChange={setVoiMode} />
      </Field>
      <Field label="Window center">
        <Input value={center} onChange={v => setCenter(+v || 0)} mono />
      </Field>
      <Field label="Window width">
        <Input value={width} onChange={v => setWidth(+v || 0)} mono />
      </Field>
      <SectionTitle>Body part preset</SectionTitle>
      <Field label="Preset">
        <Select value={bodyPart}
          options={["Chest PA", "Chest LAT", "Abdomen", "Skull AP", "Wrist LAT", "Pelvis AP"]}
          onChange={setBodyPart} />
      </Field>
      <button style={btnPrimary}>Apply preset</button>
      <SectionTitle>Modality LUT</SectionTitle>
      <Field label="Rescale slope"><Input value="1.0" mono /></Field>
      <Field label="Rescale intercept"><Input value="0.0" mono /></Field>
      <div style={{ display: "flex", alignItems: "center", gap: 8, margin: "8px 0 12px" }}>
        <input type="checkbox" />
        <span style={{ fontSize: 12, color: A.textDim }}>GSDF (validation pending)</span>
      </div>
      <button style={btnPrimary}>Apply display pipeline</button>
    </div>
  );
}

const btnPrimary = {
  width: "100%", padding: "8px 10px", marginTop: 8,
  background: A.accent, color: "#fff",
  border: "none", borderRadius: 4, fontSize: 12, fontWeight: 500,
  cursor: "pointer",
};

function PathRow({ label }) {
  return (
    <div style={{ marginBottom: 10 }}>
      <div style={{ fontSize: 11, color: A.textDim, marginBottom: 4 }}>{label}</div>
      <div style={{ display: "flex", gap: 6 }}>
        <input style={{
          flex: 1, padding: "6px 8px", background: A.bg, border: `1px solid ${A.border}`,
          color: A.text, fontSize: 11, borderRadius: 3, fontFamily: "Consolas, monospace",
        }} defaultValue={`./calibration/${label.toLowerCase()}/`} />
        <button style={{
          padding: "6px 12px", background: A.panel, color: A.textDim,
          border: `1px solid ${A.border}`, borderRadius: 3, fontSize: 11, cursor: "pointer",
        }}>…</button>
      </div>
    </div>
  );
}

function SectionTitle({ children, style }) {
  return (
    <div style={{
      fontSize: 10, fontWeight: 700, letterSpacing: 1.2,
      color: A.textMute, textTransform: "uppercase",
      marginBottom: 10, ...style,
    }}>{children}</div>
  );
}

function Field({ label, children }) {
  return (
    <div style={{ marginBottom: 10 }}>
      <div style={{ fontSize: 11, color: A.textDim, marginBottom: 4 }}>{label}</div>
      {children}
    </div>
  );
}

function Input({ value, onChange, mono }) {
  return (
    <input value={value} onChange={e => onChange?.(e.target.value)} style={{
      width: "100%", padding: "6px 8px", background: A.bg,
      border: `1px solid ${A.border}`, color: A.text,
      fontSize: 12, borderRadius: 3,
      fontFamily: mono ? "Consolas, monospace" : "inherit",
      boxSizing: "border-box",
    }} />
  );
}

function Select({ value, options, onChange }) {
  return (
    <select value={value} onChange={e => onChange?.(e.target.value)} style={{
      width: "100%", padding: "6px 8px", background: A.bg,
      border: `1px solid ${A.border}`, color: A.text,
      fontSize: 12, borderRadius: 3,
    }}>
      {options.map(o => <option key={o} value={o}>{o}</option>)}
    </select>
  );
}

function Segmented({ value, options, onChange }) {
  return (
    <div style={{
      display: "flex", background: A.bg, borderRadius: 3,
      border: `1px solid ${A.border}`, padding: 1,
    }}>
      {options.map(o => (
        <div key={o} onClick={() => onChange(o)} style={{
          flex: 1, padding: "4px 6px", textAlign: "center",
          fontSize: 11, fontWeight: 500, cursor: "pointer", borderRadius: 2,
          background: value === o
            ? (o === "On" ? "#2d5a3a" : o === "Off" ? "#5a2d36" : A.accentDim)
            : "transparent",
          color: value === o ? "#fff" : A.textDim,
        }}>{o}</div>
      ))}
    </div>
  );
}

function StatusPill({ color, text }) {
  return (
    <div style={{
      display: "inline-flex", alignItems: "center", gap: 6,
      padding: "3px 8px", background: A.bg, borderRadius: 10,
      border: `1px solid ${A.border}`, fontSize: 11,
    }}>
      <span style={{ width: 6, height: 6, borderRadius: 3, background: color }} />
      <span>{text}</span>
    </div>
  );
}

function KvList({ rows }) {
  return (
    <div style={{ marginBottom: 10 }}>
      {rows.map(([k, v]) => (
        <div key={k} style={{
          display: "flex", justifyContent: "space-between",
          padding: "5px 0", borderBottom: `1px solid ${A.divider}`,
          fontSize: 11,
        }}>
          <span style={{ color: A.textMute }}>{k}</span>
          <span style={{ color: A.text, fontFamily: "Consolas, monospace" }}>{v}</span>
        </div>
      ))}
    </div>
  );
}

function SummaryBlock({ title, children }) {
  return (
    <div style={{ flex: 1 }}>
      <div style={{
        fontSize: 10, fontWeight: 700, letterSpacing: 1.2,
        color: A.textMute, marginBottom: 8,
      }}>{title}</div>
      {children}
    </div>
  );
}

function Kv({ k, v, mute }) {
  return (
    <div style={{
      display: "flex", justifyContent: "space-between",
      padding: "3px 0", fontSize: 11,
    }}>
      <span style={{ color: A.textMute }}>{k}</span>
      <span style={{
        color: mute ? A.textMute : A.text,
        fontFamily: "Consolas, monospace",
      }}>{v}</span>
    </div>
  );
}

function DiagPanel({ title, count, children }) {
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minHeight: 0 }}>
      <div style={{
        height: 32, padding: "0 12px",
        background: A.panel, borderBottom: `1px solid ${A.border}`,
        display: "flex", alignItems: "center", gap: 8,
        fontSize: 12, fontWeight: 600,
      }}>
        <span>{title}</span>
        <span style={{
          padding: "1px 6px", background: A.bg, borderRadius: 8,
          fontSize: 10, color: A.textDim, fontWeight: 500,
        }}>{count}</span>
        <div style={{ flex: 1 }} />
        <span style={{ fontSize: 11, color: A.textMute, cursor: "pointer" }}>Clear</span>
      </div>
      <div style={{ flex: 1, overflowY: "auto", padding: "8px 12px" }}>
        {children}
      </div>
    </div>
  );
}

function LogRow({ t, lvl, msg }) {
  const lvlColor = lvl === "ERR" ? A.red : lvl === "WARN" ? A.amber : lvl === "DEBG" ? A.textMute : A.textDim;
  return (
    <div style={{
      fontFamily: "Consolas, monospace", fontSize: 11,
      padding: "3px 0", display: "flex", gap: 8, lineHeight: 1.5,
    }}>
      <span style={{ color: A.textMute }}>{t}</span>
      <span style={{ color: lvlColor, fontWeight: 600, minWidth: 36 }}>{lvl}</span>
      <span style={{ color: A.text, flex: 1 }}>{msg}</span>
    </div>
  );
}

function Alert({ lvl, msg }) {
  const bg = lvl === "ERROR" ? "#3a1a20" : lvl === "WARN" ? "#3a2c14" : "#1a2a3a";
  const accent = lvl === "ERROR" ? A.red : lvl === "WARN" ? A.amber : A.accent;
  return (
    <div style={{
      padding: "8px 10px", marginBottom: 6,
      background: bg, borderLeft: `3px solid ${accent}`,
      borderRadius: 3, fontSize: 11, color: A.text, lineHeight: 1.4,
    }}>
      <div style={{
        fontSize: 10, fontWeight: 700, color: accent,
        letterSpacing: 1, marginBottom: 3,
      }}>{lvl}</div>
      <div>{msg}</div>
    </div>
  );
}

window.VariantA = VariantA;
