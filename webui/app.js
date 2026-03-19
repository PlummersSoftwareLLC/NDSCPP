const STORAGE_KEY = "ndscpp.navigator.settings.v1";
const DEFAULT_REFRESH_MS = 2000;
const DEFAULT_PORT = 49152;
const QUEUE_VISUAL_MAX = 25;
const DAY_OPTIONS = [
  { label: "Sun", mask: 0x01 },
  { label: "Mon", mask: 0x02 },
  { label: "Tue", mask: 0x04 },
  { label: "Wed", mask: 0x08 },
  { label: "Thu", mask: 0x10 },
  { label: "Fri", mask: 0x20 },
  { label: "Sat", mask: 0x40 },
];
const NESTED_PRIMARY_COLUMN_WIDTH = "3%";
const FEATURE_TABLE_COLUMNS = [
  { label: "Name", width: "8%" },
  { label: "Host", width: "11%" },
  { label: "Port", width: "6%", align: "right" },
  { label: "Width", width: "4%", align: "right" },
  { label: "Height", width: "4%", align: "right" },
  { label: "Offset X", width: "7%", align: "right" },
  { label: "Offset Y", width: "7%", align: "right" },
  { label: "Channel", width: "6%", align: "right" },
  { label: "Queue", width: "17%" },
  { label: "Buffer", width: "18%" },
  { label: "Status", width: "9%" },
];
const EFFECT_TABLE_COLUMNS = [
  { label: "Name", width: "12%" },
  { label: "Type", width: "12%" },
  { label: "Summary", width: "53%" },
  { label: "Schedule", width: "20%" },
];

const state = {
  controller: null,
  rows: [],
  canvases: [],
  effectCatalog: [],
  filter: "",
  sortKey: "name",
  sortDirection: "asc",
  refreshMs: DEFAULT_REFRESH_MS,
  autoRefresh: true,
  refreshTimer: null,
  requestInFlight: false,
  lastUpdatedAt: null,
  connected: false,
  dirty: false,
  expandedCanvasIds: new Set(),
  sectionExpansions: {},
  selectedFeatures: {},  // { canvasId: Set<featureId> }
  selectedEffects: {},   // { canvasId: Set<effectIndex> }
  selectedCanvases: new Set(),
  dialogs: {
    canvasMode: "create",
    canvasId: null,
    featureMode: "create",
    featureCanvasId: null,
    featureId: null,
    effectMode: "create",
    effectCanvasId: null,
    effectIndex: null,
  },
};

const elements = {};

document.addEventListener("DOMContentLoaded", async () => {
  bindElements();
  loadSettings();
  buildScheduleDayInputs();
  bindEvents();
  syncControls();

  try {
    await ensureEffectCatalog();
  } catch (error) {
    console.error(error);
  }

  refreshLoop(true);
});

function bindElements() {
  elements.filterInput = document.getElementById("filterInput");
  elements.refreshIntervalButton = document.getElementById("refreshIntervalButton");
  elements.refreshDropdownToggle = document.getElementById("refreshDropdownToggle");
  elements.refreshDropdownMenu = document.getElementById("refreshDropdownMenu");
  document.body.appendChild(elements.refreshDropdownMenu);
  elements.autoRefreshToggle = document.getElementById("autoRefreshToggle");
  elements.refreshButton = document.getElementById("refreshButton");
  elements.startAllButton = document.getElementById("startAllButton");
  elements.stopAllButton = document.getElementById("stopAllButton");
  elements.newConfigButton = document.getElementById("newConfigButton");
  elements.loadConfigButton = document.getElementById("loadConfigButton");
  elements.loadDropdownToggle = document.getElementById("loadDropdownToggle");
  elements.loadDropdownMenu = document.getElementById("loadDropdownMenu");
  document.body.appendChild(elements.loadDropdownMenu);
  elements.saveConfigButton = document.getElementById("saveConfigButton");
  elements.configFileInput = document.getElementById("configFileInput");

  elements.collisionDialog = document.getElementById("collisionDialog");
  elements.collisionDialogTitle = document.getElementById("collisionDialogTitle");
  elements.collisionDialogCopy = document.getElementById("collisionDialogCopy");
  elements.collisionRepeatCheckbox = document.getElementById("collisionRepeatCheckbox");
  elements.collisionYesButton = document.getElementById("collisionYesButton");
  elements.collisionNoButton = document.getElementById("collisionNoButton");


  elements.connectionStatus = document.getElementById("connectionStatus");
  elements.connectionStatusText = document.getElementById("connectionStatusText");
  elements.apiPortValue = document.getElementById("apiPortValue");
  elements.webPortValue = document.getElementById("webPortValue");

  elements.tableMetaText = document.getElementById("tableMetaText");
  elements.canvasTableBody = document.getElementById("canvasTableBody");
  elements.canvasFolderTab = document.getElementById("canvasFolderTab");
  elements.sortButtons = Array.from(document.querySelectorAll(".sort-button"));

  elements.summaryCanvases = document.getElementById("summaryCanvases");
  elements.summaryCanvasesNote = document.getElementById("summaryCanvasesNote");
  elements.summaryFeatures = document.getElementById("summaryFeatures");
  elements.summaryFeaturesNote = document.getElementById("summaryFeaturesNote");
  elements.summaryOnline = document.getElementById("summaryOnline");
  elements.summaryOnlineNote = document.getElementById("summaryOnlineNote");
  elements.summaryFps = document.getElementById("summaryFps");
  elements.summaryFpsNote = document.getElementById("summaryFpsNote");
  elements.summaryBandwidth = document.getElementById("summaryBandwidth");
  elements.summaryBandwidthNote = document.getElementById("summaryBandwidthNote");
  elements.summaryQueue = document.getElementById("summaryQueue");
  elements.summaryQueueNote = document.getElementById("summaryQueueNote");
  elements.summaryDelta = document.getElementById("summaryDelta");
  elements.summaryDeltaNote = document.getElementById("summaryDeltaNote");
  elements.summaryEffects = document.getElementById("summaryEffects");
  elements.summaryEffectsNote = document.getElementById("summaryEffectsNote");

  elements.canvasDialog = document.getElementById("canvasDialog");
  elements.canvasForm = document.getElementById("canvasForm");
  elements.canvasDialogEyebrow = document.getElementById("canvasDialogEyebrow");
  elements.canvasDialogTitle = document.getElementById("canvasDialogTitle");
  elements.canvasDialogCopy = document.getElementById("canvasDialogCopy");
  elements.canvasDialogMessage = document.getElementById("canvasDialogMessage");
  elements.canvasDeleteButton = document.getElementById("canvasDeleteButton");
  elements.canvasNameInput = document.getElementById("canvasNameInput");
  elements.canvasWidthInput = document.getElementById("canvasWidthInput");
  elements.canvasHeightInput = document.getElementById("canvasHeightInput");
  elements.canvasFpsInput = document.getElementById("canvasFpsInput");
  elements.canvasRunningInput = document.getElementById("canvasRunningInput");
  elements.canvasSeedFeatureSection = document.getElementById("canvasSeedFeatureSection");
  elements.canvasSeedFeatureEnabled = document.getElementById("canvasSeedFeatureEnabled");
  elements.seedFeatureFriendlyNameInput = document.getElementById("seedFeatureFriendlyNameInput");
  elements.seedFeatureHostNameInput = document.getElementById("seedFeatureHostNameInput");
  elements.seedFeaturePortInput = document.getElementById("seedFeaturePortInput");
  elements.seedFeatureWidthInput = document.getElementById("seedFeatureWidthInput");
  elements.seedFeatureHeightInput = document.getElementById("seedFeatureHeightInput");
  elements.seedFeatureOffsetXInput = document.getElementById("seedFeatureOffsetXInput");
  elements.seedFeatureOffsetYInput = document.getElementById("seedFeatureOffsetYInput");
  elements.seedFeatureChannelInput = document.getElementById("seedFeatureChannelInput");
  elements.seedFeatureClientBufferCountInput = document.getElementById("seedFeatureClientBufferCountInput");
  elements.seedFeatureReversedInput = document.getElementById("seedFeatureReversedInput");
  elements.seedFeatureRedGreenSwapInput = document.getElementById("seedFeatureRedGreenSwapInput");

  elements.featureDialog = document.getElementById("featureDialog");
  elements.featureForm = document.getElementById("featureForm");
  elements.featureDialogEyebrow = document.getElementById("featureDialogEyebrow");
  elements.featureDialogTitle = document.getElementById("featureDialogTitle");
  elements.featureDialogCopy = document.getElementById("featureDialogCopy");
  elements.featureDialogMessage = document.getElementById("featureDialogMessage");
  elements.featureDeleteButton = document.getElementById("featureDeleteButton");
  elements.featureFriendlyNameInput = document.getElementById("featureFriendlyNameInput");
  elements.featureHostNameInput = document.getElementById("featureHostNameInput");
  elements.featurePortInput = document.getElementById("featurePortInput");
  elements.featureWidthInput = document.getElementById("featureWidthInput");
  elements.featureHeightInput = document.getElementById("featureHeightInput");
  elements.featureOffsetXInput = document.getElementById("featureOffsetXInput");
  elements.featureOffsetYInput = document.getElementById("featureOffsetYInput");
  elements.featureChannelInput = document.getElementById("featureChannelInput");
  elements.featureClientBufferCountInput = document.getElementById("featureClientBufferCountInput");
  elements.featureReversedInput = document.getElementById("featureReversedInput");
  elements.featureRedGreenSwapInput = document.getElementById("featureRedGreenSwapInput");

  elements.effectDialog = document.getElementById("effectDialog");
  elements.effectForm = document.getElementById("effectForm");
  elements.effectDialogEyebrow = document.getElementById("effectDialogEyebrow");
  elements.effectDialogTitle = document.getElementById("effectDialogTitle");
  elements.effectDialogCopy = document.getElementById("effectDialogCopy");
  elements.effectDialogMessage = document.getElementById("effectDialogMessage");
  elements.effectDeleteButton = document.getElementById("effectDeleteButton");
  elements.effectTypeInput = document.getElementById("effectTypeInput");
  elements.effectNameInput = document.getElementById("effectNameInput");
  elements.effectFields = document.getElementById("effectFields");
  elements.effectScheduleDays = document.getElementById("effectScheduleDays");
  elements.effectScheduleStartTimeInput = document.getElementById("effectScheduleStartTimeInput");
  elements.effectScheduleStopTimeInput = document.getElementById("effectScheduleStopTimeInput");
  elements.effectScheduleStartDateInput = document.getElementById("effectScheduleStartDateInput");
  elements.effectScheduleStopDateInput = document.getElementById("effectScheduleStopDateInput");
}

function bindEvents() {
  elements.filterInput.addEventListener("input", (event) => {
    state.filter = event.target.value || "";
    saveSettings();
    render();
  });

  elements.refreshDropdownToggle.addEventListener("click", (e) => {
    e.stopPropagation();
    toggleRefreshDropdown();
  });
  elements.refreshDropdownMenu.addEventListener("click", (e) => {
    e.stopPropagation();
    handleRefreshDropdownClick(e);
  });

  elements.autoRefreshToggle.addEventListener("change", (event) => {
    state.autoRefresh = event.target.checked;
    saveSettings();
    restartRefreshTimer();
  });

  elements.refreshButton.addEventListener("click", () => refreshLoop(true));
  elements.startAllButton.addEventListener("click", () => postCanvasAction("/api/canvases/start"));
  elements.stopAllButton.addEventListener("click", () => postCanvasAction("/api/canvases/stop"));
  elements.newConfigButton.addEventListener("click", handleNewConfig);
  elements.loadConfigButton.addEventListener("click", handleLoadConfig);
  elements.loadDropdownToggle.addEventListener("click", (e) => { e.stopPropagation(); toggleLoadDropdown(); });
  elements.loadDropdownMenu.addEventListener("click", (e) => { e.stopPropagation(); handleLoadDropdownClick(e); });
  elements.saveConfigButton.addEventListener("click", handleSaveConfig);
  elements.configFileInput.addEventListener("change", handleConfigFileSelected);
  document.addEventListener("click", () => {
    elements.loadDropdownMenu.classList.remove('open');
    elements.refreshDropdownMenu.classList.remove('open');
  });

  document.addEventListener("keydown", (e) => {
    if ((e.metaKey || e.ctrlKey) && e.key === "a") {
      const active = document.activeElement;
      if (active && (active.tagName === "INPUT" || active.tagName === "TEXTAREA" || active.isContentEditable)) return;
      e.preventDefault();
      if (state.selectedCanvases.size === state.canvases.length && state.canvases.length > 0) {
        state.selectedCanvases.clear();
      } else {
        state.canvases.forEach((c) => state.selectedCanvases.add(c.id));
      }
      render();
    }
  });


  elements.sortButtons.forEach((button) => {
    button.addEventListener("click", () => {
      const nextKey = button.dataset.sortKey;
      if (!nextKey) {
        return;
      }

      if (state.sortKey === nextKey) {
        state.sortDirection = state.sortDirection === "asc" ? "desc" : "asc";
      } else {
        state.sortKey = nextKey;
        state.sortDirection = "asc";
      }

      saveSettings();
      render();
    });
  });

  elements.canvasTableBody.addEventListener("click", handleActionClick);
  elements.canvasTableBody.addEventListener("change", handleTableChange);
  elements.canvasFolderTab.addEventListener("click", handleActionClick);

  document.querySelectorAll("[data-close-dialog]").forEach((button) => {
    button.addEventListener("click", () => closeDialog(button.dataset.closeDialog));
  });

  [elements.canvasDialog, elements.featureDialog, elements.effectDialog].forEach((dialog) => {
    dialog.addEventListener("close", () => clearDialogMessage(dialog));
  });

  elements.canvasForm.addEventListener("submit", handleCanvasSubmit);
  elements.featureForm.addEventListener("submit", handleFeatureSubmit);
  elements.effectForm.addEventListener("submit", handleEffectSubmit);

  elements.canvasDeleteButton.addEventListener("click", handleCanvasDelete);
  elements.featureDeleteButton.addEventListener("click", handleFeatureDelete);
  elements.effectDeleteButton.addEventListener("click", handleEffectDelete);

  elements.canvasSeedFeatureEnabled.addEventListener("change", syncSeedFeatureSection);
  elements.canvasWidthInput.addEventListener("change", syncSeedFeatureDimensions);
  elements.canvasHeightInput.addEventListener("change", syncSeedFeatureDimensions);

  elements.effectTypeInput.addEventListener("change", () => {
    const definition = getEffectDefinition(elements.effectTypeInput.value);
    if (!definition) {
      return;
    }

    if (!elements.effectNameInput.value || state.dialogs.effectMode === "create") {
      elements.effectNameInput.value = definition.label;
    }

    renderEffectFields(createEffectDraft(definition.type));
  });
}

function buildScheduleDayInputs() {
  elements.effectScheduleDays.innerHTML = DAY_OPTIONS.map((day) => `
    <label class="day-pill">
      <input type="checkbox" data-day-mask="${day.mask}">
      <span>${day.label}</span>
    </label>
  `).join("");
}

async function ensureEffectCatalog() {
  if (state.effectCatalog.length) {
    return;
  }

  const response = await fetchJson("/api/effects/catalog");
  state.effectCatalog = Array.isArray(response.effects) ? response.effects : [];
  populateEffectTypeOptions();
}

function populateEffectTypeOptions() {
  elements.effectTypeInput.innerHTML = state.effectCatalog
    .map((definition) => `<option value="${escapeAttribute(definition.type)}">${escapeHtml(definition.label)}</option>`)
    .join("");
}

function loadSettings() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) {
      return;
    }

    const parsed = JSON.parse(raw);
    state.filter = parsed.filter || "";
    state.sortKey = parsed.sortKey || state.sortKey;
    state.sortDirection = parsed.sortDirection || state.sortDirection;
    state.refreshMs = Number(parsed.refreshMs) || DEFAULT_REFRESH_MS;
    state.autoRefresh = parsed.autoRefresh !== false;
  } catch (_) {
  }
}

function saveSettings() {
  localStorage.setItem(
    STORAGE_KEY,
    JSON.stringify({
      filter: state.filter,
      sortKey: state.sortKey,
      sortDirection: state.sortDirection,
      refreshMs: state.refreshMs,
      autoRefresh: state.autoRefresh,
    })
  );
}

function syncControls() {
  elements.filterInput.value = state.filter;
  updateRefreshButtonLabel();
  elements.autoRefreshToggle.checked = state.autoRefresh;
}

function restartRefreshTimer() {
  if (state.refreshTimer) {
    clearInterval(state.refreshTimer);
    state.refreshTimer = null;
  }

  if (state.autoRefresh) {
    state.refreshTimer = window.setInterval(() => refreshLoop(false), state.refreshMs);
  }
}

async function refreshLoop(force) {
  if (state.requestInFlight && !force) {
    return;
  }

  state.requestInFlight = true;
  if (force) setBusy(true);

  try {
    const requests = [fetchJson("/api/controller")];
    if (!state.effectCatalog.length) {
      requests.push(fetchJson("/api/effects/catalog"));
    }

    const responses = await Promise.all(requests);
    const controllerResponse = responses[0];
    if (responses[1]) {
      state.effectCatalog = Array.isArray(responses[1].effects) ? responses[1].effects : [];
      populateEffectTypeOptions();
    }

    state.controller = controllerResponse.controller;
    state.canvases = Array.isArray(state.controller?.canvases) ? state.controller.canvases : [];
    state.rows = buildRows(state.canvases);
    state.lastUpdatedAt = new Date();
    state.connected = true;
    pruneExpandedSet();
    render();
    syncOpenDialogs();
  } catch (error) {
    console.error(error);
    state.connected = false;
    renderError(error);
  } finally {
    state.requestInFlight = false;
    if (force) setBusy(false);
    restartRefreshTimer();
  }
}

async function postCanvasAction(endpoint, canvasIds) {
  setBusy(true);
  try {
    await fetchJson(endpoint, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(canvasIds ? { canvasIds } : {}),
    });
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    renderError(error);
  } finally {
    setBusy(false);
  }
}

function confirmDestructiveAction(message) {
  if (!state.dirty) return true;
  return confirm(message);
}

async function handleNewConfig() {
  if (!confirmDestructiveAction("All unsaved changes will be lost. Create a new empty configuration?")) {
    return;
  }

  setBusy(true);
  try {
    await fetchJson("/api/controller/reset", { method: "POST" });
    state.dirty = false;
    state.expandedCanvasIds.clear();
    state.selectedCanvases.clear();
    state.selectedFeatures = {};
    state.selectedEffects = {};
    state.sectionExpansions = {};
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    renderError(error);
  } finally {
    setBusy(false);
  }
}

function handleLoadConfig() {
  state.loadMode = "load";
  elements.configFileInput.value = "";
  elements.configFileInput.click();
}

function positionMenuAtButton(menu, button) {
  menu.classList.add('open');
  const rect = button.getBoundingClientRect();
  const menuWidth = menu.offsetWidth;
  menu.style.top  = (rect.bottom + 4) + 'px';
  menu.style.left = (rect.right - menuWidth) + 'px';
}

function toggleLoadDropdown() {
  const menu = elements.loadDropdownMenu;
  const wasHidden = !menu.classList.contains('open');
  elements.refreshDropdownMenu.classList.remove('open');
  if (wasHidden) {
    positionMenuAtButton(menu, elements.loadDropdownToggle);
  } else {
    menu.classList.remove('open');
  }
}

function toggleRefreshDropdown() {
  const menu = elements.refreshDropdownMenu;
  const wasHidden = !menu.classList.contains('open');
  elements.loadDropdownMenu.classList.remove('open');
  if (wasHidden) {
    positionMenuAtButton(menu, elements.refreshDropdownToggle);
  } else {
    menu.classList.remove('open');
  }
}

function handleRefreshDropdownClick(event) {
  const item = event.target.closest('[data-interval]');
  if (!item) return;
  elements.refreshDropdownMenu.classList.remove('open');
  state.refreshMs = Number(item.dataset.interval) || DEFAULT_REFRESH_MS;
  saveSettings();
  restartRefreshTimer();
  updateRefreshButtonLabel();
}

const INTERVAL_LABELS = { 1000: '1 sec', 2000: '2 sec', 5000: '5 sec', 10000: '10 sec' };
function updateRefreshButtonLabel() {
  elements.refreshIntervalButton.textContent = '\u23F1 ' + (INTERVAL_LABELS[state.refreshMs] || (state.refreshMs / 1000) + ' sec');
}

function handleLoadDropdownClick(event) {
  const item = event.target.closest("[data-load-mode]");
  if (!item) return;
  elements.loadDropdownMenu.classList.remove('open');
  state.loadMode = item.dataset.loadMode;
  elements.configFileInput.value = "";
  elements.configFileInput.click();
}

function normalizeImportedControllerConfig(json) {
  const payload = json?.controller || json;

  if (Array.isArray(payload)) {
    return { canvases: payload };
  }

  if (payload && typeof payload === "object") {
    return payload;
  }

  throw new Error("Configuration must be a controller object or an array of canvases.");
}

async function handleConfigFileSelected(event) {
  const file = event.target.files[0];
  if (!file) return;

  const mode = state.loadMode || "load";
  console.log(`[${mode}] File selected:`, file.name, file.size, "bytes");

  setBusy(true);
  try {
    const text = await file.text();
    const json = JSON.parse(text);
    const payload = normalizeImportedControllerConfig(json);
    const incomingCanvases = Array.isArray(payload.canvases) ? payload.canvases : [];
    console.log(`[${mode}] Parsed OK, canvases:`, incomingCanvases.length);

    if (incomingCanvases.length === 0) {
      alert(`The selected file contains 0 canvases.\n\nFile: ${file.name}`);
      return;
    }

    if (mode === "insert") {
      await insertCanvases(incomingCanvases);
    } else {
      await fetchJson("/api/controller", {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      console.log("[Load] PUT succeeded");
      state.dirty = false;
    }

    state.expandedCanvasIds.clear();
    state.selectedCanvases.clear();
    state.selectedFeatures = {};
    state.selectedEffects = {};
    state.sectionExpansions = {};
    await refreshLoop(true);
    console.log(`[${mode}] Refresh complete, canvases:`, state.canvases.length);
  } catch (error) {
    console.error(`[${mode}] Error:`, error);
    alert(`Failed to ${mode} configuration: ` + (error instanceof Error ? error.message : String(error)));
  } finally {
    setBusy(false);
  }
}

async function insertCanvases(incomingCanvases) {
  // Find insertion index: position of first selected canvas, or end of list
  let insertIdx = state.canvases.length;
  if (state.selectedCanvases.size > 0) {
    const idx = state.canvases.findIndex((c) => state.selectedCanvases.has(c.id));
    if (idx >= 0) insertIdx = idx;
  }

  const existingByName = new Map(
    state.canvases.map((c, i) => [(c.name || "").toLowerCase(), { canvas: c, index: i }])
  );

  let repeatAnswer = null;
  const toInsert = [];           // new canvases to splice in at insertIdx
  const toOverwrite = new Map(); // index -> replacement canvas data

  for (const incoming of incomingCanvases) {
    const name = incoming.name || "Unnamed";
    const match = existingByName.get(name.toLowerCase());

    if (match) {
      let overwrite;
      if (repeatAnswer !== null) {
        overwrite = repeatAnswer;
      } else {
        const result = await showCollisionDialog(name);
        overwrite = result.overwrite;
        if (result.repeat) repeatAnswer = overwrite;
      }

      if (!overwrite) {
        console.log(`[Insert] Skipping duplicate: ${name}`);
        continue;
      }

      // Replace in-place at the existing position
      toOverwrite.set(match.index, incoming);
      console.log(`[Insert] Will overwrite canvas: ${name}`);
    } else {
      toInsert.push(incoming);
      existingByName.set(name.toLowerCase(), { canvas: incoming, index: -1 });
      console.log(`[Insert] Will add canvas: ${name}`);
    }
  }

  // Build the merged canvas array
  const merged = state.canvases.map((c, i) => toOverwrite.has(i) ? toOverwrite.get(i) : c);
  merged.splice(insertIdx, 0, ...toInsert);

  // PUT the entire controller with the merged order
  await fetchJson("/api/controller", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ canvases: merged }),
  });

  state.dirty = true;
  console.log(`[Insert] PUT complete — ${toInsert.length} added at index ${insertIdx}, ${toOverwrite.size} overwritten`);
}

function showCollisionDialog(canvasName) {
  return new Promise((resolve) => {
    elements.collisionDialogTitle.textContent = `Duplicate: ${canvasName}`;
    elements.collisionDialogCopy.textContent =
      `A canvas named "${canvasName}" already exists. Overwrite it with the imported version?`;
    elements.collisionRepeatCheckbox.checked = false;

    function cleanup() {
      elements.collisionYesButton.removeEventListener("click", onYes);
      elements.collisionNoButton.removeEventListener("click", onNo);
      elements.collisionDialog.close();
    }
    function onYes() {
      cleanup();
      resolve({ overwrite: true, repeat: elements.collisionRepeatCheckbox.checked });
    }
    function onNo() {
      cleanup();
      resolve({ overwrite: false, repeat: elements.collisionRepeatCheckbox.checked });
    }

    elements.collisionYesButton.addEventListener("click", onYes);
    elements.collisionNoButton.addEventListener("click", onNo);
    elements.collisionDialog.showModal();
  });
}

async function handleSaveConfig() {
  try {
    const data = await fetchJson("/api/controller");
    const controller = data.controller || data;
    const blob = new Blob([JSON.stringify(controller, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "config.led";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    state.dirty = false;
  } catch (error) {
    console.error(error);
    alert("Failed to save configuration: " + (error instanceof Error ? error.message : String(error)));
  }
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  if (!response.ok) {
    const message = await response.text();
    throw new Error(message || `Request failed: ${response.status}`);
  }

  const text = await response.text();
  return text ? JSON.parse(text) : {};
}

function buildRows(canvases) {
  const nowSeconds = Date.now() / 1000;
  const rows = [];

  canvases.forEach((canvas) => {
    const features = Array.isArray(canvas.features) ? canvas.features : [];
    features.forEach((feature) => {
      const stats = feature.lastClientResponse || null;
      const isConnected = Boolean(feature.isConnected);
      const reconnectCount = Number.isFinite(feature.reconnectCount) ? feature.reconnectCount : null;
      const featureFps = isConnected && stats ? Number(stats.fpsDrawing || 0) : null;
      const canvasFps = Number(canvas?.effectsManager?.fps || 0);
      const queueDepth = Number.isFinite(feature.queueDepth) ? feature.queueDepth : null;
      const currentClock = isConnected && stats && Number.isFinite(stats.currentClock) ? Number(stats.currentClock) : null;
      const deltaSeconds = currentClock !== null ? currentClock - nowSeconds : null;
      const bandwidth = Number.isFinite(feature.bytesPerSecond) ? Number(feature.bytesPerSecond) : 0;
      const sizeSort = Number(feature.width || 0) * Number(feature.height || 0);
      rows.push({
        canvasId: Number(canvas.id || 0),
        featureId: Number(feature.id || 0),
        canvasName: String(canvas.name || ""),
        featureName: String(feature.friendlyName || ""),
        hostName: String(feature.hostName || ""),
        currentEffectName: String(canvas.currentEffectName || "---"),
        reconnectCount,
        featureFps,
        canvasFps,
        queueDepth,
        bandwidth,
        deltaSeconds,
        sizeSort,
        isConnected,
      });
    });
  });

  return rows;
}

function render() {
  renderConnectionStatus();
  renderSummary();
  renderTable();
  updateSortButtons();
}

function renderConnectionStatus() {
  const controller = state.controller || {};
  elements.apiPortValue.textContent = controller.port ?? "--";
  elements.webPortValue.textContent = controller.webuiport ?? "--";
  elements.connectionStatus.classList.toggle("online", state.connected);
  elements.connectionStatus.classList.toggle("offline", !state.connected);
  elements.connectionStatusText.textContent = state.connected
    ? `Live telemetry | ${state.canvases.length} canvases`
    : "API unavailable";

  if (state.lastUpdatedAt) {
  }
}

function renderSummary() {
  const canvases = state.canvases;
  const features = state.rows;
  const onlineCount = features.filter((row) => row.isConnected).length;
  const totalBandwidth = features.reduce((sum, row) => sum + row.bandwidth, 0);
  const uniqueEffects = new Set(canvases.map((canvas) => String(canvas.currentEffectName || "---")));
  const connectedRows = features.filter((row) => row.isConnected && row.featureFps !== null);
  const avgMeasuredFps = connectedRows.length
    ? connectedRows.reduce((sum, row) => sum + row.featureFps, 0) / connectedRows.length
    : 0;
  const avgTargetFps = connectedRows.length
    ? connectedRows.reduce((sum, row) => sum + row.canvasFps, 0) / connectedRows.length
    : 0;
  const maxQueue = features.reduce((max, row) => Math.max(max, row.queueDepth || 0), 0);
  const deltaCandidates = features.map((row) => row.deltaSeconds).filter((delta) => typeof delta === "number");
  const worstDelta = deltaCandidates.length
    ? Math.max(...deltaCandidates.map((delta) => Math.abs(delta)))
    : null;

  elements.summaryCanvases.textContent = String(canvases.length);
  elements.summaryCanvasesNote.textContent = `${canvases.filter((canvas) => (canvas.features || []).length > 1).length} multi-feature surfaces`;

  elements.summaryFeatures.textContent = String(features.length);
  elements.summaryFeaturesNote.textContent = `${features.reduce((sum, row) => sum + row.sizeSort, 0).toLocaleString()} addressable LEDs`;

  elements.summaryOnline.textContent = String(onlineCount);
  elements.summaryOnlineNote.textContent = `${features.length - onlineCount} offline`;

  elements.summaryFps.textContent = connectedRows.length
    ? `${avgMeasuredFps.toFixed(1)} / ${avgTargetFps.toFixed(1)}`
    : "--";
  elements.summaryFpsNote.textContent = connectedRows.length ? "Measured / target" : "No connected clients";

  elements.summaryBandwidth.textContent = formatBandwidth(totalBandwidth);
  elements.summaryBandwidthNote.textContent = `${onlineCount || 0} active links`;

  elements.summaryQueue.textContent = String(maxQueue);
  elements.summaryQueueNote.textContent = `visual max ${QUEUE_VISUAL_MAX}`;

  elements.summaryDelta.textContent = worstDelta !== null ? `${worstDelta.toFixed(1)}s` : "--";
  elements.summaryDeltaNote.textContent = worstDelta !== null ? "Absolute clock drift" : "No clock data";

  elements.summaryEffects.textContent = String(uniqueEffects.size);
  elements.summaryEffectsNote.textContent = Array.from(uniqueEffects).slice(0, 2).join(" | ") || "No effects";
}

function renderTable() {
  const visibleCanvases = state.canvases
    .filter(matchesCanvasFilter)
    .slice()
    .sort(compareCanvases);

  elements.tableMetaText.textContent = `${visibleCanvases.length} visible canvas${visibleCanvases.length === 1 ? "" : "es"}`;

  if (!visibleCanvases.length) {
    elements.canvasTableBody.innerHTML = '<tr><td colspan="9" class="empty-cell">No matching canvases.</td></tr>';
    elements.canvasFolderTab.innerHTML = renderCanvasFolderTab();
    return;
  }

  elements.canvasTableBody.innerHTML = visibleCanvases.map((canvas) => renderCanvasRows(canvas)).join("");
  elements.canvasFolderTab.innerHTML = renderCanvasFolderTab();
}

function renderCanvasRows(canvas) {
  const canvasId = Number(canvas.id || 0);
  const expanded = state.expandedCanvasIds.has(canvasId);
  const sections = ensureSectionExpansion(canvasId);
  const features = Array.isArray(canvas.features) ? canvas.features : [];
  const effects = Array.isArray(canvas?.effectsManager?.effects) ? canvas.effectsManager.effects : [];
  const targetFps = Number(canvas?.effectsManager?.fps || 0);
  const canvasStatus = classifyCanvasStatus(canvas);
  const isSelected = state.selectedCanvases.has(canvasId);

  return `
    <tr class="canvas-main-row${isSelected ? " selected" : ""}">
      <td class="checkbox-cell">
        <input type="checkbox" class="row-checkbox" data-action="toggle-canvas-select" data-canvas-id="${canvasId}" ${isSelected ? "checked" : ""}>
      </td>
      <td class="expander-column">
        <button class="expand-button" type="button" data-action="toggle-expand" data-canvas-id="${canvasId}" aria-label="${expanded ? "Collapse" : "Expand"} canvas">
          ${expanded ? "v" : ">"}
        </button>
      </td>
      <td>
        <div class="canvas-name">
          <span>${escapeHtml(canvas.name || "Unnamed Canvas")}</span>
        </div>
      </td>
      <td class="mono">${Number(canvas.width || 0)}x${Number(canvas.height || 0)}</td>
      <td>${features.length}</td>
      <td>${effects.length}</td>
      <td>${escapeHtml(canvas.currentEffectName || "No Effect Selected")}</td>
      <td class="mono">${targetFps}</td>
      <td><span class="status-chip ${canvasStatus.className}">${canvasStatus.label}</span></td>
    </tr>
    ${expanded ? `
      <tr>
        <td colspan="9" class="detail-cell">
          <div class="detail-shell">
            <div class="detail-stack">
              <section class="nested-card card-yellow">
                ${renderSectionHeaderRow({
                  canvasId,
                  section: "features",
                  expanded: sections.features,
                  title: `Features (${features.length})`,
                  firstColumnWidth: NESTED_PRIMARY_COLUMN_WIDTH,
                  columns: FEATURE_TABLE_COLUMNS,
                })}
                ${sections.features ? `
                  <div class="table-wrap nested-table-wrap">
                    <table class="nested-table">
                      ${renderNestedColgroup(NESTED_PRIMARY_COLUMN_WIDTH, FEATURE_TABLE_COLUMNS)}
                      <tbody>
                        ${renderFeaturesTable(canvas)}
                      </tbody>
                    </table>
                  </div>
                  ${renderFolderTab(canvasId, "feature", features.length)}
                ` : ""}
              </section>
              <section class="nested-card card-cyan">
                ${renderSectionHeaderRow({
                  canvasId,
                  section: "effects",
                  expanded: sections.effects,
                  title: `Effects (${effects.length})`,
                  firstColumnWidth: NESTED_PRIMARY_COLUMN_WIDTH,
                  columns: EFFECT_TABLE_COLUMNS,
                })}
                ${sections.effects ? `
                  <div class="table-wrap nested-table-wrap">
                    <table class="nested-table">
                      ${renderNestedColgroup(NESTED_PRIMARY_COLUMN_WIDTH, EFFECT_TABLE_COLUMNS)}
                      <tbody>
                        ${renderEffectsTable(canvas)}
                      </tbody>
                    </table>
                  </div>
                  ${renderFolderTab(canvasId, "effect", effects.length)}
                ` : ""}
              </section>
            </div>
          </div>
        </td>
      </tr>
    ` : ""}
  `;
}

function renderFeaturesTable(canvas) {
  const features = Array.isArray(canvas.features) ? canvas.features : [];
  const canvasId = Number(canvas.id || 0);
  const selected = state.selectedFeatures[canvasId] || new Set();

  if (!features.length) {
    return `<tr><td colspan="${FEATURE_TABLE_COLUMNS.length + 1}" class="empty-cell">No features configured.</td></tr>`;
  }

  return features.map((feature) => {
    const featureId = Number(feature.id || 0);
    const isSelected = selected.has(featureId);
    return `
    <tr class="selectable-row ${isSelected ? "selected" : ""}" data-canvas-id="${canvasId}" data-feature-id="${featureId}">
      <td class="checkbox-cell">
        <input type="checkbox" class="row-checkbox" data-action="toggle-feature-select" data-canvas-id="${canvasId}" data-feature-id="${featureId}" ${isSelected ? "checked" : ""}>
      </td>
      <td>${escapeHtml(feature.friendlyName || `Feature ${feature.id}`)}</td>
      <td class="mono">${escapeHtml(feature.hostName || "--")}</td>
      <td class="mono align-right">${Number(feature.port || 0)}</td>
      <td class="mono align-right">${Number(feature.width || 0)}</td>
      <td class="mono align-right">${Number(feature.height || 0)}</td>
      <td class="mono align-right">${Number(feature.offsetX || 0)}</td>
      <td class="mono align-right">${Number(feature.offsetY || 0)}</td>
      <td class="mono align-right">${Number(feature.channel || 0)}</td>
      <td>${renderQueueMeter(feature)}</td>
      <td>${renderBufferMeter(feature)}</td>
      <td><span class="status-chip ${feature.isConnected ? "good" : "danger"}">${feature.isConnected ? "ONLINE" : "OFFLINE"}</span></td>
    </tr>
  `;
  }).join("");
}

function renderSectionHeaderRow({ canvasId, section, expanded, title, firstColumnWidth, columns }) {
  // The toggle spans the checkbox column (firstColumnWidth) + the Name column (columns[0])
  const toggleWidth = `calc(${firstColumnWidth} + ${columns[0].width})`;
  const remainingColumns = columns.slice(1);
  const templateColumns = [toggleWidth, ...remainingColumns.map((column) => column.width)].join(" ");
  return `
    <div class="section-header-grid" style="grid-template-columns:${templateColumns}">
      <button
        class="section-toggle"
        type="button"
        data-action="toggle-section"
        data-canvas-id="${canvasId}"
        data-section="${section}"
        aria-expanded="${expanded ? "true" : "false"}"
      >
        <span class="section-toggle-icon">${expanded ? "v" : ">"}</span>
        <span>${escapeHtml(title)}</span>
      </button>
      ${remainingColumns.map((column) => `<div class="section-header-cell${column.align === "right" ? " align-right" : ""}">${escapeHtml(column.label)}</div>`).join("")}
    </div>
  `;
}

function renderNestedColgroup(firstColumnWidth, columns) {
  return `
    <colgroup>
      <col style="width:${firstColumnWidth}">
      ${columns.map((column) => `<col style="width:${column.width}">`).join("")}
    </colgroup>
  `;
}

function renderEffectsTable(canvas) {
  const effects = Array.isArray(canvas?.effectsManager?.effects) ? canvas.effectsManager.effects : [];
  const canvasId = Number(canvas.id || 0);
  const selected = state.selectedEffects[canvasId] || new Set();
  const currentEffectIndex = Number.isInteger(canvas?.effectsManager?.currentEffectIndex)
    ? canvas.effectsManager.currentEffectIndex
    : -1;

  if (!effects.length) {
    return `<tr><td colspan="${EFFECT_TABLE_COLUMNS.length + 1}" class="empty-cell">No effects configured.</td></tr>`;
  }

  return effects.map((effect, index) => {
    const definition = getEffectDefinition(effect.type);
    const isSelected = selected.has(index);
    return `
      <tr class="selectable-row ${isSelected ? "selected" : ""}" data-canvas-id="${canvasId}" data-effect-index="${index}">
        <td class="checkbox-cell">
          <input type="checkbox" class="row-checkbox" data-action="toggle-effect-select" data-canvas-id="${canvasId}" data-effect-index="${index}" ${isSelected ? "checked" : ""}>
        </td>
        <td>${escapeHtml(effect.name || `Effect ${index + 1}`)}${index === currentEffectIndex ? ' <span class="status-chip good">CURRENT</span>' : ""}</td>
        <td>${escapeHtml(definition ? definition.label : effect.type || "Unknown")}</td>
        <td title="${escapeAttribute(summarizeEffect(effect, definition))}">${escapeHtml(summarizeEffect(effect, definition))}</td>
        <td>${escapeHtml(formatSchedule(effect.schedule))}</td>
      </tr>
    `;
  }).join("");
}

function renderCanvasFolderTab() {
  const selectedCount = state.selectedCanvases.size;
  const canEdit = selectedCount === 1;
  const canDelete = selectedCount >= 1;
  const selectedId = canEdit ? Array.from(state.selectedCanvases)[0] : null;

  return `
    <div class="folder-tab folder-tab-green">
      <a href="#" class="folder-tab-action" data-action="add-canvas">&#x2795; New</a>
      <span class="folder-tab-sep">&#x2502;</span>
      <a href="#" class="folder-tab-action ${canEdit ? "" : "disabled"}" 
         ${canEdit ? `data-action="edit-canvas" data-canvas-id="${selectedId}"` : ""}>
        &#x270E; Edit
      </a>
      <span class="folder-tab-sep">&#x2502;</span>
      <a href="#" class="folder-tab-action ${canDelete ? "" : "disabled"}" 
         ${canDelete ? `data-action="delete-selected-canvases"` : ""}>
        &#x1F5D1; Delete${selectedCount > 1 ? ` (${selectedCount})` : ""}
      </a>
    </div>
  `;
}

function renderFolderTab(canvasId, type, itemCount) {
  const isFeature = type === "feature";
  const selectedSet = isFeature 
    ? (state.selectedFeatures[canvasId] || new Set())
    : (state.selectedEffects[canvasId] || new Set());
  const selectedCount = selectedSet.size;
  const canEdit = selectedCount === 1;
  const canDelete = selectedCount >= 1;
  const colorClass = isFeature ? "yellow" : "cyan";

  const getSelectedId = () => {
    if (selectedCount !== 1) return null;
    return Array.from(selectedSet)[0];
  };

  const selectedId = getSelectedId();

  return `
    <div class="folder-tab folder-tab-${colorClass}">
      <a href="#" class="folder-tab-action" data-action="add-${type}" data-canvas-id="${canvasId}">&#x2795; New</a>
      <span class="folder-tab-sep">&#x2502;</span>
      <a href="#" class="folder-tab-action ${canEdit ? "" : "disabled"}" 
         ${canEdit ? `data-action="edit-${type}" data-canvas-id="${canvasId}" data-${isFeature ? "feature-id" : "effect-index"}="${selectedId}"` : ""}>
        &#x270E; Edit
      </a>
      <span class="folder-tab-sep">&#x2502;</span>
      <a href="#" class="folder-tab-action ${canDelete ? "" : "disabled"}" 
         ${canDelete ? `data-action="delete-selected-${type}s" data-canvas-id="${canvasId}"` : ""}>
        &#x1F5D1; Delete${selectedCount > 1 ? ` (${selectedCount})` : ""}
      </a>
    </div>
  `;
}

function renderBufferMeter(feature) {
  const response = feature.lastClientResponse || {};
  const bufferPos = Number(response.bufferPos || 0);
  const bufferSize = Number(response.bufferSize || 0);
  const maxSize = Number(feature.clientBufferCount || bufferSize || 1);
  const ratio = Math.min(Math.max(bufferPos / Math.max(maxSize, 1), 0), 1);
  const status = ratio > 0.8 ? "danger" : ratio > 0.45 ? "warning" : "good";

  return `
    <div class="queue-meter ${status}">
      <span class="queue-meter-fill" style="width:${(ratio * 100).toFixed(1)}%"></span>
      <span class="queue-meter-value">${bufferPos}/${maxSize}</span>
    </div>
  `;
}

function renderQueueMeter(feature) {
  const queueDepth = Number(feature.queueDepth || 0);
  const queueMaxSize = Number(feature.queueMaxSize || QUEUE_VISUAL_MAX);
  const ratio = Math.min(Math.max(queueDepth / Math.max(queueMaxSize || 1, 1), 0), 1);
  const status = queueDepth > 0.8 * queueMaxSize ? "danger" : queueDepth > 0.45 * queueMaxSize ? "warning" : "good";

  return `
    <div class="queue-meter ${status}">
      <span class="queue-meter-fill" style="width:${(ratio * 100).toFixed(1)}%"></span>
      <span class="queue-meter-value">${queueDepth}/${queueMaxSize}</span>
    </div>
  `;
}

function updateSortButtons() {
  elements.sortButtons.forEach((button) => {
    const active = button.dataset.sortKey === state.sortKey;
    button.classList.toggle("active", active);
    button.dataset.sortDir = active ? (state.sortDirection === "asc" ? "^" : "v") : "";
  });
}

function renderError(error) {
  elements.connectionStatus.classList.remove("online");
  elements.connectionStatus.classList.add("offline");
  elements.connectionStatusText.textContent = "API unavailable";
}

function setBusy(isBusy) {
  [
    elements.refreshButton,
    elements.startAllButton,
    elements.stopAllButton,
  ].forEach((button) => {
    button.disabled = isBusy;
  });
}

function handleActionClick(event) {
  // Skip checkbox inputs — they are handled by the change event in handleTableChange
  if (event.target instanceof HTMLInputElement && event.target.type === "checkbox") {
    return;
  }

  const trigger = event.target.closest("[data-action]");
  if (!(trigger instanceof HTMLElement)) {
    return;
  }

  // Prevent default for anchor elements to avoid page jump
  if (trigger.tagName === "A") {
    event.preventDefault();
  }

  const action = trigger.dataset.action;
  const canvasId = Number(trigger.dataset.canvasId || 0) || null;
  const featureId = Number(trigger.dataset.featureId || 0) || null;
  const effectIndex = trigger.dataset.effectIndex !== undefined ? Number(trigger.dataset.effectIndex) : null;

  switch (action) {
    case "toggle-expand":
      if (canvasId !== null) {
        toggleExpandedCanvas(canvasId);
      }
      break;
    case "toggle-section":
      if (canvasId !== null && trigger.dataset.section) {
        toggleSectionExpansion(canvasId, trigger.dataset.section);
      }
      break;
    case "start":
      if (canvasId !== null) {
        postCanvasAction("/api/canvases/start", [canvasId]);
      }
      break;
    case "stop":
      if (canvasId !== null) {
        postCanvasAction("/api/canvases/stop", [canvasId]);
      }
      break;
    case "toggle-canvas-select":
      if (canvasId !== null) {
        toggleCanvasSelection(canvasId);
      }
      break;
    case "delete-selected-canvases":
      deleteSelectedCanvases();
      break;
    case "add-canvas":
      openCanvasDialog("create");
      break;
    case "edit-canvas":
      if (canvasId !== null) {
        openCanvasDialog("edit", canvasId);
      }
      break;
    case "delete-canvas":
      if (canvasId !== null) {
        deleteCanvasById(canvasId);
      }
      break;
    case "add-feature":
      if (canvasId !== null) {
        openFeatureDialog("create", canvasId);
      }
      break;
    case "edit-feature":
      if (canvasId !== null && featureId !== null) {
        openFeatureDialog("edit", canvasId, featureId);
      }
      break;
    case "delete-feature":
      if (canvasId !== null && featureId !== null) {
        deleteFeatureById(canvasId, featureId);
      }
      break;
    case "add-effect":
      if (canvasId !== null) {
        openEffectDialog("create", canvasId);
      }
      break;
    case "edit-effect":
      if (canvasId !== null && effectIndex !== null) {
        openEffectDialog("edit", canvasId, effectIndex);
      }
      break;
    case "delete-effect":
      if (canvasId !== null && effectIndex !== null) {
        deleteEffectByIndex(canvasId, effectIndex);
      }
      break;
    case "toggle-feature-select":
      if (canvasId !== null && featureId !== null) {
        toggleFeatureSelection(canvasId, featureId);
      }
      break;
    case "toggle-effect-select":
      if (canvasId !== null && effectIndex !== null) {
        toggleEffectSelection(canvasId, effectIndex);
      }
      break;
    case "delete-selected-features":
      if (canvasId !== null) {
        deleteSelectedFeatures(canvasId);
      }
      break;
    case "delete-selected-effects":
      if (canvasId !== null) {
        deleteSelectedEffects(canvasId);
      }
      break;
    default:
      break;
  }
}

async function handleTableChange(event) {
  const target = event.target;
  
  // Handle checkbox selection changes
  if (target instanceof HTMLInputElement && target.type === "checkbox" && target.classList.contains("row-checkbox")) {
    const action = target.dataset.action;
    const canvasId = Number(target.dataset.canvasId || 0);
    
    if (action === "toggle-canvas-select") {
      if (canvasId) {
        toggleCanvasSelection(canvasId);
      }
    } else if (action === "toggle-feature-select") {
      const featureId = Number(target.dataset.featureId || 0);
      if (canvasId && featureId !== null) {
        toggleFeatureSelection(canvasId, featureId);
      }
    } else if (action === "toggle-effect-select") {
      const effectIndex = Number(target.dataset.effectIndex);
      if (canvasId && effectIndex !== null) {
        toggleEffectSelection(canvasId, effectIndex);
      }
    }
    return;
  }

  if (!(target instanceof HTMLSelectElement)) {
    return;
  }

  if (target.dataset.action !== "current-effect") {
    return;
  }

  const canvasId = Number(target.dataset.canvasId || 0);
  if (!canvasId) {
    return;
  }

  try {
    await saveCanvasMutation(canvasId, (canvas) => {
      ensureEffectsManager(canvas);
      canvas.effectsManager.currentEffectIndex = Number(target.value);
    });
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    renderError(error);
  }
}

function toggleExpandedCanvas(canvasId) {
  if (state.expandedCanvasIds.has(canvasId)) {
    state.expandedCanvasIds.delete(canvasId);
  } else {
    ensureSectionExpansion(canvasId);
    state.expandedCanvasIds.add(canvasId);
  }
  renderTable();
}

function toggleCanvasSelection(canvasId) {
  if (state.selectedCanvases.has(canvasId)) {
    state.selectedCanvases.delete(canvasId);
  } else {
    state.selectedCanvases.add(canvasId);
  }
  renderTable();
}

async function deleteSelectedCanvases() {
  const canvasIds = Array.from(state.selectedCanvases);
  if (canvasIds.length === 0) return;

  const count = canvasIds.length;
  if (!confirm(`Delete ${count} selected canvas${count > 1 ? "es" : ""} and all their features?`)) {
    return;
  }

  try {
    for (const canvasId of canvasIds) {
      await fetchJson(`/api/canvases/${canvasId}`, { method: "DELETE" });
      state.expandedCanvasIds.delete(canvasId);
    }
    state.selectedCanvases.clear();
    state.dirty = true;
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    renderError(error);
  }
}

function toggleFeatureSelection(canvasId, featureId) {
  if (!state.selectedFeatures[canvasId]) {
    state.selectedFeatures[canvasId] = new Set();
  }
  const selected = state.selectedFeatures[canvasId];
  if (selected.has(featureId)) {
    selected.delete(featureId);
  } else {
    selected.add(featureId);
  }
  renderTable();
}

function toggleEffectSelection(canvasId, effectIndex) {
  if (!state.selectedEffects[canvasId]) {
    state.selectedEffects[canvasId] = new Set();
  }
  const selected = state.selectedEffects[canvasId];
  if (selected.has(effectIndex)) {
    selected.delete(effectIndex);
  } else {
    selected.add(effectIndex);
  }
  renderTable();
}

async function deleteSelectedFeatures(canvasId) {
  const selected = state.selectedFeatures[canvasId];
  if (!selected || selected.size === 0) return;
  
  const featureIds = Array.from(selected);
  const count = featureIds.length;
  
  if (!confirm(`Delete ${count} selected feature${count > 1 ? "s" : ""}?`)) {
    return;
  }
  
  try {
    for (const featureId of featureIds) {
      await deleteFeatureById(canvasId, featureId, null, true);
    }
    state.selectedFeatures[canvasId] = new Set();
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    renderError(error);
  }
}

async function deleteSelectedEffects(canvasId) {
  const selected = state.selectedEffects[canvasId];
  if (!selected || selected.size === 0) return;
  
  // Sort descending so we delete from end first (to preserve indices)
  const indices = Array.from(selected).sort((a, b) => b - a);
  const count = indices.length;
  
  if (!confirm(`Delete ${count} selected effect${count > 1 ? "s" : ""}?`)) {
    return;
  }
  
  try {
    for (const effectIndex of indices) {
      await deleteEffectByIndex(canvasId, effectIndex, null, true);
    }
    state.selectedEffects[canvasId] = new Set();
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    renderError(error);
  }
}

function pruneExpandedSet() {
  const validIds = new Set(state.canvases.map((canvas) => Number(canvas.id || 0)));
  state.expandedCanvasIds.forEach((canvasId) => {
    if (!validIds.has(canvasId)) {
      state.expandedCanvasIds.delete(canvasId);
    }
  });
  state.selectedCanvases.forEach((canvasId) => {
    if (!validIds.has(canvasId)) {
      state.selectedCanvases.delete(canvasId);
    }
  });
  Object.keys(state.sectionExpansions).forEach((canvasId) => {
    if (!validIds.has(Number(canvasId))) {
      delete state.sectionExpansions[canvasId];
    }
  });
  
  // Prune stale feature selections
  Object.keys(state.selectedFeatures).forEach((canvasId) => {
    const numCanvasId = Number(canvasId);
    if (!validIds.has(numCanvasId)) {
      delete state.selectedFeatures[canvasId];
    } else {
      const canvas = state.canvases.find((c) => Number(c.id) === numCanvasId);
      const validFeatureIds = new Set((canvas?.features || []).map((f) => Number(f.id)));
      state.selectedFeatures[canvasId].forEach((featureId) => {
        if (!validFeatureIds.has(featureId)) {
          state.selectedFeatures[canvasId].delete(featureId);
        }
      });
    }
  });
  
  // Prune stale effect selections
  Object.keys(state.selectedEffects).forEach((canvasId) => {
    const numCanvasId = Number(canvasId);
    if (!validIds.has(numCanvasId)) {
      delete state.selectedEffects[canvasId];
    } else {
      const canvas = state.canvases.find((c) => Number(c.id) === numCanvasId);
      const effectCount = canvas?.effectsManager?.effects?.length || 0;
      state.selectedEffects[canvasId].forEach((effectIndex) => {
        if (effectIndex >= effectCount) {
          state.selectedEffects[canvasId].delete(effectIndex);
        }
      });
    }
  });
}

function ensureSectionExpansion(canvasId) {
  const key = String(canvasId);
  if (!state.sectionExpansions[key]) {
    state.sectionExpansions[key] = {
      features: true,
      effects: true,
    };
  }
  return state.sectionExpansions[key];
}

function toggleSectionExpansion(canvasId, section) {
  if (section !== "features" && section !== "effects") {
    return;
  }
  const sections = ensureSectionExpansion(canvasId);
  sections[section] = !sections[section];
  renderTable();
}

function matchesCanvasFilter(canvas) {
  if (!state.filter) {
    return true;
  }

  const features = Array.isArray(canvas.features) ? canvas.features : [];
  const effects = Array.isArray(canvas?.effectsManager?.effects) ? canvas.effectsManager.effects : [];
  const searchIndex = [
    canvas.name,
    canvas.currentEffectName,
    ...features.map((feature) => feature.friendlyName),
    ...features.map((feature) => feature.hostName),
    ...effects.map((effect) => effect.name),
  ].join(" ").toLowerCase();

  return searchIndex.includes(state.filter.toLowerCase());
}

function compareCanvases(left, right) {
  const direction = state.sortDirection === "asc" ? 1 : -1;
  const leftValue = sortableCanvasValue(left, state.sortKey);
  const rightValue = sortableCanvasValue(right, state.sortKey);

  if (typeof leftValue === "string" || typeof rightValue === "string") {
    return String(leftValue).localeCompare(String(rightValue)) * direction;
  }

  return ((leftValue || 0) - (rightValue || 0)) * direction;
}

function sortableCanvasValue(canvas, key) {
  switch (key) {
    case "dimensions":
      return Number(canvas.width || 0) * Number(canvas.height || 0);
    case "featureCount":
      return Array.isArray(canvas.features) ? canvas.features.length : 0;
    case "effectCount":
      return Array.isArray(canvas?.effectsManager?.effects) ? canvas.effectsManager.effects.length : 0;
    case "currentEffectName":
      return String(canvas.currentEffectName || "");
    case "fps":
      return Number(canvas?.effectsManager?.fps || 0);
    case "status":
      return classifyCanvasStatus(canvas).rank;
    case "name":
    default:
      return String(canvas.name || "");
  }
}

function classifyCanvasStatus(canvas) {
  const features = Array.isArray(canvas.features) ? canvas.features : [];
  const onlineCount = features.filter((feature) => feature.isConnected).length;
  const isRunning = Boolean(canvas?.effectsManager?.running);

  if (!features.length) {
    return { className: isRunning ? "warning" : "danger", label: isRunning ? "NO FEATURES" : "IDLE", rank: isRunning ? 2 : 0 };
  }

  if (!isRunning) {
    return { className: "danger", label: "STOPPED", rank: 0 };
  }

  if (onlineCount === features.length) {
    return { className: "good", label: "ONLINE", rank: 3 };
  }

  if (onlineCount > 0) {
    return { className: "warning", label: "PARTIAL", rank: 2 };
  }

  return { className: "danger", label: "OFFLINE", rank: 1 };
}

function openCanvasDialog(mode, canvasId = null) {
  clearDialogMessage(elements.canvasDialog);
  state.dialogs.canvasMode = mode;
  state.dialogs.canvasId = canvasId;

  if (mode === "create") {
    elements.canvasDialogEyebrow.textContent = "Canvas Builder";
    elements.canvasDialogTitle.textContent = "Create Canvas";
    elements.canvasDialogCopy.textContent = "Define the canvas dimensions, timing, and optional first feature.";
    elements.canvasDeleteButton.hidden = true;
    elements.canvasSeedFeatureSection.hidden = false;
    elements.canvasNameInput.value = "";
    elements.canvasWidthInput.value = "100";
    elements.canvasHeightInput.value = "1";
    elements.canvasFpsInput.value = "30";
    elements.canvasRunningInput.checked = true;
    elements.canvasSeedFeatureEnabled.checked = false;
    elements.seedFeatureFriendlyNameInput.value = "";
    elements.seedFeatureHostNameInput.value = "";
    elements.seedFeaturePortInput.value = String(DEFAULT_PORT);
    elements.seedFeatureWidthInput.value = "100";
    elements.seedFeatureHeightInput.value = "1";
    elements.seedFeatureOffsetXInput.value = "0";
    elements.seedFeatureOffsetYInput.value = "0";
    elements.seedFeatureChannelInput.value = "0";
    elements.seedFeatureClientBufferCountInput.value = "8";
    elements.seedFeatureReversedInput.checked = false;
    elements.seedFeatureRedGreenSwapInput.checked = false;
    syncSeedFeatureSection();
  } else {
    const canvas = getCanvasById(canvasId);
    if (!canvas) {
      return;
    }

    elements.canvasDialogEyebrow.textContent = `Canvas ${canvas.id}`;
    elements.canvasDialogTitle.textContent = `Edit Canvas | ${canvas.name}`;
    elements.canvasDialogCopy.textContent = "Resize, rename, and retune the selected canvas.";
    elements.canvasDeleteButton.hidden = false;
    elements.canvasSeedFeatureSection.hidden = true;
    elements.canvasNameInput.value = canvas.name || "";
    elements.canvasWidthInput.value = String(canvas.width || 1);
    elements.canvasHeightInput.value = String(canvas.height || 1);
    elements.canvasFpsInput.value = String(canvas?.effectsManager?.fps || 30);
    elements.canvasRunningInput.checked = Boolean(canvas?.effectsManager?.running);
  }

  elements.canvasDialog.showModal();
}

function openFeatureDialog(mode, canvasId, featureId = null) {
  clearDialogMessage(elements.featureDialog);
  state.dialogs.featureMode = mode;
  state.dialogs.featureCanvasId = canvasId;
  state.dialogs.featureId = featureId;

  const canvas = getCanvasById(canvasId);
  if (!canvas) {
    return;
  }

  elements.featureDialogEyebrow.textContent = canvas.name || "Canvas";
  elements.featureDialogCopy.textContent = `Canvas ${canvas.name} | ${canvas.width}x${canvas.height}`;

  if (mode === "create") {
    elements.featureDialogTitle.textContent = "Add Feature";
    elements.featureDeleteButton.hidden = true;
    elements.featureFriendlyNameInput.value = "";
    elements.featureHostNameInput.value = "";
    elements.featurePortInput.value = String(DEFAULT_PORT);
    elements.featureWidthInput.value = String(canvas.width || 1);
    elements.featureHeightInput.value = String(canvas.height || 1);
    elements.featureOffsetXInput.value = "0";
    elements.featureOffsetYInput.value = "0";
    elements.featureChannelInput.value = "0";
    elements.featureClientBufferCountInput.value = "8";
    elements.featureReversedInput.checked = false;
    elements.featureRedGreenSwapInput.checked = false;
  } else {
    const feature = getFeatureById(canvasId, featureId);
    if (!feature) {
      return;
    }

    elements.featureDialogTitle.textContent = `Edit Feature | ${feature.friendlyName}`;
    elements.featureDeleteButton.hidden = false;
    elements.featureFriendlyNameInput.value = feature.friendlyName || "";
    elements.featureHostNameInput.value = feature.hostName || "";
    elements.featurePortInput.value = String(feature.port || DEFAULT_PORT);
    elements.featureWidthInput.value = String(feature.width || 1);
    elements.featureHeightInput.value = String(feature.height || 1);
    elements.featureOffsetXInput.value = String(feature.offsetX || 0);
    elements.featureOffsetYInput.value = String(feature.offsetY || 0);
    elements.featureChannelInput.value = String(feature.channel || 0);
    elements.featureClientBufferCountInput.value = String(feature.clientBufferCount || 8);
    elements.featureReversedInput.checked = Boolean(feature.reversed);
    elements.featureRedGreenSwapInput.checked = Boolean(feature.redGreenSwap);
  }

  elements.featureDialog.showModal();
}

function openEffectDialog(mode, canvasId, effectIndex = null) {
  clearDialogMessage(elements.effectDialog);
  state.dialogs.effectMode = mode;
  state.dialogs.effectCanvasId = canvasId;
  state.dialogs.effectIndex = effectIndex;

  const canvas = getCanvasById(canvasId);
  if (!canvas) {
    return;
  }

  const existingEffect = mode === "edit"
    ? (canvas?.effectsManager?.effects || [])[effectIndex] || null
    : null;
  const selectedType = existingEffect?.type || state.effectCatalog[0]?.type || "";

  elements.effectDialogEyebrow.textContent = canvas.name || "Canvas";
  elements.effectDialogTitle.textContent = mode === "create" ? "Add Effect" : `Edit Effect | ${existingEffect?.name || ""}`;
  elements.effectDialogCopy.textContent = `Canvas ${canvas.name} | ${canvas.width}x${canvas.height}`;
  elements.effectDeleteButton.hidden = mode !== "edit";
  elements.effectTypeInput.value = selectedType;
  elements.effectNameInput.value = existingEffect?.name || getEffectDefinition(selectedType)?.label || "";
  renderEffectFields(existingEffect || createEffectDraft(selectedType));
  populateScheduleFields(existingEffect?.schedule || null);

  elements.effectDialog.showModal();
}

function renderEffectFields(effectDraft) {
  const definition = getEffectDefinition(elements.effectTypeInput.value);
  if (!definition) {
    elements.effectFields.innerHTML = "";
    return;
  }

  const draft = effectDraft || createEffectDraft(definition.type);
  const sections = definition.fields.map((field) => renderDynamicField(field, getValueAtPath(draft, field.path)));
  elements.effectFields.innerHTML = sections.length
    ? `<div class="dialog-grid compact-grid">${sections.join("")}</div>`
    : '<div class="subpanel"><p class="muted">This effect exposes no additional parameters.</p></div>';
}

function renderDynamicField(field, value) {
  const inputId = `effect-field-${field.path.replaceAll(".", "-")}`;
  if (field.input === "checkbox") {
    return `
      <label class="toggle dialog-toggle">
        <input id="${inputId}" type="checkbox" data-field-path="${escapeAttribute(field.path)}" data-input-kind="checkbox" ${value ? "checked" : ""}>
        <span>${escapeHtml(field.label)}</span>
      </label>
    `;
  }

  if (field.input === "json") {
    return `
      <label class="field textarea-field wide-field">
        <span>${escapeHtml(field.label)}</span>
        <textarea id="${inputId}" rows="6" data-field-path="${escapeAttribute(field.path)}" data-input-kind="json">${escapeHtml(JSON.stringify(value ?? [], null, 2))}</textarea>
      </label>
    `;
  }

  if (field.input === "color") {
    return `
      <label class="field">
        <span>${escapeHtml(field.label)}</span>
        <input id="${inputId}" type="color" data-field-path="${escapeAttribute(field.path)}" data-input-kind="color" value="${rgbToHex(value || { r: 255, g: 255, b: 255 })}">
      </label>
    `;
  }

  const type = field.input === "number" ? "number" : "text";
  const minAttr = field.min !== undefined ? ` min="${field.min}"` : "";
  const maxAttr = field.max !== undefined ? ` max="${field.max}"` : "";
  const stepAttr = field.step !== undefined ? ` step="${field.step}"` : "";

  return `
    <label class="field ${field.input === "text" ? "wide-field" : ""}">
      <span>${escapeHtml(field.label)}</span>
      <input id="${inputId}" type="${type}" data-field-path="${escapeAttribute(field.path)}" data-input-kind="${escapeAttribute(field.input)}" value="${escapeAttribute(value ?? "")}"${minAttr}${maxAttr}${stepAttr}>
    </label>
  `;
}

async function handleCanvasSubmit(event) {
  event.preventDefault();
  clearDialogMessage(elements.canvasDialog);

  try {
    const canvasPayload = {
      id: -1,
      name: requireText(elements.canvasNameInput.value, "Canvas name is required."),
      width: requirePositiveInt(elements.canvasWidthInput.value, "Canvas width must be greater than zero."),
      height: requirePositiveInt(elements.canvasHeightInput.value, "Canvas height must be greater than zero."),
      currentEffectName: "No Effect Selected",
      features: [],
      effectsManager: {
        fps: requirePositiveInt(elements.canvasFpsInput.value, "Canvas FPS must be greater than zero."),
        currentEffectIndex: -1,
        running: elements.canvasRunningInput.checked,
        effects: [],
      },
    };

    if (state.dialogs.canvasMode === "create") {
      const createResponse = await fetchJson("/api/canvases", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(canvasPayload),
      });

      const canvasId = Number(createResponse.id);
      if (elements.canvasSeedFeatureEnabled.checked) {
        const seedFeature = readFeatureInputs("seed");
        await fetchJson(`/api/canvases/${canvasId}/features`, {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: JSON.stringify(seedFeature),
        });
      }
    } else {
      const canvasId = state.dialogs.canvasId;
      await saveCanvasMutation(canvasId, (canvas) => {
        canvas.name = canvasPayload.name;
        canvas.width = canvasPayload.width;
        canvas.height = canvasPayload.height;
        ensureEffectsManager(canvas);
        canvas.effectsManager.fps = canvasPayload.effectsManager.fps;
        canvas.effectsManager.running = canvasPayload.effectsManager.running;
      });
    }

    closeDialog("canvasDialog");
    state.dirty = true;
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    setDialogMessage(elements.canvasDialog, error instanceof Error ? error.message : String(error));
  }
}

async function handleFeatureSubmit(event) {
  event.preventDefault();
  clearDialogMessage(elements.featureDialog);

  try {
    const canvasId = state.dialogs.featureCanvasId;
    const payload = readFeatureInputs("feature");

    if (state.dialogs.featureMode === "create") {
      await fetchJson(`/api/canvases/${canvasId}/features`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(payload),
      });
    } else {
      const featureId = state.dialogs.featureId;
      await saveCanvasMutation(canvasId, (canvas) => {
        const features = Array.isArray(canvas.features) ? canvas.features : [];
        const index = features.findIndex((feature) => Number(feature.id) === Number(featureId));
        if (index === -1) {
          throw new Error(`Feature not found: ${featureId}`);
        }
        features[index] = { ...features[index], ...payload, id: featureId };
        canvas.features = features;
      });
    }

    closeDialog("featureDialog");
    state.dirty = true;
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    setDialogMessage(elements.featureDialog, error instanceof Error ? error.message : String(error));
  }
}

async function handleEffectSubmit(event) {
  event.preventDefault();
  clearDialogMessage(elements.effectDialog);

  try {
    const canvasId = state.dialogs.effectCanvasId;
    const payload = readEffectInputs();

    if (state.dialogs.effectMode === "create") {
      await fetchJson(`/api/canvases/${canvasId}/effects`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(payload),
      });
    } else {
      await fetchJson(`/api/canvases/${canvasId}/effects/${state.dialogs.effectIndex}`, {
        method: "PUT",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(payload),
      });
    }

    closeDialog("effectDialog");
    state.dirty = true;
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    setDialogMessage(elements.effectDialog, error instanceof Error ? error.message : String(error));
  }
}

async function handleCanvasDelete() {
  const canvasId = state.dialogs.canvasId;
  if (canvasId === null) {
    return;
  }
  await deleteCanvasById(canvasId, elements.canvasDialog);
}

async function handleFeatureDelete() {
  const canvasId = state.dialogs.featureCanvasId;
  const featureId = state.dialogs.featureId;
  if (canvasId === null || featureId === null) {
    return;
  }
  await deleteFeatureById(canvasId, featureId, elements.featureDialog);
}

async function handleEffectDelete() {
  const canvasId = state.dialogs.effectCanvasId;
  const effectIndex = state.dialogs.effectIndex;
  if (canvasId === null || effectIndex === null) {
    return;
  }
  await deleteEffectByIndex(canvasId, effectIndex, elements.effectDialog);
}

async function deleteCanvasById(canvasId, dialogSource = null) {
  if (!window.confirm("Delete this canvas and all of its features?")) {
    return;
  }

  try {
    await fetchJson(`/api/canvases/${canvasId}`, { method: "DELETE" });
    state.dirty = true;
    if (dialogSource) {
      dialogSource.close();
    }
    state.expandedCanvasIds.delete(Number(canvasId));
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    if (dialogSource) {
      setDialogMessage(dialogSource, error instanceof Error ? error.message : String(error));
    } else {
      renderError(error);
    }
  }
}

async function deleteFeatureById(canvasId, featureId, dialogSource = null, skipConfirm = false) {
  if (!skipConfirm && !window.confirm("Delete this feature?")) {
    return;
  }

  try {
    await fetchJson(`/api/canvases/${canvasId}/features/${featureId}`, { method: "DELETE" });
    state.dirty = true;
    if (dialogSource) {
      dialogSource.close();
    }
    if (!skipConfirm) {
      await refreshLoop(true);
    }
  } catch (error) {
    console.error(error);
    if (dialogSource) {
      setDialogMessage(dialogSource, error instanceof Error ? error.message : String(error));
    } else {
      renderError(error);
    }
  }
}

async function deleteEffectByIndex(canvasId, effectIndex, dialogSource = null, skipConfirm = false) {
  if (!skipConfirm && !window.confirm("Delete this effect?")) {
    return;
  }

  try {
    await fetchJson(`/api/canvases/${canvasId}/effects/${effectIndex}`, { method: "DELETE" });
    state.dirty = true;
    if (dialogSource) {
      dialogSource.close();
    }
    if (!skipConfirm) {
      await refreshLoop(true);
    }
  } catch (error) {
    console.error(error);
    if (dialogSource) {
      setDialogMessage(dialogSource, error instanceof Error ? error.message : String(error));
    } else {
      renderError(error);
    }
  }
}

async function saveCanvasMutation(canvasId, mutator) {
  const canvas = getCanvasById(canvasId);
  if (!canvas) {
    throw new Error(`Canvas not found: ${canvasId}`);
  }

  const payload = cloneCanvas(canvas);
  ensureEffectsManager(payload);
  mutator(payload);
  normalizeCanvasPayload(payload);

  await fetchJson(`/api/canvases/${canvasId}`, {
    method: "PUT",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });
}

function readFeatureInputs(prefix) {
  const lookup = prefix === "seed"
    ? {
        friendlyName: elements.seedFeatureFriendlyNameInput,
        hostName: elements.seedFeatureHostNameInput,
        port: elements.seedFeaturePortInput,
        width: elements.seedFeatureWidthInput,
        height: elements.seedFeatureHeightInput,
        offsetX: elements.seedFeatureOffsetXInput,
        offsetY: elements.seedFeatureOffsetYInput,
        channel: elements.seedFeatureChannelInput,
        clientBufferCount: elements.seedFeatureClientBufferCountInput,
        reversed: elements.seedFeatureReversedInput,
        redGreenSwap: elements.seedFeatureRedGreenSwapInput,
      }
    : {
        friendlyName: elements.featureFriendlyNameInput,
        hostName: elements.featureHostNameInput,
        port: elements.featurePortInput,
        width: elements.featureWidthInput,
        height: elements.featureHeightInput,
        offsetX: elements.featureOffsetXInput,
        offsetY: elements.featureOffsetYInput,
        channel: elements.featureChannelInput,
        clientBufferCount: elements.featureClientBufferCountInput,
        reversed: elements.featureReversedInput,
        redGreenSwap: elements.featureRedGreenSwapInput,
      };

  return {
    hostName: requireText(lookup.hostName.value, "Feature host is required."),
    friendlyName: requireText(lookup.friendlyName.value, "Feature name is required."),
    port: requirePositiveInt(lookup.port.value, "Feature port must be greater than zero."),
    width: requirePositiveInt(lookup.width.value, "Feature width must be greater than zero."),
    height: requirePositiveInt(lookup.height.value, "Feature height must be greater than zero."),
    offsetX: requireNonNegativeInt(lookup.offsetX.value, "Feature offset X must be zero or greater."),
    offsetY: requireNonNegativeInt(lookup.offsetY.value, "Feature offset Y must be zero or greater."),
    reversed: lookup.reversed.checked,
    channel: requireNonNegativeInt(lookup.channel.value, "Feature channel must be zero or greater."),
    redGreenSwap: lookup.redGreenSwap.checked,
    clientBufferCount: requirePositiveInt(lookup.clientBufferCount.value, "Buffer depth must be greater than zero."),
  };
}

function readEffectInputs() {
  const type = elements.effectTypeInput.value;
  const definition = getEffectDefinition(type);
  if (!definition) {
    throw new Error("Unknown effect type selected.");
  }

  const payload = createEffectDraft(type);
  payload.name = requireText(elements.effectNameInput.value, "Effect name is required.");
  payload.type = type;

  const fields = Array.from(elements.effectFields.querySelectorAll("[data-field-path]"));
  fields.forEach((field) => {
    const path = field.dataset.fieldPath;
    const inputKind = field.dataset.inputKind;
    let value;

    if (inputKind === "checkbox") {
      value = field.checked;
    } else if (inputKind === "color") {
      value = hexToRgb(field.value);
    } else if (inputKind === "json") {
      value = JSON.parse(field.value || "[]");
    } else if (inputKind === "number") {
      value = Number(field.value);
      if (!Number.isFinite(value)) {
        throw new Error(`${path} must be numeric.`);
      }
    } else {
      value = field.value;
    }

    setValueAtPath(payload, path, value);
  });

  const schedule = readScheduleInputs();
  if (schedule) {
    payload.schedule = schedule;
  } else {
    delete payload.schedule;
  }

  return payload;
}

function readScheduleInputs() {
  const daysOfWeek = getSelectedDaysMask();
  const startTime = normalizeTimeValue(elements.effectScheduleStartTimeInput.value);
  const stopTime = normalizeTimeValue(elements.effectScheduleStopTimeInput.value);
  const startDate = elements.effectScheduleStartDateInput.value || null;
  const stopDate = elements.effectScheduleStopDateInput.value || null;

  const schedule = {};
  if (daysOfWeek) {
    schedule.daysOfWeek = daysOfWeek;
  }
  if (startTime) {
    schedule.startTime = startTime;
  }
  if (stopTime) {
    schedule.stopTime = stopTime;
  }
  if (startDate) {
    schedule.startDate = startDate;
  }
  if (stopDate) {
    schedule.stopDate = stopDate;
  }

  return Object.keys(schedule).length ? schedule : null;
}

function populateScheduleFields(schedule) {
  const daysMask = schedule?.daysOfWeek || 0;
  elements.effectScheduleDays.querySelectorAll("input[data-day-mask]").forEach((input) => {
    const mask = Number(input.dataset.dayMask || 0);
    input.checked = Boolean(daysMask & mask);
  });
  elements.effectScheduleStartTimeInput.value = schedule?.startTime ? timeToInputValue(schedule.startTime) : "";
  elements.effectScheduleStopTimeInput.value = schedule?.stopTime ? timeToInputValue(schedule.stopTime) : "";
  elements.effectScheduleStartDateInput.value = schedule?.startDate || "";
  elements.effectScheduleStopDateInput.value = schedule?.stopDate || "";
}

function getSelectedDaysMask() {
  return Array.from(elements.effectScheduleDays.querySelectorAll("input[data-day-mask]"))
    .filter((input) => input.checked)
    .reduce((mask, input) => mask | Number(input.dataset.dayMask || 0), 0);
}

function syncSeedFeatureSection() {
  const enabled = elements.canvasSeedFeatureEnabled.checked;
  if (enabled) {
    syncSeedFeatureDimensions();
  }
  elements.canvasSeedFeatureSection.classList.toggle("subpanel-disabled", !enabled);
  elements.canvasSeedFeatureSection.querySelectorAll("input").forEach((input) => {
    if (input === elements.canvasSeedFeatureEnabled) {
      return;
    }
    input.disabled = !enabled;
  });
}

function syncSeedFeatureDimensions() {
  if (elements.canvasSeedFeatureEnabled.checked) {
    elements.seedFeatureWidthInput.value = elements.canvasWidthInput.value;
    elements.seedFeatureHeightInput.value = elements.canvasHeightInput.value;
  }
}

function closeDialog(dialogId) {
  const dialog = elements[dialogId];
  if (dialog?.open) {
    dialog.close();
  }
}

function clearDialogMessage(dialog) {
  const message = dialog.querySelector(".dialog-message");
  if (!message) {
    return;
  }
  message.textContent = "";
  message.classList.remove("error-message");
}

function setDialogMessage(dialog, text) {
  const message = dialog.querySelector(".dialog-message");
  if (!message) {
    return;
  }
  message.textContent = text;
  message.classList.add("error-message");
}

function syncOpenDialogs() {
  if (elements.effectDialog.open) {
    const canvas = getCanvasById(state.dialogs.effectCanvasId);
    if (!canvas) {
      closeDialog("effectDialog");
    }
  }

  if (elements.featureDialog.open && state.dialogs.featureMode === "edit") {
    const feature = getFeatureById(state.dialogs.featureCanvasId, state.dialogs.featureId);
    if (!feature) {
      closeDialog("featureDialog");
    }
  }

  if (elements.canvasDialog.open && state.dialogs.canvasMode === "edit") {
    const canvas = getCanvasById(state.dialogs.canvasId);
    if (!canvas) {
      closeDialog("canvasDialog");
    }
  }
}

function getCanvasById(canvasId) {
  return state.canvases.find((canvas) => Number(canvas.id) === Number(canvasId)) || null;
}

function getFeatureById(canvasId, featureId) {
  const canvas = getCanvasById(canvasId);
  if (!canvas) {
    return null;
  }
  return (canvas.features || []).find((feature) => Number(feature.id) === Number(featureId)) || null;
}

function getEffectDefinition(type) {
  return state.effectCatalog.find((definition) => definition.type === type) || null;
}

function createEffectDraft(type) {
  const definition = getEffectDefinition(type);
  const defaults = deepClone(definition?.defaults || {});
  return {
    ...defaults,
    type,
    name: definition?.label || "Effect",
  };
}

function ensureEffectsManager(canvas) {
  if (!canvas.effectsManager) {
    canvas.effectsManager = {
      fps: 30,
      currentEffectIndex: -1,
      running: true,
      effects: [],
    };
  }

  if (!Array.isArray(canvas.effectsManager.effects)) {
    canvas.effectsManager.effects = [];
  }
}

function normalizeCanvasPayload(canvas) {
  canvas.id = Number(canvas.id || 0);
  ensureEffectsManager(canvas);
  canvas.effectsManager.fps = Number(canvas.effectsManager.fps || 30);
  if (!Array.isArray(canvas.features)) {
    canvas.features = [];
  }
}

function cloneCanvas(canvas) {
  return deepClone(canvas);
}

function summarizeEffect(effect, definition) {
  if (!definition || !Array.isArray(definition.fields) || !definition.fields.length) {
    return definition ? definition.label : "No editable parameters.";
  }

  const values = definition.fields.slice(0, 3).map((field) => {
    const value = getValueAtPath(effect, field.path);
    if (field.input === "checkbox") {
      return `${field.label}: ${value ? "On" : "Off"}`;
    }
    if (field.input === "color") {
      return `${field.label}: ${rgbToHex(value || { r: 255, g: 255, b: 255 })}`;
    }
    if (field.input === "json") {
      return `${field.label}: ${Array.isArray(value) ? value.length : 0} entries`;
    }
    return `${field.label}: ${value}`;
  });

  return values.join(" | ");
}

function formatSchedule(schedule) {
  if (!schedule) {
    return "No schedule restrictions.";
  }

  const parts = [];
  if (schedule.daysOfWeek) {
    const labels = DAY_OPTIONS.filter((day) => schedule.daysOfWeek & day.mask).map((day) => day.label);
    if (labels.length) {
      parts.push(labels.join(", "));
    }
  }
  if (schedule.startTime || schedule.stopTime) {
    parts.push(`${schedule.startTime || "--:--:--"} to ${schedule.stopTime || "--:--:--"}`);
  }
  if (schedule.startDate || schedule.stopDate) {
    parts.push(`${schedule.startDate || "open"} -> ${schedule.stopDate || "open"}`);
  }

  return parts.join(" | ") || "No schedule restrictions.";
}

function formatTimestamp(date) {
  return date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function formatBandwidth(bytesPerSecond) {
  const value = Number(bytesPerSecond || 0);
  if (value >= 1024 * 1024) {
    return `${(value / (1024 * 1024)).toFixed(2)} MB/s`;
  }
  if (value >= 1024) {
    return `${(value / 1024).toFixed(1)} KB/s`;
  }
  return `${value.toFixed(0)} B/s`;
}

function requireText(value, message) {
  const normalized = String(value || "").trim();
  if (!normalized) {
    throw new Error(message);
  }
  return normalized;
}

function requirePositiveInt(value, message) {
  const parsed = Number(value);
  if (!Number.isInteger(parsed) || parsed <= 0) {
    throw new Error(message);
  }
  return parsed;
}

function requireNonNegativeInt(value, message) {
  const parsed = Number(value);
  if (!Number.isInteger(parsed) || parsed < 0) {
    throw new Error(message);
  }
  return parsed;
}

function normalizeTimeValue(value) {
  if (!value) {
    return null;
  }
  return `${value}:00`;
}

function timeToInputValue(value) {
  return String(value || "").slice(0, 5);
}

function getValueAtPath(target, path) {
  return path.split(".").reduce((value, key) => value?.[key], target);
}

function setValueAtPath(target, path, value) {
  const keys = path.split(".");
  let cursor = target;
  keys.slice(0, -1).forEach((key) => {
    if (!cursor[key] || typeof cursor[key] !== "object") {
      cursor[key] = {};
    }
    cursor = cursor[key];
  });
  cursor[keys[keys.length - 1]] = value;
}

function rgbToHex(color) {
  const r = clampColor(color.r);
  const g = clampColor(color.g);
  const b = clampColor(color.b);
  return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
}

function hexToRgb(hex) {
  const normalized = String(hex || "#ffffff").replace("#", "");
  return {
    r: parseInt(normalized.slice(0, 2), 16),
    g: parseInt(normalized.slice(2, 4), 16),
    b: parseInt(normalized.slice(4, 6), 16),
  };
}

function clampColor(value) {
  return Math.max(0, Math.min(255, Number(value || 0)));
}

function toHex(value) {
  return value.toString(16).padStart(2, "0");
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function escapeAttribute(value) {
  return escapeHtml(value).replaceAll("`", "&#96;");
}

function deepClone(value) {
  return JSON.parse(JSON.stringify(value));
}
