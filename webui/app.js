const STORAGE_KEY = "ndscpp.dashboard.settings.v1";
const DEFAULT_REFRESH_MS = 2000;
const QUEUE_VISUAL_MAX = 25;
const DEFAULT_PORT = 49152;
const DAY_OPTIONS = [
  { label: "Sun", mask: 0x01 },
  { label: "Mon", mask: 0x02 },
  { label: "Tue", mask: 0x04 },
  { label: "Wed", mask: 0x08 },
  { label: "Thu", mask: 0x10 },
  { label: "Fri", mask: 0x20 },
  { label: "Sat", mask: 0x40 },
];

const state = {
  controller: null,
  rows: [],
  canvases: [],
  effectCatalog: [],
  filter: "",
  sortKey: "canvasName",
  sortDirection: "asc",
  refreshMs: DEFAULT_REFRESH_MS,
  autoRefresh: true,
  refreshTimer: null,
  requestInFlight: false,
  lastUpdatedAt: null,
  connected: false,
  dialogs: {
    canvasMode: "create",
    canvasId: null,
    featureMode: "create",
    featureCanvasId: null,
    featureId: null,
    effectsCanvasId: null,
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
  elements.refreshIntervalSelect = document.getElementById("refreshIntervalSelect");
  elements.autoRefreshToggle = document.getElementById("autoRefreshToggle");
  elements.refreshButton = document.getElementById("refreshButton");
  elements.startAllButton = document.getElementById("startAllButton");
  elements.stopAllButton = document.getElementById("stopAllButton");
  elements.newCanvasButton = document.getElementById("newCanvasButton");

  elements.connectionStatus = document.getElementById("connectionStatus");
  elements.connectionStatusText = document.getElementById("connectionStatusText");
  elements.apiPortValue = document.getElementById("apiPortValue");
  elements.webPortValue = document.getElementById("webPortValue");
  elements.lastUpdatedText = document.getElementById("lastUpdatedText");
  elements.tableMetaText = document.getElementById("tableMetaText");
  elements.canvasMetaText = document.getElementById("canvasMetaText");
  elements.metricsTableBody = document.getElementById("metricsTableBody");
  elements.canvasList = document.getElementById("canvasList");
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

  elements.effectsDialog = document.getElementById("effectsDialog");
  elements.effectsDialogEyebrow = document.getElementById("effectsDialogEyebrow");
  elements.effectsDialogTitle = document.getElementById("effectsDialogTitle");
  elements.effectsDialogCopy = document.getElementById("effectsDialogCopy");
  elements.effectsDialogMessage = document.getElementById("effectsDialogMessage");
  elements.effectsList = document.getElementById("effectsList");
  elements.addEffectButton = document.getElementById("addEffectButton");
  elements.currentEffectSelect = document.getElementById("currentEffectSelect");

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

  elements.refreshIntervalSelect.addEventListener("change", (event) => {
    state.refreshMs = Number(event.target.value) || DEFAULT_REFRESH_MS;
    saveSettings();
    restartRefreshTimer();
  });

  elements.autoRefreshToggle.addEventListener("change", (event) => {
    state.autoRefresh = event.target.checked;
    saveSettings();
    restartRefreshTimer();
  });

  elements.refreshButton.addEventListener("click", () => refreshLoop(true));
  elements.startAllButton.addEventListener("click", () => postCanvasAction("/api/canvases/start"));
  elements.stopAllButton.addEventListener("click", () => postCanvasAction("/api/canvases/stop"));
  elements.newCanvasButton.addEventListener("click", () => openCanvasDialog("create"));

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

  elements.metricsTableBody.addEventListener("click", handleActionClick);
  elements.canvasList.addEventListener("click", handleActionClick);
  elements.effectsList.addEventListener("click", handleActionClick);

  document.querySelectorAll("[data-close-dialog]").forEach((button) => {
    button.addEventListener("click", () => closeDialog(button.dataset.closeDialog));
  });

  [elements.canvasDialog, elements.featureDialog, elements.effectsDialog, elements.effectDialog].forEach((dialog) => {
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

  elements.addEffectButton.addEventListener("click", () => {
    if (state.dialogs.effectsCanvasId !== null) {
      openEffectDialog("create", state.dialogs.effectsCanvasId);
    }
  });

  elements.currentEffectSelect.addEventListener("change", async (event) => {
    const canvasId = state.dialogs.effectsCanvasId;
    if (canvasId === null) {
      return;
    }

    try {
      const currentIndex = Number(event.target.value);
      await saveCanvasMutation(canvasId, (canvas) => {
        ensureEffectsManager(canvas);
        canvas.effectsManager.currentEffectIndex = currentIndex;
      });
      await refreshLoop(true);
      renderEffectsDialog();
    } catch (error) {
      console.error(error);
      setDialogMessage(elements.effectsDialog, error instanceof Error ? error.message : String(error));
    }
  });

  elements.effectTypeInput.addEventListener("change", () => {
    const definition = getEffectDefinition(elements.effectTypeInput.value);
    if (definition) {
      if (!elements.effectNameInput.value || state.dialogs.effectMode === "create") {
        elements.effectNameInput.value = definition.label;
      }
      renderEffectFields(createEffectDraft(definition.type));
    }
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
  const options = state.effectCatalog
    .map((definition) => `<option value="${escapeAttribute(definition.type)}">${escapeHtml(definition.label)}</option>`)
    .join("");
  elements.effectTypeInput.innerHTML = options;
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
  elements.refreshIntervalSelect.value = String(state.refreshMs);
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
  setBusy(true);

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
    render();
    syncOpenDialogs();
  } catch (error) {
    console.error(error);
    state.connected = false;
    renderError(error);
  } finally {
    state.requestInFlight = false;
    setBusy(false);
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
      const queueMaxSize = Number.isFinite(feature.queueMaxSize) ? feature.queueMaxSize : QUEUE_VISUAL_MAX;
      const bufferPos = isConnected && stats && Number.isFinite(stats.bufferPos) ? Number(stats.bufferPos) : null;
      const bufferSize = isConnected && stats && Number.isFinite(stats.bufferSize) ? Number(stats.bufferSize) : null;
      const bufferRatio = bufferPos !== null && bufferSize ? bufferPos / Math.max(bufferSize, 1) : null;
      const currentClock = isConnected && stats && Number.isFinite(stats.currentClock) ? Number(stats.currentClock) : null;
      const deltaSeconds = currentClock !== null ? currentClock - nowSeconds : null;
      const wifiSignalRaw = isConnected && stats && Number.isFinite(stats.wifiSignal) ? Number(stats.wifiSignal) : null;
      const wifiSignalAbs = wifiSignalRaw !== null ? Math.abs(wifiSignalRaw) : null;
      const bandwidth = Number.isFinite(feature.bytesPerSecond) ? Number(feature.bytesPerSecond) : 0;
      const flashVersion = isConnected && stats && stats.flashVersion !== undefined ? String(stats.flashVersion) : "";
      const sizeSort = Number(feature.width || 0) * Number(feature.height || 0);
      const sizeText = `${feature.width || 0}x${feature.height || 0}`;
      const effectName = String(canvas.currentEffectName || "---");
      const index = [canvas.name, feature.friendlyName, feature.hostName, effectName].join(" ").toLowerCase();

      rows.push({
        id: `${canvas.id}:${feature.id}`,
        canvasId: Number(canvas.id || 0),
        featureId: Number(feature.id || 0),
        canvasName: String(canvas.name || ""),
        featureName: String(feature.friendlyName || ""),
        hostName: String(feature.hostName || ""),
        sizeText,
        sizeSort,
        reconnectCount,
        canvasFps,
        featureFps,
        queueDepth,
        queueMaxSize,
        bufferPos,
        bufferSize,
        bufferRatio,
        wifiSignalAbs,
        wifiSignalText: formatWifiSignal(wifiSignalRaw),
        bandwidth,
        deltaSeconds,
        flashVersion,
        flashVersionSort: flashVersion ? Number.parseFloat(flashVersion) || 0 : -1,
        statusText: isConnected ? "ONLINE" : "OFFLINE",
        statusSort: isConnected ? 1 : 0,
        isConnected,
        currentEffectName: effectName,
        index,
      });
    });
  });

  return rows;
}

function render() {
  renderConnectionStatus();
  renderSummary();
  renderTable();
  renderCanvasCards();
  updateSortButtons();
}

function renderConnectionStatus() {
  const controller = state.controller || {};
  elements.apiPortValue.textContent = controller.port ?? "--";
  elements.webPortValue.textContent = controller.webuiport ?? "--";

  elements.connectionStatus.classList.toggle("online", state.connected);
  elements.connectionStatus.classList.toggle("offline", !state.connected);
  elements.connectionStatusText.textContent = state.connected
    ? `Live telemetry | ${state.rows.length} feature rows`
    : "API unavailable";

  if (state.lastUpdatedAt) {
    elements.lastUpdatedText.textContent = `Last sample ${formatTimestamp(state.lastUpdatedAt)} | every ${state.refreshMs / 1000}s`;
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
  const filteredRows = state.rows.filter((row) => !state.filter || row.index.includes(state.filter.toLowerCase()));
  const sortedRows = filteredRows.slice().sort(compareRows);
  elements.tableMetaText.textContent = `${sortedRows.length} visible rows`;

  if (!sortedRows.length) {
    elements.metricsTableBody.innerHTML = '<tr><td colspan="14" class="empty-cell">No matching canvases or features.</td></tr>';
    return;
  }

  elements.metricsTableBody.innerHTML = sortedRows.map((row) => {
    const fpsStatus = classifyFps(row);
    const queueStatus = classifyQueue(row);
    const bufferStatus = classifyBuffer(row);
    const signalStatus = classifySignal(row);
    const deltaStatus = classifyDelta(row);

    return `
      <tr>
        <td class="label-strong"><button class="entity-button" type="button" data-action="edit-canvas" data-canvas-id="${row.canvasId}">${escapeHtml(row.canvasName)}</button></td>
        <td><button class="entity-button" type="button" data-action="edit-feature" data-canvas-id="${row.canvasId}" data-feature-id="${row.featureId}">${escapeHtml(row.featureName)}</button></td>
        <td class="mono">${escapeHtml(row.hostName)}</td>
        <td class="mono">${escapeHtml(row.sizeText)}</td>
        <td class="${statusClassForReconnect(row.reconnectCount)}">${formatReconnect(row.reconnectCount)}</td>
        <td>${renderFpsCell(row, fpsStatus)}</td>
        <td class="progress-cell">${renderQueueCell(row, queueStatus)}</td>
        <td class="progress-cell">${renderBufferCell(row, bufferStatus)}</td>
        <td class="${signalStatus}">${escapeHtml(row.wifiSignalText)}</td>
        <td class="mono">${row.isConnected ? formatBandwidth(row.bandwidth) : "--"}</td>
        <td class="delta-cell">${renderDeltaCell(row, deltaStatus)}</td>
        <td class="mono">${row.flashVersion ? `v${escapeHtml(row.flashVersion)}` : "--"}</td>
        <td><span class="status-chip ${row.isConnected ? "good" : "danger"}">${row.statusText}</span></td>
        <td title="${escapeHtml(row.currentEffectName)}"><button class="entity-button subtle" type="button" data-action="edit-effects" data-canvas-id="${row.canvasId}">${escapeHtml(row.currentEffectName)}</button></td>
      </tr>
    `;
  }).join("");
}

function renderCanvasCards() {
  const canvases = state.canvases.slice().sort((left, right) => String(left.name).localeCompare(String(right.name)));
  elements.canvasMetaText.textContent = `${canvases.length} configured`;

  if (!canvases.length) {
    elements.canvasList.innerHTML = '<div class="canvas-empty">No canvases loaded.</div>';
    return;
  }

  elements.canvasList.innerHTML = canvases.map((canvas) => {
    const features = Array.isArray(canvas.features) ? canvas.features : [];
    const featureCount = features.length;
    const onlineCount = features.filter((feature) => feature.isConnected).length;
    const bandwidth = features.reduce((sum, feature) => sum + Number(feature.bytesPerSecond || 0), 0);
    const queueDepth = features.reduce((max, feature) => Math.max(max, Number(feature.queueDepth || 0)), 0);
    const filterValue = [canvas.name, canvas.currentEffectName].join(" ");
    const featureButtons = features.length
      ? features.map((feature) => `
          <button class="feature-chip" type="button" data-action="edit-feature" data-canvas-id="${canvas.id}" data-feature-id="${feature.id}">
            ${escapeHtml(feature.friendlyName || `Feature ${feature.id}`)}
          </button>
        `).join("")
      : '<span class="muted">No features configured.</span>';

    return `
      <div class="canvas-card">
        <div class="canvas-card-header">
          <div>
            <div class="canvas-kicker">Canvas ${canvas.id}</div>
            <h3><button class="entity-button card-link" type="button" data-action="edit-canvas" data-canvas-id="${canvas.id}">${escapeHtml(canvas.name || "Unnamed")}</button></h3>
          </div>
          <div class="canvas-pill">${onlineCount}/${featureCount} online</div>
        </div>
        <div class="canvas-stats">
          <span>${canvas.width || 0} x ${canvas.height || 0}</span>
          <span>${featureCount} feature${featureCount === 1 ? "" : "s"}</span>
          <span>${Number(canvas?.effectsManager?.fps || 0)} fps target</span>
        </div>
        <div class="canvas-meta">
          <span>Effect <strong class="label-strong">${escapeHtml(canvas.currentEffectName || "---")}</strong></span>
          <span>Bandwidth <strong class="label-strong">${formatBandwidth(bandwidth)}</strong></span>
          <span>Queue <strong class="label-strong">${queueDepth}</strong></span>
        </div>
        <div class="feature-chip-strip">${featureButtons}</div>
        <div class="canvas-actions">
          <button class="action-button accent" type="button" data-action="start" data-canvas-id="${canvas.id}">Start</button>
          <button class="action-button warning" type="button" data-action="stop" data-canvas-id="${canvas.id}">Stop</button>
          <button class="action-button" type="button" data-action="edit-effects" data-canvas-id="${canvas.id}">Effects</button>
          <button class="action-button" type="button" data-action="add-feature" data-canvas-id="${canvas.id}">Add Feature</button>
          <button class="action-button" type="button" data-action="filter" data-canvas-id="${canvas.id}" data-filter="${escapeAttribute(filterValue)}">Focus</button>
          <button class="action-button" type="button" data-action="delete-canvas" data-canvas-id="${canvas.id}">Delete</button>
        </div>
      </div>
    `;
  }).join("");
}

function renderEffectsDialog() {
  const canvas = getCanvasById(state.dialogs.effectsCanvasId);
  if (!canvas) {
    closeDialog("effectsDialog");
    return;
  }

  const effects = Array.isArray(canvas?.effectsManager?.effects) ? canvas.effectsManager.effects : [];
  const currentEffectIndex = Number.isInteger(canvas?.effectsManager?.currentEffectIndex)
    ? canvas.effectsManager.currentEffectIndex
    : -1;

  elements.effectsDialogEyebrow.textContent = canvas.name || "Canvas";
  elements.effectsDialogTitle.textContent = `Effects | ${canvas.name || "Canvas"}`;
  elements.effectsDialogCopy.textContent = `${effects.length} configured effect${effects.length === 1 ? "" : "s"}. Active: ${canvas.currentEffectName || "---"}`;

  elements.currentEffectSelect.innerHTML = effects.length
    ? effects.map((effect, index) => `
        <option value="${index}" ${index === currentEffectIndex ? "selected" : ""}>${escapeHtml(effect.name || `Effect ${index + 1}`)}</option>
      `).join("")
    : '<option value="-1">No effects</option>';
  elements.currentEffectSelect.disabled = !effects.length;

  if (!effects.length) {
    elements.effectsList.innerHTML = '<div class="canvas-empty">No effects configured.</div>';
    return;
  }

  elements.effectsList.innerHTML = effects.map((effect, index) => {
    const definition = getEffectDefinition(effect.type);
    const typeLabel = definition ? definition.label : effect.type;
    const scheduleText = formatSchedule(effect.schedule);
    return `
      <article class="effect-card panel ${index % 2 === 0 ? "card-blue" : "card-pink"}">
        <div class="effect-card-header">
          <div>
            <div class="canvas-kicker">${index === currentEffectIndex ? "Current Effect" : "Effect Slot"}</div>
            <h3>${escapeHtml(effect.name || `Effect ${index + 1}`)}</h3>
          </div>
          <div class="canvas-pill">${escapeHtml(typeLabel)}</div>
        </div>
        <div class="effect-card-body">
          <p>${escapeHtml(summarizeEffect(effect, definition))}</p>
          <p class="muted">${escapeHtml(scheduleText)}</p>
        </div>
        <div class="canvas-actions tight-actions">
          <button class="action-button" type="button" data-action="edit-effect" data-canvas-id="${canvas.id}" data-effect-index="${index}">Edit</button>
          <button class="action-button warning" type="button" data-action="delete-effect" data-canvas-id="${canvas.id}" data-effect-index="${index}">Delete</button>
        </div>
      </article>
    `;
  }).join("");
}

function syncOpenDialogs() {
  if (elements.effectsDialog.open) {
    renderEffectsDialog();
  }

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
  elements.lastUpdatedText.textContent = error instanceof Error ? error.message : String(error);
}

function setBusy(isBusy) {
  [
    elements.refreshButton,
    elements.startAllButton,
    elements.stopAllButton,
    elements.newCanvasButton,
  ].forEach((button) => {
    button.disabled = isBusy;
  });
}

function handleActionClick(event) {
  const trigger = event.target.closest("[data-action]");
  if (!(trigger instanceof HTMLElement)) {
    return;
  }

  const action = trigger.dataset.action;
  const canvasId = Number(trigger.dataset.canvasId || 0) || null;
  const featureId = Number(trigger.dataset.featureId || 0) || null;
  const effectIndex = trigger.dataset.effectIndex !== undefined ? Number(trigger.dataset.effectIndex) : null;

  switch (action) {
    case "filter":
      state.filter = trigger.dataset.filter || "";
      elements.filterInput.value = state.filter;
      saveSettings();
      render();
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
    case "edit-effects":
      if (canvasId !== null) {
        openEffectsDialog(canvasId);
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
    default:
      break;
  }
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

function openEffectsDialog(canvasId) {
  clearDialogMessage(elements.effectsDialog);
  state.dialogs.effectsCanvasId = canvasId;
  renderEffectsDialog();
  elements.effectsDialog.showModal();
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
      <label class="toggle dialog-toggle dynamic-toggle">
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
    await refreshLoop(true);
    if (elements.effectsDialog.open && state.dialogs.effectsCanvasId === canvasId) {
      renderEffectsDialog();
    }
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

  if (!window.confirm("Delete this feature?")) {
    return;
  }

  try {
    await fetchJson(`/api/canvases/${canvasId}/features/${featureId}`, { method: "DELETE" });
    closeDialog("featureDialog");
    await refreshLoop(true);
  } catch (error) {
    console.error(error);
    setDialogMessage(elements.featureDialog, error instanceof Error ? error.message : String(error));
  }
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
    if (dialogSource) {
      dialogSource.close();
    }
    if (elements.effectsDialog.open && state.dialogs.effectsCanvasId === canvasId) {
      closeDialog("effectsDialog");
    }
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

async function deleteEffectByIndex(canvasId, effectIndex, dialogSource = null) {
  if (!window.confirm("Delete this effect?")) {
    return;
  }

  try {
    await fetchJson(`/api/canvases/${canvasId}/effects/${effectIndex}`, { method: "DELETE" });
    if (dialogSource) {
      dialogSource.close();
    }
    await refreshLoop(true);
    if (elements.effectsDialog.open && state.dialogs.effectsCanvasId === canvasId) {
      renderEffectsDialog();
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

function compareRows(left, right) {
  const direction = state.sortDirection === "asc" ? 1 : -1;
  const leftValue = sortableValue(left, state.sortKey);
  const rightValue = sortableValue(right, state.sortKey);

  if (typeof leftValue === "string" || typeof rightValue === "string") {
    return String(leftValue).localeCompare(String(rightValue)) * direction;
  }

  return ((leftValue || 0) - (rightValue || 0)) * direction;
}

function sortableValue(row, key) {
  switch (key) {
    case "featureFps":
      return row.featureFps ?? -1;
    case "queueDepth":
      return row.queueDepth ?? -1;
    case "bufferRatio":
      return row.bufferRatio ?? -1;
    case "wifiSignalAbs":
      return row.wifiSignalAbs ?? 999;
    case "bandwidth":
      return row.bandwidth ?? -1;
    case "deltaSeconds":
      return row.deltaSeconds === null ? 999 : Math.abs(row.deltaSeconds);
    default:
      return row[key];
  }
}

function classifyFps(row) {
  if (!row.isConnected || row.featureFps === null) {
    return "muted";
  }
  return row.featureFps < 0.8 * row.canvasFps ? "metric-warning" : "metric-good";
}

function classifyQueue(row) {
  if (!row.isConnected || row.queueDepth === null) {
    return "muted";
  }
  if (row.queueDepth < 100) {
    return "good";
  }
  if (row.queueDepth < 250) {
    return "warning";
  }
  return "danger";
}

function classifyBuffer(row) {
  if (!row.isConnected || row.bufferRatio === null) {
    return "muted";
  }
  if (row.bufferRatio >= 0.25 && row.bufferRatio <= 0.85) {
    return "good";
  }
  if (row.bufferRatio > 0.95) {
    return "danger";
  }
  return "warning";
}

function classifySignal(row) {
  if (!row.isConnected || row.wifiSignalAbs === null) {
    return "muted";
  }
  if (row.wifiSignalAbs >= 100) {
    return "muted";
  }
  if (row.wifiSignalAbs < 70) {
    return "metric-good";
  }
  if (row.wifiSignalAbs < 80) {
    return "metric-warning";
  }
  return "metric-danger";
}

function classifyDelta(row) {
  if (!row.isConnected || row.deltaSeconds === null) {
    return "muted";
  }
  const delta = Math.abs(row.deltaSeconds);
  if (delta < 2.0) {
    return "good";
  }
  if (delta < 3.0) {
    return "warning";
  }
  return "danger";
}

function statusClassForReconnect(reconnectCount) {
  if (reconnectCount === null || reconnectCount === 0) {
    return "metric-good";
  }
  if (reconnectCount < 3) {
    return "metric-good";
  }
  if (reconnectCount < 10) {
    return "metric-warning";
  }
  return "metric-danger";
}

function renderFpsCell(row, statusClass) {
  if (!row.isConnected || row.featureFps === null) {
    return '<span class="muted">--</span>';
  }
  return `<span class="${statusClass}">${row.featureFps}</span><span class="muted"> / ${row.canvasFps}</span>`;
}

function renderQueueCell(row, status) {
  if (!row.isConnected || row.queueDepth === null) {
    return '<span class="muted">--</span>';
  }
  const width = Math.min(100, (row.queueDepth / QUEUE_VISUAL_MAX) * 100);
  return `
    <div class="meter ${status}">
      <span class="meter-fill" style="width:${width}%"></span>
      <span class="meter-value">${row.queueDepth}/${row.queueMaxSize || QUEUE_VISUAL_MAX}</span>
    </div>
  `;
}

function renderBufferCell(row, status) {
  if (!row.isConnected || row.bufferPos === null || row.bufferSize === null) {
    return '<span class="muted">--</span>';
  }
  const width = Math.min(100, Math.max(0, row.bufferRatio * 100));
  return `
    <div class="meter ${status}">
      <span class="meter-fill" style="width:${width}%"></span>
      <span class="meter-value">${row.bufferPos}/${row.bufferSize}</span>
    </div>
  `;
}

function renderDeltaCell(row, status) {
  if (!row.isConnected || row.deltaSeconds === null) {
    return '<span class="muted">--</span>';
  }
  const displayText = Math.abs(row.deltaSeconds) > 100
    ? "Unset"
    : `${row.deltaSeconds >= 0 ? "+" : ""}${row.deltaSeconds.toFixed(1)}s`;
  const normalized = Math.max(0, Math.min(1, ((row.deltaSeconds / 3) + 1) / 2));
  return `
    <div class="delta-stack">
      <span class="mono ${status === "good" ? "metric-good" : status === "warning" ? "metric-warning" : status === "danger" ? "metric-danger" : "muted"}">${displayText}</span>
      <span class="delta-track ${status}">
        <span class="delta-marker" style="left: calc(${(normalized * 100).toFixed(2)}% - 5px)"></span>
      </span>
    </div>
  `;
}

function requireText(value, message) {
  const text = String(value || "").trim();
  if (!text) {
    throw new Error(message);
  }
  return text;
}

function requirePositiveInt(value, message) {
  const parsed = Number.parseInt(String(value), 10);
  if (!Number.isFinite(parsed) || parsed <= 0) {
    throw new Error(message);
  }
  return parsed;
}

function requireNonNegativeInt(value, message) {
  const parsed = Number.parseInt(String(value), 10);
  if (!Number.isFinite(parsed) || parsed < 0) {
    throw new Error(message);
  }
  return parsed;
}

function getValueAtPath(object, path) {
  return String(path || "").split(".").reduce((current, part) => (current && current[part] !== undefined ? current[part] : undefined), object);
}

function setValueAtPath(object, path, value) {
  const parts = String(path).split(".");
  let current = object;
  while (parts.length > 1) {
    const part = parts.shift();
    if (!current[part] || typeof current[part] !== "object") {
      current[part] = {};
    }
    current = current[part];
  }
  current[parts[0]] = value;
}

function normalizeTimeValue(value) {
  if (!value) {
    return null;
  }
  return value.length === 5 ? `${value}:00` : value;
}

function timeToInputValue(value) {
  return value ? String(value).slice(0, 5) : "";
}

function rgbToHex(color) {
  const red = clampColorComponent(color?.r);
  const green = clampColorComponent(color?.g);
  const blue = clampColorComponent(color?.b);
  return `#${red.toString(16).padStart(2, "0")}${green.toString(16).padStart(2, "0")}${blue.toString(16).padStart(2, "0")}`;
}

function hexToRgb(value) {
  const normalized = String(value || "#ffffff").replace("#", "");
  return {
    r: Number.parseInt(normalized.slice(0, 2), 16),
    g: Number.parseInt(normalized.slice(2, 4), 16),
    b: Number.parseInt(normalized.slice(4, 6), 16),
  };
}

function clampColorComponent(value) {
  const parsed = Number(value || 0);
  return Math.max(0, Math.min(255, Number.isFinite(parsed) ? parsed : 0));
}

function deepClone(value) {
  return JSON.parse(JSON.stringify(value));
}

function formatReconnect(reconnectCount) {
  if (reconnectCount === null) {
    return "--";
  }
  return String(reconnectCount);
}

function formatWifiSignal(signal) {
  if (signal === null || signal === undefined || Number.isNaN(signal)) {
    return "--";
  }
  if (Math.abs(signal) >= 100) {
    return "LAN";
  }
  return `${Math.round(signal)} dBm`;
}

function formatBandwidth(bytesPerSecond) {
  const units = ["B/s", "KB/s", "MB/s", "GB/s"];
  let value = Number(bytesPerSecond || 0);
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }
  const precision = unitIndex === 0 ? 0 : unitIndex === 1 ? 1 : 2;
  return `${value.toFixed(precision)} ${units[unitIndex]}`;
}

function formatTimestamp(date) {
  return date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function escapeAttribute(value) {
  return escapeHtml(value).replaceAll("\n", " ");
}
