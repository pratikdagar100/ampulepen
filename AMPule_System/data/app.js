// AMPule System dashboard — vanilla JS, no external dependencies.
// Polls the ESP32's REST API and mirrors OLED/physical device state.
// Every number/label here comes from a real /api/* field — nothing on this
// page is simulated or invented (see README "Dashboard data honesty").

const STATUS_POLL_MS = 750;
const SYSTEM_POLL_MS = 3000;
const HISTORY_POLL_MS = 2000;

const WEIGHT_LABELS = ["UNDER 40kg", "41-60kg", "61-80kg", "81kg+"];

const STEP_META = [
  { key: "WAITING_FOR_AMPULE", label: "INSERT", icon: "ampule", done: "Ampule inserted", active: "Awaiting RFID tag", pending: "" },
  { key: "RFID_SCANNING", label: "RFID", icon: "rfid", done: "UID captured", active: "Reading tag UID…", pending: "Insert ampule" },
  { key: "VERIFYING_AMPULE", label: "VERIFY", icon: "shield", done: "Verification passed", active: "Checking registry…", pending: "Awaiting scan" },
  { key: "WEIGHT_SELECTION", label: "WEIGHT", icon: "scale", done: null, active: "Select weight range", pending: "Awaiting verify" },
  { key: "DOSE_DISPLAY", label: "DOSE", icon: "pill", done: null, active: "Calculating demo dose…", pending: "Awaiting weight" },
  { key: "COMPLETED", label: "COMPLETE", icon: "flag", done: null, active: "Ampule marked used", pending: "Awaiting confirm" },
];

const STATE_TO_STEP = {
  WAITING_FOR_AMPULE: 0,
  RFID_SCANNING: 1,
  VERIFYING_AMPULE: 2,
  AMPULE_ACTIVE: 2,
  WEIGHT_SELECTION: 3,
  DOSE_DISPLAY: 4,
  COMPLETED: 5,
};

let lastHistorySignature = "";
let medicinesCache = [];

// ---------------------------------------------------------------------------
// Inline icons (no external icon font / images — keeps LittleFS payload tiny)
// ---------------------------------------------------------------------------

const ICON = {
  check: '<svg viewBox="0 0 20 20" fill="none"><path d="M4.5 10.5l3.5 3.5L15.5 6" stroke="currentColor" stroke-width="2.3" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  cross: '<svg viewBox="0 0 20 20" fill="none"><path d="M6 6l8 8M14 6l-8 8" stroke="currentColor" stroke-width="2.3" stroke-linecap="round"/></svg>',
  alert: '<svg viewBox="0 0 24 24" fill="none"><path d="M12 9v4M12 16.5h.01" stroke="currentColor" stroke-width="2" stroke-linecap="round"/><path d="M10.3 3.9 2.5 17a1.8 1.8 0 0 0 1.6 2.7h15.8a1.8 1.8 0 0 0 1.6-2.7L13.7 3.9a1.8 1.8 0 0 0-3.4 0Z" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/></svg>',
  ampule: '<svg viewBox="0 0 24 24" fill="none"><path d="M9 2h6M10 2v5.2a3 3 0 0 1-.6 1.8L6.8 12.8A4 4 0 0 0 6 15.2V19a3 3 0 0 0 3 3h6a3 3 0 0 0 3-3v-3.8a4 4 0 0 0-.8-2.4l-2.6-3.8a3 3 0 0 1-.6-1.8V2" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  rfid: '<svg viewBox="0 0 24 24" fill="none"><path d="M8.5 8.5a5 5 0 0 1 7 0M6 6a9 9 0 0 1 12 0" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/><circle cx="12" cy="15" r="2.2" fill="currentColor"/></svg>',
  shield: '<svg viewBox="0 0 24 24" fill="none"><path d="M12 3l7 3v5c0 4.5-3 7.7-7 9-4-1.3-7-4.5-7-9V6l7-3Z" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/><path d="M9 12l2 2 4-4" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  scale: '<svg viewBox="0 0 24 24" fill="none"><path d="M6 12h12M6 12a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h1M18 12a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-1M8 6V4h8v2" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  pill: '<svg viewBox="0 0 24 24" fill="none"><rect x="3" y="9" width="18" height="6" rx="3" stroke="currentColor" stroke-width="1.8"/><path d="M12 9v6" stroke="currentColor" stroke-width="1.8"/></svg>',
  flag: '<svg viewBox="0 0 24 24" fill="none"><path d="M5 21V4M5 4h13l-3 4 3 4H5" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/></svg>',
  lock: '<svg viewBox="0 0 24 24" fill="none"><rect x="5" y="10" width="14" height="10" rx="2" stroke="currentColor" stroke-width="1.8"/><path d="M8 10V7a4 4 0 0 1 8 0v3" stroke="currentColor" stroke-width="1.8"/></svg>',
  up: '<svg viewBox="0 0 20 20" fill="none"><path d="M5 12l5-5 5 5" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  down: '<svg viewBox="0 0 20 20" fill="none"><path d="M5 8l5 5 5-5" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  enter: '<svg viewBox="0 0 20 20" fill="none"><path d="M4 10h11M11 6l4 4-4 4" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  empty: '<svg viewBox="0 0 24 24" fill="none"><rect x="4" y="7" width="16" height="12" rx="2" stroke="currentColor" stroke-width="1.6"/><path d="M9 7V5a3 3 0 0 1 6 0v2" stroke="currentColor" stroke-width="1.6"/></svg>',
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

function $(id) { return document.getElementById(id); }

function el(tag, className, html) {
  const e = document.createElement(tag);
  if (className) e.className = className;
  if (html !== undefined) e.innerHTML = html;
  return e;
}

async function getJSON(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error("HTTP " + res.status);
  return res.json();
}

async function postJSON(url, body, method) {
  const res = await fetch(url, {
    method: method || "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  let data = {};
  try { data = await res.json(); } catch (e) { /* no body */ }
  if (!res.ok) throw new Error(data.error || ("HTTP " + res.status));
  return data;
}

function formatUid(uid) {
  if (!uid) return "--";
  return uid.match(/.{1,2}/g).join(":");
}

function medicineInitials(name) {
  if (!name) return "?";
  const paren = name.match(/\(([^)]+)\)/);
  if (paren) return paren[1].slice(0, 4).toUpperCase();
  return name.replace(/[^A-Za-z]/g, "").slice(0, 3).toUpperCase() || "?";
}

function formatUptime(ms) {
  const s = Math.floor(ms / 1000);
  const h = String(Math.floor(s / 3600)).padStart(2, "0");
  const m = String(Math.floor((s % 3600) / 60)).padStart(2, "0");
  const sec = String(s % 60).padStart(2, "0");
  return h + ":" + m + ":" + sec;
}

// ---------------------------------------------------------------------------
// Theme (dashboard defaults to the dark telemetry look; light is opt-in)
// ---------------------------------------------------------------------------

function applyStoredTheme() {
  let stored = null;
  try { stored = localStorage.getItem("ampule-theme"); } catch (e) { /* private mode etc */ }
  if (stored === "light") document.documentElement.setAttribute("data-theme", "light");
}

function setupThemeToggle() {
  $("themeToggle").addEventListener("click", () => {
    const isLight = document.documentElement.getAttribute("data-theme") === "light";
    if (isLight) {
      document.documentElement.removeAttribute("data-theme");
      try { localStorage.setItem("ampule-theme", "dark"); } catch (e) { /* ignore */ }
    } else {
      document.documentElement.setAttribute("data-theme", "light");
      try { localStorage.setItem("ampule-theme", "light"); } catch (e) { /* ignore */ }
    }
  });
}

function tickBrowserClock() {
  const el2 = $("rbBrowserClock");
  if (el2) el2.textContent = "viewer: " + new Date().toLocaleTimeString();
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

function setupTabs() {
  document.querySelectorAll(".tab").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach((b) => b.classList.remove("active"));
      document.querySelectorAll(".tab-panel").forEach((p) => p.classList.remove("active"));
      btn.classList.add("active");
      $("tab-" + btn.dataset.tab).classList.add("active");
      if (btn.dataset.tab === "medicines") loadMedicines();
      if (btn.dataset.tab === "settings") { loadAmpules(); loadSettings(); }
    });
  });
}

// ---------------------------------------------------------------------------
// Process indicator — connected-node timeline with live sub-status per step
// ---------------------------------------------------------------------------

function renderProcessIndicator(status) {
  const container = $("processIndicator");
  container.innerHTML = "";
  const state = status.state;

  if (status.isError) {
    container.appendChild(el("div", "stepper-error", ICON.alert + " WORKFLOW HALTED &mdash; see Active Ampule card for details"));
    return;
  }

  const currentStep = STATE_TO_STEP.hasOwnProperty(state) ? STATE_TO_STEP[state] : 0;
  const fillPct = (currentStep / (STEP_META.length - 1)) * 100;

  container.appendChild(el("div", "stepper-track"));
  const fill = el("div", "stepper-track-fill");
  fill.style.width = "calc((100% - 68px) * " + (fillPct / 100) + ")";
  container.appendChild(fill);

  STEP_META.forEach((meta, i) => {
    const isDone = i < currentStep;
    const isActive = i === currentStep;
    const isLast = i === STEP_META.length - 1;

    let subText = meta.pending;
    if (isDone) {
      subText = meta.done || "Done";
    } else if (isActive) {
      subText = meta.active;
      if (meta.key === "WEIGHT_SELECTION" && status.weightSelected) subText = "Confirmed: " + status.weight;
      if (meta.key === "DOSE_DISPLAY" && status.doseCalculated) subText = "Demo dose: " + status.dose + " mg";
    }

    const node = el("div", "step-node");
    let circleContent;
    if (isDone) circleContent = ICON.check;
    else if (isActive) circleContent = ICON[meta.icon];
    else if (isLast) circleContent = ICON.lock;
    else circleContent = String(i + 1);

    const circle = el("div", "step-circle" + (isDone ? " done" : isActive ? " active" : ""), circleContent);
    const label = el("div", "step-label" + (isDone ? " done" : isActive ? " active" : ""), meta.label);
    const sub = el("div", "step-sub" + (isDone ? " done" : isActive ? " active" : ""), subText);
    node.appendChild(circle);
    node.appendChild(label);
    node.appendChild(sub);
    container.appendChild(node);
  });
}

// ---------------------------------------------------------------------------
// Hardware ribbon (all fields come straight from /api/system)
// ---------------------------------------------------------------------------

function setStatusDot(dotId, statusId, ok, okText, badText) {
  const dot = $(dotId);
  const status = $(statusId);
  dot.classList.remove("ok", "bad");
  dot.classList.add(ok ? "ok" : "bad");
  status.classList.remove("ok", "bad");
  status.classList.add(ok ? "ok" : "bad");
  status.lastChild.textContent = ok ? okText : badText;
}

async function pollSystem() {
  try {
    const sys = await getJSON("/api/system");

    $("fwVersionChip").textContent = "v" + sys.firmwareVersion;

    setStatusDot("dotEsp", "rbEspStatus", sys.espOnline, "ONLINE", "OFFLINE");
    $("rbEspValue").textContent = "UP " + formatUptime(sys.uptimeMs);
    $("rbEspSub").textContent = "fw v" + sys.firmwareVersion;

    setStatusDot("dotRfid", "rbRfidStatus", sys.rfidReady, "READY", "OFFLINE");
    setStatusDot("dotOled", "rbOledStatus", sys.oledReady, "READY", "OFFLINE");
    setStatusDot("dotDb", "rbDbStatus", sys.databaseReady, "READY", "OFFLINE");

    const dotTime = $("dotTime");
    const timeStatus = $("rbTimeStatus");
    const isNtp = sys.timeSource === "NTP";
    dotTime.classList.remove("ok", "warn");
    dotTime.classList.add(isNtp ? "ok" : "warn");
    timeStatus.classList.remove("ok", "warn");
    timeStatus.classList.add(isNtp ? "ok" : "warn");
    timeStatus.lastChild.textContent = isNtp ? "NTP" : "FALLBACK";
    $("rbTimeValue").textContent = isNtp ? "NTP SYNCED" : "BUILD FALLBACK";

    $("rbApSsid").textContent = sys.apSsid;
    $("rbApIp").textContent = sys.apIp;

    $("uptimeFoot").textContent = "uptime " + formatUptime(sys.uptimeMs);

    $("footer").textContent = sys.firmwareName + " v" + sys.firmwareVersion +
      " — Prototype. Not for clinical use. (" + sys.apSsid + " @ " + sys.apIp + ")";
  } catch (e) {
    setStatusDot("dotEsp", "rbEspStatus", false, "ONLINE", "UNREACHABLE");
  }
}

// ---------------------------------------------------------------------------
// Ampule card
// ---------------------------------------------------------------------------

function setLiveChip(id, mode, text) {
  const chip = $(id);
  chip.classList.remove("idle", "error");
  if (mode !== "live") chip.classList.add(mode);
  chip.innerHTML = (mode === "live" ? "<span class='blink-dot'></span>" : "") + text;
}

function renderAmpuleCard(status) {
  const card = $("ampuleCard");
  const wrap = $("ampuleCardWrap");
  card.innerHTML = "";

  if (status.isError) {
    setLiveChip("ampuleLiveChip", "error", status.errorTitle);
    $("ampuleFootRight").textContent = "STATE: " + status.state;

    const banner = el("div", "verify-banner bad");
    banner.innerHTML =
      "<div class='icon-wrap'>" + ICON.alert + "</div>" +
      "<div><div class='label'>" + status.errorTitle + "</div>" +
      "<div class='title'>Remove the ampule to continue</div>" +
      "<div class='sub'>" + status.errorMessage + "</div></div>";
    card.appendChild(banner);
    return;
  }

  if (!status.ampuleUid) {
    setLiveChip("ampuleLiveChip", "idle", "IDLE");
    $("ampuleFootRight").textContent = "STATE: " + status.state;

    const idle = el("div", "idle-panel");
    idle.innerHTML =
      "<div class='idle-icon'>" + ICON.empty + "</div>" +
      "<div class='idle-title'>Insert Ampule</div>" +
      "<p>Waiting for an RFID tag near the DFR0231-H reader.</p>";
    card.appendChild(idle);
    return;
  }

  setLiveChip("ampuleLiveChip", "live", "ACTIVE SESSION");
  $("ampuleFootRight").textContent = "STATE: " + status.state;

  const uidRow = el("div", "uid-row");
  uidRow.innerHTML =
    "<div class='left'><div class='icon-tile'>" + ICON.rfid + "</div><div>" +
    "<div class='label'>RFID Transponder UID</div><div class='value mono'>" + formatUid(status.ampuleUid) + "</div>" +
    "</div></div><div class='proto-chip'>ISO14443A</div>";
  card.appendChild(uidRow);

  const verified = status.ampuleVerified;
  const banner = el("div", "verify-banner " + (verified ? "ok" : "bad"));
  banner.innerHTML =
    "<div class='icon-wrap'>" + (verified ? ICON.check : ICON.cross) + "</div>" +
    "<div><div class='label'>Verification State</div>" +
    "<div class='title'>" + (verified ? "VERIFIED &middot; NOT EXPIRED &middot; NOT USED" : "VERIFICATION FAILED") + "</div>" +
    "<div class='sub'>Checked against local LittleFS ampule registry</div></div>";
  card.appendChild(banner);

  const panel = el("div", "medicine-panel");
  panel.innerHTML = "<div class='med-label'>Identified Medicine</div><div class='med-name'>" + (status.medicine || "—") + "</div>";
  card.appendChild(panel);

  const grid = el("div", "stat-grid");
  grid.innerHTML =
    "<div class='stat-pill'><span class='k'>Batch</span><span class='v'>" + (status.batch || "--") + "</span></div>" +
    "<div class='stat-pill'><span class='k'>Expiry</span><span class='v " + (status.expired ? "bad" : "ok") + "'>" + (status.expiry || "--") + "</span></div>" +
    "<div class='stat-pill'><span class='k'>Used</span><span class='v " + (status.used ? "bad" : "ok") + "'>" + (status.used ? "YES" : "NO") + "</span></div>";
  card.appendChild(grid);
}

// ---------------------------------------------------------------------------
// Weight / dose card
// ---------------------------------------------------------------------------

function ringSvg(idx) {
  const r = 32, c = 2 * Math.PI * r;
  const frac = idx >= 0 ? (idx + 1) / 4 : 0;
  const offset = c * (1 - frac);
  return (
    "<div class='ring-wrap'><svg viewBox='0 0 76 76'>" +
    "<circle class='ring-track' cx='38' cy='38' r='" + r + "' fill='none' stroke-width='7'/>" +
    "<circle class='ring-fill' cx='38' cy='38' r='" + r + "' fill='none' stroke-width='7' " +
    "stroke-dasharray='" + c.toFixed(1) + "' stroke-dashoffset='" + offset.toFixed(1) + "'/>" +
    "</svg><div class='ring-center'><span class='n'>" + (idx >= 0 ? (idx + 1) + "/4" : "--") + "</span><span class='l'>CATEGORY</span></div></div>"
  );
}

function renderCtaLabel(state) {
  if (state === "AMPULE_ACTIVE") return "ENTER → START WEIGHT SELECT";
  if (state === "WEIGHT_SELECTION") return "ENTER → CONFIRM WEIGHT";
  if (state === "DOSE_DISPLAY") return "ENTER → CONFIRM &amp; MARK USED";
  return "ENTER";
}

function renderWeightCard(status) {
  const card = $("weightCard");
  card.innerHTML = "";

  if (status.state === "COMPLETED") {
    setLiveChip("weightLiveChip", "idle", "COMPLETE");
    const panel = el("div", "completed-panel");
    panel.innerHTML =
      "<div class='icon'>" + ICON.check + "</div>" +
      "<div class='title'>Ampule Used</div>" +
      "<div class='dose mono'>" + status.dose + " mg &middot; " + status.weight + "</div>" +
      "<p>Remove the ampule to return to standby.</p>";
    card.appendChild(panel);
    return;
  }

  if (!status.ampuleVerified || status.isError) {
    setLiveChip("weightLiveChip", "idle", "4 CATEGORIES");
    const idle = el("div", "idle-panel");
    idle.innerHTML = "<div class='idle-icon'>" + ICON.scale + "</div><div class='idle-title'>Standby</div><p>Verify an ampule to begin weight selection.</p>";
    card.appendChild(idle);
    return;
  }

  const cursorIdx = status.weightSelected ? status.weightIndex : (status.state === "WEIGHT_SELECTION" ? status.weightCursor : -1);
  setLiveChip("weightLiveChip", "live", status.state === "WEIGHT_SELECTION" ? "SELECTING" : "READY");

  const readout = el("div", "weight-readout-row");
  readout.innerHTML =
    ringSvg(cursorIdx) +
    "<div class='info'><div class='k'>Current Selection</div><div class='v mono'>" +
    (cursorIdx >= 0 ? WEIGHT_LABELS[cursorIdx] : "—") +
    "<span class='unit'>" + (status.weightSelected ? "CONFIRMED" : status.state === "WEIGHT_SELECTION" ? "CURSOR" : "") + "</span></div></div>";
  card.appendChild(readout);

  const grid = el("div", "weight-grid");
  WEIGHT_LABELS.forEach((label, i) => {
    const isConfirmed = status.weightSelected && status.weightIndex === i;
    const isCursor = !status.weightSelected && status.state === "WEIGHT_SELECTION" && status.weightCursor === i;
    const opt = el("div", "weight-option" + (isConfirmed ? " selected" : isCursor ? " cursor" : ""));
    opt.innerHTML =
      (isConfirmed ? "<span class='check'>" + ICON.check + "</span>" : "") +
      "<div class='n'>" + (i + 1) + "/4</div><div class='lbl'>" + label + "</div>";
    grid.appendChild(opt);
  });
  card.appendChild(grid);

  if (status.doseCalculated) {
    const dose = el("div", "dose-panel");
    dose.innerHTML =
      "<div class='k'>Demo Dose Lookup</div>" +
      "<div class='n'>" + status.dose + "<span class='unit'>mg</span></div>" +
      "<div class='demo-ribbon'>" + ICON.alert + " DEMO VALUE &mdash; NOT MEDICAL ADVICE</div>";
    card.appendChild(dose);
  }

  card.appendChild(el("p", "control-hint",
    "Physical UP / DOWN / ENTER on the device drive this. Buttons below mirror them for testing — routed through the same firmware state machine."));

  const controls = el("div", "action-row");
  controls.innerHTML =
    "<button class='btn' id='btnUp'>" + ICON.up + " UP</button>" +
    "<button class='btn' id='btnDown'>" + ICON.down + " DOWN</button>" +
    "<button class='btn primary' id='btnEnter'>" + renderCtaLabel(status.state) + "</button>";
  card.appendChild(controls);

  $("btnUp").addEventListener("click", () => postJSON("/api/control", { action: "up" }).catch(() => {}));
  $("btnDown").addEventListener("click", () => postJSON("/api/control", { action: "down" }).catch(() => {}));
  $("btnEnter").addEventListener("click", () => postJSON("/api/control", { action: "enter" }).catch(() => {}));
}

async function pollStatus() {
  try {
    const status = await getJSON("/api/status");
    renderProcessIndicator(status);
    renderAmpuleCard(status);
    renderWeightCard(status);
    $("dotRfid").classList.toggle("busy", status.state === "RFID_SCANNING");
  } catch (e) {
    // ESP32 momentarily unreachable — keep last rendered state, dot handled by pollSystem
  }
}

// ---------------------------------------------------------------------------
// Activity log (dashboard terminal) + full history (history tab)
// ---------------------------------------------------------------------------

function terminalTag(statusText) {
  const s = (statusText || "").toLowerCase();
  if (s.indexOf("rejected") !== -1) return ["ERROR", "error"];
  if (s.indexOf("verified") !== -1) return ["VERIFY", "verify"];
  if (s.indexOf("weight selected") !== -1) return ["WEIGHT", "weight"];
  if (s.indexOf("dose displayed") !== -1) return ["DOSE", "dose"];
  if (s.indexOf("marked used") !== -1) return ["SYSTEM", "system"];
  return ["EVENT", "system"];
}

function renderLogList(container, entries, limit) {
  container.innerHTML = "";
  if (!entries.length) {
    container.appendChild(el("li", "empty-note", "No activity yet."));
    return;
  }
  entries.slice(0, limit).forEach((h) => {
    const [tagText, tagClass] = terminalTag(h.status);
    const li = el("li", "terminal-row");
    li.innerHTML =
      "<span class='t-time'>" + h.time + "</span>" +
      "<span class='t-tag " + tagClass + "'>" + tagText + "</span>" +
      "<span class='t-msg'>" + h.status +
      (h.medicine && h.medicine !== "-" ? " <span class='med'>&mdash; " + h.medicine + "</span>" : "") + "</span>";
    container.appendChild(li);
  });
  $("logCountFoot").textContent = entries.length + " / 50 entries";
}

function renderHistoryTable(entries) {
  const body = $("historyBody");
  body.innerHTML = "";
  if (!entries.length) {
    body.appendChild(el("tr", null, "<td colspan='6' class='empty-note'>No history yet.</td>"));
    return;
  }
  entries.forEach((h) => {
    const tr = el("tr");
    tr.innerHTML =
      "<td>" + h.time + "</td><td class='uid'>" + formatUid(h.uid) + "</td><td>" + h.medicine +
      "</td><td>" + h.weight + "</td><td>" + (h.dose ? h.dose + " mg" : "--") +
      "</td><td>" + h.status + "</td>";
    body.appendChild(tr);
  });
}

async function pollHistory() {
  try {
    const entries = await getJSON("/api/history");
    const sig = entries.length + ":" + (entries[0] ? entries[0].time + entries[0].status : "");
    if (sig === lastHistorySignature) return;
    lastHistorySignature = sig;
    renderLogList($("activityLog"), entries, 14);
    renderHistoryTable(entries);
  } catch (e) { /* ignore, retry next tick */ }
}

// ---------------------------------------------------------------------------
// Medicines / dose configuration
// ---------------------------------------------------------------------------

function medicineFormHtml(m) {
  return (
    "<div class='medicine-form-head'>" +
    "<div class='mini-badge'>" + medicineInitials(m.name) + "</div><h3>" + m.name + "</h3>" +
    "</div>" +
    "<div class='demo-ribbon'>" + ICON.alert + " DEMO CONFIGURATION</div>" +
    "<div class='form-grid' style='margin-top:12px'>" +
    field("Under 40 kg (mg)", m.id + "_under40", m.under40) +
    field("41-60 kg (mg)", m.id + "_kg41to60", m.kg41to60) +
    field("61-80 kg (mg)", m.id + "_kg61to80", m.kg61to80) +
    field("81 kg+ (mg)", m.id + "_over81", m.over81) +
    "</div>" +
    "<div class='form-grid'>" +
    "<div><label>Admin PIN</label><input type='password' id='" + m.id + "_pin' required></div>" +
    "</div>" +
    "<button class='btn primary' type='submit'>Save</button>" +
    "<div class='form-msg' id='" + m.id + "_msg'></div>"
  );
}

function field(label, id, value) {
  return "<div><label for='" + id + "'>" + label + "</label>" +
    "<input type='number' min='0' step='1' id='" + id + "' value='" + value + "' required></div>";
}

async function loadMedicines() {
  try {
    medicinesCache = await getJSON("/api/medicines");
  } catch (e) {
    $("medicineForms").innerHTML = "<p class='empty-note'>Unable to load medicines.</p>";
    return;
  }

  const container = $("medicineForms");
  container.innerHTML = "";

  medicinesCache.forEach((m) => {
    const form = el("form", "card medicine-form", "<div class='card-body'>" + medicineFormHtml(m) + "</div>");
    form.addEventListener("submit", (ev) => {
      ev.preventDefault();
      saveMedicine(m.id);
    });
    container.appendChild(form);
  });

  const select = $("newMedicine");
  if (select) {
    select.innerHTML = "";
    medicinesCache.forEach((m) => {
      const opt = el("option", null, m.name);
      opt.value = m.id;
      select.appendChild(opt);
    });
  }
}

async function saveMedicine(id) {
  const msg = $(id + "_msg");
  const body = {
    id: id,
    under40: Number($(id + "_under40").value),
    kg41to60: Number($(id + "_kg41to60").value),
    kg61to80: Number($(id + "_kg61to80").value),
    over81: Number($(id + "_over81").value),
    pin: $(id + "_pin").value,
  };

  if ([body.under40, body.kg41to60, body.kg61to80, body.over81].some((v) => isNaN(v) || v < 0)) {
    msg.textContent = "Dose values must be non-negative numbers.";
    msg.className = "form-msg bad";
    return;
  }

  try {
    await postJSON("/api/medicines", body);
    msg.textContent = "Saved.";
    msg.className = "form-msg ok";
  } catch (e) {
    msg.textContent = e.message;
    msg.className = "form-msg bad";
  }
}

// ---------------------------------------------------------------------------
// Ampule management (settings tab)
// ---------------------------------------------------------------------------

async function loadAmpules() {
  let ampules;
  try {
    ampules = await getJSON("/api/ampules");
  } catch (e) {
    $("ampuleBody").innerHTML = "<tr><td colspan='6' class='empty-note'>Unable to load.</td></tr>";
    return;
  }

  const body = $("ampuleBody");
  body.innerHTML = "";
  if (!ampules.length) {
    body.appendChild(el("tr", null, "<td colspan='6' class='empty-note'>No ampules registered.</td></tr>"));
    return;
  }

  ampules.forEach((a) => {
    const tr = el("tr");
    tr.innerHTML =
      "<td class='uid'>" + formatUid(a.uid) + "</td><td>" + a.medicineName + "</td><td>" + a.batch +
      "</td><td>" + a.expiry + "</td>" +
      "<td><span class='badge " + (a.used ? "bad" : "ok") + "'>" + (a.used ? "USED" : "AVAILABLE") + "</span></td>" +
      "<td></td>";
    const actions = tr.lastElementChild;

    const resetBtn = el("button", "btn small", "Reset Used");
    resetBtn.addEventListener("click", () => ampuleAction("reset", a.uid));
    const delBtn = el("button", "btn small danger", "Delete");
    delBtn.addEventListener("click", () => ampuleAction("delete", a.uid));
    actions.appendChild(resetBtn);
    actions.appendChild(document.createTextNode(" "));
    actions.appendChild(delBtn);

    body.appendChild(tr);
  });
}

async function ampuleAction(action, uid) {
  const pin = prompt("Enter admin PIN to " + (action === "delete" ? "delete" : "reset") + " ampule " + formatUid(uid) + ":");
  if (pin === null) return;
  try {
    if (action === "delete") {
      await postJSON("/api/ampules?uid=" + encodeURIComponent(uid), { pin: pin }, "DELETE");
    } else {
      await postJSON("/api/reset-used", { uid: uid, pin: pin });
    }
    loadAmpules();
  } catch (e) {
    alert(e.message);
  }
}

function setupAddAmpuleForm() {
  $("addAmpuleForm").addEventListener("submit", async (ev) => {
    ev.preventDefault();
    const msg = $("addAmpuleMsg");
    const body = {
      uid: $("newUid").value.trim(),
      medicineId: $("newMedicine").value,
      batch: $("newBatch").value.trim(),
      expiry: $("newExpiry").value,
      pin: $("newPinAdd").value,
    };
    try {
      await postJSON("/api/ampules", body);
      msg.textContent = "Ampule registered.";
      msg.className = "form-msg ok";
      ev.target.reset();
      loadAmpules();
    } catch (e) {
      msg.textContent = e.message;
      msg.className = "form-msg bad";
    }
  });
}

// ---------------------------------------------------------------------------
// Settings (Wi-Fi / admin PIN)
// ---------------------------------------------------------------------------

async function loadSettings() {
  try {
    const cfg = await getJSON("/api/settings");
    $("apSsid").value = cfg.apSsid || "";
    $("staSsid").value = cfg.staSsid || "";
  } catch (e) { /* ignore */ }
}

function setupSettingsForm() {
  $("settingsForm").addEventListener("submit", async (ev) => {
    ev.preventDefault();
    const msg = $("settingsMsg");
    const body = { pin: $("settingsPin").value };
    if ($("apSsid").value) body.apSsid = $("apSsid").value;
    if ($("apPassword").value) body.apPassword = $("apPassword").value;
    if ($("staSsid").value !== undefined) body.staSsid = $("staSsid").value;
    if ($("staPassword").value) body.staPassword = $("staPassword").value;
    if ($("newAdminPin").value) body.newPin = $("newAdminPin").value;

    try {
      const res = await postJSON("/api/settings", body);
      msg.textContent = res.note || "Saved.";
      msg.className = "form-msg ok";
      $("settingsPin").value = "";
      $("apPassword").value = "";
      $("staPassword").value = "";
      $("newAdminPin").value = "";
    } catch (e) {
      msg.textContent = e.message;
      msg.className = "form-msg bad";
    }
  });
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------

document.addEventListener("DOMContentLoaded", () => {
  applyStoredTheme();
  setupThemeToggle();
  setupTabs();
  setupAddAmpuleForm();
  setupSettingsForm();

  pollStatus();
  pollSystem();
  pollHistory();
  loadMedicines();
  tickBrowserClock();

  setInterval(pollStatus, STATUS_POLL_MS);
  setInterval(pollSystem, SYSTEM_POLL_MS);
  setInterval(pollHistory, HISTORY_POLL_MS);
  setInterval(tickBrowserClock, 1000);
});
