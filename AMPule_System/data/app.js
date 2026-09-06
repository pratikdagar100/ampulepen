// AMPule System dashboard — vanilla JS, no external dependencies.
// Polls the ESP32's REST API and mirrors OLED/physical device state.

const STATUS_POLL_MS = 750;
const SYSTEM_POLL_MS = 3000;
const HISTORY_POLL_MS = 2000;

const STEPS = [
  { key: "INSERT", label: "1. INSERT AMPULE" },
  { key: "RFID", label: "2. RFID" },
  { key: "VERIFY", label: "3. VERIFY" },
  { key: "WEIGHT", label: "4. WEIGHT" },
  { key: "DOSE", label: "5. DOSE" },
  { key: "COMPLETE", label: "6. COMPLETE" },
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

const WEIGHT_LABELS = ["UNDER 40 kg", "41-60 kg", "61-80 kg", "81 kg+"];

let lastHistorySignature = "";
let medicinesCache = [];

// Small inline icons — no icon font/image files, keeps LittleFS payload tiny.
const ICON = {
  check: '<svg class="ic" viewBox="0 0 20 20" fill="none"><path d="M4.5 10.5l3.5 3.5L15.5 6" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  cross: '<svg class="ic" viewBox="0 0 20 20" fill="none"><path d="M6 6l8 8M14 6l-8 8" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"/></svg>',
  alert: '<svg class="ic" viewBox="0 0 24 24" fill="none"><path d="M12 9v4M12 16.5h.01" stroke="currentColor" stroke-width="2" stroke-linecap="round"/><path d="M10.3 3.9 2.5 17a1.8 1.8 0 0 0 1.6 2.7h15.8a1.8 1.8 0 0 0 1.6-2.7L13.7 3.9a1.8 1.8 0 0 0-3.4 0Z" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/></svg>',
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
  return uid.match(/.{1,2}/g).join(" ");
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
// Process indicator
// ---------------------------------------------------------------------------

function renderProcessIndicator(state) {
  const container = $("processIndicator");
  container.innerHTML = "";
  const isError = state.indexOf("ERROR") === 0;
  const currentStep = STATE_TO_STEP.hasOwnProperty(state) ? STATE_TO_STEP[state] : -1;

  STEPS.forEach((step, i) => {
    const isDone = !isError && i < currentStep;
    const isActive = !isError && i === currentStep;
    const marker = isDone ? ICON.check : isActive ? "<span class='step-dot'></span>" : "";
    const div = el("div", "step", marker + "<span class='step-text'>" + step.label + "</span>");
    if (isDone) div.classList.add("done");
    if (isActive) div.classList.add("active");
    container.appendChild(div);
  });
}

// ---------------------------------------------------------------------------
// Status indicators
// ---------------------------------------------------------------------------

function setDot(id, ok) {
  const dot = $(id);
  dot.classList.remove("ok", "bad");
  dot.classList.add(ok ? "ok" : "bad");
}

async function pollSystem() {
  try {
    const sys = await getJSON("/api/system");
    setDot("dotEsp", sys.espOnline);
    setDot("dotRfid", sys.rfidReady);
    setDot("dotOled", sys.oledReady);
    setDot("dotDb", sys.databaseReady);
    $("timeSourceLabel").textContent = "TIME: " + (sys.timeSource === "NTP" ? "NTP SYNCED" : "BUILD FALLBACK");
    $("footer").textContent = sys.firmwareName + " v" + sys.firmwareVersion +
      " — Prototype. Not for clinical use. (" + sys.apSsid + " @ " + sys.apIp + ")";
  } catch (e) {
    setDot("dotEsp", false);
  }
}

// ---------------------------------------------------------------------------
// Ampule card + weight/dose card
// ---------------------------------------------------------------------------

function renderAmpuleCard(status) {
  const card = $("ampuleCard");
  card.innerHTML = "";

  if (status.isError) {
    const banner = el("div", "error-banner");
    banner.innerHTML = "<div class='title'>" + ICON.alert + " " + status.errorTitle + "</div><div class='msg'>" + status.errorMessage + "</div>";
    card.appendChild(banner);
    return;
  }

  if (!status.ampuleUid) {
    card.appendChild(el("p", "medicine-name placeholder", "INSERT AMPULE"));
    card.appendChild(el("p", "empty-note", "Waiting for an RFID tag to be scanned."));
    return;
  }

  card.appendChild(el("h3", "medicine-name", status.medicine || "Identifying…"));

  const rows = [
    ["RFID UID", formatUid(status.ampuleUid)],
    ["Batch", status.batch || "--"],
    ["Expiry", status.expiry || "--"],
  ];
  rows.forEach(([label, value]) => {
    const row = el("div", "field-row");
    row.innerHTML = "<span class='field-label'>" + label + "</span><span class='field-value'>" + value + "</span>";
    card.appendChild(row);
  });

  const checkRow = (ok, label) =>
    "<li class='" + (ok ? "check-ok" : "check-bad") + "'>" + (ok ? ICON.check : ICON.cross) + " " + label + "</li>";
  const list = el("ul", "checklist");
  list.innerHTML =
    checkRow(status.ampuleVerified, "VERIFIED") +
    checkRow(!status.expired, "NOT EXPIRED") +
    checkRow(!status.used, "NOT USED");
  card.appendChild(list);
}

function renderWeightCard(status) {
  const card = $("weightCard");
  card.innerHTML = "";

  if (!status.ampuleVerified || status.isError) {
    card.appendChild(el("p", "empty-note", "Verify an ampule to begin weight selection."));
    return;
  }

  const grid = el("div", "weight-grid");
  WEIGHT_LABELS.forEach((label, i) => {
    const selected = status.weightSelected && status.weightIndex === i;
    const opt = el("div", "weight-option" + (selected ? " selected" : ""),
      (selected ? "<span class='wt-check'>" + ICON.check + "</span>" : "") + label);
    grid.appendChild(opt);
  });
  card.appendChild(grid);

  if (status.doseCalculated) {
    const dose = el("div", "dose-display");
    dose.innerHTML =
      "<div class='dose-value'>" + status.dose + " <span class='dose-unit'>mg</span></div>" +
      "<div class='demo-tag'>" + ICON.alert + " DEMO VALUE</div>";
    card.appendChild(dose);
  }

  card.appendChild(el("p", "control-hint",
    "Physical UP / DOWN / ENTER buttons on the device are primary. The buttons below mirror them for testing from the dashboard."));

  const controls = el("div", "virtual-controls");
  controls.innerHTML =
    "<button class='btn' id='btnUp'>▲ UP</button>" +
    "<button class='btn' id='btnDown'>▼ DOWN</button>" +
    "<button class='btn primary' id='btnEnter'>ENTER →</button>";
  card.appendChild(controls);

  $("btnUp").addEventListener("click", () => postJSON("/api/control", { action: "up" }).catch(() => {}));
  $("btnDown").addEventListener("click", () => postJSON("/api/control", { action: "down" }).catch(() => {}));
  $("btnEnter").addEventListener("click", () => postJSON("/api/control", { action: "enter" }).catch(() => {}));
}

async function pollStatus() {
  try {
    const status = await getJSON("/api/status");
    renderProcessIndicator(status.state);
    renderAmpuleCard(status);
    renderWeightCard(status);
  } catch (e) {
    // ESP32 momentarily unreachable — keep last rendered state, dot handled by pollSystem
  }
}

// ---------------------------------------------------------------------------
// Activity log (dashboard tab) + full history (history tab)
// ---------------------------------------------------------------------------

function logDotClass(status) {
  const s = (status || "").toLowerCase();
  if (s.indexOf("rejected") !== -1) return "bad";
  if (s.indexOf("verified") !== -1 || s.indexOf("used") !== -1) return "ok";
  return "";
}

function renderLogList(container, entries, limit) {
  container.innerHTML = "";
  if (!entries.length) {
    container.appendChild(el("li", "empty-note", "No activity yet."));
    return;
  }
  entries.slice(0, limit).forEach((h) => {
    const li = el("li");
    li.innerHTML =
      "<span class='log-dot " + logDotClass(h.status) + "'></span>" +
      "<span class='log-time'>" + h.time + "</span><span>" +
      h.status + (h.medicine && h.medicine !== "-" ? " &mdash; " + h.medicine : "") + "</span>";
    container.appendChild(li);
  });
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
      "<td>" + h.time + "</td><td>" + formatUid(h.uid) + "</td><td>" + h.medicine +
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
    renderLogList($("activityLog"), entries, 12);
    renderHistoryTable(entries);
  } catch (e) { /* ignore, retry next tick */ }
}

// ---------------------------------------------------------------------------
// Medicines / dose configuration
// ---------------------------------------------------------------------------

function medicineFormHtml(m) {
  return (
    "<h3>" + m.name + "</h3>" +
    "<div class='demo-tag'>DEMO CONFIGURATION</div>" +
    "<div class='form-grid'>" +
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
    const form = el("form", "card medicine-form", medicineFormHtml(m));
    form.addEventListener("submit", (ev) => {
      ev.preventDefault();
      saveMedicine(m.id);
    });
    container.appendChild(form);
  });

  // Also refresh the "add ampule" medicine dropdown while we have the list.
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
      "<td>" + formatUid(a.uid) + "</td><td>" + a.medicineName + "</td><td>" + a.batch +
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
  setupTabs();
  setupAddAmpuleForm();
  setupSettingsForm();

  pollStatus();
  pollSystem();
  pollHistory();
  loadMedicines();

  setInterval(pollStatus, STATUS_POLL_MS);
  setInterval(pollSystem, SYSTEM_POLL_MS);
  setInterval(pollHistory, HISTORY_POLL_MS);
});
